/*
 * Copyright © 2020  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Garret Rieger
 */

#include <string>

#include "hb-repacker.hh"
#include "hb-open-type.hh"

static void start_object(const char* tag,
                         unsigned len,
                         hb_serialize_context_t* c)
{
  c->push ();
  char* obj = c->allocate_size<char> (len);
  strncpy (obj, tag, len);
}


static unsigned add_object(const char* tag,
                           unsigned len,
                           hb_serialize_context_t* c)
{
  start_object (tag, len, c);
  return c->pop_pack (false);
}


static void add_offset (unsigned id,
                        hb_serialize_context_t* c)
{
  OT::Offset16* offset = c->start_embed<OT::Offset16> ();
  c->extend_min (offset);
  c->add_link (*offset, id);
}

static void add_wide_offset (unsigned id,
                             hb_serialize_context_t* c)
{
  OT::Offset32* offset = c->start_embed<OT::Offset32> ();
  c->extend_min (offset);
  c->add_link (*offset, id);
}

static void run_resolve_overflow_test (const char* name,
                                       hb_serialize_context_t& overflowing,
                                       hb_serialize_context_t& expected,
                                       unsigned num_iterations = 0)
{
  printf (">>> Testing overflowing resolution for %s\n",
          name);

  graph_t graph (overflowing.object_graph ());

  unsigned buffer_size = overflowing.end - overflowing.start;
  void* out_buffer = malloc (buffer_size);
  hb_serialize_context_t out (out_buffer, buffer_size);

  assert (overflowing.offset_overflow ());
  hb_resolve_overflows (overflowing.object_graph (), HB_TAG ('G', 'S', 'U', 'B'), &out, num_iterations);
  assert (!out.offset_overflow ());
  hb_bytes_t result = out.copy_bytes ();

  assert (!expected.offset_overflow ());
  hb_bytes_t expected_result = expected.copy_bytes ();

  assert (result.length == expected_result.length);
  for (unsigned i = 0; i < expected_result.length; i++)
  {
    assert (result[i] == expected_result[i]);
  }

  result.fini ();
  expected_result.fini ();
  free (out_buffer);
}

static void add_virtual_offset (unsigned id,
                                hb_serialize_context_t* c)
{
  c->add_virtual_link (id);
}

static void
populate_serializer_simple (hb_serialize_context_t* c)
{
  c->start_serialize<char> ();

  unsigned obj_1 = add_object ("ghi", 3, c);
  unsigned obj_2 = add_object ("def", 3, c);

  start_object ("abc", 3, c);
  add_offset (obj_2, c);
  add_offset (obj_1, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_overflow (hb_serialize_context_t* c)
{
  std::string large_string(50000, 'a');
  c->start_serialize<char> ();

  unsigned obj_1 = add_object (large_string.c_str(), 10000, c);
  unsigned obj_2 = add_object (large_string.c_str(), 20000, c);
  unsigned obj_3 = add_object (large_string.c_str(), 50000, c);

  start_object ("abc", 3, c);
  add_offset (obj_3, c);
  add_offset (obj_2, c);
  add_offset (obj_1, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_dedup_overflow (hb_serialize_context_t* c)
{
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_1 = add_object ("def", 3, c);

  start_object (large_string.c_str(), 60000, c);
  add_offset (obj_1, c);
  unsigned obj_2 = c->pop_pack (false);

  start_object (large_string.c_str(), 10000, c);
  add_offset (obj_2, c);
  add_offset (obj_1, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_isolation_overflow (hb_serialize_context_t* c)
{
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_4 = add_object ("4", 1, c);

  start_object (large_string.c_str(), 60000, c);
  add_offset (obj_4, c);
  unsigned obj_3 = c->pop_pack (false);

  start_object (large_string.c_str(), 10000, c);
  add_offset (obj_4, c);
  unsigned obj_2 = c->pop_pack (false);

  start_object ("1", 1, c);
  add_wide_offset (obj_3, c);
  add_offset (obj_2, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_isolation_overflow_complex (hb_serialize_context_t* c)
{
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_f = add_object ("f", 1, c);

  start_object ("e", 1, c);
  add_offset (obj_f, c);
  unsigned obj_e = c->pop_pack (false);

  start_object ("c", 1, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack (false);

  start_object ("d", 1, c);
  add_offset (obj_e, c);
  unsigned obj_d = c->pop_pack (false);

  start_object (large_string.c_str(), 60000, c);
  add_offset (obj_d, c);
  unsigned obj_h = c->pop_pack (false);

  start_object (large_string.c_str(), 60000, c);
  add_offset (obj_c, c);
  add_offset (obj_h, c);
  unsigned obj_b = c->pop_pack (false);

  start_object (large_string.c_str(), 10000, c);
  add_offset (obj_d, c);
  unsigned obj_g = c->pop_pack (false);

  start_object (large_string.c_str(), 11000, c);
  add_offset (obj_d, c);
  unsigned obj_i = c->pop_pack (false);

  start_object ("a", 1, c);
  add_wide_offset (obj_b, c);
  add_offset (obj_g, c);
  add_offset (obj_i, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_isolation_overflow_complex_expected (hb_serialize_context_t* c)
{
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();


  // space 1

  unsigned obj_f_prime = add_object ("f", 1, c);

  start_object ("e", 1, c);
  add_offset (obj_f_prime, c);
  unsigned obj_e_prime = c->pop_pack (false);

  start_object ("d", 1, c);
  add_offset (obj_e_prime, c);
  unsigned obj_d_prime = c->pop_pack (false);

  start_object (large_string.c_str(), 60000, c);
  add_offset (obj_d_prime, c);
  unsigned obj_h = c->pop_pack (false);

  start_object ("c", 1, c);
  add_offset (obj_e_prime, c);
  unsigned obj_c = c->pop_pack (false);

  start_object (large_string.c_str(), 60000, c);
  add_offset (obj_c, c);
  add_offset (obj_h, c);
  unsigned obj_b = c->pop_pack (false);

  // space 0

  unsigned obj_f = add_object ("f", 1, c);

  start_object ("e", 1, c);
  add_offset (obj_f, c);
  unsigned obj_e = c->pop_pack (false);


  start_object ("d", 1, c);
  add_offset (obj_e, c);
  unsigned obj_d = c->pop_pack (false);

  start_object (large_string.c_str(), 11000, c);
  add_offset (obj_d, c);
  unsigned obj_i = c->pop_pack (false);

  start_object (large_string.c_str(), 10000, c);
  add_offset (obj_d, c);
  unsigned obj_g = c->pop_pack (false);

  start_object ("a", 1, c);
  add_wide_offset (obj_b, c);
  add_offset (obj_g, c);
  add_offset (obj_i, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_isolation_overflow_spaces (hb_serialize_context_t* c)
{
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_d = add_object ("f", 1, c);
  unsigned obj_e = add_object ("f", 1, c);

  start_object (large_string.c_str(), 60000, c);
  add_offset (obj_d, c);
  unsigned obj_b = c->pop_pack ();

  start_object (large_string.c_str(), 60000, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack ();


  start_object ("a", 1, c);
  add_wide_offset (obj_b, c);
  add_wide_offset (obj_c, c);
  c->pop_pack ();

  c->end_serialize();
}

static void
populate_serializer_spaces (hb_serialize_context_t* c, bool with_overflow)
{
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_i;

  if (with_overflow)
    obj_i = add_object ("i", 1, c);

  // Space 2
  unsigned obj_h = add_object ("h", 1, c);

  start_object (large_string.c_str(), 30000, c);
  add_offset (obj_h, c);
  unsigned obj_e = c->pop_pack (false);

  start_object ("b", 1, c);
  add_offset (obj_e, c);
  unsigned obj_b = c->pop_pack (false);

  // Space 1
  if (!with_overflow)
    obj_i = add_object ("i", 1, c);

  start_object (large_string.c_str(), 30000, c);
  add_offset (obj_i, c);
  unsigned obj_g = c->pop_pack (false);

  start_object (large_string.c_str(), 30000, c);
  add_offset (obj_i, c);
  unsigned obj_f = c->pop_pack (false);

  start_object ("d", 1, c);
  add_offset (obj_g, c);
  unsigned obj_d = c->pop_pack (false);

  start_object ("c", 1, c);
  add_offset (obj_f, c);
  unsigned obj_c = c->pop_pack (false);

  start_object ("a", 1, c);
  add_wide_offset (obj_b, c);
  add_wide_offset (obj_c, c);
  add_wide_offset (obj_d, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_spaces_16bit_connection (hb_serialize_context_t* c)
{
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_g = add_object ("g", 1, c);
  unsigned obj_h = add_object ("h", 1, c);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_g, c);
  unsigned obj_e = c->pop_pack (false);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_h, c);
  unsigned obj_f = c->pop_pack (false);

  start_object ("c", 1, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack (false);

  start_object ("d", 1, c);
  add_offset (obj_f, c);
  unsigned obj_d = c->pop_pack (false);

  start_object ("b", 1, c);
  add_offset (obj_e, c);
  add_offset (obj_h, c);
  unsigned obj_b = c->pop_pack (false);

  start_object ("a", 1, c);
  add_offset (obj_b, c);
  add_wide_offset (obj_c, c);
  add_wide_offset (obj_d, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_spaces_16bit_connection_expected (hb_serialize_context_t* c)
{
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_g_prime = add_object ("g", 1, c);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_g_prime, c);
  unsigned obj_e_prime = c->pop_pack (false);

  start_object ("c", 1, c);
  add_offset (obj_e_prime, c);
  unsigned obj_c = c->pop_pack (false);

  unsigned obj_h_prime = add_object ("h", 1, c);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_h_prime, c);
  unsigned obj_f = c->pop_pack (false);

  start_object ("d", 1, c);
  add_offset (obj_f, c);
  unsigned obj_d = c->pop_pack (false);

  unsigned obj_g = add_object ("g", 1, c);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_g, c);
  unsigned obj_e = c->pop_pack (false);

  unsigned obj_h = add_object ("h", 1, c);

  start_object ("b", 1, c);
  add_offset (obj_e, c);
  add_offset (obj_h, c);
  unsigned obj_b = c->pop_pack (false);

  start_object ("a", 1, c);
  add_offset (obj_b, c);
  add_wide_offset (obj_c, c);
  add_wide_offset (obj_d, c);
  c->pop_pack (false);

  c->end_serialize ();
}

static void
populate_serializer_short_and_wide_subgraph_root (hb_serialize_context_t* c)
{
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_e = add_object ("e", 1, c);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack (false);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_c, c);
  unsigned obj_d = c->pop_pack (false);

  start_object ("b", 1, c);
  add_offset (obj_c, c);
  add_offset (obj_e, c);
  unsigned obj_b = c->pop_pack (false);

  start_object ("a", 1, c);
  add_offset (obj_b, c);
  add_wide_offset (obj_c, c);
  add_wide_offset (obj_d, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_short_and_wide_subgraph_root_expected (hb_serialize_context_t* c)
{
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_e_prime = add_object ("e", 1, c);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_e_prime, c);
  unsigned obj_c_prime = c->pop_pack (false);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_c_prime, c);
  unsigned obj_d = c->pop_pack (false);

  unsigned obj_e = add_object ("e", 1, c);

  start_object (large_string.c_str (), 40000, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack (false);


  start_object ("b", 1, c);
  add_offset (obj_c, c);
  add_offset (obj_e, c);
  unsigned obj_b = c->pop_pack (false);

  start_object ("a", 1, c);
  add_offset (obj_b, c);
  add_wide_offset (obj_c_prime, c);
  add_wide_offset (obj_d, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_split_spaces (hb_serialize_context_t* c)
{
  // Overflow needs to be resolved by splitting the single space
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_f = add_object ("f", 1, c);

  start_object (large_string.c_str(), 40000, c);
  add_offset (obj_f, c);
  unsigned obj_d = c->pop_pack (false);

  start_object (large_string.c_str(), 40000, c);
  add_offset (obj_f, c);
  unsigned obj_e = c->pop_pack (false);

  start_object ("b", 1, c);
  add_offset (obj_d, c);
  unsigned obj_b = c->pop_pack (false);

  start_object ("c", 1, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack (false);

  start_object ("a", 1, c);
  add_wide_offset (obj_b, c);
  add_wide_offset (obj_c, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_split_spaces_2 (hb_serialize_context_t* c)
{
  // Overflow needs to be resolved by splitting the single space
  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_f = add_object ("f", 1, c);

  start_object (large_string.c_str(), 40000, c);
  add_offset (obj_f, c);
  unsigned obj_d = c->pop_pack (false);

  start_object (large_string.c_str(), 40000, c);
  add_offset (obj_f, c);
  unsigned obj_e = c->pop_pack (false);

  start_object ("b", 1, c);
  add_offset (obj_d, c);
  unsigned obj_b = c->pop_pack (false);

  start_object ("c", 1, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack (false);

  start_object ("a", 1, c);
  add_offset (obj_b, c);
  add_wide_offset (obj_b, c);
  add_wide_offset (obj_c, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_split_spaces_expected (hb_serialize_context_t* c)
{
  // Overflow needs to be resolved by splitting the single space

  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  unsigned obj_f_prime = add_object ("f", 1, c);

  start_object (large_string.c_str(), 40000, c);
  add_offset (obj_f_prime, c);
  unsigned obj_d = c->pop_pack (false);

  start_object ("b", 1, c);
  add_offset (obj_d, c);
  unsigned obj_b = c->pop_pack (false);

  unsigned obj_f = add_object ("f", 1, c);

  start_object (large_string.c_str(), 40000, c);
  add_offset (obj_f, c);
  unsigned obj_e = c->pop_pack (false);

  start_object ("c", 1, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack (false);

  start_object ("a", 1, c);
  add_wide_offset (obj_b, c);
  add_wide_offset (obj_c, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_with_split_spaces_expected_2 (hb_serialize_context_t* c)
{
  // Overflow needs to be resolved by splitting the single space

  std::string large_string(70000, 'a');
  c->start_serialize<char> ();

  // Space 2

  unsigned obj_f_double_prime = add_object ("f", 1, c);

  start_object (large_string.c_str(), 40000, c);
  add_offset (obj_f_double_prime, c);
  unsigned obj_d_prime = c->pop_pack (false);

  start_object ("b", 1, c);
  add_offset (obj_d_prime, c);
  unsigned obj_b_prime = c->pop_pack (false);

  // Space 1

  unsigned obj_f_prime = add_object ("f", 1, c);

  start_object (large_string.c_str(), 40000, c);
  add_offset (obj_f_prime, c);
  unsigned obj_e = c->pop_pack (false);

  start_object ("c", 1, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack (false);

  // Space 0

  unsigned obj_f = add_object ("f", 1, c);

  start_object (large_string.c_str(), 40000, c);
  add_offset (obj_f, c);
  unsigned obj_d = c->pop_pack (false);

  start_object ("b", 1, c);
  add_offset (obj_d, c);
  unsigned obj_b = c->pop_pack (false);

  // Root
  start_object ("a", 1, c);
  add_offset (obj_b, c);
  add_wide_offset (obj_b_prime, c);
  add_wide_offset (obj_c, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_complex_1 (hb_serialize_context_t* c)
{
  c->start_serialize<char> ();

  unsigned obj_4 = add_object ("jkl", 3, c);
  unsigned obj_3 = add_object ("ghi", 3, c);

  start_object ("def", 3, c);
  add_offset (obj_3, c);
  unsigned obj_2 = c->pop_pack (false);

  start_object ("abc", 3, c);
  add_offset (obj_2, c);
  add_offset (obj_4, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_complex_2 (hb_serialize_context_t* c)
{
  c->start_serialize<char> ();

  unsigned obj_5 = add_object ("mn", 2, c);

  unsigned obj_4 = add_object ("jkl", 3, c);

  start_object ("ghi", 3, c);
  add_offset (obj_4, c);
  unsigned obj_3 = c->pop_pack (false);

  start_object ("def", 3, c);
  add_offset (obj_3, c);
  unsigned obj_2 = c->pop_pack (false);

  start_object ("abc", 3, c);
  add_offset (obj_2, c);
  add_offset (obj_4, c);
  add_offset (obj_5, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_complex_3 (hb_serialize_context_t* c)
{
  c->start_serialize<char> ();

  unsigned obj_6 = add_object ("opqrst", 6, c);

  unsigned obj_5 = add_object ("mn", 2, c);

  start_object ("jkl", 3, c);
  add_offset (obj_6, c);
  unsigned obj_4 = c->pop_pack (false);

  start_object ("ghi", 3, c);
  add_offset (obj_4, c);
  unsigned obj_3 = c->pop_pack (false);

  start_object ("def", 3, c);
  add_offset (obj_3, c);
  unsigned obj_2 = c->pop_pack (false);

  start_object ("abc", 3, c);
  add_offset (obj_2, c);
  add_offset (obj_4, c);
  add_offset (obj_5, c);
  c->pop_pack (false);

  c->end_serialize();
}

static void
populate_serializer_virtual_link (hb_serialize_context_t* c)
{
  c->start_serialize<char> ();

  unsigned obj_d = add_object ("d", 1, c);

  start_object ("b", 1, c);
  add_offset (obj_d, c);
  unsigned obj_b = c->pop_pack ();

  start_object ("e", 1, c);
  add_virtual_offset (obj_b, c);
  unsigned obj_e = c->pop_pack();

  start_object ("c", 1, c);
  add_offset (obj_e, c);
  unsigned obj_c = c->pop_pack ();

  start_object ("a", 1, c);
  add_offset (obj_b, c);
  add_offset (obj_c, c);
  c->pop_pack ();

  c->end_serialize();
}

static void test_sort_kahn_1 ()
{
  size_t buffer_size = 100;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_complex_1 (&c);

  graph_t graph (c.object_graph ());
  graph.sort_kahn ();

  assert(strncmp (graph.object (3).head, "abc", 3) == 0);
  assert(graph.object (3).links.length == 2);
  assert(graph.object (3).links[0].objidx == 2);
  assert(graph.object (3).links[1].objidx == 1);

  assert(strncmp (graph.object (2).head, "def", 3) == 0);
  assert(graph.object (2).links.length == 1);
  assert(graph.object (2).links[0].objidx == 0);

  assert(strncmp (graph.object (1).head, "jkl", 3) == 0);
  assert(graph.object (1).links.length == 0);

  assert(strncmp (graph.object (0).head, "ghi", 3) == 0);
  assert(graph.object (0).links.length == 0);

  free (buffer);
}

static void test_sort_kahn_2 ()
{
  size_t buffer_size = 100;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_complex_2 (&c);

  graph_t graph (c.object_graph ());
  graph.sort_kahn ();


  assert(strncmp (graph.object (4).head, "abc", 3) == 0);
  assert(graph.object (4).links.length == 3);
  assert(graph.object (4).links[0].objidx == 3);
    assert(graph.object (4).links[1].objidx == 0);
  assert(graph.object (4).links[2].objidx == 2);

  assert(strncmp (graph.object (3).head, "def", 3) == 0);
  assert(graph.object (3).links.length == 1);
  assert(graph.object (3).links[0].objidx == 1);

  assert(strncmp (graph.object (2).head, "mn", 2) == 0);
  assert(graph.object (2).links.length == 0);

  assert(strncmp (graph.object (1).head, "ghi", 3) == 0);
  assert(graph.object (1).links.length == 1);
  assert(graph.object (1).links[0].objidx == 0);

  assert(strncmp (graph.object (0).head, "jkl", 3) == 0);
  assert(graph.object (0).links.length == 0);

  free (buffer);
}

static void test_sort_shortest ()
{
  size_t buffer_size = 100;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_complex_2 (&c);

  graph_t graph (c.object_graph ());
  graph.sort_shortest_distance ();

  assert(strncmp (graph.object (4).head, "abc", 3) == 0);
  assert(graph.object (4).links.length == 3);
  assert(graph.object (4).links[0].objidx == 2);
  assert(graph.object (4).links[1].objidx == 0);
  assert(graph.object (4).links[2].objidx == 3);

  assert(strncmp (graph.object (3).head, "mn", 2) == 0);
  assert(graph.object (3).links.length == 0);

  assert(strncmp (graph.object (2).head, "def", 3) == 0);
  assert(graph.object (2).links.length == 1);
  assert(graph.object (2).links[0].objidx == 1);

  assert(strncmp (graph.object (1).head, "ghi", 3) == 0);
  assert(graph.object (1).links.length == 1);
  assert(graph.object (1).links[0].objidx == 0);

  assert(strncmp (graph.object (0).head, "jkl", 3) == 0);
  assert(graph.object (0).links.length == 0);

  free (buffer);
}

static void test_duplicate_leaf ()
{
  size_t buffer_size = 100;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_complex_2 (&c);

  graph_t graph (c.object_graph ());
  graph.duplicate (4, 1);

  assert(strncmp (graph.object (5).head, "abc", 3) == 0);
  assert(graph.object (5).links.length == 3);
  assert(graph.object (5).links[0].objidx == 3);
  assert(graph.object (5).links[1].objidx == 4);
  assert(graph.object (5).links[2].objidx == 0);

  assert(strncmp (graph.object (4).head, "jkl", 3) == 0);
  assert(graph.object (4).links.length == 0);

  assert(strncmp (graph.object (3).head, "def", 3) == 0);
  assert(graph.object (3).links.length == 1);
  assert(graph.object (3).links[0].objidx == 2);

  assert(strncmp (graph.object (2).head, "ghi", 3) == 0);
  assert(graph.object (2).links.length == 1);
  assert(graph.object (2).links[0].objidx == 1);

  assert(strncmp (graph.object (1).head, "jkl", 3) == 0);
  assert(graph.object (1).links.length == 0);

  assert(strncmp (graph.object (0).head, "mn", 2) == 0);
  assert(graph.object (0).links.length == 0);

  free (buffer);
}

static void test_duplicate_interior ()
{
  size_t buffer_size = 100;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_complex_3 (&c);

  graph_t graph (c.object_graph ());
  graph.duplicate (3, 2);

  assert(strncmp (graph.object (6).head, "abc", 3) == 0);
  assert(graph.object (6).links.length == 3);
  assert(graph.object (6).links[0].objidx == 4);
  assert(graph.object (6).links[1].objidx == 2);
  assert(graph.object (6).links[2].objidx == 1);

  assert(strncmp (graph.object (5).head, "jkl", 3) == 0);
  assert(graph.object (5).links.length == 1);
  assert(graph.object (5).links[0].objidx == 0);

  assert(strncmp (graph.object (4).head, "def", 3) == 0);
  assert(graph.object (4).links.length == 1);
  assert(graph.object (4).links[0].objidx == 3);

  assert(strncmp (graph.object (3).head, "ghi", 3) == 0);
  assert(graph.object (3).links.length == 1);
  assert(graph.object (3).links[0].objidx == 5);

  assert(strncmp (graph.object (2).head, "jkl", 3) == 0);
  assert(graph.object (2).links.length == 1);
  assert(graph.object (2).links[0].objidx == 0);

  assert(strncmp (graph.object (1).head, "mn", 2) == 0);
  assert(graph.object (1).links.length == 0);

  assert(strncmp (graph.object (0).head, "opqrst", 6) == 0);
  assert(graph.object (0).links.length == 0);

  free (buffer);
}

static void
test_serialize ()
{
  size_t buffer_size = 100;
  void* buffer_1 = malloc (buffer_size);
  hb_serialize_context_t c1 (buffer_1, buffer_size);
  populate_serializer_simple (&c1);
  hb_bytes_t expected = c1.copy_bytes ();

  void* buffer_2 = malloc (buffer_size);
  hb_serialize_context_t c2 (buffer_2, buffer_size);

  graph_t graph (c1.object_graph ());
  graph.serialize (&c2);
  hb_bytes_t actual = c2.copy_bytes ();

  assert (actual == expected);

  actual.fini ();
  expected.fini ();
  free (buffer_1);
  free (buffer_2);
}

static void test_will_overflow_1 ()
{
  size_t buffer_size = 100;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_complex_2 (&c);
  graph_t graph (c.object_graph ());

  assert (!graph.will_overflow (nullptr));

  free (buffer);
}

static void test_will_overflow_2 ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_overflow (&c);
  graph_t graph (c.object_graph ());

  assert (graph.will_overflow (nullptr));

  free (buffer);
}

static void test_will_overflow_3 ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_dedup_overflow (&c);
  graph_t graph (c.object_graph ());

  assert (graph.will_overflow (nullptr));

  free (buffer);
}

static void test_resolve_overflows_via_sort ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_overflow (&c);
  graph_t graph (c.object_graph ());

  void* out_buffer = malloc (buffer_size);
  hb_serialize_context_t out (out_buffer, buffer_size);

  hb_resolve_overflows (c.object_graph (), HB_TAG_NONE, &out);
  assert (!out.offset_overflow ());
  hb_bytes_t result = out.copy_bytes ();
  assert (result.length == (80000 + 3 + 3 * 2));

  result.fini ();
  free (buffer);
  free (out_buffer);
}

static void test_resolve_overflows_via_duplication ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_dedup_overflow (&c);
  graph_t graph (c.object_graph ());

  void* out_buffer = malloc (buffer_size);
  hb_serialize_context_t out (out_buffer, buffer_size);

  hb_resolve_overflows (c.object_graph (), HB_TAG_NONE, &out);
  assert (!out.offset_overflow ());
  hb_bytes_t result = out.copy_bytes ();
  assert (result.length == (10000 + 2 * 2 + 60000 + 2 + 3 * 2));

  result.fini ();
  free (buffer);
  free (out_buffer);
}

static void test_resolve_overflows_via_space_assignment ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_spaces (&c, true);

  void* expected_buffer = malloc (buffer_size);
  hb_serialize_context_t e (expected_buffer, buffer_size);
  populate_serializer_spaces (&e, false);

  run_resolve_overflow_test ("test_resolve_overflows_via_space_assignment",
                             c,
                             e);

  free (buffer);
  free (expected_buffer);
}

static void test_resolve_overflows_via_isolation ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_isolation_overflow (&c);
  graph_t graph (c.object_graph ());

  void* out_buffer = malloc (buffer_size);
  hb_serialize_context_t out (out_buffer, buffer_size);

  assert (c.offset_overflow ());
  hb_resolve_overflows (c.object_graph (), HB_TAG ('G', 'S', 'U', 'B'), &out, 0);
  assert (!out.offset_overflow ());
  hb_bytes_t result = out.copy_bytes ();
  assert (result.length == (1 + 10000 + 60000 + 1 + 1
                            + 4 + 3 * 2));

  result.fini ();
  free (buffer);
  free (out_buffer);
}

static void test_resolve_overflows_via_isolation_with_recursive_duplication ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_isolation_overflow_complex (&c);

  void* expected_buffer = malloc (buffer_size);
  hb_serialize_context_t e (expected_buffer, buffer_size);
  populate_serializer_with_isolation_overflow_complex_expected (&e);

  run_resolve_overflow_test ("test_resolve_overflows_via_isolation_with_recursive_duplication",
                             c,
                             e);
  free (buffer);
  free (expected_buffer);
}

static void test_resolve_overflows_via_isolating_16bit_space ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_spaces_16bit_connection (&c);

  void* expected_buffer = malloc (buffer_size);
  hb_serialize_context_t e (expected_buffer, buffer_size);
  populate_serializer_spaces_16bit_connection_expected (&e);

  run_resolve_overflow_test ("test_resolve_overflows_via_isolating_16bit_space",
                             c,
                             e);

  free (buffer);
  free (expected_buffer);
}

static void test_resolve_overflows_via_isolating_16bit_space_2 ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_short_and_wide_subgraph_root (&c);

  void* expected_buffer = malloc (buffer_size);
  hb_serialize_context_t e (expected_buffer, buffer_size);
  populate_serializer_short_and_wide_subgraph_root_expected (&e);

  run_resolve_overflow_test ("test_resolve_overflows_via_isolating_16bit_space_2",
                             c,
                             e);

  free (buffer);
  free (expected_buffer);
}

static void test_resolve_overflows_via_isolation_spaces ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_isolation_overflow_spaces (&c);
  graph_t graph (c.object_graph ());

  void* out_buffer = malloc (buffer_size);
  hb_serialize_context_t out (out_buffer, buffer_size);

  assert (c.offset_overflow ());
  hb_resolve_overflows (c.object_graph (), HB_TAG ('G', 'S', 'U', 'B'), &out, 0);
  assert (!out.offset_overflow ());
  hb_bytes_t result = out.copy_bytes ();

  unsigned expected_length = 3 + 2 * 60000; // objects
  expected_length += 2 * 4 + 2 * 2; // links
  assert (result.length == expected_length);

  result.fini ();
  free (buffer);
  free (out_buffer);
}

static void test_resolve_overflows_via_splitting_spaces ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_split_spaces (&c);

  void* expected_buffer = malloc (buffer_size);
  hb_serialize_context_t e (expected_buffer, buffer_size);
  populate_serializer_with_split_spaces_expected (&e);

  run_resolve_overflow_test ("test_resolve_overflows_via_splitting_spaces",
                             c,
                             e,
                             1);

  free (buffer);
  free (expected_buffer);

}

static void test_resolve_overflows_via_splitting_spaces_2 ()
{
  size_t buffer_size = 160000;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_with_split_spaces_2 (&c);

  void* expected_buffer = malloc (buffer_size);
  hb_serialize_context_t e (expected_buffer, buffer_size);
  populate_serializer_with_split_spaces_expected_2 (&e);

  run_resolve_overflow_test ("test_resolve_overflows_via_splitting_spaces_2",
                             c,
                             e,
                             1);
  free (buffer);
  free (expected_buffer);
}

static void test_virtual_link ()
{
  size_t buffer_size = 100;
  void* buffer = malloc (buffer_size);
  hb_serialize_context_t c (buffer, buffer_size);
  populate_serializer_virtual_link (&c);

  void* out_buffer = malloc (buffer_size);
  hb_serialize_context_t out (out_buffer, buffer_size);

  hb_resolve_overflows (c.object_graph (), HB_TAG_NONE, &out);
  assert (!out.offset_overflow ());

  hb_bytes_t result = out.copy_bytes ();
  assert (result.length == 5 + 4 * 2);
  assert (result[0]  == 'a');
  assert (result[5]  == 'c');
  assert (result[8]  == 'e');
  assert (result[9]  == 'b');
  assert (result[12] == 'd');

  result.fini ();
  free (buffer);
  free (out_buffer);
}

// TODO(garretrieger): update will_overflow tests to check the overflows array.
// TODO(garretrieger): add tests for priority raising.

int
main (int argc, char **argv)
{
  test_serialize ();
  test_sort_kahn_1 ();
  test_sort_kahn_2 ();
  test_sort_shortest ();
  test_will_overflow_1 ();
  test_will_overflow_2 ();
  test_will_overflow_3 ();
  test_resolve_overflows_via_sort ();
  test_resolve_overflows_via_duplication ();
  test_resolve_overflows_via_space_assignment ();
  test_resolve_overflows_via_isolation ();
  test_resolve_overflows_via_isolation_with_recursive_duplication ();
  test_resolve_overflows_via_isolation_spaces ();
  test_resolve_overflows_via_isolating_16bit_space ();
  test_resolve_overflows_via_isolating_16bit_space_2 ();
  test_resolve_overflows_via_splitting_spaces ();
  test_resolve_overflows_via_splitting_spaces_2 ();
  test_duplicate_leaf ();
  test_duplicate_interior ();
  test_virtual_link ();
}
