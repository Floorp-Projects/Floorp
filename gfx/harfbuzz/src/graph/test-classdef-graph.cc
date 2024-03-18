/*
 * Copyright © 2022  Google, Inc.
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

#include "gsubgpos-context.hh"
#include "classdef-graph.hh"
#include "hb-iter.hh"
#include "hb-serialize.hh"

typedef hb_codepoint_pair_t gid_and_class_t;
typedef hb_vector_t<gid_and_class_t> gid_and_class_list_t;

template<typename It>
static unsigned actual_class_def_size(It glyph_and_class) {
  char buffer[100];
  hb_serialize_context_t serializer(buffer, 100);
  OT::ClassDef_serialize (&serializer, glyph_and_class);
  serializer.end_serialize ();
  assert(!serializer.in_error());

  hb_blob_t* blob = serializer.copy_blob();
  unsigned size = hb_blob_get_length(blob);
  hb_blob_destroy(blob);
  return size;
}

static unsigned actual_class_def_size(gid_and_class_list_t consecutive_map, hb_vector_t<unsigned> classes) {
  auto filtered_it =
    + consecutive_map.as_sorted_array().iter()
    | hb_filter([&] (unsigned c) {
      for (unsigned klass : classes) {
        if (c == klass) {
          return true;
        }
      }
      return false;
    }, hb_second);
  return actual_class_def_size(+ filtered_it);
}

template<typename It>
static unsigned actual_coverage_size(It glyphs) {
  char buffer[100];
  hb_serialize_context_t serializer(buffer, 100);
  OT::Layout::Common::Coverage_serialize (&serializer, glyphs);
  serializer.end_serialize ();
  assert(!serializer.in_error());

  hb_blob_t* blob = serializer.copy_blob();
  unsigned size = hb_blob_get_length(blob);
  hb_blob_destroy(blob);
  return size;
}

static unsigned actual_coverage_size(gid_and_class_list_t consecutive_map, hb_vector_t<unsigned> classes) {
  auto filtered_it =
    + consecutive_map.as_sorted_array().iter()
    | hb_filter([&] (unsigned c) {
      for (unsigned klass : classes) {
        if (c == klass) {
          return true;
        }
      }
      return false;
    }, hb_second);
  return actual_coverage_size(+ filtered_it | hb_map_retains_sorting(hb_first));
}

static bool check_coverage_size(graph::class_def_size_estimator_t& estimator,
                                const gid_and_class_list_t& map,
                                hb_vector_t<unsigned> klasses)
{
  unsigned result = estimator.coverage_size();
  unsigned expected = actual_coverage_size(map, klasses);
  if (result != expected) {
    printf ("FAIL: estimated coverage expected size %u but was %u\n", expected, result);
    return false;
  }
  return true;
}

static bool check_add_class_def_size(graph::class_def_size_estimator_t& estimator,
                                     const gid_and_class_list_t& map,
                                     unsigned klass, hb_vector_t<unsigned> klasses)
{
  unsigned result = estimator.add_class_def_size(klass);
  unsigned expected = actual_class_def_size(map, klasses);
  if (result != expected) {
    printf ("FAIL: estimated class def expected size %u but was %u\n", expected, result);
    return false;
  }

  return check_coverage_size(estimator, map, klasses);
}

static bool check_add_class_def_size (const gid_and_class_list_t& list, unsigned klass)
{
  graph::class_def_size_estimator_t estimator (list.iter ());

  unsigned result = estimator.add_class_def_size (klass);
  auto filtered_it =
    + list.as_sorted_array().iter()
    | hb_filter([&] (unsigned c) {
      return c == klass;
    }, hb_second);

  unsigned expected = actual_class_def_size(filtered_it);
  if (result != expected)
  {
    printf ("FAIL: class def expected size %u but was %u\n", expected, result);
    return false;
  }

  auto cov_it = + filtered_it | hb_map_retains_sorting(hb_first);
  result = estimator.coverage_size ();
  expected = actual_coverage_size(cov_it);
  if (result != expected)
  {
    printf ("FAIL: coverage expected size %u but was %u\n", expected, result);
    return false;
  }

  return true;
}

static void test_class_and_coverage_size_estimates ()
{
  gid_and_class_list_t empty = {
  };
  assert (check_add_class_def_size (empty, 0));
  assert (check_add_class_def_size (empty, 1));

  gid_and_class_list_t class_zero = {
    {5, 0},
  };
  assert (check_add_class_def_size (class_zero, 0));

  gid_and_class_list_t consecutive = {
    {4, 0},
    {5, 0},

    {6, 1},
    {7, 1},

    {8, 2},
    {9, 2},
    {10, 2},
    {11, 2},
  };
  assert (check_add_class_def_size (consecutive, 0));
  assert (check_add_class_def_size (consecutive, 1));
  assert (check_add_class_def_size (consecutive, 2));

  gid_and_class_list_t non_consecutive = {
    {4, 0},
    {6, 0},

    {8, 1},
    {10, 1},

    {9, 2},
    {10, 2},
    {11, 2},
    {13, 2},
  };
  assert (check_add_class_def_size (non_consecutive, 0));
  assert (check_add_class_def_size (non_consecutive, 1));
  assert (check_add_class_def_size (non_consecutive, 2));

  gid_and_class_list_t multiple_ranges = {
    {4, 0},
    {5, 0},

    {6, 1},
    {7, 1},

    {9, 1},

    {11, 1},
    {12, 1},
    {13, 1},
  };
  assert (check_add_class_def_size (multiple_ranges, 0));
  assert (check_add_class_def_size (multiple_ranges, 1));
}

static void test_running_class_and_coverage_size_estimates () {
  // #### With consecutive gids: switches formats ###
  gid_and_class_list_t consecutive_map = {
    // range 1-4 (f1: 8 bytes), (f2: 6 bytes)
    {1, 1},
    {2, 1},
    {3, 1},
    {4, 1},

    // (f1: 2 bytes), (f2: 6 bytes)
    {5, 2},

    // (f1: 14 bytes), (f2: 6 bytes)
    {6, 3},
    {7, 3},
    {8, 3},
    {9, 3},
    {10, 3},
    {11, 3},
    {12, 3},
  };

  graph::class_def_size_estimator_t estimator1(consecutive_map.iter());
  assert(check_add_class_def_size(estimator1, consecutive_map, 1, {1}));
  assert(check_add_class_def_size(estimator1, consecutive_map, 2, {1, 2}));
  assert(check_add_class_def_size(estimator1, consecutive_map, 2, {1, 2})); // check that adding the same class again works
  assert(check_add_class_def_size(estimator1, consecutive_map, 3, {1, 2, 3}));

  estimator1.reset();
  assert(check_add_class_def_size(estimator1, consecutive_map, 2, {2}));
  assert(check_add_class_def_size(estimator1, consecutive_map, 3, {2, 3}));

  // #### With non-consecutive gids: always uses format 2 ###
  gid_and_class_list_t non_consecutive_map = {
    // range 1-4 (f1: 8 bytes), (f2: 6 bytes)
    {1, 1},
    {2, 1},
    {3, 1},
    {4, 1},

    // (f1: 2 bytes), (f2: 12 bytes)
    {6, 2},
    {8, 2},

    // (f1: 14 bytes), (f2: 6 bytes)
    {9, 3},
    {10, 3},
    {11, 3},
    {12, 3},
    {13, 3},
    {14, 3},
    {15, 3},
  };

  graph::class_def_size_estimator_t estimator2(non_consecutive_map.iter());
  assert(check_add_class_def_size(estimator2, non_consecutive_map, 1, {1}));
  assert(check_add_class_def_size(estimator2, non_consecutive_map, 2, {1, 2}));
  assert(check_add_class_def_size(estimator2, non_consecutive_map, 3, {1, 2, 3}));

  estimator2.reset();
  assert(check_add_class_def_size(estimator2, non_consecutive_map, 2, {2}));
  assert(check_add_class_def_size(estimator2, non_consecutive_map, 3, {2, 3}));
}

static void test_running_class_size_estimates_with_locally_consecutive_glyphs () {
  gid_and_class_list_t map = {
    {1, 1},
    {6, 2},
    {7, 3},
  };

  graph::class_def_size_estimator_t estimator(map.iter());
  assert(check_add_class_def_size(estimator, map, 1, {1}));
  assert(check_add_class_def_size(estimator, map, 2, {1, 2}));
  assert(check_add_class_def_size(estimator, map, 3, {1, 2, 3}));

  estimator.reset();
  assert(check_add_class_def_size(estimator, map, 2, {2}));
  assert(check_add_class_def_size(estimator, map, 3, {2, 3}));
}

int
main (int argc, char **argv)
{
  test_class_and_coverage_size_estimates ();
  test_running_class_and_coverage_size_estimates ();
  test_running_class_size_estimates_with_locally_consecutive_glyphs ();
}
