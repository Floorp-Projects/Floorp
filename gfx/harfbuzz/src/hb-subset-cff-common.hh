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

#ifndef HB_SUBSET_CFF_COMMON_HH
#define HB_SUBSET_CFF_COMMON_HH

#include "hb.hh"

#include "hb-subset-plan.hh"
#include "hb-cff-interp-cs-common.hh"

namespace CFF {

/* Used for writing a temporary charstring */
struct StrEncoder
{
  StrEncoder (StrBuff &buff_)
    : buff (buff_), error (false) {}

  void reset () { buff.resize (0); }

  void encode_byte (unsigned char b)
  {
    if (unlikely (buff.push ((const char)b) == &Crap(char)))
      set_error ();
  }

  void encode_int (int v)
  {
    if ((-1131 <= v) && (v <= 1131))
    {
      if ((-107 <= v) && (v <= 107))
	encode_byte (v + 139);
      else if (v > 0)
      {
	v -= 108;
	encode_byte ((v >> 8) + OpCode_TwoBytePosInt0);
	encode_byte (v & 0xFF);
      }
      else
      {
	v = -v - 108;
	encode_byte ((v >> 8) + OpCode_TwoByteNegInt0);
	encode_byte (v & 0xFF);
      }
    }
    else
    {
      if (unlikely (v < -32768))
	v = -32768;
      else if (unlikely (v > 32767))
	v = 32767;
      encode_byte (OpCode_shortint);
      encode_byte ((v >> 8) & 0xFF);
      encode_byte (v & 0xFF);
    }
  }

  void encode_num (const Number& n)
  {
    if (n.in_int_range ())
    {
      encode_int (n.to_int ());
    }
    else
    {
      int32_t v = n.to_fixed ();
      encode_byte (OpCode_fixedcs);
      encode_byte ((v >> 24) & 0xFF);
      encode_byte ((v >> 16) & 0xFF);
      encode_byte ((v >> 8) & 0xFF);
      encode_byte (v & 0xFF);
    }
  }

  void encode_op (OpCode op)
  {
    if (Is_OpCode_ESC (op))
    {
      encode_byte (OpCode_escape);
      encode_byte (Unmake_OpCode_ESC (op));
    }
    else
      encode_byte (op);
  }

  void copy_str (const ByteStr &str)
  {
    unsigned int  offset = buff.len;
    buff.resize (offset + str.len);
    if (unlikely (buff.len < offset + str.len))
    {
      set_error ();
      return;
    }
    memcpy (&buff[offset], &str.str[0], str.len);
  }

  bool is_error () const { return error; }

  protected:
  void set_error () { error = true; }

  StrBuff &buff;
  bool    error;
};

struct CFFSubTableOffsets {
  CFFSubTableOffsets () : privateDictsOffset (0)
  {
    topDictInfo.init ();
    FDSelectInfo.init ();
    FDArrayInfo.init ();
    charStringsInfo.init ();
    globalSubrsInfo.init ();
    localSubrsInfos.init ();
  }

  ~CFFSubTableOffsets () { localSubrsInfos.fini (); }

  TableInfo     topDictInfo;
  TableInfo     FDSelectInfo;
  TableInfo     FDArrayInfo;
  TableInfo     charStringsInfo;
  unsigned int  privateDictsOffset;
  TableInfo     globalSubrsInfo;
  hb_vector_t<TableInfo>  localSubrsInfos;
};

template <typename OPSTR=OpStr>
struct CFFTopDict_OpSerializer : OpSerializer
{
  bool serialize (hb_serialize_context_t *c,
		  const OPSTR &opstr,
		  const CFFSubTableOffsets &offsets) const
  {
    TRACE_SERIALIZE (this);

    switch (opstr.op)
    {
      case OpCode_CharStrings:
	return_trace (FontDict::serialize_offset4_op(c, opstr.op, offsets.charStringsInfo.offset));

      case OpCode_FDArray:
	return_trace (FontDict::serialize_offset4_op(c, opstr.op, offsets.FDArrayInfo.offset));

      case OpCode_FDSelect:
	return_trace (FontDict::serialize_offset4_op(c, opstr.op, offsets.FDSelectInfo.offset));

      default:
	return_trace (copy_opstr (c, opstr));
    }
    return_trace (true);
  }

  unsigned int calculate_serialized_size (const OPSTR &opstr) const
  {
    switch (opstr.op)
    {
      case OpCode_CharStrings:
      case OpCode_FDArray:
      case OpCode_FDSelect:
	return OpCode_Size (OpCode_longintdict) + 4 + OpCode_Size (opstr.op);

      default:
	return opstr.str.len;
    }
  }
};

struct CFFFontDict_OpSerializer : OpSerializer
{
  bool serialize (hb_serialize_context_t *c,
		  const OpStr &opstr,
		  const TableInfo &privateDictInfo) const
  {
    TRACE_SERIALIZE (this);

    if (opstr.op == OpCode_Private)
    {
      /* serialize the private dict size & offset as 2-byte & 4-byte integers */
      if (unlikely (!UnsizedByteStr::serialize_int2 (c, privateDictInfo.size) ||
		    !UnsizedByteStr::serialize_int4 (c, privateDictInfo.offset)))
	return_trace (false);

      /* serialize the opcode */
      HBUINT8 *p = c->allocate_size<HBUINT8> (1);
      if (unlikely (p == nullptr)) return_trace (false);
      p->set (OpCode_Private);

      return_trace (true);
    }
    else
    {
      HBUINT8 *d = c->allocate_size<HBUINT8> (opstr.str.len);
      if (unlikely (d == nullptr)) return_trace (false);
      memcpy (d, &opstr.str.str[0], opstr.str.len);
    }
    return_trace (true);
  }

  unsigned int calculate_serialized_size (const OpStr &opstr) const
  {
    if (opstr.op == OpCode_Private)
      return OpCode_Size (OpCode_longintdict) + 4 + OpCode_Size (OpCode_shortint) + 2 + OpCode_Size (OpCode_Private);
    else
      return opstr.str.len;
  }
};

struct CFFPrivateDict_OpSerializer : OpSerializer
{
  CFFPrivateDict_OpSerializer (bool desubroutinize_, bool drop_hints_)
    : desubroutinize (desubroutinize_), drop_hints (drop_hints_) {}

  bool serialize (hb_serialize_context_t *c,
		  const OpStr &opstr,
		  const unsigned int subrsOffset) const
  {
    TRACE_SERIALIZE (this);

    if (drop_hints && DictOpSet::is_hint_op (opstr.op))
      return true;
    if (opstr.op == OpCode_Subrs)
    {
      if (desubroutinize || (subrsOffset == 0))
	return_trace (true);
      else
	return_trace (FontDict::serialize_offset2_op (c, opstr.op, subrsOffset));
    }
    else
      return_trace (copy_opstr (c, opstr));
  }

  unsigned int calculate_serialized_size (const OpStr &opstr,
					  bool has_localsubr=true) const
  {
    if (drop_hints && DictOpSet::is_hint_op (opstr.op))
      return 0;
    if (opstr.op == OpCode_Subrs)
    {
      if (desubroutinize || !has_localsubr)
	return 0;
      else
	return OpCode_Size (OpCode_shortint) + 2 + OpCode_Size (opstr.op);
    }
    else
      return opstr.str.len;
  }

  protected:
  const bool  desubroutinize;
  const bool  drop_hints;
};

struct FlattenParam
{
  StrBuff     &flatStr;
  bool	drop_hints;
};

template <typename ACC, typename ENV, typename OPSET>
struct SubrFlattener
{
  SubrFlattener (const ACC &acc_,
			const hb_vector_t<hb_codepoint_t> &glyphs_,
			bool drop_hints_)
    : acc (acc_),
      glyphs (glyphs_),
      drop_hints (drop_hints_)
  {}

  bool flatten (StrBuffArray &flat_charstrings)
  {
    if (!flat_charstrings.resize (glyphs.len))
      return false;
    for (unsigned int i = 0; i < glyphs.len; i++)
      flat_charstrings[i].init ();
    for (unsigned int i = 0; i < glyphs.len; i++)
    {
      hb_codepoint_t  glyph = glyphs[i];
      const ByteStr str = (*acc.charStrings)[glyph];
      unsigned int fd = acc.fdSelect->get_fd (glyph);
      if (unlikely (fd >= acc.fdCount))
      	return false;
      CSInterpreter<ENV, OPSET, FlattenParam> interp;
      interp.env.init (str, acc, fd);
      FlattenParam  param = { flat_charstrings[i], drop_hints };
      if (unlikely (!interp.interpret (param)))
	return false;
    }
    return true;
  }

  const ACC &acc;
  const hb_vector_t<hb_codepoint_t> &glyphs;
  bool  drop_hints;
};

struct SubrClosures
{
  SubrClosures () : valid (false), global_closure (nullptr)
  { local_closures.init (); }

  void init (unsigned int fd_count)
  {
    valid = true;
    global_closure = hb_set_create ();
    if (global_closure == hb_set_get_empty ())
      valid = false;
    if (!local_closures.resize (fd_count))
      valid = false;

    for (unsigned int i = 0; i < local_closures.len; i++)
    {
      local_closures[i] = hb_set_create ();
      if (local_closures[i] == hb_set_get_empty ())
	valid = false;
    }
  }

  void fini ()
  {
    hb_set_destroy (global_closure);
    for (unsigned int i = 0; i < local_closures.len; i++)
      hb_set_destroy (local_closures[i]);
    local_closures.fini ();
  }

  void reset ()
  {
    hb_set_clear (global_closure);
    for (unsigned int i = 0; i < local_closures.len; i++)
      hb_set_clear (local_closures[i]);
  }

  bool is_valid () const { return valid; }
  bool  valid;
  hb_set_t  *global_closure;
  hb_vector_t<hb_set_t *> local_closures;
};

struct ParsedCSOp : OpStr
{
  void init (unsigned int subr_num_ = 0)
  {
    OpStr::init ();
    subr_num = subr_num_;
    drop_flag = false;
    keep_flag = false;
    skip_flag = false;
  }

  void fini () { OpStr::fini (); }

  bool for_drop () const { return drop_flag; }
  void set_drop ()       { if (!for_keep ()) drop_flag = true; }

  bool for_keep () const { return keep_flag; }
  void set_keep ()       { keep_flag = true; }

  bool for_skip () const { return skip_flag; }
  void set_skip ()       { skip_flag = true; }

  unsigned int  subr_num;

  protected:
  bool	  drop_flag : 1;
  bool	  keep_flag : 1;
  bool	  skip_flag : 1;
};

struct ParsedCStr : ParsedValues<ParsedCSOp>
{
  void init ()
  {
    SUPER::init ();
    parsed = false;
    hint_dropped = false;
    has_prefix_ = false;
  }

  void add_op (OpCode op, const SubByteStr& substr)
  {
    if (!is_parsed ())
      SUPER::add_op (op, substr);
  }

  void add_call_op (OpCode op, const SubByteStr& substr, unsigned int subr_num)
  {
    if (!is_parsed ())
    {
      unsigned int parsed_len = get_count ();
      if (likely (parsed_len > 0))
	values[parsed_len-1].set_skip ();

      ParsedCSOp val;
      val.init (subr_num);
      SUPER::add_op (op, substr, val);
    }
  }

  void set_prefix (const Number &num, OpCode op = OpCode_Invalid)
  {
    has_prefix_ = true;
    prefix_op_ = op;
    prefix_num_ = num;
  }

  bool at_end (unsigned int pos) const
  {
    return ((pos + 1 >= values.len) /* CFF2 */
	|| (values[pos + 1].op == OpCode_return));
  }

  bool is_parsed () const { return parsed; }
  void set_parsed ()      { parsed = true; }

  bool is_hint_dropped () const { return hint_dropped; }
  void set_hint_dropped ()      { hint_dropped = true; }

  bool is_vsindex_dropped () const { return vsindex_dropped; }
  void set_vsindex_dropped ()      { vsindex_dropped = true; }

  bool has_prefix () const          { return has_prefix_; }
  OpCode prefix_op () const         { return prefix_op_; }
  const Number &prefix_num () const { return prefix_num_; }

  protected:
  bool    parsed;
  bool    hint_dropped;
  bool    vsindex_dropped;
  bool    has_prefix_;
  OpCode  prefix_op_;
  Number  prefix_num_;

  private:
  typedef ParsedValues<ParsedCSOp> SUPER;
};

struct ParsedCStrs : hb_vector_t<ParsedCStr>
{
  void init (unsigned int len_ = 0)
  {
    SUPER::init ();
    resize (len_);
    for (unsigned int i = 0; i < len; i++)
      (*this)[i].init ();
  }
  void fini () { SUPER::fini_deep (); }

  private:
  typedef hb_vector_t<ParsedCStr> SUPER;
};

struct SubrSubsetParam
{
  void init (ParsedCStr *parsed_charstring_,
		    ParsedCStrs *parsed_global_subrs_, ParsedCStrs *parsed_local_subrs_,
		    hb_set_t *global_closure_, hb_set_t *local_closure_,
		    bool drop_hints_)
  {
    parsed_charstring = parsed_charstring_;
    current_parsed_str = parsed_charstring;
    parsed_global_subrs = parsed_global_subrs_;
    parsed_local_subrs = parsed_local_subrs_;
    global_closure = global_closure_;
    local_closure = local_closure_;
    drop_hints = drop_hints_;
  }

  ParsedCStr *get_parsed_str_for_context (CallContext &context)
  {
    switch (context.type)
    {
      case CSType_CharString:
	return parsed_charstring;

      case CSType_LocalSubr:
	if (likely (context.subr_num < parsed_local_subrs->len))
	  return &(*parsed_local_subrs)[context.subr_num];
	break;

      case CSType_GlobalSubr:
	if (likely (context.subr_num < parsed_global_subrs->len))
	  return &(*parsed_global_subrs)[context.subr_num];
	break;
    }
    return nullptr;
  }

  template <typename ENV>
  void set_current_str (ENV &env, bool calling)
  {
    ParsedCStr  *parsed_str = get_parsed_str_for_context (env.context);
    if (likely (parsed_str != nullptr))
    {
      /* If the called subroutine is parsed partially but not completely yet,
       * it must be because we are calling it recursively.
       * Handle it as an error. */
      if (unlikely (calling && !parsed_str->is_parsed () && (parsed_str->values.len > 0)))
      	env.set_error ();
      else
      	current_parsed_str = parsed_str;
    }
    else
      env.set_error ();
  }

  ParsedCStr    *current_parsed_str;

  ParsedCStr    *parsed_charstring;
  ParsedCStrs   *parsed_global_subrs;
  ParsedCStrs   *parsed_local_subrs;
  hb_set_t      *global_closure;
  hb_set_t      *local_closure;
  bool	  drop_hints;
};

struct SubrRemap : Remap
{
  void create (hb_set_t *closure)
  {
    /* create a remapping of subroutine numbers from old to new.
     * no optimization based on usage counts. fonttools doesn't appear doing that either.
     */
    reset (closure->get_max () + 1);
    for (hb_codepoint_t old_num = 0; old_num < len; old_num++)
    {
      if (hb_set_has (closure, old_num))
	add (old_num);
    }

    if (get_count () < 1240)
      bias = 107;
    else if (get_count () < 33900)
      bias = 1131;
    else
      bias = 32768;
  }

  hb_codepoint_t operator[] (unsigned int old_num) const
  {
    if (old_num >= len)
      return CFF_UNDEF_CODE;
    else
      return Remap::operator[] (old_num);
  }

  int biased_num (unsigned int old_num) const
  {
    hb_codepoint_t new_num = (*this)[old_num];
    return (int)new_num - bias;
  }

  protected:
  int bias;
};

struct SubrRemaps
{
  SubrRemaps ()
  {
    global_remap.init ();
    local_remaps.init ();
  }

  ~SubrRemaps () { fini (); }

  void init (unsigned int fdCount)
  {
    local_remaps.resize (fdCount);
    for (unsigned int i = 0; i < fdCount; i++)
      local_remaps[i].init ();
  }

  void create (SubrClosures& closures)
  {
    global_remap.create (closures.global_closure);
    for (unsigned int i = 0; i < local_remaps.len; i++)
      local_remaps[i].create (closures.local_closures[i]);
  }

  void fini ()
  {
    global_remap.fini ();
    local_remaps.fini_deep ();
  }

  SubrRemap	       global_remap;
  hb_vector_t<SubrRemap>  local_remaps;
};

template <typename SUBSETTER, typename SUBRS, typename ACC, typename ENV, typename OPSET>
struct SubrSubsetter
{
  SubrSubsetter ()
  {
    parsed_charstrings.init ();
    parsed_global_subrs.init ();
    parsed_local_subrs.init ();
  }

  ~SubrSubsetter ()
  {
    closures.fini ();
    remaps.fini ();
    parsed_charstrings.fini_deep ();
    parsed_global_subrs.fini_deep ();
    parsed_local_subrs.fini_deep ();
  }

  /* Subroutine subsetting with --no-desubroutinize runs in phases:
   *
   * 1. execute charstrings/subroutines to determine subroutine closures
   * 2. parse out all operators and numbers
   * 3. mark hint operators and operands for removal if --no-hinting
   * 4. re-encode all charstrings and subroutines with new subroutine numbers
   *
   * Phases #1 and #2 are done at the same time in collect_subrs ().
   * Phase #3 walks charstrings/subroutines forward then backward (hence parsing required),
   * because we can't tell if a number belongs to a hint op until we see the first moveto.
   *
   * Assumption: a callsubr/callgsubr operator must immediately follow a (biased) subroutine number
   * within the same charstring/subroutine, e.g., not split across a charstring and a subroutine.
   */
  bool subset (ACC &acc, const hb_vector_t<hb_codepoint_t> &glyphs, bool drop_hints)
  {
    closures.init (acc.fdCount);
    remaps.init (acc.fdCount);

    parsed_charstrings.init (glyphs.len);
    parsed_global_subrs.init (acc.globalSubrs->count);
    parsed_local_subrs.resize (acc.fdCount);
    for (unsigned int i = 0; i < acc.fdCount; i++)
    {
      parsed_local_subrs[i].init (acc.privateDicts[i].localSubrs->count);
    }
    if (unlikely (!closures.valid))
      return false;

    /* phase 1 & 2 */
    for (unsigned int i = 0; i < glyphs.len; i++)
    {
      hb_codepoint_t  glyph = glyphs[i];
      const ByteStr str = (*acc.charStrings)[glyph];
      unsigned int fd = acc.fdSelect->get_fd (glyph);
      if (unlikely (fd >= acc.fdCount))
      	return false;

      CSInterpreter<ENV, OPSET, SubrSubsetParam> interp;
      interp.env.init (str, acc, fd);

      SubrSubsetParam  param;
      param.init (&parsed_charstrings[i],
		  &parsed_global_subrs,  &parsed_local_subrs[fd],
		  closures.global_closure, closures.local_closures[fd],
		  drop_hints);

      if (unlikely (!interp.interpret (param)))
	return false;

      /* finalize parsed string esp. copy CFF1 width or CFF2 vsindex to the parsed charstring for encoding */
      SUBSETTER::finalize_parsed_str (interp.env, param, parsed_charstrings[i]);
    }

    if (drop_hints)
    {
      /* mark hint ops and arguments for drop */
      for (unsigned int i = 0; i < glyphs.len; i++)
      {
	unsigned int fd = acc.fdSelect->get_fd (glyphs[i]);
	if (unlikely (fd >= acc.fdCount))
	  return false;
	SubrSubsetParam  param;
	param.init (&parsed_charstrings[i],
		    &parsed_global_subrs,  &parsed_local_subrs[fd],
		    closures.global_closure, closures.local_closures[fd],
		    drop_hints);

	DropHintsParam  drop;
	if (drop_hints_in_str (parsed_charstrings[i], param, drop))
	{
	  parsed_charstrings[i].set_hint_dropped ();
	  if (drop.vsindex_dropped)
	    parsed_charstrings[i].set_vsindex_dropped ();
	}
      }

      /* after dropping hints recreate closures of actually used subrs */
      closures.reset ();
      for (unsigned int i = 0; i < glyphs.len; i++)
      {
	unsigned int fd = acc.fdSelect->get_fd (glyphs[i]);
	if (unlikely (fd >= acc.fdCount))
	  return false;
	SubrSubsetParam  param;
	param.init (&parsed_charstrings[i],
		    &parsed_global_subrs,  &parsed_local_subrs[fd],
		    closures.global_closure, closures.local_closures[fd],
		    drop_hints);
	collect_subr_refs_in_str (parsed_charstrings[i], param);
      }
    }

    remaps.create (closures);

    return true;
  }

  bool encode_charstrings (ACC &acc, const hb_vector_t<hb_codepoint_t> &glyphs, StrBuffArray &buffArray) const
  {
    if (unlikely (!buffArray.resize (glyphs.len)))
      return false;
    for (unsigned int i = 0; i < glyphs.len; i++)
    {
      unsigned int  fd = acc.fdSelect->get_fd (glyphs[i]);
      if (unlikely (fd >= acc.fdCount))
      	return false;
      if (unlikely (!encode_str (parsed_charstrings[i], fd, buffArray[i])))
	return false;
    }
    return true;
  }

  bool encode_subrs (const ParsedCStrs &subrs, const SubrRemap& remap, unsigned int fd, StrBuffArray &buffArray) const
  {
    unsigned int  count = remap.get_count ();

    if (unlikely (!buffArray.resize (count)))
      return false;
    for (unsigned int old_num = 0; old_num < subrs.len; old_num++)
    {
      hb_codepoint_t new_num = remap[old_num];
      if (new_num != CFF_UNDEF_CODE)
      {
	if (unlikely (!encode_str (subrs[old_num], fd, buffArray[new_num])))
	  return false;
      }
    }
    return true;
  }

  bool encode_globalsubrs (StrBuffArray &buffArray)
  {
    return encode_subrs (parsed_global_subrs, remaps.global_remap, 0, buffArray);
  }

  bool encode_localsubrs (unsigned int fd, StrBuffArray &buffArray) const
  {
    return encode_subrs (parsed_local_subrs[fd], remaps.local_remaps[fd], fd, buffArray);
  }

  protected:
  struct DropHintsParam
  {
    DropHintsParam ()
      : seen_moveto (false),
	ends_in_hint (false),
	vsindex_dropped (false) {}

    bool  seen_moveto;
    bool  ends_in_hint;
    bool  vsindex_dropped;
  };

  bool drop_hints_in_subr (ParsedCStr &str, unsigned int pos,
			   ParsedCStrs &subrs, unsigned int subr_num,
			   const SubrSubsetParam &param, DropHintsParam &drop)
  {
    drop.ends_in_hint = false;
    bool has_hint = drop_hints_in_str (subrs[subr_num], param, drop);

    /* if this subr ends with a stem hint (i.e., not a number a potential argument for moveto),
     * then this entire subroutine must be a hint. drop its call. */
    if (drop.ends_in_hint)
    {
      str.values[pos].set_drop ();
      /* if this subr call is at the end of the parent subr, propagate the flag
       * otherwise reset the flag */
      if (!str.at_end (pos))
	drop.ends_in_hint = false;
    }

    return has_hint;
  }

  /* returns true if it sees a hint op before the first moveto */
  bool drop_hints_in_str (ParsedCStr &str, const SubrSubsetParam &param, DropHintsParam &drop)
  {
    bool  seen_hint = false;

    for (unsigned int pos = 0; pos < str.values.len; pos++)
    {
      bool  has_hint = false;
      switch (str.values[pos].op)
      {
	case OpCode_callsubr:
	  has_hint = drop_hints_in_subr (str, pos,
					*param.parsed_local_subrs, str.values[pos].subr_num,
					param, drop);

	  break;

	case OpCode_callgsubr:
	  has_hint = drop_hints_in_subr (str, pos,
					*param.parsed_global_subrs, str.values[pos].subr_num,
					param, drop);
	  break;

	case OpCode_rmoveto:
	case OpCode_hmoveto:
	case OpCode_vmoveto:
	  drop.seen_moveto = true;
	  break;

	case OpCode_hintmask:
	case OpCode_cntrmask:
	  if (drop.seen_moveto)
	  {
	    str.values[pos].set_drop ();
	    break;
	  }
	  HB_FALLTHROUGH;

	case OpCode_hstemhm:
	case OpCode_vstemhm:
	case OpCode_hstem:
	case OpCode_vstem:
	  has_hint = true;
	  str.values[pos].set_drop ();
	  if (str.at_end (pos))
	    drop.ends_in_hint = true;
	  break;

	case OpCode_dotsection:
	  str.values[pos].set_drop ();
	  break;

	default:
	  /* NONE */
	  break;
      }
      if (has_hint)
      {
	for (int i = pos - 1; i >= 0; i--)
	{
	  ParsedCSOp  &csop = str.values[(unsigned)i];
	  if (csop.for_drop ())
	    break;
	  csop.set_drop ();
	  if (csop.op == OpCode_vsindexcs)
	    drop.vsindex_dropped = true;
	}
	seen_hint |= has_hint;
      }
    }

    return seen_hint;
  }

  void collect_subr_refs_in_subr (ParsedCStr &str, unsigned int pos,
				  unsigned int subr_num, ParsedCStrs &subrs,
				  hb_set_t *closure,
				  const SubrSubsetParam &param)
  {
    hb_set_add (closure, subr_num);
    collect_subr_refs_in_str (subrs[subr_num], param);
  }

  void collect_subr_refs_in_str (ParsedCStr &str, const SubrSubsetParam &param)
  {
    for (unsigned int pos = 0; pos < str.values.len; pos++)
    {
      if (!str.values[pos].for_drop ())
      {
	switch (str.values[pos].op)
	{
	  case OpCode_callsubr:
	    collect_subr_refs_in_subr (str, pos,
				       str.values[pos].subr_num, *param.parsed_local_subrs,
				       param.local_closure, param);
	    break;

	  case OpCode_callgsubr:
	    collect_subr_refs_in_subr (str, pos,
				       str.values[pos].subr_num, *param.parsed_global_subrs,
				       param.global_closure, param);
	    break;

	  default: break;
	}
      }
    }
  }

  bool encode_str (const ParsedCStr &str, const unsigned int fd, StrBuff &buff) const
  {
    buff.init ();
    StrEncoder  encoder (buff);
    encoder.reset ();
    /* if a prefix (CFF1 width or CFF2 vsindex) has been removed along with hints,
     * re-insert it at the beginning of charstreing */
    if (str.has_prefix () && str.is_hint_dropped ())
    {
      encoder.encode_num (str.prefix_num ());
      if (str.prefix_op () != OpCode_Invalid)
	encoder.encode_op (str.prefix_op ());
    }
    for (unsigned int i = 0; i < str.get_count(); i++)
    {
      const ParsedCSOp  &opstr = str.values[i];
      if (!opstr.for_drop () && !opstr.for_skip ())
      {
	switch (opstr.op)
	{
	  case OpCode_callsubr:
	    encoder.encode_int (remaps.local_remaps[fd].biased_num (opstr.subr_num));
	    encoder.encode_op (OpCode_callsubr);
	    break;

	  case OpCode_callgsubr:
	    encoder.encode_int (remaps.global_remap.biased_num (opstr.subr_num));
	    encoder.encode_op (OpCode_callgsubr);
	    break;

	  default:
	    encoder.copy_str (opstr.str);
	    break;
	}
      }
    }
    return !encoder.is_error ();
  }

  protected:
  SubrClosures	      closures;

  ParsedCStrs	       parsed_charstrings;
  ParsedCStrs	       parsed_global_subrs;
  hb_vector_t<ParsedCStrs>  parsed_local_subrs;

  SubrRemaps		remaps;

  private:
  typedef typename SUBRS::count_type subr_count_type;
};
};  /* namespace CFF */

HB_INTERNAL bool
hb_plan_subset_cff_fdselect (const hb_vector_t<hb_codepoint_t> &glyphs,
			    unsigned int fdCount,
			    const CFF::FDSelect &src, /* IN */
			    unsigned int &subset_fd_count /* OUT */,
			    unsigned int &subset_fdselect_size /* OUT */,
			    unsigned int &subset_fdselect_format /* OUT */,
			    hb_vector_t<CFF::code_pair> &fdselect_ranges /* OUT */,
			    CFF::Remap &fdmap /* OUT */);

HB_INTERNAL bool
hb_serialize_cff_fdselect (hb_serialize_context_t *c,
			  unsigned int num_glyphs,
			  const CFF::FDSelect &src,
			  unsigned int fd_count,
			  unsigned int fdselect_format,
			  unsigned int size,
			  const hb_vector_t<CFF::code_pair> &fdselect_ranges);

#endif /* HB_SUBSET_CFF_COMMON_HH */
