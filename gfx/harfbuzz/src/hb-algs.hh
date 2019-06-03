/*
 * Copyright © 2017  Google, Inc.
 * Copyright © 2019  Google, Inc.
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
 * Facebook Author(s): Behdad Esfahbod
 */

#ifndef HB_ALGS_HH
#define HB_ALGS_HH

#include "hb.hh"
#include "hb-meta.hh"
#include "hb-null.hh"


/* Encodes three unsigned integers in one 64-bit number.  If the inputs have more than 21 bits,
 * values will be truncated / overlap, and might not decode exactly. */
#define HB_CODEPOINT_ENCODE3(x,y,z) (((uint64_t) (x) << 42) | ((uint64_t) (y) << 21) | (uint64_t) (z))
#define HB_CODEPOINT_DECODE3_1(v) ((hb_codepoint_t) ((v) >> 42))
#define HB_CODEPOINT_DECODE3_2(v) ((hb_codepoint_t) ((v) >> 21) & 0x1FFFFFu)
#define HB_CODEPOINT_DECODE3_3(v) ((hb_codepoint_t) (v) & 0x1FFFFFu)


struct
{
  /* Note.  This is dangerous in that if it's passed an rvalue, it returns rvalue-reference. */
  template <typename T> auto
  operator () (T&& v) const HB_AUTO_RETURN ( hb_forward<T> (v) )
}
HB_FUNCOBJ (hb_identity);
struct
{
  /* Like identity(), but only retains lvalue-references.  Rvalues are returned as rvalues. */
  template <typename T> T&
  operator () (T& v) const { return v; }

  template <typename T> hb_remove_reference<T>
  operator () (T&& v) const { return v; }
}
HB_FUNCOBJ (hb_lidentity);
struct
{
  /* Like identity(), but always returns rvalue. */
  template <typename T> hb_remove_reference<T>
  operator () (T&& v) const { return v; }
}
HB_FUNCOBJ (hb_ridentity);

struct
{
  template <typename T> bool
  operator () (T&& v) const { return bool (hb_forward<T> (v)); }
}
HB_FUNCOBJ (hb_bool);

struct
{
  private:
  template <typename T> auto
  impl (const T& v, hb_priority<1>) const HB_RETURN (uint32_t, hb_deref (v).hash ())

  template <typename T,
	    hb_enable_if (hb_is_integral (T))> auto
  impl (const T& v, hb_priority<0>) const HB_AUTO_RETURN
  (
    /* Knuth's multiplicative method: */
    (uint32_t) v * 2654435761u
  )

  public:

  template <typename T> auto
  operator () (const T& v) const HB_RETURN (uint32_t, impl (v, hb_prioritize))
}
HB_FUNCOBJ (hb_hash);


struct
{
  private:

  /* Pointer-to-member-function. */
  template <typename Appl, typename T, typename ...Ts> auto
  impl (Appl&& a, hb_priority<2>, T &&v, Ts&&... ds) const HB_AUTO_RETURN
  ((hb_deref (hb_forward<T> (v)).*hb_forward<Appl> (a)) (hb_forward<Ts> (ds)...))

  /* Pointer-to-member. */
  template <typename Appl, typename T> auto
  impl (Appl&& a, hb_priority<1>, T &&v) const HB_AUTO_RETURN
  ((hb_deref (hb_forward<T> (v))).*hb_forward<Appl> (a))

  /* Operator(). */
  template <typename Appl, typename ...Ts> auto
  impl (Appl&& a, hb_priority<0>, Ts&&... ds) const HB_AUTO_RETURN
  (hb_deref (hb_forward<Appl> (a)) (hb_forward<Ts> (ds)...))

  public:

  template <typename Appl, typename ...Ts> auto
  operator () (Appl&& a, Ts&&... ds) const HB_AUTO_RETURN
  (
    impl (hb_forward<Appl> (a),
	  hb_prioritize,
	  hb_forward<Ts> (ds)...)
  )
}
HB_FUNCOBJ (hb_invoke);

template <unsigned Pos, typename Appl, typename V>
struct hb_partial_t
{
  hb_partial_t (Appl a, V v) : a (a), v (v) {}

  static_assert (Pos > 0, "");

  template <typename ...Ts,
	    unsigned P = Pos,
	    hb_enable_if (P == 1)> auto
  operator () (Ts&& ...ds) -> decltype (hb_invoke (hb_declval (Appl),
						   hb_declval (V),
						   hb_declval (Ts)...))
  {
    return hb_invoke (hb_forward<Appl> (a),
		      hb_forward<V> (v),
		      hb_forward<Ts> (ds)...);
  }
  template <typename T0, typename ...Ts,
	    unsigned P = Pos,
	    hb_enable_if (P == 2)> auto
  operator () (T0&& d0, Ts&& ...ds) -> decltype (hb_invoke (hb_declval (Appl),
							    hb_declval (T0),
							    hb_declval (V),
							    hb_declval (Ts)...))
  {
    return hb_invoke (hb_forward<Appl> (a),
		      hb_forward<T0> (d0),
		      hb_forward<V> (v),
		      hb_forward<Ts> (ds)...);
  }

  private:
  hb_reference_wrapper<Appl> a;
  V v;
};
template <unsigned Pos=1, typename Appl, typename V>
auto hb_partial (Appl&& a, V&& v) HB_AUTO_RETURN
(( hb_partial_t<Pos, Appl, V> (a, v) ))

/* The following hacky replacement version is to make Visual Stuiod build:. */ \
/* https://github.com/harfbuzz/harfbuzz/issues/1730 */ \
#ifdef _MSC_VER
#define HB_PARTIALIZE(Pos) \
  template <typename _T> \
  decltype(auto) operator () (_T&& _v) const \
  { return hb_partial<Pos> (this, hb_forward<_T> (_v)); } \
  static_assert (true, "")
#else
#define HB_PARTIALIZE(Pos) \
  template <typename _T> \
  auto operator () (_T&& _v) const HB_AUTO_RETURN \
  (hb_partial<Pos> (this, hb_forward<_T> (_v))) \
  static_assert (true, "")
#endif


struct
{
  private:

  template <typename Pred, typename Val> auto
  impl (Pred&& p, Val &&v, hb_priority<1>) const HB_AUTO_RETURN
  (hb_deref (hb_forward<Pred> (p)).has (hb_forward<Val> (v)))

  template <typename Pred, typename Val> auto
  impl (Pred&& p, Val &&v, hb_priority<0>) const HB_AUTO_RETURN
  (
    hb_invoke (hb_forward<Pred> (p),
	       hb_forward<Val> (v))
  )

  public:

  template <typename Pred, typename Val> auto
  operator () (Pred&& p, Val &&v) const HB_RETURN (bool,
    impl (hb_forward<Pred> (p),
	  hb_forward<Val> (v),
	  hb_prioritize)
  )
}
HB_FUNCOBJ (hb_has);

struct
{
  private:

  template <typename Pred, typename Val> auto
  impl (Pred&& p, Val &&v, hb_priority<1>) const HB_AUTO_RETURN
  (
    hb_has (hb_forward<Pred> (p),
	    hb_forward<Val> (v))
  )

  template <typename Pred, typename Val> auto
  impl (Pred&& p, Val &&v, hb_priority<0>) const HB_AUTO_RETURN
  (
    hb_forward<Pred> (p) == hb_forward<Val> (v)
  )

  public:

  template <typename Pred, typename Val> auto
  operator () (Pred&& p, Val &&v) const HB_RETURN (bool,
    impl (hb_forward<Pred> (p),
	  hb_forward<Val> (v),
	  hb_prioritize)
  )
}
HB_FUNCOBJ (hb_match);

struct
{
  private:

  template <typename Proj, typename Val> auto
  impl (Proj&& f, Val &&v, hb_priority<2>) const HB_AUTO_RETURN
  (hb_deref (hb_forward<Proj> (f)).get (hb_forward<Val> (v)))

  template <typename Proj, typename Val> auto
  impl (Proj&& f, Val &&v, hb_priority<1>) const HB_AUTO_RETURN
  (
    hb_invoke (hb_forward<Proj> (f),
	       hb_forward<Val> (v))
  )

  template <typename Proj, typename Val> auto
  impl (Proj&& f, Val &&v, hb_priority<0>) const HB_AUTO_RETURN
  (
    hb_forward<Proj> (f)[hb_forward<Val> (v)]
  )

  public:

  template <typename Proj, typename Val> auto
  operator () (Proj&& f, Val &&v) const HB_AUTO_RETURN
  (
    impl (hb_forward<Proj> (f),
	  hb_forward<Val> (v),
	  hb_prioritize)
  )
}
HB_FUNCOBJ (hb_get);


template <typename T1, typename T2>
struct hb_pair_t
{
  typedef T1 first_t;
  typedef T2 second_t;
  typedef hb_pair_t<T1, T2> pair_t;

  hb_pair_t (T1 a, T2 b) : first (a), second (b) {}

  template <typename Q1, typename Q2,
	    hb_enable_if (hb_is_convertible (T1, Q1) &&
			  hb_is_convertible (T2, T2))>
  operator hb_pair_t<Q1, Q2> () { return hb_pair_t<Q1, Q2> (first, second); }

  hb_pair_t<T1, T2> reverse () const
  { return hb_pair_t<T1, T2> (second, first); }

  bool operator == (const pair_t& o) const { return first == o.first && second == o.second; }
  bool operator != (const pair_t& o) const { return !(*this == o); }
  bool operator < (const pair_t& o) const { return first < o.first || (first == o.first && second < o.second); }
  bool operator >= (const pair_t& o) const { return !(*this < o); }
  bool operator > (const pair_t& o) const { return first > o.first || (first == o.first && second > o.second); }
  bool operator <= (const pair_t& o) const { return !(*this > o); }

  T1 first;
  T2 second;
};
#define hb_pair_t(T1,T2) hb_pair_t<T1, T2>
template <typename T1, typename T2> static inline hb_pair_t<T1, T2>
hb_pair (T1&& a, T2&& b) { return hb_pair_t<T1, T2> (a, b); }

struct
{
  template <typename Pair> typename Pair::first_t
  operator () (const Pair& pair) const { return pair.first; }
}
HB_FUNCOBJ (hb_first);

struct
{
  template <typename Pair> typename Pair::second_t
  operator () (const Pair& pair) const { return pair.second; }
}
HB_FUNCOBJ (hb_second);

/* Note.  In min/max impl, we can use hb_type_identity<T> for second argument.
 * However, that would silently convert between different-signedness integers.
 * Instead we accept two different types, such that compiler can err if
 * comparing integers of different signedness. */
struct
{
  template <typename T, typename T2> auto
  operator () (T&& a, T2&& b) const HB_AUTO_RETURN
  (hb_forward<T> (a) <= hb_forward<T2> (b) ? hb_forward<T> (a) : hb_forward<T2> (b))
}
HB_FUNCOBJ (hb_min);
struct
{
  template <typename T, typename T2> auto
  operator () (T&& a, T2&& b) const HB_AUTO_RETURN
  (hb_forward<T> (a) >= hb_forward<T2> (b) ? hb_forward<T> (a) : hb_forward<T2> (b))
}
HB_FUNCOBJ (hb_max);


/*
 * Bithacks.
 */

/* Return the number of 1 bits in v. */
template <typename T>
static inline HB_CONST_FUNC unsigned int
hb_popcount (T v)
{
#if (defined(__GNUC__) && (__GNUC__ >= 4)) || defined(__clang__)
  if (sizeof (T) <= sizeof (unsigned int))
    return __builtin_popcount (v);

  if (sizeof (T) <= sizeof (unsigned long))
    return __builtin_popcountl (v);

  if (sizeof (T) <= sizeof (unsigned long long))
    return __builtin_popcountll (v);
#endif

  if (sizeof (T) <= 4)
  {
    /* "HACKMEM 169" */
    uint32_t y;
    y = (v >> 1) &033333333333;
    y = v - y - ((y >>1) & 033333333333);
    return (((y + (y >> 3)) & 030707070707) % 077);
  }

  if (sizeof (T) == 8)
  {
    unsigned int shift = 32;
    return hb_popcount<uint32_t> ((uint32_t) v) + hb_popcount ((uint32_t) (v >> shift));
  }

  if (sizeof (T) == 16)
  {
    unsigned int shift = 64;
    return hb_popcount<uint64_t> ((uint64_t) v) + hb_popcount ((uint64_t) (v >> shift));
  }

  assert (0);
  return 0; /* Shut up stupid compiler. */
}

/* Returns the number of bits needed to store number */
template <typename T>
static inline HB_CONST_FUNC unsigned int
hb_bit_storage (T v)
{
  if (unlikely (!v)) return 0;

#if (defined(__GNUC__) && (__GNUC__ >= 4)) || defined(__clang__)
  if (sizeof (T) <= sizeof (unsigned int))
    return sizeof (unsigned int) * 8 - __builtin_clz (v);

  if (sizeof (T) <= sizeof (unsigned long))
    return sizeof (unsigned long) * 8 - __builtin_clzl (v);

  if (sizeof (T) <= sizeof (unsigned long long))
    return sizeof (unsigned long long) * 8 - __builtin_clzll (v);
#endif

#if (defined(_MSC_VER) && _MSC_VER >= 1500) || defined(__MINGW32__)
  if (sizeof (T) <= sizeof (unsigned int))
  {
    unsigned long where;
    _BitScanReverse (&where, v);
    return 1 + where;
  }
# if defined(_WIN64)
  if (sizeof (T) <= 8)
  {
    unsigned long where;
    _BitScanReverse64 (&where, v);
    return 1 + where;
  }
# endif
#endif

  if (sizeof (T) <= 4)
  {
    /* "bithacks" */
    const unsigned int b[] = {0x2, 0xC, 0xF0, 0xFF00, 0xFFFF0000};
    const unsigned int S[] = {1, 2, 4, 8, 16};
    unsigned int r = 0;
    for (int i = 4; i >= 0; i--)
      if (v & b[i])
      {
	v >>= S[i];
	r |= S[i];
      }
    return r + 1;
  }
  if (sizeof (T) <= 8)
  {
    /* "bithacks" */
    const uint64_t b[] = {0x2ULL, 0xCULL, 0xF0ULL, 0xFF00ULL, 0xFFFF0000ULL, 0xFFFFFFFF00000000ULL};
    const unsigned int S[] = {1, 2, 4, 8, 16, 32};
    unsigned int r = 0;
    for (int i = 5; i >= 0; i--)
      if (v & b[i])
      {
	v >>= S[i];
	r |= S[i];
      }
    return r + 1;
  }
  if (sizeof (T) == 16)
  {
    unsigned int shift = 64;
    return (v >> shift) ? hb_bit_storage<uint64_t> ((uint64_t) (v >> shift)) + shift :
			  hb_bit_storage<uint64_t> ((uint64_t) v);
  }

  assert (0);
  return 0; /* Shut up stupid compiler. */
}

/* Returns the number of zero bits in the least significant side of v */
template <typename T>
static inline HB_CONST_FUNC unsigned int
hb_ctz (T v)
{
  if (unlikely (!v)) return 0;

#if (defined(__GNUC__) && (__GNUC__ >= 4)) || defined(__clang__)
  if (sizeof (T) <= sizeof (unsigned int))
    return __builtin_ctz (v);

  if (sizeof (T) <= sizeof (unsigned long))
    return __builtin_ctzl (v);

  if (sizeof (T) <= sizeof (unsigned long long))
    return __builtin_ctzll (v);
#endif

#if (defined(_MSC_VER) && _MSC_VER >= 1500) || defined(__MINGW32__)
  if (sizeof (T) <= sizeof (unsigned int))
  {
    unsigned long where;
    _BitScanForward (&where, v);
    return where;
  }
# if defined(_WIN64)
  if (sizeof (T) <= 8)
  {
    unsigned long where;
    _BitScanForward64 (&where, v);
    return where;
  }
# endif
#endif

  if (sizeof (T) <= 4)
  {
    /* "bithacks" */
    unsigned int c = 32;
    v &= - (int32_t) v;
    if (v) c--;
    if (v & 0x0000FFFF) c -= 16;
    if (v & 0x00FF00FF) c -= 8;
    if (v & 0x0F0F0F0F) c -= 4;
    if (v & 0x33333333) c -= 2;
    if (v & 0x55555555) c -= 1;
    return c;
  }
  if (sizeof (T) <= 8)
  {
    /* "bithacks" */
    unsigned int c = 64;
    v &= - (int64_t) (v);
    if (v) c--;
    if (v & 0x00000000FFFFFFFFULL) c -= 32;
    if (v & 0x0000FFFF0000FFFFULL) c -= 16;
    if (v & 0x00FF00FF00FF00FFULL) c -= 8;
    if (v & 0x0F0F0F0F0F0F0F0FULL) c -= 4;
    if (v & 0x3333333333333333ULL) c -= 2;
    if (v & 0x5555555555555555ULL) c -= 1;
    return c;
  }
  if (sizeof (T) == 16)
  {
    unsigned int shift = 64;
    return (uint64_t) v ? hb_bit_storage<uint64_t> ((uint64_t) v) :
			  hb_bit_storage<uint64_t> ((uint64_t) (v >> shift)) + shift;
  }

  assert (0);
  return 0; /* Shut up stupid compiler. */
}


/*
 * Tiny stuff.
 */

/* ASCII tag/character handling */
static inline bool ISALPHA (unsigned char c)
{ return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static inline bool ISALNUM (unsigned char c)
{ return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); }
static inline bool ISSPACE (unsigned char c)
{ return c == ' ' || c =='\f'|| c =='\n'|| c =='\r'|| c =='\t'|| c =='\v'; }
static inline unsigned char TOUPPER (unsigned char c)
{ return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c; }
static inline unsigned char TOLOWER (unsigned char c)
{ return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c; }

static inline unsigned int DIV_CEIL (const unsigned int a, unsigned int b)
{ return (a + (b - 1)) / b; }


#undef  ARRAY_LENGTH
template <typename Type, unsigned int n>
static inline unsigned int ARRAY_LENGTH (const Type (&)[n]) { return n; }
/* A const version, but does not detect erratically being called on pointers. */
#define ARRAY_LENGTH_CONST(__array) ((signed int) (sizeof (__array) / sizeof (__array[0])))


static inline int
hb_memcmp (const void *a, const void *b, unsigned int len)
{
  /* It's illegal to pass NULL to memcmp(), even if len is zero.
   * So, wrap it.
   * https://sourceware.org/bugzilla/show_bug.cgi?id=23878 */
  if (!len) return 0;
  return memcmp (a, b, len);
}

static inline bool
hb_unsigned_mul_overflows (unsigned int count, unsigned int size)
{
  return (size > 0) && (count >= ((unsigned int) -1) / size);
}

static inline unsigned int
hb_ceil_to_4 (unsigned int v)
{
  return ((v - 1) | 3) + 1;
}

template <typename T> static inline bool
hb_in_range (T u, T lo, T hi)
{
  static_assert (!hb_is_signed<T>::value, "");

  /* The casts below are important as if T is smaller than int,
   * the subtract results will become a signed int! */
  return (T)(u - lo) <= (T)(hi - lo);
}
template <typename T> static inline bool
hb_in_ranges (T u, T lo1, T hi1, T lo2, T hi2)
{
  return hb_in_range (u, lo1, hi1) || hb_in_range (u, lo2, hi2);
}
template <typename T> static inline bool
hb_in_ranges (T u, T lo1, T hi1, T lo2, T hi2, T lo3, T hi3)
{
  return hb_in_range (u, lo1, hi1) || hb_in_range (u, lo2, hi2) || hb_in_range (u, lo3, hi3);
}


/*
 * Sort and search.
 */

static inline void *
hb_bsearch (const void *key, const void *base,
	    size_t nmemb, size_t size,
	    int (*compar)(const void *_key, const void *_item))
{
  int min = 0, max = (int) nmemb - 1;
  while (min <= max)
  {
    int mid = (min + max) / 2;
    const void *p = (const void *) (((const char *) base) + (mid * size));
    int c = compar (key, p);
    if (c < 0)
      max = mid - 1;
    else if (c > 0)
      min = mid + 1;
    else
      return (void *) p;
  }
  return nullptr;
}

static inline void *
hb_bsearch_r (const void *key, const void *base,
	      size_t nmemb, size_t size,
	      int (*compar)(const void *_key, const void *_item, void *_arg),
	      void *arg)
{
  int min = 0, max = (int) nmemb - 1;
  while (min <= max)
  {
    int mid = ((unsigned int) min + (unsigned int) max) / 2;
    const void *p = (const void *) (((const char *) base) + (mid * size));
    int c = compar (key, p, arg);
    if (c < 0)
      max = mid - 1;
    else if (c > 0)
      min = mid + 1;
    else
      return (void *) p;
  }
  return nullptr;
}


/* From https://github.com/noporpoise/sort_r
 * With following modifications:
 *
 * 10 November 2018:
 * https://github.com/noporpoise/sort_r/issues/7
 */

/* Isaac Turner 29 April 2014 Public Domain */

/*

hb_sort_r function to be exported.

Parameters:
  base is the array to be sorted
  nel is the number of elements in the array
  width is the size in bytes of each element of the array
  compar is the comparison function
  arg is a pointer to be passed to the comparison function

void hb_sort_r(void *base, size_t nel, size_t width,
               int (*compar)(const void *_a, const void *_b, void *_arg),
               void *arg);
*/


/* swap a, b iff a>b */
/* __restrict is same as restrict but better support on old machines */
static int sort_r_cmpswap(char *__restrict a, char *__restrict b, size_t w,
			  int (*compar)(const void *_a, const void *_b,
					void *_arg),
			  void *arg)
{
  char tmp, *end = a+w;
  if(compar(a, b, arg) > 0) {
    for(; a < end; a++, b++) { tmp = *a; *a = *b; *b = tmp; }
    return 1;
  }
  return 0;
}

/* Note: quicksort is not stable, equivalent values may be swapped */
static inline void sort_r_simple(void *base, size_t nel, size_t w,
				 int (*compar)(const void *_a, const void *_b,
					       void *_arg),
				 void *arg)
{
  char *b = (char *)base, *end = b + nel*w;
  if(nel < 7) {
    /* Insertion sort for arbitrarily small inputs */
    char *pi, *pj;
    for(pi = b+w; pi < end; pi += w) {
      for(pj = pi; pj > b && sort_r_cmpswap(pj-w,pj,w,compar,arg); pj -= w) {}
    }
  }
  else
  {
    /* nel > 6; Quicksort */

    /* Use median of first, middle and last items as pivot */
    char *x, *y, *xend, ch;
    char *pl, *pm, *pr;
    char *last = b+w*(nel-1), *tmp;
    char *l[3];
    l[0] = b;
    l[1] = b+w*(nel/2);
    l[2] = last;

    if(compar(l[0],l[1],arg) > 0) { tmp=l[0]; l[0]=l[1]; l[1]=tmp; }
    if(compar(l[1],l[2],arg) > 0) {
      tmp=l[1]; l[1]=l[2]; l[2]=tmp; /* swap(l[1],l[2]) */
      if(compar(l[0],l[1],arg) > 0) { tmp=l[0]; l[0]=l[1]; l[1]=tmp; }
    }

    /* swap l[id], l[2] to put pivot as last element */
    for(x = l[1], y = last, xend = x+w; x<xend; x++, y++) {
      ch = *x; *x = *y; *y = ch;
    }

    pl = b;
    pr = last;

    while(pl < pr) {
      pm = pl+((pr-pl+1)>>1);
      for(; pl < pm; pl += w) {
        if(sort_r_cmpswap(pl, pr, w, compar, arg)) {
          pr -= w; /* pivot now at pl */
          break;
        }
      }
      pm = pl+((pr-pl)>>1);
      for(; pm < pr; pr -= w) {
        if(sort_r_cmpswap(pl, pr, w, compar, arg)) {
          pl += w; /* pivot now at pr */
          break;
        }
      }
    }

    sort_r_simple(b, (pl-b)/w, w, compar, arg);
    sort_r_simple(pl+w, (end-(pl+w))/w, w, compar, arg);
  }
}

static inline void
hb_sort_r (void *base, size_t nel, size_t width,
	   int (*compar)(const void *_a, const void *_b, void *_arg),
	   void *arg)
{
    sort_r_simple(base, nel, width, compar, arg);
}


template <typename T, typename T2, typename T3> static inline void
hb_stable_sort (T *array, unsigned int len, int(*compar)(const T2 *, const T2 *), T3 *array2)
{
  for (unsigned int i = 1; i < len; i++)
  {
    unsigned int j = i;
    while (j && compar (&array[j - 1], &array[i]) > 0)
      j--;
    if (i == j)
      continue;
    /* Move item i to occupy place for item j, shift what's in between. */
    {
      T t = array[i];
      memmove (&array[j + 1], &array[j], (i - j) * sizeof (T));
      array[j] = t;
    }
    if (array2)
    {
      T3 t = array2[i];
      memmove (&array2[j + 1], &array2[j], (i - j) * sizeof (T3));
      array2[j] = t;
    }
  }
}

template <typename T> static inline void
hb_stable_sort (T *array, unsigned int len, int(*compar)(const T *, const T *))
{
  hb_stable_sort (array, len, compar, (int *) nullptr);
}

static inline hb_bool_t
hb_codepoint_parse (const char *s, unsigned int len, int base, hb_codepoint_t *out)
{
  /* Pain because we don't know whether s is nul-terminated. */
  char buf[64];
  len = hb_min (ARRAY_LENGTH (buf) - 1, len);
  strncpy (buf, s, len);
  buf[len] = '\0';

  char *end;
  errno = 0;
  unsigned long v = strtoul (buf, &end, base);
  if (errno) return false;
  if (*end) return false;
  *out = v;
  return true;
}


/* Operators. */

struct hb_bitwise_and
{ HB_PARTIALIZE(2);
  static constexpr bool passthru_left = false;
  static constexpr bool passthru_right = false;
  template <typename T> auto
  operator () (const T &a, const T &b) const HB_AUTO_RETURN (a & b)
}
HB_FUNCOBJ (hb_bitwise_and);
struct hb_bitwise_or
{ HB_PARTIALIZE(2);
  static constexpr bool passthru_left = true;
  static constexpr bool passthru_right = true;
  template <typename T> auto
  operator () (const T &a, const T &b) const HB_AUTO_RETURN (a | b)
}
HB_FUNCOBJ (hb_bitwise_or);
struct hb_bitwise_xor
{ HB_PARTIALIZE(2);
  static constexpr bool passthru_left = true;
  static constexpr bool passthru_right = true;
  template <typename T> auto
  operator () (const T &a, const T &b) const HB_AUTO_RETURN (a ^ b)
}
HB_FUNCOBJ (hb_bitwise_xor);
struct hb_bitwise_sub
{ HB_PARTIALIZE(2);
  static constexpr bool passthru_left = true;
  static constexpr bool passthru_right = false;
  template <typename T> auto
  operator () (const T &a, const T &b) const HB_AUTO_RETURN (a & ~b)
}
HB_FUNCOBJ (hb_bitwise_sub);

struct
{ HB_PARTIALIZE(2);
  template <typename T, typename T2> auto
  operator () (const T &a, const T2 &b) const HB_AUTO_RETURN (a + b)
}
HB_FUNCOBJ (hb_add);
struct
{ HB_PARTIALIZE(2);
  template <typename T, typename T2> auto
  operator () (const T &a, const T2 &b) const HB_AUTO_RETURN (a - b)
}
HB_FUNCOBJ (hb_sub);
struct
{ HB_PARTIALIZE(2);
  template <typename T, typename T2> auto
  operator () (const T &a, const T2 &b) const HB_AUTO_RETURN (a * b)
}
HB_FUNCOBJ (hb_mul);
struct
{ HB_PARTIALIZE(2);
  template <typename T, typename T2> auto
  operator () (const T &a, const T2 &b) const HB_AUTO_RETURN (a / b)
}
HB_FUNCOBJ (hb_div);
struct
{ HB_PARTIALIZE(2);
  template <typename T, typename T2> auto
  operator () (const T &a, const T2 &b) const HB_AUTO_RETURN (a % b)
}
HB_FUNCOBJ (hb_mod);
struct
{
  template <typename T> auto
  operator () (const T &a) const HB_AUTO_RETURN (+a)
}
HB_FUNCOBJ (hb_pos);
struct
{
  template <typename T> auto
  operator () (const T &a) const HB_AUTO_RETURN (-a)
}
HB_FUNCOBJ (hb_neg);


/* Compiler-assisted vectorization. */

/* Type behaving similar to vectorized vars defined using __attribute__((vector_size(...))),
 * using vectorized operations if HB_VECTOR_SIZE is set to **bit** numbers (eg 128).
 * Define that to 0 to disable. */
template <typename elt_t, unsigned int byte_size>
struct hb_vector_size_t
{
  elt_t& operator [] (unsigned int i) { return u.v[i]; }
  const elt_t& operator [] (unsigned int i) const { return u.v[i]; }

  void clear (unsigned char v = 0) { memset (this, v, sizeof (*this)); }

  template <typename Op>
  hb_vector_size_t process (const Op& op, const hb_vector_size_t &o) const
  {
    hb_vector_size_t r;
#if HB_VECTOR_SIZE
    if (HB_VECTOR_SIZE && 0 == (byte_size * 8) % HB_VECTOR_SIZE)
      for (unsigned int i = 0; i < ARRAY_LENGTH (u.vec); i++)
	r.u.vec[i] = op (u.vec[i], o.u.vec[i]);
    else
#endif
      for (unsigned int i = 0; i < ARRAY_LENGTH (u.v); i++)
	r.u.v[i] = op (u.v[i], o.u.v[i]);
    return r;
  }
  hb_vector_size_t operator | (const hb_vector_size_t &o) const
  { return process (hb_bitwise_or, o); }
  hb_vector_size_t operator & (const hb_vector_size_t &o) const
  { return process (hb_bitwise_and, o); }
  hb_vector_size_t operator ^ (const hb_vector_size_t &o) const
  { return process (hb_bitwise_xor, o); }
  hb_vector_size_t operator ~ () const
  {
    hb_vector_size_t r;
#if HB_VECTOR_SIZE && 0
    if (HB_VECTOR_SIZE && 0 == (byte_size * 8) % HB_VECTOR_SIZE)
      for (unsigned int i = 0; i < ARRAY_LENGTH (u.vec); i++)
	r.u.vec[i] = ~u.vec[i];
    else
#endif
    for (unsigned int i = 0; i < ARRAY_LENGTH (u.v); i++)
      r.u.v[i] = ~u.v[i];
    return r;
  }

  private:
  static_assert (byte_size / sizeof (elt_t) * sizeof (elt_t) == byte_size, "");
  union {
    elt_t v[byte_size / sizeof (elt_t)];
#if HB_VECTOR_SIZE
    hb_vector_size_impl_t vec[byte_size / sizeof (hb_vector_size_impl_t)];
#endif
  } u;
};


#endif /* HB_ALGS_HH */
