/*
 * Copyright (C) 2007,2008,2009,2010  Red Hat, Inc.
 * Copyright (C) 2010  Google, Inc.
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
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_OT_LAYOUT_GSUBGPOS_PRIVATE_HH
#define HB_OT_LAYOUT_GSUBGPOS_PRIVATE_HH

#include "hb-buffer-private.hh"
#include "hb-ot-layout-gdef-private.hh"

HB_BEGIN_DECLS


/* buffer var allocations */
#define lig_id() var2.u16[0] /* unique ligature id */
#define lig_comp() var2.u16[1] /* component number in the ligature (0 = base) */


#ifndef HB_DEBUG_APPLY
#define HB_DEBUG_APPLY (HB_DEBUG+0)
#endif

#define TRACE_APPLY() \
	hb_trace_t<HB_DEBUG_APPLY> trace (&c->debug_depth, "APPLY", HB_FUNC, this); \


HB_BEGIN_DECLS

struct hb_apply_context_t
{
  unsigned int debug_depth;
  hb_ot_layout_context_t *layout;
  hb_buffer_t *buffer;
  hb_mask_t lookup_mask;
  unsigned int context_length;
  unsigned int nesting_level_left;
  unsigned int lookup_props;
  unsigned int property; /* propety of first glyph */


  inline void replace_glyph (hb_codepoint_t glyph_index) const
  {
    clear_property ();
    buffer->replace_glyph (glyph_index);
  }
  inline void replace_glyphs_be16 (unsigned int num_in,
				   unsigned int num_out,
				   const uint16_t *glyph_data_be) const
  {
    clear_property ();
    buffer->replace_glyphs_be16 (num_in, num_out, glyph_data_be);
  }

  inline void guess_glyph_class (unsigned int klass)
  {
    /* XXX if ! has gdef */
    buffer->info[buffer->i].props_cache() = klass;
  }

  private:
  inline void clear_property (void) const
  {
    /* XXX if has gdef */
    buffer->info[buffer->i].props_cache() = 0;
  }
};



typedef bool (*match_func_t) (hb_codepoint_t glyph_id, const USHORT &value, const void *data);
typedef bool (*apply_lookup_func_t) (hb_apply_context_t *c, unsigned int lookup_index);

struct ContextFuncs
{
  match_func_t match;
  apply_lookup_func_t apply;
};


static inline bool match_glyph (hb_codepoint_t glyph_id, const USHORT &value, const void *data HB_UNUSED)
{
  return glyph_id == value;
}

static inline bool match_class (hb_codepoint_t glyph_id, const USHORT &value, const void *data)
{
  const ClassDef &class_def = *reinterpret_cast<const ClassDef *>(data);
  return class_def.get_class (glyph_id) == value;
}

static inline bool match_coverage (hb_codepoint_t glyph_id, const USHORT &value, const void *data)
{
  const OffsetTo<Coverage> &coverage = (const OffsetTo<Coverage>&)value;
  return (data+coverage) (glyph_id) != NOT_COVERED;
}


static inline bool match_input (hb_apply_context_t *c,
				unsigned int count, /* Including the first glyph (not matched) */
				const USHORT input[], /* Array of input values--start with second glyph */
				match_func_t match_func,
				const void *match_data,
				unsigned int *context_length_out)
{
  unsigned int i;
  unsigned int j = c->buffer->i;
  unsigned int end = MIN (c->buffer->len, j + c->context_length);
  if (unlikely (j + count > end))
    return false;

  for (i = 1; i < count; i++)
  {
    do
    {
      j++;
      if (unlikely (j >= end))
	return false;
    } while (_hb_ot_layout_skip_mark (c->layout->face, &c->buffer->info[j], c->lookup_props, NULL));

    if (likely (!match_func (c->buffer->info[j].codepoint, input[i - 1], match_data)))
      return false;
  }

  *context_length_out = j - c->buffer->i + 1;

  return true;
}

static inline bool match_backtrack (hb_apply_context_t *c,
				    unsigned int count,
				    const USHORT backtrack[],
				    match_func_t match_func,
				    const void *match_data)
{
  unsigned int j = c->buffer->backtrack_len ();

  for (unsigned int i = 0; i < count; i++)
  {
    do
    {
      if (unlikely (!j))
	return false;
      j--;
    } while (_hb_ot_layout_skip_mark (c->layout->face, &c->buffer->out_info[j], c->lookup_props, NULL));

    if (likely (!match_func (c->buffer->out_info[j].codepoint, backtrack[i], match_data)))
      return false;
  }

  return true;
}

static inline bool match_lookahead (hb_apply_context_t *c,
				    unsigned int count,
				    const USHORT lookahead[],
				    match_func_t match_func,
				    const void *match_data,
				    unsigned int offset)
{
  unsigned int i;
  unsigned int j = c->buffer->i + offset - 1;
  unsigned int end = MIN (c->buffer->len, c->buffer->i + c->context_length);

  for (i = 0; i < count; i++)
  {
    do
    {
      j++;
      if (unlikely (j >= end))
	return false;
    } while (_hb_ot_layout_skip_mark (c->layout->face, &c->buffer->info[j], c->lookup_props, NULL));

    if (likely (!match_func (c->buffer->info[j].codepoint, lookahead[i], match_data)))
      return false;
  }

  return true;
}

HB_END_DECLS


struct LookupRecord
{
  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return c->check_struct (this);
  }

  USHORT	sequenceIndex;		/* Index into current glyph
					 * sequence--first glyph = 0 */
  USHORT	lookupListIndex;	/* Lookup to apply to that
					 * position--zero--based */
  public:
  DEFINE_SIZE_STATIC (4);
};


HB_BEGIN_DECLS

static inline bool apply_lookup (hb_apply_context_t *c,
				 unsigned int count, /* Including the first glyph */
				 unsigned int lookupCount,
				 const LookupRecord lookupRecord[], /* Array of LookupRecords--in design order */
				 apply_lookup_func_t apply_func)
{
  unsigned int end = MIN (c->buffer->len, c->buffer->i + c->context_length);
  if (unlikely (count == 0 || c->buffer->i + count > end))
    return false;

  /* TODO We don't support lookupRecord arrays that are not increasing:
   *      Should be easy for in_place ones at least. */

  /* Note: If sublookup is reverse, i will underflow after the first loop
   * and we jump out of it.  Not entirely disastrous.  So we don't check
   * for reverse lookup here.
   */
  for (unsigned int i = 0; i < count; /* NOP */)
  {
    while (_hb_ot_layout_skip_mark (c->layout->face, &c->buffer->info[c->buffer->i], c->lookup_props, NULL))
    {
      if (unlikely (c->buffer->i == end))
	return true;
      /* No lookup applied for this index */
      c->buffer->next_glyph ();
    }

    if (lookupCount && i == lookupRecord->sequenceIndex)
    {
      unsigned int old_pos = c->buffer->i;

      /* Apply a lookup */
      bool done = apply_func (c, lookupRecord->lookupListIndex);

      lookupRecord++;
      lookupCount--;
      /* Err, this is wrong if the lookup jumped over some glyphs */
      i += c->buffer->i - old_pos;
      if (unlikely (c->buffer->i == end))
	return true;

      if (!done)
	goto not_applied;
    }
    else
    {
    not_applied:
      /* No lookup applied for this index */
      c->buffer->next_glyph ();
      i++;
    }
  }

  return true;
}

HB_END_DECLS


/* Contextual lookups */

struct ContextLookupContext
{
  ContextFuncs funcs;
  const void *match_data;
};

static inline bool context_lookup (hb_apply_context_t *c,
				   unsigned int inputCount, /* Including the first glyph (not matched) */
				   const USHORT input[], /* Array of input values--start with second glyph */
				   unsigned int lookupCount,
				   const LookupRecord lookupRecord[],
				   ContextLookupContext &lookup_context)
{
  hb_apply_context_t new_context = *c;
  return match_input (c,
		      inputCount, input,
		      lookup_context.funcs.match, lookup_context.match_data,
		      &new_context.context_length)
      && apply_lookup (&new_context,
		       inputCount,
		       lookupCount, lookupRecord,
		       lookup_context.funcs.apply);
}

struct Rule
{
  friend struct RuleSet;

  private:
  inline bool apply (hb_apply_context_t *c, ContextLookupContext &lookup_context) const
  {
    TRACE_APPLY ();
    const LookupRecord *lookupRecord = &StructAtOffset<LookupRecord> (input, input[0].static_size * (inputCount ? inputCount - 1 : 0));
    return context_lookup (c,
			   inputCount, input,
			   lookupCount, lookupRecord,
			   lookup_context);
  }

  public:
  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return inputCount.sanitize (c)
	&& lookupCount.sanitize (c)
	&& c->check_range (input,
				 input[0].static_size * inputCount
				 + lookupRecordX[0].static_size * lookupCount);
  }

  private:
  USHORT	inputCount;		/* Total number of glyphs in input
					 * glyph sequence--includes the  first
					 * glyph */
  USHORT	lookupCount;		/* Number of LookupRecords */
  USHORT	input[VAR];		/* Array of match inputs--start with
					 * second glyph */
  LookupRecord	lookupRecordX[VAR];	/* Array of LookupRecords--in
					 * design order */
  public:
  DEFINE_SIZE_ARRAY2 (4, input, lookupRecordX);
};

struct RuleSet
{
  inline bool apply (hb_apply_context_t *c, ContextLookupContext &lookup_context) const
  {
    TRACE_APPLY ();
    unsigned int num_rules = rule.len;
    for (unsigned int i = 0; i < num_rules; i++)
    {
      if ((this+rule[i]).apply (c, lookup_context))
        return true;
    }

    return false;
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return rule.sanitize (c, this);
  }

  private:
  OffsetArrayOf<Rule>
		rule;			/* Array of Rule tables
					 * ordered by preference */
  public:
  DEFINE_SIZE_ARRAY (2, rule);
};


struct ContextFormat1
{
  friend struct Context;

  private:
  inline bool apply (hb_apply_context_t *c, apply_lookup_func_t apply_func) const
  {
    TRACE_APPLY ();
    unsigned int index = (this+coverage) (c->buffer->info[c->buffer->i].codepoint);
    if (likely (index == NOT_COVERED))
      return false;

    const RuleSet &rule_set = this+ruleSet[index];
    struct ContextLookupContext lookup_context = {
      {match_glyph, apply_func},
      NULL
    };
    return rule_set.apply (c, lookup_context);
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return coverage.sanitize (c, this)
	&& ruleSet.sanitize (c, this);
  }

  private:
  USHORT	format;			/* Format identifier--format = 1 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  OffsetArrayOf<RuleSet>
		ruleSet;		/* Array of RuleSet tables
					 * ordered by Coverage Index */
  public:
  DEFINE_SIZE_ARRAY (6, ruleSet);
};


struct ContextFormat2
{
  friend struct Context;

  private:
  inline bool apply (hb_apply_context_t *c, apply_lookup_func_t apply_func) const
  {
    TRACE_APPLY ();
    unsigned int index = (this+coverage) (c->buffer->info[c->buffer->i].codepoint);
    if (likely (index == NOT_COVERED))
      return false;

    const ClassDef &class_def = this+classDef;
    index = class_def (c->buffer->info[c->buffer->i].codepoint);
    const RuleSet &rule_set = this+ruleSet[index];
    struct ContextLookupContext lookup_context = {
      {match_class, apply_func},
      &class_def
    };
    return rule_set.apply (c, lookup_context);
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return coverage.sanitize (c, this)
        && classDef.sanitize (c, this)
	&& ruleSet.sanitize (c, this);
  }

  private:
  USHORT	format;			/* Format identifier--format = 2 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  OffsetTo<ClassDef>
		classDef;		/* Offset to glyph ClassDef table--from
					 * beginning of table */
  OffsetArrayOf<RuleSet>
		ruleSet;		/* Array of RuleSet tables
					 * ordered by class */
  public:
  DEFINE_SIZE_ARRAY (8, ruleSet);
};


struct ContextFormat3
{
  friend struct Context;

  private:
  inline bool apply (hb_apply_context_t *c, apply_lookup_func_t apply_func) const
  {
    TRACE_APPLY ();
    unsigned int index = (this+coverage[0]) (c->buffer->info[c->buffer->i].codepoint);
    if (likely (index == NOT_COVERED))
      return false;

    const LookupRecord *lookupRecord = &StructAtOffset<LookupRecord> (coverage, coverage[0].static_size * glyphCount);
    struct ContextLookupContext lookup_context = {
      {match_coverage, apply_func},
      this
    };
    return context_lookup (c,
			   glyphCount, (const USHORT *) (coverage + 1),
			   lookupCount, lookupRecord,
			   lookup_context);
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    if (!c->check_struct (this)) return false;
    unsigned int count = glyphCount;
    if (!c->check_array (coverage, coverage[0].static_size, count)) return false;
    for (unsigned int i = 0; i < count; i++)
      if (!coverage[i].sanitize (c, this)) return false;
    LookupRecord *lookupRecord = &StructAtOffset<LookupRecord> (coverage, coverage[0].static_size * count);
    return c->check_array (lookupRecord, lookupRecord[0].static_size, lookupCount);
  }

  private:
  USHORT	format;			/* Format identifier--format = 3 */
  USHORT	glyphCount;		/* Number of glyphs in the input glyph
					 * sequence */
  USHORT	lookupCount;		/* Number of LookupRecords */
  OffsetTo<Coverage>
		coverage[VAR];		/* Array of offsets to Coverage
					 * table in glyph sequence order */
  LookupRecord	lookupRecordX[VAR];	/* Array of LookupRecords--in
					 * design order */
  public:
  DEFINE_SIZE_ARRAY2 (6, coverage, lookupRecordX);
};

struct Context
{
  protected:
  inline bool apply (hb_apply_context_t *c, apply_lookup_func_t apply_func) const
  {
    TRACE_APPLY ();
    switch (u.format) {
    case 1: return u.format1.apply (c, apply_func);
    case 2: return u.format2.apply (c, apply_func);
    case 3: return u.format3.apply (c, apply_func);
    default:return false;
    }
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    if (!u.format.sanitize (c)) return false;
    switch (u.format) {
    case 1: return u.format1.sanitize (c);
    case 2: return u.format2.sanitize (c);
    case 3: return u.format3.sanitize (c);
    default:return true;
    }
  }

  private:
  union {
  USHORT		format;		/* Format identifier */
  ContextFormat1	format1;
  ContextFormat2	format2;
  ContextFormat3	format3;
  } u;
};


/* Chaining Contextual lookups */

struct ChainContextLookupContext
{
  ContextFuncs funcs;
  const void *match_data[3];
};

static inline bool chain_context_lookup (hb_apply_context_t *c,
					 unsigned int backtrackCount,
					 const USHORT backtrack[],
					 unsigned int inputCount, /* Including the first glyph (not matched) */
					 const USHORT input[], /* Array of input values--start with second glyph */
					 unsigned int lookaheadCount,
					 const USHORT lookahead[],
					 unsigned int lookupCount,
					 const LookupRecord lookupRecord[],
					 ChainContextLookupContext &lookup_context)
{
  /* First guess */
  if (unlikely (c->buffer->backtrack_len () < backtrackCount ||
		c->buffer->i + inputCount + lookaheadCount > c->buffer->len ||
		inputCount + lookaheadCount > c->context_length))
    return false;

  hb_apply_context_t new_context = *c;
  return match_backtrack (c,
			  backtrackCount, backtrack,
			  lookup_context.funcs.match, lookup_context.match_data[0])
      && match_input (c,
		      inputCount, input,
		      lookup_context.funcs.match, lookup_context.match_data[1],
		      &new_context.context_length)
      && match_lookahead (c,
			  lookaheadCount, lookahead,
			  lookup_context.funcs.match, lookup_context.match_data[2],
			  new_context.context_length)
      && apply_lookup (&new_context,
		       inputCount,
		       lookupCount, lookupRecord,
		       lookup_context.funcs.apply);
}

struct ChainRule
{
  friend struct ChainRuleSet;

  private:
  inline bool apply (hb_apply_context_t *c, ChainContextLookupContext &lookup_context) const
  {
    TRACE_APPLY ();
    const HeadlessArrayOf<USHORT> &input = StructAfter<HeadlessArrayOf<USHORT> > (backtrack);
    const ArrayOf<USHORT> &lookahead = StructAfter<ArrayOf<USHORT> > (input);
    const ArrayOf<LookupRecord> &lookup = StructAfter<ArrayOf<LookupRecord> > (lookahead);
    return chain_context_lookup (c,
				 backtrack.len, backtrack.array,
				 input.len, input.array,
				 lookahead.len, lookahead.array,
				 lookup.len, lookup.array,
				 lookup_context);
  }

  public:
  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    if (!backtrack.sanitize (c)) return false;
    HeadlessArrayOf<USHORT> &input = StructAfter<HeadlessArrayOf<USHORT> > (backtrack);
    if (!input.sanitize (c)) return false;
    ArrayOf<USHORT> &lookahead = StructAfter<ArrayOf<USHORT> > (input);
    if (!lookahead.sanitize (c)) return false;
    ArrayOf<LookupRecord> &lookup = StructAfter<ArrayOf<LookupRecord> > (lookahead);
    return lookup.sanitize (c);
  }

  private:
  ArrayOf<USHORT>
		backtrack;		/* Array of backtracking values
					 * (to be matched before the input
					 * sequence) */
  HeadlessArrayOf<USHORT>
		inputX;			/* Array of input values (start with
					 * second glyph) */
  ArrayOf<USHORT>
		lookaheadX;		/* Array of lookahead values's (to be
					 * matched after the input sequence) */
  ArrayOf<LookupRecord>
		lookupX;		/* Array of LookupRecords--in
					 * design order) */
  public:
  DEFINE_SIZE_MIN (8);
};

struct ChainRuleSet
{
  inline bool apply (hb_apply_context_t *c, ChainContextLookupContext &lookup_context) const
  {
    TRACE_APPLY ();
    unsigned int num_rules = rule.len;
    for (unsigned int i = 0; i < num_rules; i++)
    {
      if ((this+rule[i]).apply (c, lookup_context))
        return true;
    }

    return false;
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return rule.sanitize (c, this);
  }

  private:
  OffsetArrayOf<ChainRule>
		rule;			/* Array of ChainRule tables
					 * ordered by preference */
  public:
  DEFINE_SIZE_ARRAY (2, rule);
};

struct ChainContextFormat1
{
  friend struct ChainContext;

  private:
  inline bool apply (hb_apply_context_t *c, apply_lookup_func_t apply_func) const
  {
    TRACE_APPLY ();
    unsigned int index = (this+coverage) (c->buffer->info[c->buffer->i].codepoint);
    if (likely (index == NOT_COVERED))
      return false;

    const ChainRuleSet &rule_set = this+ruleSet[index];
    struct ChainContextLookupContext lookup_context = {
      {match_glyph, apply_func},
      {NULL, NULL, NULL}
    };
    return rule_set.apply (c, lookup_context);
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return coverage.sanitize (c, this)
	&& ruleSet.sanitize (c, this);
  }

  private:
  USHORT	format;			/* Format identifier--format = 1 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  OffsetArrayOf<ChainRuleSet>
		ruleSet;		/* Array of ChainRuleSet tables
					 * ordered by Coverage Index */
  public:
  DEFINE_SIZE_ARRAY (6, ruleSet);
};

struct ChainContextFormat2
{
  friend struct ChainContext;

  private:
  inline bool apply (hb_apply_context_t *c, apply_lookup_func_t apply_func) const
  {
    TRACE_APPLY ();
    unsigned int index = (this+coverage) (c->buffer->info[c->buffer->i].codepoint);
    if (likely (index == NOT_COVERED))
      return false;

    const ClassDef &backtrack_class_def = this+backtrackClassDef;
    const ClassDef &input_class_def = this+inputClassDef;
    const ClassDef &lookahead_class_def = this+lookaheadClassDef;

    index = input_class_def (c->buffer->info[c->buffer->i].codepoint);
    const ChainRuleSet &rule_set = this+ruleSet[index];
    struct ChainContextLookupContext lookup_context = {
      {match_class, apply_func},
      {&backtrack_class_def,
       &input_class_def,
       &lookahead_class_def}
    };
    return rule_set.apply (c, lookup_context);
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return coverage.sanitize (c, this)
	&& backtrackClassDef.sanitize (c, this)
	&& inputClassDef.sanitize (c, this)
	&& lookaheadClassDef.sanitize (c, this)
	&& ruleSet.sanitize (c, this);
  }

  private:
  USHORT	format;			/* Format identifier--format = 2 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  OffsetTo<ClassDef>
		backtrackClassDef;	/* Offset to glyph ClassDef table
					 * containing backtrack sequence
					 * data--from beginning of table */
  OffsetTo<ClassDef>
		inputClassDef;		/* Offset to glyph ClassDef
					 * table containing input sequence
					 * data--from beginning of table */
  OffsetTo<ClassDef>
		lookaheadClassDef;	/* Offset to glyph ClassDef table
					 * containing lookahead sequence
					 * data--from beginning of table */
  OffsetArrayOf<ChainRuleSet>
		ruleSet;		/* Array of ChainRuleSet tables
					 * ordered by class */
  public:
  DEFINE_SIZE_ARRAY (12, ruleSet);
};

struct ChainContextFormat3
{
  friend struct ChainContext;

  private:

  inline bool apply (hb_apply_context_t *c, apply_lookup_func_t apply_func) const
  {
    TRACE_APPLY ();
    const OffsetArrayOf<Coverage> &input = StructAfter<OffsetArrayOf<Coverage> > (backtrack);

    unsigned int index = (this+input[0]) (c->buffer->info[c->buffer->i].codepoint);
    if (likely (index == NOT_COVERED))
      return false;

    const OffsetArrayOf<Coverage> &lookahead = StructAfter<OffsetArrayOf<Coverage> > (input);
    const ArrayOf<LookupRecord> &lookup = StructAfter<ArrayOf<LookupRecord> > (lookahead);
    struct ChainContextLookupContext lookup_context = {
      {match_coverage, apply_func},
      {this, this, this}
    };
    return chain_context_lookup (c,
				 backtrack.len, (const USHORT *) backtrack.array,
				 input.len, (const USHORT *) input.array + 1,
				 lookahead.len, (const USHORT *) lookahead.array,
				 lookup.len, lookup.array,
				 lookup_context);
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    if (!backtrack.sanitize (c, this)) return false;
    OffsetArrayOf<Coverage> &input = StructAfter<OffsetArrayOf<Coverage> > (backtrack);
    if (!input.sanitize (c, this)) return false;
    OffsetArrayOf<Coverage> &lookahead = StructAfter<OffsetArrayOf<Coverage> > (input);
    if (!lookahead.sanitize (c, this)) return false;
    ArrayOf<LookupRecord> &lookup = StructAfter<ArrayOf<LookupRecord> > (lookahead);
    return lookup.sanitize (c);
  }

  private:
  USHORT	format;			/* Format identifier--format = 3 */
  OffsetArrayOf<Coverage>
		backtrack;		/* Array of coverage tables
					 * in backtracking sequence, in  glyph
					 * sequence order */
  OffsetArrayOf<Coverage>
		inputX		;	/* Array of coverage
					 * tables in input sequence, in glyph
					 * sequence order */
  OffsetArrayOf<Coverage>
		lookaheadX;		/* Array of coverage tables
					 * in lookahead sequence, in glyph
					 * sequence order */
  ArrayOf<LookupRecord>
		lookupX;		/* Array of LookupRecords--in
					 * design order) */
  public:
  DEFINE_SIZE_MIN (10);
};

struct ChainContext
{
  protected:
  inline bool apply (hb_apply_context_t *c, apply_lookup_func_t apply_func) const
  {
    TRACE_APPLY ();
    switch (u.format) {
    case 1: return u.format1.apply (c, apply_func);
    case 2: return u.format2.apply (c, apply_func);
    case 3: return u.format3.apply (c, apply_func);
    default:return false;
    }
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    if (!u.format.sanitize (c)) return false;
    switch (u.format) {
    case 1: return u.format1.sanitize (c);
    case 2: return u.format2.sanitize (c);
    case 3: return u.format3.sanitize (c);
    default:return true;
    }
  }

  private:
  union {
  USHORT		format;	/* Format identifier */
  ChainContextFormat1	format1;
  ChainContextFormat2	format2;
  ChainContextFormat3	format3;
  } u;
};


struct ExtensionFormat1
{
  friend struct Extension;

  protected:
  inline unsigned int get_type (void) const { return extensionLookupType; }
  inline unsigned int get_offset (void) const { return extensionOffset; }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return c->check_struct (this);
  }

  private:
  USHORT	format;			/* Format identifier. Set to 1. */
  USHORT	extensionLookupType;	/* Lookup type of subtable referenced
					 * by ExtensionOffset (i.e. the
					 * extension subtable). */
  ULONG		extensionOffset;	/* Offset to the extension subtable,
					 * of lookup type subtable. */
  public:
  DEFINE_SIZE_STATIC (8);
};

struct Extension
{
  inline unsigned int get_type (void) const
  {
    switch (u.format) {
    case 1: return u.format1.get_type ();
    default:return 0;
    }
  }
  inline unsigned int get_offset (void) const
  {
    switch (u.format) {
    case 1: return u.format1.get_offset ();
    default:return 0;
    }
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    if (!u.format.sanitize (c)) return false;
    switch (u.format) {
    case 1: return u.format1.sanitize (c);
    default:return true;
    }
  }

  private:
  union {
  USHORT		format;		/* Format identifier */
  ExtensionFormat1	format1;
  } u;
};


/*
 * GSUB/GPOS Common
 */

struct GSUBGPOS
{
  static const hb_tag_t GSUBTag	= HB_OT_TAG_GSUB;
  static const hb_tag_t GPOSTag	= HB_OT_TAG_GPOS;

  inline unsigned int get_script_count (void) const
  { return (this+scriptList).len; }
  inline const Tag& get_script_tag (unsigned int i) const
  { return (this+scriptList).get_tag (i); }
  inline unsigned int get_script_tags (unsigned int start_offset,
				       unsigned int *script_count /* IN/OUT */,
				       hb_tag_t     *script_tags /* OUT */) const
  { return (this+scriptList).get_tags (start_offset, script_count, script_tags); }
  inline const Script& get_script (unsigned int i) const
  { return (this+scriptList)[i]; }
  inline bool find_script_index (hb_tag_t tag, unsigned int *index) const
  { return (this+scriptList).find_index (tag, index); }

  inline unsigned int get_feature_count (void) const
  { return (this+featureList).len; }
  inline const Tag& get_feature_tag (unsigned int i) const
  { return (this+featureList).get_tag (i); }
  inline unsigned int get_feature_tags (unsigned int start_offset,
					unsigned int *feature_count /* IN/OUT */,
					hb_tag_t     *feature_tags /* OUT */) const
  { return (this+featureList).get_tags (start_offset, feature_count, feature_tags); }
  inline const Feature& get_feature (unsigned int i) const
  { return (this+featureList)[i]; }
  inline bool find_feature_index (hb_tag_t tag, unsigned int *index) const
  { return (this+featureList).find_index (tag, index); }

  inline unsigned int get_lookup_count (void) const
  { return (this+lookupList).len; }
  inline const Lookup& get_lookup (unsigned int i) const
  { return (this+lookupList)[i]; }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return version.sanitize (c) && likely (version.major == 1)
	&& scriptList.sanitize (c, this)
	&& featureList.sanitize (c, this)
	&& lookupList.sanitize (c, this);
  }

  protected:
  FixedVersion	version;	/* Version of the GSUB/GPOS table--initially set
				 * to 0x00010000 */
  OffsetTo<ScriptList>
		scriptList;  	/* ScriptList table */
  OffsetTo<FeatureList>
		featureList; 	/* FeatureList table */
  OffsetTo<LookupList>
		lookupList; 	/* LookupList table */
  public:
  DEFINE_SIZE_STATIC (10);
};


HB_END_DECLS

#endif /* HB_OT_LAYOUT_GSUBGPOS_PRIVATE_HH */
