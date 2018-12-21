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

#ifndef HB_OT_CFF2_TABLE_HH
#define HB_OT_CFF2_TABLE_HH

#include "hb-ot-head-table.hh"
#include "hb-ot-cff-common.hh"
#include "hb-subset-cff2.hh"

namespace CFF {

/*
 * CFF2 -- Compact Font Format (CFF) Version 2
 * https://docs.microsoft.com/en-us/typography/opentype/spec/cff2
 */
#define HB_OT_TAG_cff2 HB_TAG('C','F','F','2')

typedef CFFIndex<HBUINT32>  CFF2Index;
template <typename Type> struct CFF2IndexOf : CFFIndexOf<HBUINT32, Type> {};

typedef CFF2Index         CFF2CharStrings;
typedef FDArray<HBUINT32> CFF2FDArray;
typedef Subrs<HBUINT32>   CFF2Subrs;

typedef FDSelect3_4<HBUINT32, HBUINT16> FDSelect4;
typedef FDSelect3_4_Range<HBUINT32, HBUINT16> FDSelect4_Range;

struct CFF2FDSelect
{
  bool sanitize (hb_sanitize_context_t *c, unsigned int fdcount) const
  {
    TRACE_SANITIZE (this);

    return_trace (likely (c->check_struct (this) && (format == 0 || format == 3 || format == 4) &&
			  (format == 0)?
			  u.format0.sanitize (c, fdcount):
			    ((format == 3)?
			    u.format3.sanitize (c, fdcount):
			    u.format4.sanitize (c, fdcount))));
  }

  bool serialize (hb_serialize_context_t *c, const CFF2FDSelect &src, unsigned int num_glyphs)
  {
    TRACE_SERIALIZE (this);
    unsigned int size = src.get_size (num_glyphs);
    CFF2FDSelect *dest = c->allocate_size<CFF2FDSelect> (size);
    if (unlikely (dest == nullptr)) return_trace (false);
    memcpy (dest, &src, size);
    return_trace (true);
  }

  unsigned int calculate_serialized_size (unsigned int num_glyphs) const
  { return get_size (num_glyphs); }

  unsigned int get_size (unsigned int num_glyphs) const
  {
    unsigned int size = format.static_size;
    if (format == 0)
      size += u.format0.get_size (num_glyphs);
    else if (format == 3)
      size += u.format3.get_size ();
    else
      size += u.format4.get_size ();
    return size;
  }

  hb_codepoint_t get_fd (hb_codepoint_t glyph) const
  {
    if (this == &Null(CFF2FDSelect))
      return 0;
    if (format == 0)
      return u.format0.get_fd (glyph);
    else if (format == 3)
      return u.format3.get_fd (glyph);
    else
      return u.format4.get_fd (glyph);
  }

  HBUINT8       format;
  union {
    FDSelect0   format0;
    FDSelect3   format3;
    FDSelect4   format4;
  } u;

  DEFINE_SIZE_MIN (2);
};

struct CFF2VariationStore
{
  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this)) && c->check_range (&varStore, size) && varStore.sanitize (c));
  }

  bool serialize (hb_serialize_context_t *c, const CFF2VariationStore *varStore)
  {
    TRACE_SERIALIZE (this);
    unsigned int size_ = varStore->get_size ();
    CFF2VariationStore *dest = c->allocate_size<CFF2VariationStore> (size_);
    if (unlikely (dest == nullptr)) return_trace (false);
    memcpy (dest, varStore, size_);
    return_trace (true);
  }

  unsigned int get_size () const { return HBUINT16::static_size + size; }

  HBUINT16	size;
  VariationStore  varStore;

  DEFINE_SIZE_MIN (2 + VariationStore::min_size);
};

struct CFF2TopDictValues : TopDictValues<>
{
  void init ()
  {
    TopDictValues<>::init ();
    vstoreOffset = 0;
    FDSelectOffset = 0;
  }
  void fini () { TopDictValues<>::fini (); }

  unsigned int calculate_serialized_size () const
  {
    unsigned int size = 0;
    for (unsigned int i = 0; i < get_count (); i++)
    {
      OpCode op = get_value (i).op;
      switch (op)
      {
	case OpCode_vstore:
	case OpCode_FDSelect:
	  size += OpCode_Size (OpCode_longintdict) + 4 + OpCode_Size (op);
	  break;
	default:
	  size += TopDictValues<>::calculate_serialized_op_size (get_value (i));
	  break;
      }
    }
    return size;
  }

  unsigned int  vstoreOffset;
  unsigned int  FDSelectOffset;
};

struct CFF2TopDictOpSet : TopDictOpSet<>
{
  static void process_op (OpCode op, NumInterpEnv& env, CFF2TopDictValues& dictval)
  {
    switch (op) {
      case OpCode_FontMatrix:
	{
	  DictVal val;
	  val.init ();
	  dictval.add_op (op, env.substr);
	  env.clear_args ();
	}
	break;

      case OpCode_vstore:
	dictval.vstoreOffset = env.argStack.pop_uint ();
	env.clear_args ();
	break;
      case OpCode_FDSelect:
	dictval.FDSelectOffset = env.argStack.pop_uint ();
	env.clear_args ();
	break;

      default:
	SUPER::process_op (op, env, dictval);
	/* Record this operand below if stack is empty, otherwise done */
	if (!env.argStack.is_empty ()) return;
    }

    if (unlikely (env.in_error ())) return;

    dictval.add_op (op, env.substr);
  }

  typedef TopDictOpSet<> SUPER;
};

struct CFF2FontDictValues : DictValues<OpStr>
{
  void init ()
  {
    DictValues<OpStr>::init ();
    privateDictInfo.init ();
  }
  void fini () { DictValues<OpStr>::fini (); }

  TableInfo    privateDictInfo;
};

struct CFF2FontDictOpSet : DictOpSet
{
  static void process_op (OpCode op, NumInterpEnv& env, CFF2FontDictValues& dictval)
  {
    switch (op) {
      case OpCode_Private:
	dictval.privateDictInfo.offset = env.argStack.pop_uint ();
	dictval.privateDictInfo.size = env.argStack.pop_uint ();
	env.clear_args ();
	break;

      default:
	SUPER::process_op (op, env);
	if (!env.argStack.is_empty ())
	  return;
    }

    if (unlikely (env.in_error ())) return;

    dictval.add_op (op, env.substr);
  }

  private:
  typedef DictOpSet SUPER;
};

template <typename VAL>
struct CFF2PrivateDictValues_Base : DictValues<VAL>
{
  void init ()
  {
    DictValues<VAL>::init ();
    subrsOffset = 0;
    localSubrs = &Null(CFF2Subrs);
    ivs = 0;
  }
  void fini () { DictValues<VAL>::fini (); }

  unsigned int calculate_serialized_size () const
  {
    unsigned int size = 0;
    for (unsigned int i = 0; i < DictValues<VAL>::get_count; i++)
      if (DictValues<VAL>::get_value (i).op == OpCode_Subrs)
	size += OpCode_Size (OpCode_shortint) + 2 + OpCode_Size (OpCode_Subrs);
      else
	size += DictValues<VAL>::get_value (i).str.len;
    return size;
  }

  unsigned int      subrsOffset;
  const CFF2Subrs   *localSubrs;
  unsigned int      ivs;
};

typedef CFF2PrivateDictValues_Base<OpStr> CFF2PrivateDictValues_Subset;
typedef CFF2PrivateDictValues_Base<NumDictVal> CFF2PrivateDictValues;

struct CFF2PrivDictInterpEnv : NumInterpEnv
{
  void init (const ByteStr &str)
  {
    NumInterpEnv::init (str);
    ivs = 0;
    seen_vsindex = false;
  }

  void process_vsindex ()
  {
    if (likely (!seen_vsindex))
    {
      set_ivs (argStack.pop_uint ());
    }
    seen_vsindex = true;
  }

  unsigned int get_ivs () const { return ivs; }
  void	 set_ivs (unsigned int ivs_) { ivs = ivs_; }

  protected:
  unsigned int  ivs;
  bool	  seen_vsindex;
};

struct CFF2PrivateDictOpSet : DictOpSet
{
  static void process_op (OpCode op, CFF2PrivDictInterpEnv& env, CFF2PrivateDictValues& dictval)
  {
    NumDictVal val;
    val.init ();

    switch (op) {
      case OpCode_StdHW:
      case OpCode_StdVW:
      case OpCode_BlueScale:
      case OpCode_BlueShift:
      case OpCode_BlueFuzz:
      case OpCode_ExpansionFactor:
      case OpCode_LanguageGroup:
	val.single_val = env.argStack.pop_num ();
	env.clear_args ();
	break;
      case OpCode_BlueValues:
      case OpCode_OtherBlues:
      case OpCode_FamilyBlues:
      case OpCode_FamilyOtherBlues:
      case OpCode_StemSnapH:
      case OpCode_StemSnapV:
	env.clear_args ();
	break;
      case OpCode_Subrs:
	dictval.subrsOffset = env.argStack.pop_uint ();
	env.clear_args ();
	break;
      case OpCode_vsindexdict:
	env.process_vsindex ();
	dictval.ivs = env.get_ivs ();
	env.clear_args ();
	break;
      case OpCode_blenddict:
	break;

      default:
	DictOpSet::process_op (op, env);
	if (!env.argStack.is_empty ()) return;
	break;
    }

    if (unlikely (env.in_error ())) return;

    dictval.add_op (op, env.substr, val);
  }
};

struct CFF2PrivateDictOpSet_Subset : DictOpSet
{
  static void process_op (OpCode op, CFF2PrivDictInterpEnv& env, CFF2PrivateDictValues_Subset& dictval)
  {
    switch (op) {
      case OpCode_BlueValues:
      case OpCode_OtherBlues:
      case OpCode_FamilyBlues:
      case OpCode_FamilyOtherBlues:
      case OpCode_StdHW:
      case OpCode_StdVW:
      case OpCode_BlueScale:
      case OpCode_BlueShift:
      case OpCode_BlueFuzz:
      case OpCode_StemSnapH:
      case OpCode_StemSnapV:
      case OpCode_LanguageGroup:
      case OpCode_ExpansionFactor:
	env.clear_args ();
	break;

      case OpCode_blenddict:
	env.clear_args ();
	return;

      case OpCode_Subrs:
	dictval.subrsOffset = env.argStack.pop_uint ();
	env.clear_args ();
	break;

      default:
	SUPER::process_op (op, env);
	if (!env.argStack.is_empty ()) return;
	break;
    }

    if (unlikely (env.in_error ())) return;

    dictval.add_op (op, env.substr);
  }

  private:
  typedef DictOpSet SUPER;
};

typedef DictInterpreter<CFF2TopDictOpSet, CFF2TopDictValues> CFF2TopDict_Interpreter;
typedef DictInterpreter<CFF2FontDictOpSet, CFF2FontDictValues> CFF2FontDict_Interpreter;

}; /* namespace CFF */

namespace OT {

using namespace CFF;

struct cff2
{
  static const hb_tag_t tableTag	= HB_OT_TAG_cff2;

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  likely (version.major == 2));
  }

  template <typename PRIVOPSET, typename PRIVDICTVAL>
  struct accelerator_templ_t
  {
    void init (hb_face_t *face)
    {
      topDict.init ();
      fontDicts.init ();
      privateDicts.init ();

      this->blob = sc.reference_table<cff2> (face);

      /* setup for run-time santization */
      sc.init (this->blob);
      sc.start_processing ();

      const OT::cff2 *cff2 = this->blob->template as<OT::cff2> ();

      if (cff2 == &Null(OT::cff2))
      { fini (); return; }

      { /* parse top dict */
	ByteStr topDictStr (cff2 + cff2->topDict, cff2->topDictSize);
	if (unlikely (!topDictStr.sanitize (&sc))) { fini (); return; }
	CFF2TopDict_Interpreter top_interp;
	top_interp.env.init (topDictStr);
	topDict.init ();
	if (unlikely (!top_interp.interpret (topDict))) { fini (); return; }
      }

      globalSubrs = &StructAtOffset<CFF2Subrs> (cff2, cff2->topDict + cff2->topDictSize);
      varStore = &StructAtOffsetOrNull<CFF2VariationStore> (cff2, topDict.vstoreOffset);
      charStrings = &StructAtOffsetOrNull<CFF2CharStrings> (cff2, topDict.charStringsOffset);
      fdArray = &StructAtOffsetOrNull<CFF2FDArray> (cff2, topDict.FDArrayOffset);
      fdSelect = &StructAtOffsetOrNull<CFF2FDSelect> (cff2, topDict.FDSelectOffset);

      if (((varStore != &Null(CFF2VariationStore)) && unlikely (!varStore->sanitize (&sc))) ||
	  (charStrings == &Null(CFF2CharStrings)) || unlikely (!charStrings->sanitize (&sc)) ||
	  (globalSubrs == &Null(CFF2Subrs)) || unlikely (!globalSubrs->sanitize (&sc)) ||
	  (fdArray == &Null(CFF2FDArray)) || unlikely (!fdArray->sanitize (&sc)) ||
	  (((fdSelect != &Null(CFF2FDSelect)) && unlikely (!fdSelect->sanitize (&sc, fdArray->count)))))
      { fini (); return; }

      num_glyphs = charStrings->count;
      if (num_glyphs != sc.get_num_glyphs ())
      { fini (); return; }

      fdCount = fdArray->count;
      privateDicts.resize (fdCount);

      /* parse font dicts and gather private dicts */
      for (unsigned int i = 0; i < fdCount; i++)
      {
	const ByteStr fontDictStr = (*fdArray)[i];
	if (unlikely (!fontDictStr.sanitize (&sc))) { fini (); return; }
	CFF2FontDictValues  *font;
	CFF2FontDict_Interpreter font_interp;
	font_interp.env.init (fontDictStr);
	font = fontDicts.push ();
	if (unlikely (font == &Crap(CFF2FontDictValues))) { fini (); return; }
	font->init ();
	if (unlikely (!font_interp.interpret (*font))) { fini (); return; }

	const ByteStr privDictStr (StructAtOffsetOrNull<UnsizedByteStr> (cff2, font->privateDictInfo.offset), font->privateDictInfo.size);
	if (unlikely (!privDictStr.sanitize (&sc))) { fini (); return; }
	DictInterpreter<PRIVOPSET, PRIVDICTVAL, CFF2PrivDictInterpEnv>  priv_interp;
	priv_interp.env.init(privDictStr);
	privateDicts[i].init ();
	if (unlikely (!priv_interp.interpret (privateDicts[i]))) { fini (); return; }

	privateDicts[i].localSubrs = &StructAtOffsetOrNull<CFF2Subrs> (privDictStr.str, privateDicts[i].subrsOffset);
	if (privateDicts[i].localSubrs != &Null(CFF2Subrs) &&
	  unlikely (!privateDicts[i].localSubrs->sanitize (&sc)))
	{ fini (); return; }
      }
    }

    void fini ()
    {
      sc.end_processing ();
      fontDicts.fini_deep ();
      privateDicts.fini_deep ();
      hb_blob_destroy (blob);
      blob = nullptr;
    }

    bool is_valid () const { return blob != nullptr; }

    protected:
    hb_blob_t	       *blob;
    hb_sanitize_context_t   sc;

    public:
    CFF2TopDictValues	 topDict;
    const CFF2Subrs	   *globalSubrs;
    const CFF2VariationStore  *varStore;
    const CFF2CharStrings     *charStrings;
    const CFF2FDArray	 *fdArray;
    const CFF2FDSelect	*fdSelect;
    unsigned int	      fdCount;

    hb_vector_t<CFF2FontDictValues>     fontDicts;
    hb_vector_t<PRIVDICTVAL>  privateDicts;

    unsigned int	    num_glyphs;
  };

  struct accelerator_t : accelerator_templ_t<CFF2PrivateDictOpSet, CFF2PrivateDictValues>
  {
    HB_INTERNAL bool get_extents (hb_font_t *font,
				  hb_codepoint_t glyph,
				  hb_glyph_extents_t *extents) const;
  };

  typedef accelerator_templ_t<CFF2PrivateDictOpSet_Subset, CFF2PrivateDictValues_Subset> accelerator_subset_t;

  bool subset (hb_subset_plan_t *plan) const
  {
    hb_blob_t *cff2_prime = nullptr;

    bool success = true;
    if (hb_subset_cff2 (plan, &cff2_prime)) {
      success = success && plan->add_table (HB_OT_TAG_cff2, cff2_prime);
      hb_blob_t *head_blob = hb_sanitize_context_t().reference_table<head> (plan->source);
      success = success && head_blob && plan->add_table (HB_OT_TAG_head, head_blob);
      hb_blob_destroy (head_blob);
    } else {
      success = false;
    }
    hb_blob_destroy (cff2_prime);

    return success;
  }

  public:
  FixedVersion<HBUINT8> version;	/* Version of CFF2 table. set to 0x0200u */
  OffsetTo<TopDict, HBUINT8, false> topDict;   /* headerSize = Offset to Top DICT. */
  HBUINT16       topDictSize;	   /* Top DICT size */

  public:
  DEFINE_SIZE_STATIC (5);
};

struct cff2_accelerator_t : cff2::accelerator_t {};
} /* namespace OT */

#endif /* HB_OT_CFF2_TABLE_HH */
