﻿/*
 * Copyright 2016-2018 libfptu authors: please see AUTHORS file.
 *
 * This file is part of libfptu, aka "Fast Positive Tuples".
 *
 * libfptu is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libfptu is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libfptu.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "fptu_test.h"
#include "shuffle6.hpp"

#ifdef _MSC_VER
#pragma warning(push, 1)
#if _MSC_VER < 1900
/* LY: workaround for dead code:
       microsoft visual studio 12.0\vc\include\xtree(1826) */
#pragma warning(disable : 4702) /* unreachable code */
#endif
#endif /* _MSC_VER */

#include <algorithm>
#include <array>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

class schema_dict {
public:
  template <typename A, typename B> struct hash_pair {
    cxx14_constexpr std::size_t operator()(std::pair<A, B> const &v) const {
      const std::size_t a = std::hash<A>()(v.first);
      const std::size_t b = std::hash<B>()(v.second);
      return a * 3139864919 ^ (~b + (a >> 11));
    }
  };

  schema_dict() {}
  schema_dict(const schema_dict &) = delete;
  schema_dict(schema_dict &&) = default;
  schema_dict &operator=(schema_dict &&) = default;

#if defined(_MSC_VER) && _MSC_VER < 1910 /* obsolete and trouble full */
#pragma warning(push)
#pragma warning(disable : 4268) /* 'const' static / global data initialized    \
                                   with compiler generated default constructor \
                                   fills the object with zeros */
#endif                          /* _MSC_VER < 1910 */
  static constexpr const std::array<fptu_type, 31> fptu_types{
      {fptu_null,         fptu_uint16,       fptu_int32,
       fptu_uint32,       fptu_fp32,         fptu_int64,
       fptu_uint64,       fptu_fp64,         fptu_datetime,
       fptu_96,           fptu_128,          fptu_160,
       fptu_256,          fptu_cstr,         fptu_opaque,
       fptu_nested,       fptu_array_uint16, fptu_array_int32,
       fptu_array_uint32, fptu_array_fp32,   fptu_array_int64,
       fptu_array_uint64, fptu_array_fp64,   fptu_array_datetime,
       fptu_array_96,     fptu_array_128,    fptu_array_160,
       fptu_array_256,    fptu_array_cstr,   fptu_array_opaque,
       fptu_array_nested}};
#if defined(_MSC_VER) && _MSC_VER < 1910 /* obsolete and trouble full */
#pragma warning(push)
#endif /* _MSC_VER < 1910 */

protected:
  std::unordered_map<unsigned, std::string> map_tag2name;
  std::unordered_map<fptu::string_view, unsigned> map_name2tag;

  std::unordered_map<std::pair<unsigned, unsigned>, std::string,
                     hash_pair<unsigned, unsigned>>
      map_value2enum;
  std::unordered_map<std::pair<fptu::string_view, unsigned>, unsigned,
                     hash_pair<fptu::string_view, unsigned>>
      map_enum2value;

  enum dict_of_schema_ids {
    dsid_field = 0,
    dsid_name = 0,
    dsid_colnum = 0,
    dsid_type = 0,
    dsid_enum_def = 1,
    dsid_enum_value = 1
  };

public:
  void add_field(const fptu::string_view &name, fptu_type type,
                 unsigned colnum);
  void add_enum_value(unsigned colnum, const fptu::string_view &name,
                      unsigned value);

  std::string schema2json() const;
  static schema_dict dict_of_schema();
  static const char *tag2name(const void *schema_ctx, unsigned tag);
  static const char *value2enum(const void *schema_ctx, unsigned tag,
                                unsigned value);
};

constexpr const std::array<fptu_type, 31> schema_dict::fptu_types;

void schema_dict::add_field(const fptu::string_view &name, fptu_type type,
                            unsigned colnum) {
  const unsigned tag = fptu::make_tag(colnum, type);
  const auto pair = map_tag2name.emplace(tag, name);
  if (unlikely(!pair.second))
    throw std::logic_error(fptu::format(
        "schema_dict::add_field: Duplicate field tag (colnum %u, type %s)",
        colnum, fptu_type_name(type)));

  assert(pair.first->second.data() != name.data());
  if (!pair.first->second.empty()) {
    const bool inserted =
        map_name2tag.emplace(fptu::string_view(pair.first->second), tag).second;
    if (unlikely(!inserted))
      throw std::logic_error(
          fptu::format("schema_dict::add_field: Duplicate field name '%.*s'",
                       (int)name.length(), name.data()));
    assert(map_name2tag.at(name) == tag);
  }
  assert(map_tag2name.at(tag) == name);
}

void schema_dict::add_enum_value(unsigned colnum, const fptu::string_view &name,
                                 unsigned value) {
  const unsigned tag = fptu::make_tag(colnum, fptu_enum);
  const auto pair = map_value2enum.emplace(std::make_pair(tag, value), name);
  if (unlikely(!pair.second))
    throw std::logic_error(fptu::format(
        "schema_dict::add_field: Duplicate enum item (colnum %u, value %u)",
        colnum, value));

  assert(pair.first->second.data() != name.data());
  if (!pair.first->second.empty()) {
    const auto inserted = map_enum2value.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(fptu::string_view(pair.first->second), tag),
        std::forward_as_tuple(value));
    if (unlikely(!inserted.second))
      throw std::logic_error(fptu::format("schema_dict::add_field: Duplicate "
                                          "enum item (colnum %u, name '%.*s')",
                                          colnum, (int)name.length(),
                                          name.data()));
    assert(map_enum2value.at(std::make_pair(name, tag)) == value);
  }

  assert(map_value2enum.at(std::make_pair(tag, value)) == name);
}

const char *schema_dict::tag2name(const void *schema_ctx, unsigned tag) {
  const schema_dict *dist = static_cast<const schema_dict *>(schema_ctx);
  const auto search = dist->map_tag2name.find(tag);
  return (search != dist->map_tag2name.end()) ? search->second.c_str()
                                              : nullptr;
}

const char *schema_dict::value2enum(const void *schema_ctx, unsigned tag,
                                    unsigned value) {
  const schema_dict *dist = static_cast<const schema_dict *>(schema_ctx);
  const auto search = dist->map_value2enum.find(std::make_pair(tag, value));
  return (search != dist->map_value2enum.end()) ? search->second.c_str()
                                                : nullptr;
}

schema_dict schema_dict::dict_of_schema() {
  schema_dict dict;
  dict.add_field("field", fptu_nested, dsid_field);
  dict.add_field("name", fptu_cstr, dsid_name);
  dict.add_field("colnum", fptu_uint32, dsid_colnum);
  dict.add_field("type", fptu_enum, dsid_type);
  dict.add_field("enum", fptu_nested, dsid_enum_def);
  dict.add_field("value", fptu_uint16, dsid_enum_value);
  for (const auto type : fptu_types)
    dict.add_enum_value(dsid_type, fptu_type_name(type), type);
  return dict;
}

std::string schema_dict::schema2json() const {
  std::vector<std::pair<unsigned, fptu::string_view>> fieldlist;
  for (const auto &i : map_tag2name) {
    const unsigned tag = i.first;
    const fptu::string_view name(i.second);
    fieldlist.emplace_back(std::make_pair(tag, name));
  }
  std::sort(fieldlist.begin(), fieldlist.end());

  fptu::tuple_ptr schema(
      fptu_rw::create(1 + map_tag2name.size(), fptu_max_tuple_bytes));
  fptu::tuple_ptr field(fptu_rw::create(fptu_max_fields, fptu_max_tuple_bytes));
  fptu::tuple_ptr item(fptu_rw::create(2, fptu_max_tuple_bytes));

  for (const auto &i : fieldlist) {
    const unsigned tag = i.first;
    const fptu::string_view &name = i.second;
    fptu_error err = fptu_clear(field.get());
    if (unlikely(err))
      fptu::throw_error(err);

    err = fptu_insert_string(field.get(), dsid_name, name);
    if (unlikely(err))
      fptu::throw_error(err);
    err = fptu_insert_uint32(field.get(), dsid_colnum, fptu_get_colnum(tag));
    if (unlikely(err))
      fptu::throw_error(err);
    err = fptu_insert_uint16(field.get(), dsid_type, fptu_get_type(tag));
    if (unlikely(err))
      fptu::throw_error(err);
    if (fptu_get_type(tag) == fptu_enum ||
        fptu_get_type(tag) == fptu_array_enum) {
      for (unsigned value = 0; value < UINT16_MAX; value++) {
        if (value == FPTU_DENIL_UINT16)
          continue;
        const char *enum_item = value2enum(this, tag, value);
        if (enum_item) {
          err = fptu_upsert_uint16(item.get(), dsid_enum_value, value);
          if (unlikely(err))
            fptu::throw_error(err);
          err = fptu_upsert_string(item.get(), dsid_name,
                                   fptu::format("enum:%s", enum_item));
          if (unlikely(err))
            fptu::throw_error(err);

          err = fptu_insert_nested(field.get(), dsid_enum_def,
                                   fptu_take_noshrink(item.get()));
          if (unlikely(err))
            fptu::throw_error(err);
        }
      }
    }

    err = fptu_insert_nested(schema.get(), dsid_field,
                             fptu_take_noshrink(field.get()));
    if (unlikely(err))
      fptu::throw_error(err);
  }

  const schema_dict dict = dict_of_schema();
  std::string result = fptu::tuple2json(
      fptu_take_noshrink(schema.get()), "  ", 0, &dict, tag2name, value2enum,
      fptu_json_default /*fptu_json_enable_ObjectNameProperty*/);
  return result;
}

//------------------------------------------------------------------------------

static schema_dict create_schemaX() {
  // Cоздается простой словарь схемы, в которой для каждого типа
  // по 10 (римская X) штук полей и массивов.
  // TODO: При этом 10-е поля делаются скрытыми (с пустым именем).
  schema_dict dict;
  for (unsigned n = 1; n < 10; n++) {
    for (const auto type : schema_dict::fptu_types) {
      if (type >= fptu_farray)
        break;

      const std::string field = fptu::format("f%u_%s", n, fptu_type_name(type));
      dict.add_field(fptu::string_view(field), type, n);

      if (type > fptu_null) {
        const std::string array =
            fptu::format("a%u_%s", n, fptu_type_name(type));
        dict.add_field(fptu::string_view(array), fptu_type_array_of(type), n);
      }
    }
  }

  // Для 9-го поля добавляем два пустых имени,
  // чтобы использовать встроенные true/false для bool,
  // а также не-пустое имя для проверки enum
  dict.add_enum_value(9, "", 0);
  dict.add_enum_value(9, "", 1);
  dict.add_enum_value(9, "item42", 42);

  return dict;
}

static const std::string
make_json(const schema_dict &dict, const fptu_ro &ro, bool indentation = false,
          const fptu_json_options options = fptu_json_default) {
  return fptu::tuple2json(ro, indentation ? "  " : nullptr, 0, &dict,
                          schema_dict::tag2name, schema_dict::value2enum,
                          options);
}

static const std::string
make_json(const schema_dict &dict, fptu_rw *pt, bool indentation = false,
          const fptu_json_options options = fptu_json_default) {
  return make_json(dict, fptu_take_noshrink(pt), indentation, options);
}

static const std::string
make_json(const schema_dict &dict, const fptu::tuple_ptr &pt,
          bool indentation = false,
          const fptu_json_options options = fptu_json_default) {
  return make_json(dict, pt.get(), indentation, options);
}

static const char *json(const schema_dict &dict, const fptu::tuple_ptr &pt,
                        bool indentation = false,
                        const fptu_json_options options = fptu_json_default) {
  static std::string json_holder;
  json_holder = make_json(dict, pt, indentation, options);
  return json_holder.c_str();
}

//------------------------------------------------------------------------------

TEST(Emit, Null) {
  schema_dict dict;
  EXPECT_NO_THROW(dict = create_schemaX());

  fptu::tuple_ptr pt(fptu_rw::create(67, 12345));
  ASSERT_NE(nullptr, pt.get());
  ASSERT_STREQ(nullptr, fptu::check(pt.get()));

  // пустой кортеж
  ASSERT_TRUE(fptu::is_empty(pt.get()));
  EXPECT_STREQ("null", json(dict, pt));

  ASSERT_STREQ(nullptr, fptu::check(pt.get()));
}

TEST(Emit, UnsignedInt16) {
  schema_dict dict;
  EXPECT_NO_THROW(dict = create_schemaX());

  fptu::tuple_ptr pt(fptu_rw::create(67, 12345));
  ASSERT_NE(nullptr, pt.get());
  ASSERT_STREQ(nullptr, fptu::check(pt.get()));

  // несколько разных полей uint16 включая DENIL
  ASSERT_EQ(FPTU_OK, fptu_upsert_uint16(pt.get(), 1, 0));
  ASSERT_EQ(FPTU_OK, fptu_upsert_uint16(pt.get(), 2, 35671));
  ASSERT_EQ(FPTU_OK, fptu_upsert_uint16(pt.get(), 3, FPTU_DENIL_UINT16));
  ASSERT_EQ(FPTU_OK, fptu_upsert_uint16(pt.get(), 4, 42));
  EXPECT_STREQ("{f1_uint16:0,f2_uint16:35671,f3_uint16:null,f4_uint16:42}",
               json(dict, pt));

  ASSERT_STREQ(nullptr, fptu::check(pt.get()));
}

TEST(Emit, BoolAndEnum) {
  schema_dict dict;
  EXPECT_NO_THROW(dict = create_schemaX());

  fptu::tuple_ptr pt(fptu_rw::create(67, 12345));
  ASSERT_NE(nullptr, pt.get());
  ASSERT_STREQ(nullptr, fptu::check(pt.get()));

  // коллекция из bool, включая DENIL
  ASSERT_EQ(FPTU_OK, fptu_clear(pt.get()));
  ASSERT_EQ(FPTU_OK, fptu_insert_bool(pt.get(), 9, true));
  ASSERT_EQ(FPTU_OK, fptu_insert_uint16(pt.get(), 9, FPTU_DENIL_UINT16));
  ASSERT_EQ(FPTU_OK, fptu_insert_bool(pt.get(), 9, false));
  EXPECT_STREQ("{f9_uint16:[true,null,false]}", json(dict, pt));

  // коллекция из enum, включая DENIL
  ASSERT_EQ(FPTU_OK, fptu_clear(pt.get()));
  ASSERT_EQ(FPTU_OK, fptu_insert_uint16(pt.get(), 9, 42));
  ASSERT_EQ(FPTU_OK, fptu_insert_uint16(pt.get(), 9, FPTU_DENIL_UINT16));
  ASSERT_EQ(FPTU_OK, fptu_insert_uint16(pt.get(), 9, 33));
  EXPECT_STREQ("{f9_uint16:[\"item42\",null,33]}", json(dict, pt));
}

//------------------------------------------------------------------------------

TEST(Emit, UnignedInt32) {
  schema_dict dict;
  EXPECT_NO_THROW(dict = create_schemaX());

  fptu::tuple_ptr pt(fptu_rw::create(67, 12345));
  ASSERT_NE(nullptr, pt.get());
  ASSERT_STREQ(nullptr, fptu::check(pt.get()));

  // несколько разных полей uint32 включая DENIL
  ASSERT_EQ(FPTU_OK, fptu_clear(pt.get()));
  ASSERT_EQ(FPTU_OK, fptu_upsert_uint32(pt.get(), 1, 0));
  ASSERT_EQ(FPTU_OK, fptu_upsert_uint32(pt.get(), 2, 4242424242));
  ASSERT_EQ(FPTU_OK, fptu_upsert_uint32(pt.get(), 3, 1));
  ASSERT_EQ(FPTU_OK, fptu_upsert_uint32(pt.get(), 4, FPTU_DENIL_UINT32));
  EXPECT_STREQ("{f1_uint32:0,f2_uint32:4242424242,f3_uint32:1,f4_uint32:null}",
               json(dict, pt));
}

TEST(Emit, SignedInt32) {
  schema_dict dict;
  EXPECT_NO_THROW(dict = create_schemaX());

  fptu::tuple_ptr pt(fptu_rw::create(67, 12345));
  ASSERT_NE(nullptr, pt.get());
  ASSERT_STREQ(nullptr, fptu::check(pt.get()));

  // несколько разных полей uint32 включая DENIL
  ASSERT_EQ(FPTU_OK, fptu_clear(pt.get()));
  ASSERT_EQ(FPTU_OK, fptu_upsert_int32(pt.get(), 1, FPTU_DENIL_SINT32));
  ASSERT_EQ(FPTU_OK, fptu_upsert_int32(pt.get(), 2, 0));
  ASSERT_EQ(FPTU_OK, fptu_upsert_int32(pt.get(), 3, 2121212121));
  ASSERT_EQ(FPTU_OK, fptu_upsert_int32(pt.get(), 4, -1));
  EXPECT_STREQ("{f1_int32:null,f2_int32:0,f3_int32:2121212121,f4_int32:-1}",
               json(dict, pt));

  ASSERT_STREQ(nullptr, fptu::check(pt.get()));
}

TEST(Emit, UnsignedInt64) {
  schema_dict dict;
  EXPECT_NO_THROW(dict = create_schemaX());

  fptu::tuple_ptr pt(fptu_rw::create(67, 12345));
  ASSERT_NE(nullptr, pt.get());
  ASSERT_STREQ(nullptr, fptu::check(pt.get()));

  // несколько разных полей uint64 включая DENIL
  ASSERT_EQ(FPTU_OK, fptu_clear(pt.get()));
  ASSERT_EQ(FPTU_OK, fptu_upsert_uint64(pt.get(), 1, 0));
  ASSERT_EQ(FPTU_OK, fptu_upsert_uint64(pt.get(), 2, 4242424242));
  ASSERT_EQ(FPTU_OK, fptu_upsert_uint64(pt.get(), 3, INT64_MAX));
  ASSERT_EQ(FPTU_OK, fptu_upsert_uint64(pt.get(), 4, FPTU_DENIL_UINT64));
  EXPECT_STREQ(
      "{f1_uint64:0,f2_uint64:4242424242,f3_uint64:9223372036854775807,"
      "f4_uint64:null}",
      json(dict, pt));

  ASSERT_STREQ(nullptr, fptu::check(pt.get()));
}

TEST(Emit, SignedInt64) {
  schema_dict dict;
  EXPECT_NO_THROW(dict = create_schemaX());

  fptu::tuple_ptr pt(fptu_rw::create(67, 12345));
  ASSERT_NE(nullptr, pt.get());
  ASSERT_STREQ(nullptr, fptu::check(pt.get()));

  // несколько разных полей int64 включая DENIL
  ASSERT_EQ(FPTU_OK, fptu_clear(pt.get()));
  ASSERT_EQ(FPTU_OK, fptu_upsert_int64(pt.get(), 1, 0));
  ASSERT_EQ(FPTU_OK, fptu_upsert_int64(pt.get(), 2, 4242424242));
  ASSERT_EQ(FPTU_OK, fptu_upsert_int64(pt.get(), 3, -INT64_MAX));
  ASSERT_EQ(FPTU_OK, fptu_upsert_int64(pt.get(), 4, FPTU_DENIL_SINT64));
  EXPECT_STREQ(
      "{f1_int64:0,f2_int64:4242424242,f3_int64:-9223372036854775807,f4_"
      "int64:null}",
      json(dict, pt));
}

//------------------------------------------------------------------------------

TEST(Emit, String) {
  schema_dict dict;
  EXPECT_NO_THROW(dict = create_schemaX());

  fptu::tuple_ptr pt(fptu_rw::create(67, 12345));
  ASSERT_NE(nullptr, pt.get());
  ASSERT_STREQ(nullptr, fptu::check(pt.get()));

  ASSERT_EQ(FPTU_OK, fptu_upsert_cstr(pt.get(), 0, ""));
  ASSERT_EQ(FPTU_OK, fptu_upsert_cstr(pt.get(), 1, "строка"));
  ASSERT_EQ(FPTU_OK, fptu_upsert_cstr(pt.get(), 2, "42"));
  ASSERT_EQ(FPTU_OK, fptu_insert_cstr(pt.get(), 2, "string"));
  ASSERT_EQ(FPTU_OK, fptu_insert_cstr(pt.get(), 2, "null"));
  ASSERT_EQ(FPTU_OK, fptu_insert_cstr(pt.get(), 2, "true"));
  ASSERT_EQ(FPTU_OK, fptu_insert_cstr(pt.get(), 2, "false"));

  EXPECT_STREQ("{\"@13\":\"\",f1_cstr:"
               "\"\xD1\x81\xD1\x82\xD1\x80\xD0\xBE\xD0\xBA\xD0\xB0\",f2_cstr:["
               "\"42\",\"string\",\"null\",\"true\",\"false\"]}",
               json(dict, pt));

  EXPECT_STREQ("{\"@13\":\"\",\"f1_cstr\":"
               "\"\xD1\x81\xD1\x82\xD1\x80\xD0\xBE\xD0\xBA\xD0\xB0\",\"f2_"
               "cstr\":[\"42\",\"string\",\"null\",\"true\",\"false\"]}",
               json(dict, pt, false, fptu_json_disable_JSON5));

  ASSERT_EQ(FPTU_OK, fptu_clear(pt.get()));
  ASSERT_EQ(FPTU_OK, fptu_upsert_cstr(pt.get(), 1, "\\"));
  ASSERT_EQ(FPTU_OK, fptu_upsert_cstr(pt.get(), 2, "\""));
  ASSERT_EQ(FPTU_OK, fptu_upsert_cstr(pt.get(), 3, "'"));
  ASSERT_EQ(FPTU_OK, fptu_upsert_cstr(pt.get(), 4, "\n\r\t\b\f"));
  ASSERT_EQ(FPTU_OK, fptu_upsert_cstr(pt.get(), 5, "\1\2\3ddfg\xff\x1f"));
  EXPECT_STREQ(
      "{f1_cstr:\"\\\\\",f2_cstr:\"\\\"\",f3_cstr:\"'\",f4_cstr:"
      "\"\\n\\r\\t\\b\\f\",f5_cstr:\"\\u0001\\u0002\\u0003ddfg\xFF\\u0031\"}",
      json(dict, pt));

  ASSERT_EQ(FPTU_OK, fptu_clear(pt.get()));
  ASSERT_EQ(FPTU_OK, fptu_upsert_string(pt.get(), 1, std::string(1111, 'A')));
  EXPECT_STREQ(
      "{f1_cstr:"
      "\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}",
      json(dict, pt));

  ASSERT_STREQ(nullptr, fptu::check(pt.get()));
}

//------------------------------------------------------------------------------

TEST(Emit, FloatAndDouble) { /* TODO */
}

//------------------------------------------------------------------------------

TEST(Emit, Datetime) { /* TODO */
}

//------------------------------------------------------------------------------

TEST(Emit, FixbinAndOpacity) { /* TODO */
}

//------------------------------------------------------------------------------

TEST(SchemaDict, SchemaOfSchema) { /* TODO */
}

//------------------------------------------------------------------------------

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
