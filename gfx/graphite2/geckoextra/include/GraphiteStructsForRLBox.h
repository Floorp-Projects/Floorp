// -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vim: set ts=2 et sw=2 tw=80:
// This Source Code is subject to the terms of the Mozilla Public License
// version 2.0 (the "License"). You can obtain a copy of the License at
// http://mozilla.org/MPL/2.0/.

#ifndef GraphiteStructsForRLBox_h__
#define GraphiteStructsForRLBox_h__

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#elif defined(__GNUC__) || defined(__GNUG__)
// Can't turn off the variadic macro warning emitted from -pedantic
#  pragma GCC system_header
#elif defined(_MSC_VER)
// Doesn't seem to emit the warning
#else
// Don't know the compiler... just let it go through
#endif

// Note that the two size fields below are actually size_t in the headers
// However RLBox currently does not handle these correctly. See Bug 1722127.
// Use a workaround of unsiged int instead of size_t.
#define sandbox_fields_reflection_graphite_class_gr_font_ops(f, g, ...)                       \
  f(unsigned int, size, FIELD_NORMAL, ##__VA_ARGS__) g()                                            \
  f(float (*)(const void*, unsigned short), glyph_advance_x, FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(float (*)(const void*, unsigned short), glyph_advance_y, FIELD_NORMAL, ##__VA_ARGS__) g()

#define sandbox_fields_reflection_graphite_class_gr_face_ops(f, g, ...)                              \
  f(unsigned int, size, FIELD_NORMAL, ##__VA_ARGS__) g()                                                   \
  f(const void* (*)(const void*, unsigned int, unsigned int*), get_table, FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(void (*)(const void*, const void*), release_table, FIELD_NORMAL, ##__VA_ARGS__) g()

#define sandbox_fields_reflection_graphite_class_gr_glyph_to_char_cluster(f, g, ...) \
  f(unsigned int, baseChar, FIELD_NORMAL, ##__VA_ARGS__) g()                               \
  f(unsigned int, baseGlyph, FIELD_NORMAL, ##__VA_ARGS__) g()                              \
  f(unsigned int, nChars, FIELD_NORMAL, ##__VA_ARGS__) g()                                 \
  f(unsigned int, nGlyphs, FIELD_NORMAL, ##__VA_ARGS__) g()

#define sandbox_fields_reflection_graphite_class_gr_glyph_to_char_association(f, g, ...) \
  f(gr_glyph_to_char_cluster*, clusters, FIELD_NORMAL, ##__VA_ARGS__) g()           \
  f(unsigned short*, gids, FIELD_NORMAL, ##__VA_ARGS__) g()                               \
  f(float*, xLocs, FIELD_NORMAL, ##__VA_ARGS__) g()                                       \
  f(float*, yLocs, FIELD_NORMAL, ##__VA_ARGS__) g()                                       \
  f(unsigned int, cIndex, FIELD_NORMAL, ##__VA_ARGS__) g()

#define sandbox_fields_reflection_graphite_class_gr_faceinfo(f, g, ...)                    \
  f(short, extra_ascent, FIELD_NORMAL, ##__VA_ARGS__) g()                                  \
  f(short, extra_descent, FIELD_NORMAL, ##__VA_ARGS__) g()                                 \
  f(short, upem, FIELD_NORMAL, ##__VA_ARGS__) g()                                          \
  f(gr_faceinfo::gr_space_contextuals, space_contextuals, FIELD_NORMAL, ##__VA_ARGS__) g() \
  f(unsigned int, has_bidi_pass, FIELD_NORMAL, ##__VA_ARGS__) g()
  // Remaining bitfields skipped, as bitfields are not fully supported

#define sandbox_fields_reflection_graphite_allClasses(f, ...)                  \
  f(gr_font_ops, graphite, ##__VA_ARGS__)                                      \
  f(gr_face_ops, graphite, ##__VA_ARGS__)                                      \
  f(gr_glyph_to_char_cluster, graphite, ##__VA_ARGS__)                         \
  f(gr_glyph_to_char_association, graphite, ##__VA_ARGS__)                     \
  f(gr_faceinfo, graphite, ##__VA_ARGS__)

#if defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(__GNUC__) || defined(__GNUG__)
#elif defined(_MSC_VER)
#else
#endif

#endif