/*
 * Copyright (C) 2007,2008,2009,2010  Red Hat, Inc.
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
 * Red Hat Author(s): Behdad Esfahbod
 */

#ifndef HB_OPEN_TYPE_PRIVATE_HH
#define HB_OPEN_TYPE_PRIVATE_HH

#include "hb-private.h"

#include "hb-blob.h"

HB_BEGIN_DECLS
HB_END_DECLS


/*
 * Casts
 */

/* Cast to struct T, reference to reference */
template<typename Type, typename TObject>
inline const Type& CastR(const TObject &X)
{ return reinterpret_cast<const Type&> (X); }
template<typename Type, typename TObject>
inline Type& CastR(TObject &X)
{ return reinterpret_cast<Type&> (X); }

/* Cast to struct T, pointer to pointer */
template<typename Type, typename TObject>
inline const Type* CastP(const TObject *X)
{ return reinterpret_cast<const Type*> (X); }
template<typename Type, typename TObject>
inline Type* CastP(TObject *X)
{ return reinterpret_cast<Type*> (X); }

/* StructAtOffset<T>(P,Ofs) returns the struct T& that is placed at memory
 * location pointed to by P plus Ofs bytes. */
template<typename Type>
inline const Type& StructAtOffset(const void *P, unsigned int offset)
{ return * reinterpret_cast<const Type*> ((const char *) P + offset); }
template<typename Type>
inline Type& StructAtOffset(void *P, unsigned int offset)
{ return * reinterpret_cast<Type*> ((char *) P + offset); }

/* StructAfter<T>(X) returns the struct T& that is placed after X.
 * Works with X of variable size also.  X must implement get_size() */
template<typename Type, typename TObject>
inline const Type& StructAfter(const TObject &X)
{ return StructAtOffset<Type>(&X, X.get_size()); }
template<typename Type, typename TObject>
inline Type& StructAfter(TObject &X)
{ return StructAtOffset<Type>(&X, X.get_size()); }



/*
 * Size checking
 */

/* Check _assertion in a method environment */
#define _DEFINE_SIZE_ASSERTION(_assertion) \
  inline void _size_assertion (void) const \
  { ASSERT_STATIC (_assertion); }
/* Check that _code compiles in a method environment */
#define _DEFINE_COMPILES_ASSERTION(_code) \
  inline void _compiles_assertion (void) const \
  { _code; }


#define DEFINE_SIZE_STATIC(size) \
  _DEFINE_SIZE_ASSERTION (sizeof (*this) == (size)); \
  static const unsigned int static_size = (size); \
  static const unsigned int min_size = (size)

/* Size signifying variable-sized array */
#define VAR 1

#define DEFINE_SIZE_UNION(size, _member) \
  _DEFINE_SIZE_ASSERTION (this->u._member.static_size == (size)); \
  static const unsigned int min_size = (size)

#define DEFINE_SIZE_MIN(size) \
  _DEFINE_SIZE_ASSERTION (sizeof (*this) >= (size)); \
  static const unsigned int min_size = (size)

#define DEFINE_SIZE_ARRAY(size, array) \
  _DEFINE_SIZE_ASSERTION (sizeof (*this) == (size) + sizeof (array[0])); \
  _DEFINE_COMPILES_ASSERTION ((void) array[0].static_size) \
  static const unsigned int min_size = (size)

#define DEFINE_SIZE_ARRAY2(size, array1, array2) \
  _DEFINE_SIZE_ASSERTION (sizeof (*this) == (size) + sizeof (this->array1[0]) + sizeof (this->array2[0])); \
  _DEFINE_COMPILES_ASSERTION ((void) array1[0].static_size; (void) array2[0].static_size) \
  static const unsigned int min_size = (size)



/*
 * Null objects
 */

/* Global nul-content Null pool.  Enlarge as necessary. */
static const void *_NullPool[64 / sizeof (void *)];

/* Generic nul-content Null objects. */
template <typename Type>
static inline const Type& Null (void) {
  ASSERT_STATIC (Type::min_size <= sizeof (_NullPool));
  return *CastP<Type> (_NullPool);
}

/* Specializaiton for arbitrary-content arbitrary-sized Null objects. */
#define DEFINE_NULL_DATA(Type, data) \
static const char _Null##Type[Type::min_size + 1] = data; /* +1 is for nul-termination in data */ \
template <> \
inline const Type& Null<Type> (void) { \
  return *CastP<Type> (_Null##Type); \
} /* The following line really exists such that we end in a place needing semicolon */ \
ASSERT_STATIC (Type::min_size + 1 <= sizeof (_Null##Type))

/* Accessor macro. */
#define Null(Type) Null<Type>()


/*
 * Trace
 */


template <int max_depth>
struct hb_trace_t {
  explicit hb_trace_t (unsigned int *pdepth, const char *what, const char *function, const void *obj) : pdepth(pdepth) {
    (void) (*pdepth < max_depth &&
	    fprintf (stderr, "%s(%p) %-*d-> %s\n", what, obj, *pdepth, *pdepth, function));
    if (max_depth) ++*pdepth;
  }
  ~hb_trace_t (void) { if (max_depth) --*pdepth; }

  private:
  unsigned int *pdepth;
};
template <> /* Optimize when tracing is disabled */
struct hb_trace_t<0> {
  explicit hb_trace_t (unsigned int *pdepth HB_UNUSED, const char *what HB_UNUSED, const char *function HB_UNUSED, const void *obj HB_UNUSED) {}
};



/*
 * Sanitize
 */

#ifndef HB_DEBUG_SANITIZE
#define HB_DEBUG_SANITIZE (HB_DEBUG+0)
#endif


#define TRACE_SANITIZE() \
	hb_trace_t<HB_DEBUG_SANITIZE> trace (&c->debug_depth, "SANITIZE", HB_FUNC, this); \


struct hb_sanitize_context_t
{
  inline void init (hb_blob_t *blob)
  {
    this->blob = hb_blob_reference (blob);
    this->start = hb_blob_lock (blob);
    this->end = this->start + hb_blob_get_length (blob);
    this->writable = hb_blob_is_writable (blob);
    this->edit_count = 0;
    this->debug_depth = 0;

    (void) (HB_DEBUG_SANITIZE &&
      fprintf (stderr, "sanitize %p init [%p..%p] (%lu bytes)\n",
	       this->blob, this->start, this->end,
	       (unsigned long) (this->end - this->start)));
  }

  inline void finish (void)
  {
    (void) (HB_DEBUG_SANITIZE &&
      fprintf (stderr, "sanitize %p fini [%p..%p] %u edit requests\n",
	       this->blob, this->start, this->end, this->edit_count));

    hb_blob_unlock (this->blob);
    hb_blob_destroy (this->blob);
    this->blob = NULL;
    this->start = this->end = NULL;
  }

  inline bool check_range (const void *base, unsigned int len) const
  {
    const char *p = (const char *) base;
    bool ret = this->start <= p &&
	       p <= this->end &&
	       (unsigned int) (this->end - p) >= len;

    (void) (HB_DEBUG_SANITIZE && (int) this->debug_depth < (int) HB_DEBUG_SANITIZE &&
      fprintf (stderr, "SANITIZE(%p) %-*d-> range [%p..%p] (%d bytes) in [%p..%p] -> %s\n",
	       p,
	       this->debug_depth, this->debug_depth,
	       p, p + len, len,
	       this->start, this->end,
	       ret ? "pass" : "FAIL"));

    return likely (ret);
  }

  inline bool check_array (const void *base, unsigned int record_size, unsigned int len) const
  {
    const char *p = (const char *) base;
    bool overflows = record_size > 0 && len >= ((unsigned int) -1) / record_size;

    (void) (HB_DEBUG_SANITIZE && (int) this->debug_depth < (int) HB_DEBUG_SANITIZE &&
      fprintf (stderr, "SANITIZE(%p) %-*d-> array [%p..%p] (%d*%d=%ld bytes) in [%p..%p] -> %s\n",
	       p,
	       this->debug_depth, this->debug_depth,
	       p, p + (record_size * len), record_size, len, (unsigned long) record_size * len,
	       this->start, this->end,
	       !overflows ? "does not overflow" : "OVERFLOWS FAIL"));

    return likely (!overflows && this->check_range (base, record_size * len));
  }

  template <typename Type>
  inline bool check_struct (const Type *obj) const
  {
    return likely (this->check_range (obj, obj->min_size));
  }

  inline bool can_edit (const void *base HB_UNUSED, unsigned int len HB_UNUSED)
  {
    const char *p = (const char *) base;
    this->edit_count++;

    (void) (HB_DEBUG_SANITIZE && (int) this->debug_depth < (int) HB_DEBUG_SANITIZE &&
      fprintf (stderr, "SANITIZE(%p) %-*d-> edit(%u) [%p..%p] (%d bytes) in [%p..%p] -> %s\n",
	       p,
	       this->debug_depth, this->debug_depth,
	       this->edit_count,
	       p, p + len, len,
	       this->start, this->end,
	       this->writable ? "granted" : "REJECTED"));

    return this->writable;
  }

  unsigned int debug_depth;
  const char *start, *end;
  bool writable;
  unsigned int edit_count;
  hb_blob_t *blob;
};



/* Template to sanitize an object. */
template <typename Type>
struct Sanitizer
{
  static hb_blob_t *sanitize (hb_blob_t *blob) {
    hb_sanitize_context_t c[1] = {{0}};
    bool sane;

    /* TODO is_sane() stuff */

    if (!blob)
      return hb_blob_create_empty ();

  retry:
    (void) (HB_DEBUG_SANITIZE &&
      fprintf (stderr, "Sanitizer %p start %s\n", blob, HB_FUNC));

    c->init (blob);

    if (unlikely (!c->start)) {
      c->finish ();
      return blob;
    }

    Type *t = CastP<Type> (const_cast<char *> (c->start));

    sane = t->sanitize (c);
    if (sane) {
      if (c->edit_count) {
	(void) (HB_DEBUG_SANITIZE &&
	  fprintf (stderr, "Sanitizer %p passed first round with %d edits; doing a second round %s\n",
		   blob, c->edit_count, HB_FUNC));

        /* sanitize again to ensure no toe-stepping */
        c->edit_count = 0;
	sane = t->sanitize (c);
	if (c->edit_count) {
	  (void) (HB_DEBUG_SANITIZE &&
	    fprintf (stderr, "Sanitizer %p requested %d edits in second round; FAILLING %s\n",
		     blob, c->edit_count, HB_FUNC));
	  sane = false;
	}
      }
      c->finish ();
    } else {
      unsigned int edit_count = c->edit_count;
      c->finish ();
      if (edit_count && !hb_blob_is_writable (blob) && hb_blob_try_writable (blob)) {
        /* ok, we made it writable by relocating.  try again */
	(void) (HB_DEBUG_SANITIZE &&
	  fprintf (stderr, "Sanitizer %p retry %s\n", blob, HB_FUNC));
        goto retry;
      }
    }

    (void) (HB_DEBUG_SANITIZE &&
      fprintf (stderr, "Sanitizer %p %s %s\n", blob, sane ? "passed" : "FAILED", HB_FUNC));
    if (sane)
      return blob;
    else {
      hb_blob_destroy (blob);
      return hb_blob_create_empty ();
    }
  }

  static const Type* lock_instance (hb_blob_t *blob) {
    const char *base = hb_blob_lock (blob);
    return unlikely (!base) ? &Null(Type) : CastP<Type> (base);
  }
};




/*
 *
 * The OpenType Font File: Data Types
 */


/* "The following data types are used in the OpenType font file.
 *  All OpenType fonts use Motorola-style byte ordering (Big Endian):" */

/*
 * Int types
 */


template <typename Type, int Bytes> class BEInt;

/* LONGTERMTODO: On machines allowing unaligned access, we can make the
 * following tighter by using byteswap instructions on ints directly. */
template <typename Type>
class BEInt<Type, 2>
{
  public:
  inline void set (Type i) { hb_be_uint16_put (v,i); }
  inline operator Type (void) const { return hb_be_uint16_get (v); }
  inline bool operator == (const BEInt<Type, 2>& o) const { return hb_be_uint16_cmp (v, o.v); }
  inline bool operator != (const BEInt<Type, 2>& o) const { return !(*this == o); }
  private: uint8_t v[2];
};
template <typename Type>
class BEInt<Type, 4>
{
  public:
  inline void set (Type i) { hb_be_uint32_put (v,i); }
  inline operator Type (void) const { return hb_be_uint32_get (v); }
  inline bool operator == (const BEInt<Type, 4>& o) const { return hb_be_uint32_cmp (v, o.v); }
  inline bool operator != (const BEInt<Type, 4>& o) const { return !(*this == o); }
  private: uint8_t v[4];
};

/* Integer types in big-endian order and no alignment requirement */
template <typename Type>
struct IntType
{
  inline void set (Type i) { v.set (i); }
  inline operator Type(void) const { return v; }
  inline bool operator == (const IntType<Type> &o) const { return v == o.v; }
  inline bool operator != (const IntType<Type> &o) const { return v != o.v; }
  inline int cmp (Type a) const { Type b = v; return a < b ? -1 : a == b ? 0 : +1; }
  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return likely (c->check_struct (this));
  }
  protected:
  BEInt<Type, sizeof (Type)> v;
  public:
  DEFINE_SIZE_STATIC (sizeof (Type));
};

typedef IntType<uint16_t> USHORT;	/* 16-bit unsigned integer. */
typedef IntType<int16_t>  SHORT;	/* 16-bit signed integer. */
typedef IntType<uint32_t> ULONG;	/* 32-bit unsigned integer. */
typedef IntType<int32_t>  LONG;		/* 32-bit signed integer. */

/* Date represented in number of seconds since 12:00 midnight, January 1,
 * 1904. The value is represented as a signed 64-bit integer. */
struct LONGDATETIME
{
  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return likely (c->check_struct (this));
  }
  private:
  LONG major;
  ULONG minor;
  public:
  DEFINE_SIZE_STATIC (8);
};

/* Array of four uint8s (length = 32 bits) used to identify a script, language
 * system, feature, or baseline */
struct Tag : ULONG
{
  /* What the char* converters return is NOT nul-terminated.  Print using "%.4s" */
  inline operator const char* (void) const { return reinterpret_cast<const char *> (&this->v); }
  inline operator char* (void) { return reinterpret_cast<char *> (&this->v); }
  public:
  DEFINE_SIZE_STATIC (4);
};
DEFINE_NULL_DATA (Tag, "    ");

/* Glyph index number, same as uint16 (length = 16 bits) */
typedef USHORT GlyphID;

/* Script/language-system/feature index */
struct Index : USHORT {
  static const unsigned int NOT_FOUND_INDEX = 0xFFFF;
};
DEFINE_NULL_DATA (Index, "\xff\xff");

/* Offset to a table, same as uint16 (length = 16 bits), Null offset = 0x0000 */
typedef USHORT Offset;

/* LongOffset to a table, same as uint32 (length = 32 bits), Null offset = 0x00000000 */
typedef ULONG LongOffset;


/* CheckSum */
struct CheckSum : ULONG
{
  static uint32_t CalcTableChecksum (ULONG *Table, uint32_t Length)
  {
    uint32_t Sum = 0L;
    ULONG *EndPtr = Table+((Length+3) & ~3) / ULONG::static_size;

    while (Table < EndPtr)
      Sum += *Table++;
    return Sum;
  }
  public:
  DEFINE_SIZE_STATIC (4);
};


/*
 * Version Numbers
 */

struct FixedVersion
{
  inline operator uint32_t (void) const { return (major << 16) + minor; }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return c->check_struct (this);
  }

  USHORT major;
  USHORT minor;
  public:
  DEFINE_SIZE_STATIC (4);
};



/*
 * Template subclasses of Offset and LongOffset that do the dereferencing.
 * Use: (base+offset)
 */

template <typename OffsetType, typename Type>
struct GenericOffsetTo : OffsetType
{
  inline const Type& operator () (const void *base) const
  {
    unsigned int offset = *this;
    if (unlikely (!offset)) return Null(Type);
    return StructAtOffset<Type> (base, offset);
  }

  inline bool sanitize (hb_sanitize_context_t *c, void *base) {
    TRACE_SANITIZE ();
    if (unlikely (!c->check_struct (this))) return false;
    unsigned int offset = *this;
    if (unlikely (!offset)) return true;
    Type &obj = StructAtOffset<Type> (base, offset);
    return likely (obj.sanitize (c)) || neuter (c);
  }
  template <typename T>
  inline bool sanitize (hb_sanitize_context_t *c, void *base, T user_data) {
    TRACE_SANITIZE ();
    if (unlikely (!c->check_struct (this))) return false;
    unsigned int offset = *this;
    if (unlikely (!offset)) return true;
    Type &obj = StructAtOffset<Type> (base, offset);
    return likely (obj.sanitize (c, user_data)) || neuter (c);
  }

  private:
  /* Set the offset to Null */
  inline bool neuter (hb_sanitize_context_t *c) {
    if (c->can_edit (this, this->static_size)) {
      this->set (0); /* 0 is Null offset */
      return true;
    }
    return false;
  }
};
template <typename Base, typename OffsetType, typename Type>
inline const Type& operator + (const Base &base, GenericOffsetTo<OffsetType, Type> offset) { return offset (base); }

template <typename Type>
struct OffsetTo : GenericOffsetTo<Offset, Type> {};

template <typename Type>
struct LongOffsetTo : GenericOffsetTo<LongOffset, Type> {};


/*
 * Array Types
 */

template <typename LenType, typename Type>
struct GenericArrayOf
{
  const Type *sub_array (unsigned int start_offset, unsigned int *pcount /* IN/OUT */) const
  {
    unsigned int count = len;
    if (unlikely (start_offset > count))
      count = 0;
    else
      count -= start_offset;
    count = MIN (count, *pcount);
    *pcount = count;
    return array + start_offset;
  }

  inline const Type& operator [] (unsigned int i) const
  {
    if (unlikely (i >= len)) return Null(Type);
    return array[i];
  }
  inline unsigned int get_size (void) const
  { return len.static_size + len * Type::static_size; }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    if (unlikely (!sanitize_shallow (c))) return false;

    /* Note: for structs that do not reference other structs,
     * we do not need to call their sanitize() as we already did
     * a bound check on the aggregate array size.  We just include
     * a small unreachable expression to make sure the structs
     * pointed to do have a simple sanitize(), ie. they do not
     * reference other structs via offsets.
     */
    (void) (false && array[0].sanitize (c));

    return true;
  }
  inline bool sanitize (hb_sanitize_context_t *c, void *base) {
    TRACE_SANITIZE ();
    if (unlikely (!sanitize_shallow (c))) return false;
    unsigned int count = len;
    for (unsigned int i = 0; i < count; i++)
      if (unlikely (!array[i].sanitize (c, base)))
        return false;
    return true;
  }
  template <typename T>
  inline bool sanitize (hb_sanitize_context_t *c, void *base, T user_data) {
    TRACE_SANITIZE ();
    if (unlikely (!sanitize_shallow (c))) return false;
    unsigned int count = len;
    for (unsigned int i = 0; i < count; i++)
      if (unlikely (!array[i].sanitize (c, base, user_data)))
        return false;
    return true;
  }

  private:
  inline bool sanitize_shallow (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return c->check_struct (this)
	&& c->check_array (this, Type::static_size, len);
  }

  public:
  LenType len;
  Type array[VAR];
  public:
  DEFINE_SIZE_ARRAY (sizeof (LenType), array);
};

/* An array with a USHORT number of elements. */
template <typename Type>
struct ArrayOf : GenericArrayOf<USHORT, Type> {};

/* An array with a ULONG number of elements. */
template <typename Type>
struct LongArrayOf : GenericArrayOf<ULONG, Type> {};

/* Array of Offset's */
template <typename Type>
struct OffsetArrayOf : ArrayOf<OffsetTo<Type> > {};

/* Array of LongOffset's */
template <typename Type>
struct LongOffsetArrayOf : ArrayOf<LongOffsetTo<Type> > {};

/* LongArray of LongOffset's */
template <typename Type>
struct LongOffsetLongArrayOf : LongArrayOf<LongOffsetTo<Type> > {};

/* Array of offsets relative to the beginning of the array itself. */
template <typename Type>
struct OffsetListOf : OffsetArrayOf<Type>
{
  inline const Type& operator [] (unsigned int i) const
  {
    if (unlikely (i >= this->len)) return Null(Type);
    return this+this->array[i];
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return OffsetArrayOf<Type>::sanitize (c, this);
  }
  template <typename T>
  inline bool sanitize (hb_sanitize_context_t *c, T user_data) {
    TRACE_SANITIZE ();
    return OffsetArrayOf<Type>::sanitize (c, this, user_data);
  }
};


/* An array with a USHORT number of elements,
 * starting at second element. */
template <typename Type>
struct HeadlessArrayOf
{
  inline const Type& operator [] (unsigned int i) const
  {
    if (unlikely (i >= len || !i)) return Null(Type);
    return array[i-1];
  }
  inline unsigned int get_size (void) const
  { return len.static_size + (len ? len - 1 : 0) * Type::static_size; }

  inline bool sanitize_shallow (hb_sanitize_context_t *c) {
    return c->check_struct (this)
	&& c->check_array (this, Type::static_size, len);
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    if (unlikely (!sanitize_shallow (c))) return false;

    /* Note: for structs that do not reference other structs,
     * we do not need to call their sanitize() as we already did
     * a bound check on the aggregate array size.  We just include
     * a small unreachable expression to make sure the structs
     * pointed to do have a simple sanitize(), ie. they do not
     * reference other structs via offsets.
     */
    (void) (false && array[0].sanitize (c));

    return true;
  }

  USHORT len;
  Type array[VAR];
  public:
  DEFINE_SIZE_ARRAY (sizeof (USHORT), array);
};

#if __GNUC__ && (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 4))
// work around GCC 4.3 bug where the search() function gets improperly
// optimized away from some instantiations of this template
#pragma GCC visibility push(default)
#endif

/* An array with sorted elements.  Supports binary searching. */
template <typename Type>
struct SortedArrayOf : ArrayOf<Type> {

  template <typename SearchType>
  inline int search (const SearchType &x) const {
    class Cmp {
      public: static int cmp (const SearchType *a, const Type *b) { return b->cmp (*a); }
    };
    const Type *p = (const Type *) bsearch (&x, this->array, this->len, sizeof (this->array[0]), (hb_compare_func_t) Cmp::cmp);
    return p ? p - this->array : -1;
  }
};

#if __GNUC__ && (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 4))
#pragma GCC visibility pop
#endif

HB_BEGIN_DECLS
HB_END_DECLS

#endif /* HB_OPEN_TYPE_PRIVATE_HH */
