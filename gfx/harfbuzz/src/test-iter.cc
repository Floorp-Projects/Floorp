/*
 * Copyright © 2018  Google, Inc.
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
 * Google Author(s): Behdad Esfahbod
 */

#include "hb.hh"
#include "hb-iter.hh"

#include "hb-array.hh"
#include "hb-set.hh"


template <typename T>
struct array_iter_t : hb_iter_t<array_iter_t<T>, T>, hb_iter_mixin_t<array_iter_t<T>, T>
{
  array_iter_t (hb_array_t<T> arr_) : arr (arr_) {}

  typedef T __item_type__;
  T& __item_at__ (unsigned i) const { return arr[i]; }
  void __forward__ (unsigned n) { arr += n; }
  void __rewind__ (unsigned n) { arr -= n; }
  unsigned __len__ () const { return arr.length; }
  bool __random_access__ () const { return true; }

  private:
  hb_array_t<T> arr;
};

template <typename T>
struct some_array_t
{
  some_array_t (hb_array_t<T> arr_) : arr (arr_) {}

  typedef array_iter_t<T> iter_t;
  array_iter_t<T> iter () { return array_iter_t<T> (arr); }
  operator array_iter_t<T> () { return iter (); }
  operator hb_iter_t<array_iter_t<T> > () { return iter (); }

  private:
  hb_array_t<T> arr;
};

int
main (int argc, char **argv)
{
  const int src[10] = {};
  int dst[20];
  hb_vector_t<int> v;

  array_iter_t<const int> s (src); /* Implicit conversion from static array. */
  array_iter_t<const int> s2 (v); /* Implicit conversion from vector. */
  array_iter_t<int> t (dst);

  some_array_t<const int> a (src);

  s2 = s;

  hb_fill (t, 42);
  hb_copy (t, s);
 // hb_copy (t, a.iter ());

  return 0;
}
