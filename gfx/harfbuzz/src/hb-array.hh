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


template <typename Type>
struct hb_sorted_array_t;

template <typename Type>
struct hb_array_t
{
  typedef Type ItemType;
  enum { item_size = hb_static_size (Type) };

  /*
   * Constructors.
   */
  hb_array_t () : arrayZ (nullptr), len (0) {}
  hb_array_t (const hb_array_t &o) : arrayZ (o.arrayZ), len (o.len) {}
  hb_array_t (Type *array_, unsigned int len_) : arrayZ (array_), len (len_) {}
  template <unsigned int len_> hb_array_t (Type (&array_)[len_]) : arrayZ (array_), len (len_) {}

  /*
   * Operators.
   */

  Type& operator [] (int i_) const
  {
    unsigned int i = (unsigned int) i_;
    if (unlikely (i >= len)) return CrapOrNull(Type);
    return arrayZ[i];
  }

  explicit_operator bool () const { return len; }
  Type * operator & () const { return arrayZ; }
  Type & operator * () { return (this->operator [])[0]; }
  operator hb_array_t<const Type> () { return hb_array_t<const Type> (arrayZ, len); }
  template <typename T> operator T * () const { return arrayZ; }

  hb_array_t<Type> & operator += (unsigned int count)
  {
    if (unlikely (count > len))
      count = len;
    len -= count;
    arrayZ += count;
    return *this;
  }
  hb_array_t<Type> & operator -= (unsigned int count)
  {
    if (unlikely (count > len))
      count = len;
    len -= count;
    return *this;
  }
  hb_array_t<Type> & operator ++ () { *this += 1; }
  hb_array_t<Type> & operator -- () { *this -= 1; }
  hb_array_t<Type> operator + (unsigned int count)
  { hb_array_t<Type> copy (*this); *this += count; return copy; }
  hb_array_t<Type> operator - (unsigned int count)
  { hb_array_t<Type> copy (*this); *this -= count; return copy; }
  hb_array_t<Type>  operator ++ (int)
  { hb_array_t<Type> copy (*this); ++*this; return copy; }
  hb_array_t<Type>  operator -- (int)
  { hb_array_t<Type> copy (*this); --*this; return copy; }

  /*
   * Compare, Sort, and Search.
   */

  /* Note: our compare is NOT lexicographic; it also does NOT call Type::cmp. */
  int cmp (const hb_array_t<Type> &a) const
  {
    if (len != a.len)
      return (int) a.len - (int) len;
    return hb_memcmp (a.arrayZ, arrayZ, get_size ());
  }
  static int cmp (const void *pa, const void *pb)
  {
    hb_array_t<Type> *a = (hb_array_t<Type> *) pa;
    hb_array_t<Type> *b = (hb_array_t<Type> *) pb;
    return b->cmp (*a);
  }

  template <typename T>
  Type *lsearch (const T &x, Type *not_found = nullptr)
  {
    unsigned int count = len;
    for (unsigned int i = 0; i < count; i++)
      if (!this->arrayZ[i].cmp (x))
	return &this->arrayZ[i];
    return not_found;
  }
  template <typename T>
  const Type *lsearch (const T &x, const Type *not_found = nullptr) const
  {
    unsigned int count = len;
    for (unsigned int i = 0; i < count; i++)
      if (!this->arrayZ[i].cmp (x))
	return &this->arrayZ[i];
    return not_found;
  }

  hb_sorted_array_t<Type> qsort (int (*cmp_)(const void*, const void*))
  {
    ::qsort (arrayZ, len, item_size, cmp_);
    return hb_sorted_array_t<Type> (*this);
  }
  hb_sorted_array_t<Type> qsort ()
  {
    ::qsort (arrayZ, len, item_size, Type::cmp);
    return hb_sorted_array_t<Type> (*this);
  }
  void qsort (unsigned int start, unsigned int end)
  {
    end = MIN (end, len);
    assert (start <= end);
    ::qsort (arrayZ + start, end - start, item_size, Type::cmp);
  }

  /*
   * Other methods.
   */

  unsigned int get_size () const { return len * item_size; }

  hb_array_t<Type> sub_array (unsigned int start_offset = 0, unsigned int *seg_count = nullptr /* IN/OUT */) const
  {
    if (!start_offset && !seg_count)
      return *this;

    unsigned int count = len;
    if (unlikely (start_offset > count))
      count = 0;
    else
      count -= start_offset;
    if (seg_count)
      count = *seg_count = MIN (count, *seg_count);
    return hb_array_t<Type> (arrayZ + start_offset, count);
  }
  hb_array_t<Type> sub_array (unsigned int start_offset, unsigned int seg_count) const
  { return sub_array (start_offset, &seg_count); }

  /* Only call if you allocated the underlying array using malloc() or similar. */
  void free ()
  { ::free ((void *) arrayZ); arrayZ = nullptr; len = 0; }

  template <typename hb_sanitize_context_t>
  bool sanitize (hb_sanitize_context_t *c) const
  { return c->check_array (arrayZ, len); }

  /*
   * Members
   */

  public:
  Type *arrayZ;
  unsigned int len;
};
template <typename T>
inline hb_array_t<T> hb_array (T *array, unsigned int len)
{ return hb_array_t<T> (array, len); }


enum hb_bfind_not_found_t
{
  HB_BFIND_NOT_FOUND_DONT_STORE,
  HB_BFIND_NOT_FOUND_STORE,
  HB_BFIND_NOT_FOUND_STORE_CLOSEST,
};

template <typename Type>
struct hb_sorted_array_t : hb_array_t<Type>
{
  hb_sorted_array_t () : hb_array_t<Type> () {}
  hb_sorted_array_t (const hb_array_t<Type> &o) : hb_array_t<Type> (o) {}
  hb_sorted_array_t (Type *array_, unsigned int len_) : hb_array_t<Type> (array_, len_) {}

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
    int min = 0, max = (int) this->len - 1;
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
	  if (max < 0 || (max < (int) this->len && array[max].cmp (x) > 0))
	    max++;
	  *i = max;
	  break;
      }
    }
    return false;
  }
};
template <typename T>
inline hb_sorted_array_t<T> hb_sorted_array (T *array, unsigned int len)
{ return hb_sorted_array_t<T> (array, len); }


typedef hb_array_t<const char> hb_bytes_t;
typedef hb_array_t<const unsigned char> hb_ubytes_t;


#endif /* HB_ARRAY_HH */
