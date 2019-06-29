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

#ifndef HB_ARRAY_HH
#define HB_ARRAY_HH

#include "hb.hh"
#include "hb-algs.hh"
#include "hb-iter.hh"
#include "hb-null.hh"


template <typename Type>
struct hb_sorted_array_t;

template <typename Type>
struct hb_array_t : hb_iter_with_fallback_t<hb_array_t<Type>, Type&>
{
  /*
   * Constructors.
   */
  hb_array_t () : arrayZ (nullptr), length (0), backwards_length (0) {}
  hb_array_t (Type *array_, unsigned int length_) : arrayZ (array_), length (length_), backwards_length (0) {}
  template <unsigned int length_>
  hb_array_t (Type (&array_)[length_]) : arrayZ (array_), length (length_), backwards_length (0) {}

  template <typename U,
	    hb_enable_if (hb_is_cr_convertible(U, Type))>
  hb_array_t (const hb_array_t<U> &o) :
    hb_iter_with_fallback_t<hb_array_t<Type>, Type&> (),
    arrayZ (o.arrayZ), length (o.length), backwards_length (o.backwards_length) {}
  template <typename U,
	    hb_enable_if (hb_is_cr_convertible(U, Type))>
  hb_array_t& operator = (const hb_array_t<U> &o)
  { arrayZ = o.arrayZ; length = o.length; backwards_length = o.backwards_length; return *this; }

  /*
   * Iterator implementation.
   */
  typedef Type& __item_t__;
  static constexpr bool is_random_access_iterator = true;
  Type& __item_at__ (unsigned i) const
  {
    if (unlikely (i >= length)) return CrapOrNull (Type);
    return arrayZ[i];
  }
  void __forward__ (unsigned n)
  {
    if (unlikely (n > length))
      n = length;
    length -= n;
    backwards_length += n;
    arrayZ += n;
  }
  void __rewind__ (unsigned n)
  {
    if (unlikely (n > backwards_length))
      n = backwards_length;
    length += n;
    backwards_length -= n;
    arrayZ -= n;
  }
  unsigned __len__ () const { return length; }
  /* Ouch. The operator== compares the contents of the array.  For range-based for loops,
   * it's best if we can just compare arrayZ, though comparing contents is still fast,
   * but also would require that Type has operator==.  As such, we optimize this operator
   * for range-based for loop and just compare arrayZ.  No need to compare length, as we
   * assume we're only compared to .end(). */
  bool operator != (const hb_array_t& o) const
  { return arrayZ != o.arrayZ; }

  /* Extra operators.
   */
  Type * operator & () const { return arrayZ; }
  operator hb_array_t<const Type> () { return hb_array_t<const Type> (arrayZ, length); }
  template <typename T> operator T * () const { return arrayZ; }

  HB_INTERNAL bool operator == (const hb_array_t &o) const;
  HB_INTERNAL uint32_t hash () const;

  /*
   * Compare, Sort, and Search.
   */

  /* Note: our compare is NOT lexicographic; it also does NOT call Type::cmp. */
  int cmp (const hb_array_t<Type> &a) const
  {
    if (length != a.length)
      return (int) a.length - (int) length;
    return hb_memcmp (a.arrayZ, arrayZ, get_size ());
  }
  HB_INTERNAL static int cmp (const void *pa, const void *pb)
  {
    hb_array_t<Type> *a = (hb_array_t<Type> *) pa;
    hb_array_t<Type> *b = (hb_array_t<Type> *) pb;
    return b->cmp (*a);
  }

  template <typename T>
  Type *lsearch (const T &x, Type *not_found = nullptr)
  {
    unsigned int count = length;
    for (unsigned int i = 0; i < count; i++)
      if (!this->arrayZ[i].cmp (x))
	return &this->arrayZ[i];
    return not_found;
  }
  template <typename T>
  const Type *lsearch (const T &x, const Type *not_found = nullptr) const
  {
    unsigned int count = length;
    for (unsigned int i = 0; i < count; i++)
      if (!this->arrayZ[i].cmp (x))
	return &this->arrayZ[i];
    return not_found;
  }

  hb_sorted_array_t<Type> qsort (int (*cmp_)(const void*, const void*))
  {
    if (likely (length))
      hb_qsort (arrayZ, length, this->item_size, cmp_);
    return hb_sorted_array_t<Type> (*this);
  }
  hb_sorted_array_t<Type> qsort ()
  {
    if (likely (length))
      hb_qsort (arrayZ, length, this->item_size, Type::cmp);
    return hb_sorted_array_t<Type> (*this);
  }
  void qsort (unsigned int start, unsigned int end)
  {
    end = hb_min (end, length);
    assert (start <= end);
    if (likely (start < end))
      hb_qsort (arrayZ + start, end - start, this->item_size, Type::cmp);
  }

  /*
   * Other methods.
   */

  unsigned int get_size () const { return length * this->item_size; }

  hb_array_t<Type> sub_array (unsigned int start_offset = 0, unsigned int *seg_count = nullptr /* IN/OUT */) const
  {
    if (!start_offset && !seg_count)
      return *this;

    unsigned int count = length;
    if (unlikely (start_offset > count))
      count = 0;
    else
      count -= start_offset;
    if (seg_count)
      count = *seg_count = hb_min (count, *seg_count);
    return hb_array_t<Type> (arrayZ + start_offset, count);
  }
  hb_array_t<Type> sub_array (unsigned int start_offset, unsigned int seg_count) const
  { return sub_array (start_offset, &seg_count); }

  /* Only call if you allocated the underlying array using malloc() or similar. */
  void free ()
  { ::free ((void *) arrayZ); arrayZ = nullptr; length = 0; }

  template <typename hb_serialize_context_t>
  hb_array_t copy (hb_serialize_context_t *c) const
  {
    TRACE_SERIALIZE (this);
    auto* out = c->start_embed (arrayZ);
    if (unlikely (!c->extend_size (out, get_size ()))) return_trace (hb_array_t ());
    for (unsigned i = 0; i < length; i++)
      out[i] = arrayZ[i]; /* TODO: add version that calls c->copy() */
    return_trace (hb_array_t (out, length));
  }

  template <typename hb_sanitize_context_t>
  bool sanitize (hb_sanitize_context_t *c) const
  { return c->check_array (arrayZ, length); }

  /*
   * Members
   */

  public:
  Type *arrayZ;
  unsigned int length;
  unsigned int backwards_length;
};
template <typename T> inline hb_array_t<T>
hb_array (T *array, unsigned int length)
{ return hb_array_t<T> (array, length); }
template <typename T, unsigned int length_> inline hb_array_t<T>
hb_array (T (&array_)[length_])
{ return hb_array_t<T> (array_); }

enum hb_bfind_not_found_t
{
  HB_BFIND_NOT_FOUND_DONT_STORE,
  HB_BFIND_NOT_FOUND_STORE,
  HB_BFIND_NOT_FOUND_STORE_CLOSEST,
};

template <typename Type>
struct hb_sorted_array_t :
	hb_iter_t<hb_sorted_array_t<Type>, Type&>,
	hb_array_t<Type>
{
  typedef hb_iter_t<hb_sorted_array_t<Type>, Type&> iter_base_t;
  HB_ITER_USING (iter_base_t);
  static constexpr bool is_random_access_iterator = true;
  static constexpr bool is_sorted_iterator = true;

  hb_sorted_array_t () : hb_array_t<Type> () {}
  hb_sorted_array_t (Type *array_, unsigned int length_) : hb_array_t<Type> (array_, length_) {}
  template <unsigned int length_>
  hb_sorted_array_t (Type (&array_)[length_]) : hb_array_t<Type> (array_) {}

  template <typename U,
	    hb_enable_if (hb_is_cr_convertible(U, Type))>
  hb_sorted_array_t (const hb_array_t<U> &o) :
    hb_iter_t<hb_sorted_array_t<Type>, Type&> (),
    hb_array_t<Type> (o) {}
  template <typename U,
	    hb_enable_if (hb_is_cr_convertible(U, Type))>
  hb_sorted_array_t& operator = (const hb_array_t<U> &o)
  { hb_array_t<Type> (*this) = o; return *this; }

  /* Iterator implementation. */
  bool operator != (const hb_sorted_array_t& o) const
  { return this->arrayZ != o.arrayZ || this->length != o.length; }

  hb_sorted_array_t<Type> sub_array (unsigned int start_offset, unsigned int *seg_count /* IN/OUT */) const
  { return hb_sorted_array_t<Type> (((const hb_array_t<Type> *) (this))->sub_array (start_offset, seg_count)); }
  hb_sorted_array_t<Type> sub_array (unsigned int start_offset, unsigned int seg_count) const
  { return sub_array (start_offset, &seg_count); }

  template <typename T>
  Type *bsearch (const T &x, Type *not_found = nullptr)
  {
    unsigned int i;
    return bfind (x, &i) ? &this->arrayZ[i] : not_found;
  }
  template <typename T>
  const Type *bsearch (const T &x, const Type *not_found = nullptr) const
  {
    unsigned int i;
    return bfind (x, &i) ? &this->arrayZ[i] : not_found;
  }
  template <typename T>
  bool bfind (const T &x, unsigned int *i = nullptr,
		     hb_bfind_not_found_t not_found = HB_BFIND_NOT_FOUND_DONT_STORE,
		     unsigned int to_store = (unsigned int) -1) const
  {
    int min = 0, max = (int) this->length - 1;
    const Type *array = this->arrayZ;
    while (min <= max)
    {
      int mid = ((unsigned int) min + (unsigned int) max) / 2;
      int c = array[mid].cmp (x);
      if (c < 0)
        max = mid - 1;
      else if (c > 0)
        min = mid + 1;
      else
      {
	if (i)
	  *i = mid;
	return true;
      }
    }
    if (i)
    {
      switch (not_found)
      {
	case HB_BFIND_NOT_FOUND_DONT_STORE:
	  break;

	case HB_BFIND_NOT_FOUND_STORE:
	  *i = to_store;
	  break;

	case HB_BFIND_NOT_FOUND_STORE_CLOSEST:
	  if (max < 0 || (max < (int) this->length && array[max].cmp (x) > 0))
	    max++;
	  *i = max;
	  break;
      }
    }
    return false;
  }
};
template <typename T> inline hb_sorted_array_t<T>
hb_sorted_array (T *array, unsigned int length)
{ return hb_sorted_array_t<T> (array, length); }
template <typename T, unsigned int length_> inline hb_sorted_array_t<T>
hb_sorted_array (T (&array_)[length_])
{ return hb_sorted_array_t<T> (array_); }

template <typename T>
bool hb_array_t<T>::operator == (const hb_array_t<T> &o) const
{
  return length == o.length &&
  + hb_zip (*this, o)
  | hb_map ([] (hb_pair_t<T&, T&> &&_) { return _.first == _.second; })
  | hb_all
  ;
}
template <typename T>
uint32_t hb_array_t<T>::hash () const
{
  return
  + hb_iter (*this)
  | hb_map (hb_hash)
  | hb_reduce ([] (uint32_t a, uint32_t b) { return a * 31 + b; }, 0)
  ;
}

typedef hb_array_t<const char> hb_bytes_t;
typedef hb_array_t<const unsigned char> hb_ubytes_t;

/* TODO Specialize opeator==/hash() for hb_bytes_t and hb_ubytes_t. */
//template <>
//uint32_t hb_array_t<const char>::hash () const { return 0; }

#endif /* HB_ARRAY_HH */
