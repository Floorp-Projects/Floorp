/*
 * Copyright © 2018 Adobe Inc.
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
 * Adobe Author(s): Michiharu Ariza
 */
#ifndef HB_CFF_INTERP_COMMON_HH
#define HB_CFF_INTERP_COMMON_HH

namespace CFF {

using namespace OT;

typedef unsigned int OpCode;


/* === Dict operators === */

/* One byte operators (0-31) */
#define OpCode_version		  0 /* CFF Top */
#define OpCode_Notice		  1 /* CFF Top */
#define OpCode_FullName		  2 /* CFF Top */
#define OpCode_FamilyName	  3 /* CFF Top */
#define OpCode_Weight		  4 /* CFF Top */
#define OpCode_FontBBox		  5 /* CFF Top */
#define OpCode_BlueValues	  6 /* CFF Private, CFF2 Private */
#define OpCode_OtherBlues	  7 /* CFF Private, CFF2 Private */
#define OpCode_FamilyBlues	  8 /* CFF Private, CFF2 Private */
#define OpCode_FamilyOtherBlues	  9 /* CFF Private, CFF2 Private */
#define OpCode_StdHW		 10 /* CFF Private, CFF2 Private */
#define OpCode_StdVW		 11 /* CFF Private, CFF2 Private */
#define OpCode_escape		 12 /* All. Shared with CS */
#define OpCode_UniqueID		 13 /* CFF Top */
#define OpCode_XUID		 14 /* CFF Top */
#define OpCode_charset		 15 /* CFF Top (0) */
#define OpCode_Encoding		 16 /* CFF Top (0) */
#define OpCode_CharStrings	 17 /* CFF Top, CFF2 Top */
#define OpCode_Private		 18 /* CFF Top, CFF2 FD */
#define OpCode_Subrs		 19 /* CFF Private, CFF2 Private */
#define OpCode_defaultWidthX	 20 /* CFF Private (0) */
#define OpCode_nominalWidthX	 21 /* CFF Private (0) */
#define OpCode_vsindexdict	 22 /* CFF2 Private/CS */
#define OpCode_blenddict	 23 /* CFF2 Private/CS */
#define OpCode_vstore		 24 /* CFF2 Top */
#define OpCode_reserved25	 25
#define OpCode_reserved26	 26
#define OpCode_reserved27	 27

/* Numbers */
#define OpCode_shortint		 28 /* 16-bit integer, All */
#define OpCode_longintdict	 29 /* 32-bit integer, All */
#define OpCode_BCD		 30 /* Real number, CFF2 Top/FD */
#define OpCode_reserved31	 31

/* 1-byte integers */
#define OpCode_OneByteIntFirst	 32 /* All. beginning of the range of first byte ints */
#define OpCode_OneByteIntLast	246 /* All. ending of the range of first byte int */

/* 2-byte integers */
#define OpCode_TwoBytePosInt0	247 /* All. first byte of two byte positive int (+108 to +1131) */
#define OpCode_TwoBytePosInt1	248
#define OpCode_TwoBytePosInt2	249
#define OpCode_TwoBytePosInt3	250

#define OpCode_TwoByteNegInt0	251 /* All. first byte of two byte negative int (-1131 to -108) */
#define OpCode_TwoByteNegInt1	252
#define OpCode_TwoByteNegInt2	253
#define OpCode_TwoByteNegInt3	254

/* Two byte escape operators 12, (0-41) */
#define OpCode_ESC_Base		256
#define Make_OpCode_ESC(byte2)	((OpCode)(OpCode_ESC_Base + (byte2)))

inline OpCode Unmake_OpCode_ESC (OpCode op)  { return (OpCode)(op - OpCode_ESC_Base); }
inline bool Is_OpCode_ESC (OpCode op) { return op >= OpCode_ESC_Base; }
inline unsigned int OpCode_Size (OpCode op) { return Is_OpCode_ESC (op) ? 2: 1; }

#define OpCode_Copyright	Make_OpCode_ESC(0) /* CFF Top */
#define OpCode_isFixedPitch	Make_OpCode_ESC(1) /* CFF Top (false) */
#define OpCode_ItalicAngle	Make_OpCode_ESC(2) /* CFF Top (0) */
#define OpCode_UnderlinePosition Make_OpCode_ESC(3) /* CFF Top (-100) */
#define OpCode_UnderlineThickness Make_OpCode_ESC(4) /* CFF Top (50) */
#define OpCode_PaintType	Make_OpCode_ESC(5) /* CFF Top (0) */
#define OpCode_CharstringType	Make_OpCode_ESC(6) /* CFF Top (2) */
#define OpCode_FontMatrix	Make_OpCode_ESC(7) /* CFF Top, CFF2 Top (.001 0 0 .001 0 0)*/
#define OpCode_StrokeWidth	Make_OpCode_ESC(8) /* CFF Top (0) */
#define OpCode_BlueScale	Make_OpCode_ESC(9) /* CFF Private, CFF2 Private (0.039625) */
#define OpCode_BlueShift	Make_OpCode_ESC(10) /* CFF Private, CFF2 Private (7) */
#define OpCode_BlueFuzz		Make_OpCode_ESC(11) /* CFF Private, CFF2 Private (1) */
#define OpCode_StemSnapH	Make_OpCode_ESC(12) /* CFF Private, CFF2 Private */
#define OpCode_StemSnapV	Make_OpCode_ESC(13) /* CFF Private, CFF2 Private */
#define OpCode_ForceBold	Make_OpCode_ESC(14) /* CFF Private (false) */
#define OpCode_reservedESC15	Make_OpCode_ESC(15)
#define OpCode_reservedESC16	Make_OpCode_ESC(16)
#define OpCode_LanguageGroup	Make_OpCode_ESC(17) /* CFF Private, CFF2 Private (0) */
#define OpCode_ExpansionFactor	Make_OpCode_ESC(18) /* CFF Private, CFF2 Private (0.06) */
#define OpCode_initialRandomSeed Make_OpCode_ESC(19) /* CFF Private (0) */
#define OpCode_SyntheticBase	Make_OpCode_ESC(20) /* CFF Top */
#define OpCode_PostScript	Make_OpCode_ESC(21) /* CFF Top */
#define OpCode_BaseFontName	Make_OpCode_ESC(22) /* CFF Top */
#define OpCode_BaseFontBlend	Make_OpCode_ESC(23) /* CFF Top */
#define OpCode_reservedESC24	Make_OpCode_ESC(24)
#define OpCode_reservedESC25	Make_OpCode_ESC(25)
#define OpCode_reservedESC26	Make_OpCode_ESC(26)
#define OpCode_reservedESC27	Make_OpCode_ESC(27)
#define OpCode_reservedESC28	Make_OpCode_ESC(28)
#define OpCode_reservedESC29	Make_OpCode_ESC(29)
#define OpCode_ROS		Make_OpCode_ESC(30) /* CFF Top_CID */
#define OpCode_CIDFontVersion	Make_OpCode_ESC(31) /* CFF Top_CID (0) */
#define OpCode_CIDFontRevision	Make_OpCode_ESC(32) /* CFF Top_CID (0) */
#define OpCode_CIDFontType	Make_OpCode_ESC(33) /* CFF Top_CID (0) */
#define OpCode_CIDCount		Make_OpCode_ESC(34) /* CFF Top_CID (8720) */
#define OpCode_UIDBase		Make_OpCode_ESC(35) /* CFF Top_CID */
#define OpCode_FDArray		Make_OpCode_ESC(36) /* CFF Top_CID, CFF2 Top */
#define OpCode_FDSelect		Make_OpCode_ESC(37) /* CFF Top_CID, CFF2 Top */
#define OpCode_FontName		Make_OpCode_ESC(38) /* CFF Top_CID */


/* === CharString operators === */

#define OpCode_hstem		  1 /* CFF, CFF2 */
#define OpCode_Reserved2	  2
#define OpCode_vstem		  3 /* CFF, CFF2 */
#define OpCode_vmoveto		  4 /* CFF, CFF2 */
#define OpCode_rlineto		  5 /* CFF, CFF2 */
#define OpCode_hlineto		  6 /* CFF, CFF2 */
#define OpCode_vlineto		  7 /* CFF, CFF2 */
#define OpCode_rrcurveto	  8 /* CFF, CFF2 */
#define OpCode_Reserved9	  9
#define OpCode_callsubr		 10 /* CFF, CFF2 */
#define OpCode_return		 11 /* CFF */
//#define OpCode_escape		 12 /* CFF, CFF2 */
#define OpCode_Reserved13	 13
#define OpCode_endchar		 14 /* CFF */
#define OpCode_vsindexcs	 15 /* CFF2 */
#define OpCode_blendcs		 16 /* CFF2 */
#define OpCode_Reserved17	 17
#define OpCode_hstemhm		 18 /* CFF, CFF2 */
#define OpCode_hintmask		 19 /* CFF, CFF2 */
#define OpCode_cntrmask		 20 /* CFF, CFF2 */
#define OpCode_rmoveto		 21 /* CFF, CFF2 */
#define OpCode_hmoveto		 22 /* CFF, CFF2 */
#define OpCode_vstemhm		 23 /* CFF, CFF2 */
#define OpCode_rcurveline	 24 /* CFF, CFF2 */
#define OpCode_rlinecurve	 25 /* CFF, CFF2 */
#define OpCode_vvcurveto	 26 /* CFF, CFF2 */
#define OpCode_hhcurveto	 27 /* CFF, CFF2 */
//#define OpCode_shortint	 28 /* CFF, CFF2 */
#define OpCode_callgsubr	 29 /* CFF, CFF2 */
#define OpCode_vhcurveto	 30 /* CFF, CFF2 */
#define OpCode_hvcurveto	 31 /* CFF, CFF2 */

#define OpCode_fixedcs		255 /* 32-bit fixed */

/* Two byte escape operators 12, (0-41) */
#define OpCode_dotsection	Make_OpCode_ESC(0) /* CFF (obsoleted) */
#define OpCode_ReservedESC1	Make_OpCode_ESC(1)
#define OpCode_ReservedESC2	Make_OpCode_ESC(2)
#define OpCode_and		Make_OpCode_ESC(3) /* CFF */
#define OpCode_or		Make_OpCode_ESC(4) /* CFF */
#define OpCode_not		Make_OpCode_ESC(5) /* CFF */
#define OpCode_ReservedESC6	Make_OpCode_ESC(6)
#define OpCode_ReservedESC7	Make_OpCode_ESC(7)
#define OpCode_ReservedESC8	Make_OpCode_ESC(8)
#define OpCode_abs		Make_OpCode_ESC(9) /* CFF */
#define OpCode_add		Make_OpCode_ESC(10) /* CFF */
#define OpCode_sub		Make_OpCode_ESC(11) /* CFF */
#define OpCode_div		Make_OpCode_ESC(12) /* CFF */
#define OpCode_ReservedESC13	Make_OpCode_ESC(13)
#define OpCode_neg		Make_OpCode_ESC(14) /* CFF */
#define OpCode_eq		Make_OpCode_ESC(15) /* CFF */
#define OpCode_ReservedESC16	Make_OpCode_ESC(16)
#define OpCode_ReservedESC17	Make_OpCode_ESC(17)
#define OpCode_drop		Make_OpCode_ESC(18) /* CFF */
#define OpCode_ReservedESC19	Make_OpCode_ESC(19)
#define OpCode_put		Make_OpCode_ESC(20) /* CFF */
#define OpCode_get		Make_OpCode_ESC(21) /* CFF */
#define OpCode_ifelse		Make_OpCode_ESC(22) /* CFF */
#define OpCode_random		Make_OpCode_ESC(23) /* CFF */
#define OpCode_mul		Make_OpCode_ESC(24) /* CFF */
//#define OpCode_reservedESC25	Make_OpCode_ESC(25)
#define OpCode_sqrt		Make_OpCode_ESC(26) /* CFF */
#define OpCode_dup		Make_OpCode_ESC(27) /* CFF */
#define OpCode_exch		Make_OpCode_ESC(28) /* CFF */
#define OpCode_index		Make_OpCode_ESC(29) /* CFF */
#define OpCode_roll		Make_OpCode_ESC(30) /* CFF */
#define OpCode_reservedESC31	Make_OpCode_ESC(31)
#define OpCode_reservedESC32	Make_OpCode_ESC(32)
#define OpCode_reservedESC33	Make_OpCode_ESC(33)
#define OpCode_hflex		Make_OpCode_ESC(34) /* CFF, CFF2 */
#define OpCode_flex		Make_OpCode_ESC(35) /* CFF, CFF2 */
#define OpCode_hflex1		Make_OpCode_ESC(36) /* CFF, CFF2 */
#define OpCode_flex1		Make_OpCode_ESC(37) /* CFF, CFF2 */


#define OpCode_Invalid		0xFFFFu


struct Number
{
  void init () { set_real (0.0); }
  void fini () {}

  void set_int (int v)       { value = (double) v; }
  int to_int () const        { return (int) value; }

  void set_fixed (int32_t v) { value = v / 65536.0; }
  int32_t to_fixed () const  { return (int32_t) (value * 65536.0); }

  void set_real (double v)	 { value = v; }
  double to_real () const    { return value; }

  int ceil () const          { return (int) ::ceil (value); }
  int floor () const         { return (int) ::floor (value); }

  bool in_int_range () const
  { return ((double) (int16_t) to_int () == value); }

  bool operator > (const Number &n) const
  { return value > n.to_real (); }

  bool operator < (const Number &n) const
  { return n > *this; }

  bool operator >= (const Number &n) const
  { return !(*this < n); }

  bool operator <= (const Number &n) const
  { return !(*this > n); }

  const Number &operator += (const Number &n)
  {
    set_real (to_real () + n.to_real ());

    return *this;
  }

  protected:
  double  value;
};

/* byte string */
struct UnsizedByteStr : UnsizedArrayOf <HBUINT8>
{
  // encode 2-byte int (Dict/CharString) or 4-byte int (Dict)
  template <typename INTTYPE, int minVal, int maxVal>
  static bool serialize_int (hb_serialize_context_t *c, OpCode intOp, int value)
  {
    TRACE_SERIALIZE (this);

    if (unlikely ((value < minVal || value > maxVal)))
      return_trace (false);

    HBUINT8 *p = c->allocate_size<HBUINT8> (1);
    if (unlikely (p == nullptr)) return_trace (false);
    p->set (intOp);

    INTTYPE *ip = c->allocate_size<INTTYPE> (INTTYPE::static_size);
    if (unlikely (ip == nullptr)) return_trace (false);
    ip->set ((unsigned int)value);

    return_trace (true);
  }

  static bool serialize_int4 (hb_serialize_context_t *c, int value)
  { return serialize_int<HBUINT32, 0, 0x7FFFFFFF> (c, OpCode_longintdict, value); }

  static bool serialize_int2 (hb_serialize_context_t *c, int value)
  { return serialize_int<HBUINT16, 0, 0x7FFF> (c, OpCode_shortint, value); }

  /* Defining null_size allows a Null object may be created. Should be safe because:
   * A descendent struct Dict uses a Null pointer to indicate a missing table,
   * checked before access.
   * ByteStr, a wrapper struct pairing a byte pointer along with its length, always
   * checks the length before access. A Null pointer is used as the initial pointer
   * along with zero length by the default ctor.
   */
  DEFINE_SIZE_MIN(0);
};

struct ByteStr
{
  ByteStr ()
    : str (&Null(UnsizedByteStr)), len (0) {}
  ByteStr (const UnsizedByteStr& s, unsigned int l)
    : str (&s), len (l) {}
  ByteStr (const char *s, unsigned int l=0)
    : str ((const UnsizedByteStr *)s), len (l) {}
  /* sub-string */
  ByteStr (const ByteStr &bs, unsigned int offset, unsigned int len_)
  {
    str = (const UnsizedByteStr *)&bs.str[offset];
    len = len_;
  }

  bool sanitize (hb_sanitize_context_t *c) const { return str->sanitize (c, len); }

  const HBUINT8& operator [] (unsigned int i) const
  {
    if (likely (str && (i < len)))
      return (*str)[i];
    else
      return Null(HBUINT8);
  }

  bool serialize (hb_serialize_context_t *c, const ByteStr &src)
  {
    TRACE_SERIALIZE (this);
    HBUINT8 *dest = c->allocate_size<HBUINT8> (src.len);
    if (unlikely (dest == nullptr))
      return_trace (false);
    memcpy (dest, src.str, src.len);
    return_trace (true);
  }

  unsigned int get_size () const { return len; }

  bool check_limit (unsigned int offset, unsigned int count) const
  { return (offset + count <= len); }

  const UnsizedByteStr *str;
  unsigned int len;
};

struct SubByteStr
{
  SubByteStr ()
  { init (); }

  void init ()
  {
    str = ByteStr (0);
    offset = 0;
    error = false;
  }

  void fini () {}

  SubByteStr (const ByteStr &str_, unsigned int offset_ = 0)
    : str (str_), offset (offset_), error (false) {}

  void reset (const ByteStr &str_, unsigned int offset_ = 0)
  {
    str = str_;
    offset = offset_;
    error = false;
  }

  const HBUINT8& operator [] (int i) {
    if (unlikely ((unsigned int)(offset + i) >= str.len))
    {
      set_error ();
      return Null(HBUINT8);
    }
    else
      return str[offset + i];
  }

  operator ByteStr () const { return ByteStr (str, offset, str.len - offset); }

  bool avail (unsigned int count=1) const
  {
    return (!in_error () && str.check_limit (offset, count));
  }
  void inc (unsigned int count=1)
  {
    if (likely (!in_error () && (offset <= str.len) && (offset + count <= str.len)))
    {
      offset += count;
    }
    else
    {
      offset = str.len;
      set_error ();
    }
  }

  void set_error ()      { error = true; }
  bool in_error () const { return error; }

  ByteStr       str;
  unsigned int  offset; /* beginning of the sub-string within str */

  protected:
  bool	  error;
};

typedef hb_vector_t<ByteStr> ByteStrArray;

/* stack */
template <typename ELEM, int LIMIT>
struct Stack
{
  void init ()
  {
    error = false;
    count = 0;
    elements.init ();
    elements.resize (kSizeLimit);
    for (unsigned int i = 0; i < elements.len; i++)
      elements[i].init ();
  }

  void fini ()
  {
    elements.fini_deep ();
  }

  ELEM& operator [] (unsigned int i)
  {
    if (unlikely (i >= count)) set_error ();
    return elements[i];
  }

  void push (const ELEM &v)
  {
    if (likely (count < elements.len))
      elements[count++] = v;
    else
      set_error ();
  }

  ELEM &push ()
  {
    if (likely (count < elements.len))
      return elements[count++];
    else
    {
      set_error ();
      return Crap(ELEM);
    }
  }

  ELEM& pop ()
  {
    if (likely (count > 0))
      return elements[--count];
    else
    {
      set_error ();
      return Crap(ELEM);
    }
  }

  void pop (unsigned int n)
  {
    if (likely (count >= n))
      count -= n;
    else
      set_error ();
  }

  const ELEM& peek ()
  {
    if (likely (count > 0))
      return elements[count-1];
    else
    {
      set_error ();
      return Null(ELEM);
    }
  }

  void unpop ()
  {
    if (likely (count < elements.len))
      count++;
    else
      set_error ();
  }

  void clear () { count = 0; }

  bool in_error () const { return (error || elements.in_error ()); }
  void set_error ()      { error = true; }

  unsigned int get_count () const { return count; }
  bool is_empty () const { return count == 0; }

  static const unsigned int kSizeLimit = LIMIT;

  protected:
  bool error;
  unsigned int count;
  hb_vector_t<ELEM, kSizeLimit> elements;
};

/* argument stack */
template <typename ARG=Number>
struct ArgStack : Stack<ARG, 513>
{
  void push_int (int v)
  {
    ARG &n = S::push ();
    n.set_int (v);
  }

  void push_fixed (int32_t v)
  {
    ARG &n = S::push ();
    n.set_fixed (v);
  }

  void push_real (double v)
  {
    ARG &n = S::push ();
    n.set_real (v);
  }

  ARG& pop_num () { return this->pop (); }

  int pop_int ()  { return this->pop ().to_int (); }

  unsigned int pop_uint ()
  {
    int i = pop_int ();
    if (unlikely (i < 0))
    {
      i = 0;
      S::set_error ();
    }
    return (unsigned)i;
  }

  void push_longint_from_substr (SubByteStr& substr)
  {
    push_int ((substr[0] << 24) | (substr[1] << 16) | (substr[2] << 8) | (substr[3]));
    substr.inc (4);
  }

  bool push_fixed_from_substr (SubByteStr& substr)
  {
    if (unlikely (!substr.avail (4)))
      return false;
    push_fixed ((int32_t)*(const HBUINT32*)&substr[0]);
    substr.inc (4);
    return true;
  }

  hb_array_t<const ARG> get_subarray (unsigned int start) const
  {
    return S::elements.sub_array (start);
  }

  private:
  typedef Stack<ARG, 513> S;
};

/* an operator prefixed by its operands in a byte string */
struct OpStr
{
  void init () {}
  void fini () {}

  OpCode  op;
  ByteStr str;
};

/* base of OP_SERIALIZER */
struct OpSerializer
{
  protected:
  bool copy_opstr (hb_serialize_context_t *c, const OpStr& opstr) const
  {
    TRACE_SERIALIZE (this);

    HBUINT8 *d = c->allocate_size<HBUINT8> (opstr.str.len);
    if (unlikely (d == nullptr)) return_trace (false);
    memcpy (d, &opstr.str.str[0], opstr.str.len);
    return_trace (true);
  }
};

template <typename VAL>
struct ParsedValues
{
  void init ()
  {
    opStart = 0;
    values.init ();
  }
  void fini () { values.fini_deep (); }

  void add_op (OpCode op, const SubByteStr& substr = SubByteStr ())
  {
    VAL *val = values.push ();
    val->op = op;
    val->str = ByteStr (substr.str, opStart, substr.offset - opStart);
    opStart = substr.offset;
  }

  void add_op (OpCode op, const SubByteStr& substr, const VAL &v)
  {
    VAL *val = values.push (v);
    val->op = op;
    val->str = ByteStr (substr.str, opStart, substr.offset - opStart);
    opStart = substr.offset;
  }

  bool has_op (OpCode op) const
  {
    for (unsigned int i = 0; i < get_count (); i++)
      if (get_value (i).op == op) return true;
    return false;
  }

  unsigned get_count () const { return values.len; }
  const VAL &get_value (unsigned int i) const { return values[i]; }
  const VAL &operator [] (unsigned int i) const { return get_value (i); }

  unsigned int       opStart;
  hb_vector_t<VAL>   values;
};

template <typename ARG=Number>
struct InterpEnv
{
  void init (const ByteStr &str_)
  {
    substr.reset (str_);
    argStack.init ();
    error = false;
  }
  void fini () { argStack.fini (); }

  bool in_error () const
  { return error || substr.in_error () || argStack.in_error (); }

  void set_error () { error = true; }

  OpCode fetch_op ()
  {
    OpCode  op = OpCode_Invalid;
    if (unlikely (!substr.avail ()))
      return OpCode_Invalid;
    op = (OpCode)(unsigned char)substr[0];
    if (op == OpCode_escape) {
      if (unlikely (!substr.avail ()))
	return OpCode_Invalid;
      op = Make_OpCode_ESC(substr[1]);
      substr.inc ();
    }
    substr.inc ();
    return op;
  }

  const ARG& eval_arg (unsigned int i)
  {
    return argStack[i];
  }

  ARG& pop_arg ()
  {
    return argStack.pop ();
  }

  void pop_n_args (unsigned int n)
  {
    argStack.pop (n);
  }

  void clear_args ()
  {
    pop_n_args (argStack.get_count ());
  }

  SubByteStr    substr;
  ArgStack<ARG> argStack;
  protected:
  bool	  error;
};

typedef InterpEnv<> NumInterpEnv;

template <typename ARG=Number>
struct OpSet
{
  static void process_op (OpCode op, InterpEnv<ARG>& env)
  {
    switch (op) {
      case OpCode_shortint:
	env.argStack.push_int ((int16_t)((env.substr[0] << 8) | env.substr[1]));
	env.substr.inc (2);
	break;

      case OpCode_TwoBytePosInt0: case OpCode_TwoBytePosInt1:
      case OpCode_TwoBytePosInt2: case OpCode_TwoBytePosInt3:
	env.argStack.push_int ((int16_t)((op - OpCode_TwoBytePosInt0) * 256 + env.substr[0] + 108));
	env.substr.inc ();
	break;

      case OpCode_TwoByteNegInt0: case OpCode_TwoByteNegInt1:
      case OpCode_TwoByteNegInt2: case OpCode_TwoByteNegInt3:
	env.argStack.push_int ((int16_t)(-(op - OpCode_TwoByteNegInt0) * 256 - env.substr[0] - 108));
	env.substr.inc ();
	break;

      default:
	/* 1-byte integer */
	if (likely ((OpCode_OneByteIntFirst <= op) && (op <= OpCode_OneByteIntLast)))
	{
	  env.argStack.push_int ((int)op - 139);
	} else {
	  /* invalid unknown operator */
	  env.clear_args ();
	  env.set_error ();
	}
	break;
    }
  }
};

template <typename ENV>
struct Interpreter {

  ~Interpreter() { fini (); }

  void fini () { env.fini (); }

  ENV env;
};

} /* namespace CFF */

#endif /* HB_CFF_INTERP_COMMON_HH */
