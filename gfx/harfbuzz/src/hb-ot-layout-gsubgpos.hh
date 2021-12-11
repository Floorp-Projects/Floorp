/*
 * Copyright © 2007,2008,2009,2010  Red Hat, Inc.
 * Copyright © 2010,2012  Google, Inc.
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

#ifndef HB_OT_LAYOUT_GSUBGPOS_HH
#define HB_OT_LAYOUT_GSUBGPOS_HH

#include "hb.hh"
#include "hb-buffer.hh"
#include "hb-map.hh"
#include "hb-set.hh"
#include "hb-ot-map.hh"
#include "hb-ot-layout-common.hh"
#include "hb-ot-layout-gdef-table.hh"


namespace OT {


struct hb_intersects_context_t :
       hb_dispatch_context_t<hb_intersects_context_t, bool>
{
  template <typename T>
  return_t dispatch (const T &obj) { return obj.intersects (this->glyphs); }
  static return_t default_return_value () { return false; }
  bool stop_sublookup_iteration (return_t r) const { return r; }

  const hb_set_t *glyphs;

  hb_intersects_context_t (const hb_set_t *glyphs_) :
                            glyphs (glyphs_) {}
};

struct hb_have_non_1to1_context_t :
       hb_dispatch_context_t<hb_have_non_1to1_context_t, bool>
{
  template <typename T>
  return_t dispatch (const T &obj) { return obj.may_have_non_1to1 (); }
  static return_t default_return_value () { return false; }
  bool stop_sublookup_iteration (return_t r) const { return r; }
};

struct hb_closure_context_t :
       hb_dispatch_context_t<hb_closure_context_t>
{
  typedef return_t (*recurse_func_t) (hb_closure_context_t *c, unsigned lookup_index, hb_set_t *covered_seq_indicies, unsigned seq_index, unsigned end_index);
  template <typename T>
  return_t dispatch (const T &obj) { obj.closure (this); return hb_empty_t (); }
  static return_t default_return_value () { return hb_empty_t (); }
  void recurse (unsigned lookup_index, hb_set_t *covered_seq_indicies, unsigned seq_index, unsigned end_index)
  {
    if (unlikely (nesting_level_left == 0 || !recurse_func))
      return;

    nesting_level_left--;
    recurse_func (this, lookup_index, covered_seq_indicies, seq_index, end_index);
    nesting_level_left++;
  }

  bool lookup_limit_exceeded ()
  { return lookup_count > HB_MAX_LOOKUP_INDICES; }

  bool should_visit_lookup (unsigned int lookup_index)
  {
    if (lookup_count++ > HB_MAX_LOOKUP_INDICES)
      return false;

    if (is_lookup_done (lookup_index))
      return false;

    return true;
  }

  bool is_lookup_done (unsigned int lookup_index)
  {
    if (done_lookups_glyph_count->in_error () ||
        done_lookups_glyph_set->in_error ())
      return true;

    /* Have we visited this lookup with the current set of glyphs? */
    if (done_lookups_glyph_count->get (lookup_index) != glyphs->get_population ())
    {
      done_lookups_glyph_count->set (lookup_index, glyphs->get_population ());

      if (!done_lookups_glyph_set->get (lookup_index))
      {
	hb_set_t* empty_set = hb_set_create ();
	if (unlikely (!done_lookups_glyph_set->set (lookup_index, empty_set)))
	{
	  hb_set_destroy (empty_set);
	  return true;
	}
      }

      hb_set_clear (done_lookups_glyph_set->get (lookup_index));
    }

    hb_set_t *covered_glyph_set = done_lookups_glyph_set->get (lookup_index);
    if (unlikely (covered_glyph_set->in_error ()))
      return true;
    if (parent_active_glyphs ()->is_subset (*covered_glyph_set))
      return true;

    hb_set_union (covered_glyph_set, parent_active_glyphs ());
    return false;
  }

  hb_set_t* parent_active_glyphs ()
  {
    if (active_glyphs_stack.length < 1)
      return glyphs;

    return active_glyphs_stack.tail ();
  }

  void push_cur_active_glyphs (hb_set_t* cur_active_glyph_set)
  {
    active_glyphs_stack.push (cur_active_glyph_set);
  }

  bool pop_cur_done_glyphs ()
  {
    if (active_glyphs_stack.length < 1)
      return false;

    active_glyphs_stack.pop ();
    return true;
  }

  hb_face_t *face;
  hb_set_t *glyphs;
  hb_set_t *cur_intersected_glyphs;
  hb_set_t output[1];
  hb_vector_t<hb_set_t *> active_glyphs_stack;
  recurse_func_t recurse_func;
  unsigned int nesting_level_left;

  hb_closure_context_t (hb_face_t *face_,
			hb_set_t *glyphs_,
			hb_set_t *cur_intersected_glyphs_,
			hb_map_t *done_lookups_glyph_count_,
			hb_hashmap_t<unsigned, hb_set_t *, (unsigned)-1, nullptr> *done_lookups_glyph_set_,
			unsigned int nesting_level_left_ = HB_MAX_NESTING_LEVEL) :
			  face (face_),
			  glyphs (glyphs_),
			  cur_intersected_glyphs (cur_intersected_glyphs_),
			  recurse_func (nullptr),
			  nesting_level_left (nesting_level_left_),
			  done_lookups_glyph_count (done_lookups_glyph_count_),
			  done_lookups_glyph_set (done_lookups_glyph_set_),
			  lookup_count (0)
  {
    push_cur_active_glyphs (glyphs_);
  }

  ~hb_closure_context_t () { flush (); }

  void set_recurse_func (recurse_func_t func) { recurse_func = func; }

  void flush ()
  {
    hb_set_del_range (output, face->get_num_glyphs (), HB_SET_VALUE_INVALID);	/* Remove invalid glyphs. */
    hb_set_union (glyphs, output);
    hb_set_clear (output);
    active_glyphs_stack.pop ();
    active_glyphs_stack.fini ();
  }

  private:
  hb_map_t *done_lookups_glyph_count;
  hb_hashmap_t<unsigned, hb_set_t *, (unsigned)-1, nullptr> *done_lookups_glyph_set;
  unsigned int lookup_count;
};



struct hb_closure_lookups_context_t :
       hb_dispatch_context_t<hb_closure_lookups_context_t>
{
  typedef return_t (*recurse_func_t) (hb_closure_lookups_context_t *c, unsigned lookup_index);
  template <typename T>
  return_t dispatch (const T &obj) { obj.closure_lookups (this); return hb_empty_t (); }
  static return_t default_return_value () { return hb_empty_t (); }
  void recurse (unsigned lookup_index)
  {
    if (unlikely (nesting_level_left == 0 || !recurse_func))
      return;

    /* Return if new lookup was recursed to before. */
    if (is_lookup_visited (lookup_index))
      return;

    nesting_level_left--;
    recurse_func (this, lookup_index);
    nesting_level_left++;
  }

  void set_lookup_visited (unsigned lookup_index)
  { visited_lookups->add (lookup_index); }

  void set_lookup_inactive (unsigned lookup_index)
  { inactive_lookups->add (lookup_index); }

  bool lookup_limit_exceeded ()
  { return lookup_count > HB_MAX_LOOKUP_INDICES; }

  bool is_lookup_visited (unsigned lookup_index)
  {
    if (unlikely (lookup_count++ > HB_MAX_LOOKUP_INDICES))
      return true;

    if (unlikely (visited_lookups->in_error ()))
      return true;

    return visited_lookups->has (lookup_index);
  }

  hb_face_t *face;
  const hb_set_t *glyphs;
  recurse_func_t recurse_func;
  unsigned int nesting_level_left;

  hb_closure_lookups_context_t (hb_face_t *face_,
				const hb_set_t *glyphs_,
				hb_set_t *visited_lookups_,
				hb_set_t *inactive_lookups_,
				unsigned nesting_level_left_ = HB_MAX_NESTING_LEVEL) :
				face (face_),
				glyphs (glyphs_),
				recurse_func (nullptr),
				nesting_level_left (nesting_level_left_),
				visited_lookups (visited_lookups_),
				inactive_lookups (inactive_lookups_),
				lookup_count (0) {}

  void set_recurse_func (recurse_func_t func) { recurse_func = func; }

  private:
  hb_set_t *visited_lookups;
  hb_set_t *inactive_lookups;
  unsigned int lookup_count;
};

struct hb_would_apply_context_t :
       hb_dispatch_context_t<hb_would_apply_context_t, bool>
{
  template <typename T>
  return_t dispatch (const T &obj) { return obj.would_apply (this); }
  static return_t default_return_value () { return false; }
  bool stop_sublookup_iteration (return_t r) const { return r; }

  hb_face_t *face;
  const hb_codepoint_t *glyphs;
  unsigned int len;
  bool zero_context;

  hb_would_apply_context_t (hb_face_t *face_,
			    const hb_codepoint_t *glyphs_,
			    unsigned int len_,
			    bool zero_context_) :
			      face (face_),
			      glyphs (glyphs_),
			      len (len_),
			      zero_context (zero_context_) {}
};

struct hb_collect_glyphs_context_t :
       hb_dispatch_context_t<hb_collect_glyphs_context_t>
{
  typedef return_t (*recurse_func_t) (hb_collect_glyphs_context_t *c, unsigned int lookup_index);
  template <typename T>
  return_t dispatch (const T &obj) { obj.collect_glyphs (this); return hb_empty_t (); }
  static return_t default_return_value () { return hb_empty_t (); }
  void recurse (unsigned int lookup_index)
  {
    if (unlikely (nesting_level_left == 0 || !recurse_func))
      return;

    /* Note that GPOS sets recurse_func to nullptr already, so it doesn't get
     * past the previous check.  For GSUB, we only want to collect the output
     * glyphs in the recursion.  If output is not requested, we can go home now.
     *
     * Note further, that the above is not exactly correct.  A recursed lookup
     * is allowed to match input that is not matched in the context, but that's
     * not how most fonts are built.  It's possible to relax that and recurse
     * with all sets here if it proves to be an issue.
     */

    if (output == hb_set_get_empty ())
      return;

    /* Return if new lookup was recursed to before. */
    if (recursed_lookups->has (lookup_index))
      return;

    hb_set_t *old_before = before;
    hb_set_t *old_input  = input;
    hb_set_t *old_after  = after;
    before = input = after = hb_set_get_empty ();

    nesting_level_left--;
    recurse_func (this, lookup_index);
    nesting_level_left++;

    before = old_before;
    input  = old_input;
    after  = old_after;

    recursed_lookups->add (lookup_index);
  }

  hb_face_t *face;
  hb_set_t *before;
  hb_set_t *input;
  hb_set_t *after;
  hb_set_t *output;
  recurse_func_t recurse_func;
  hb_set_t *recursed_lookups;
  unsigned int nesting_level_left;

  hb_collect_glyphs_context_t (hb_face_t *face_,
			       hb_set_t  *glyphs_before, /* OUT.  May be NULL */
			       hb_set_t  *glyphs_input,  /* OUT.  May be NULL */
			       hb_set_t  *glyphs_after,  /* OUT.  May be NULL */
			       hb_set_t  *glyphs_output, /* OUT.  May be NULL */
			       unsigned int nesting_level_left_ = HB_MAX_NESTING_LEVEL) :
			      face (face_),
			      before (glyphs_before ? glyphs_before : hb_set_get_empty ()),
			      input  (glyphs_input  ? glyphs_input  : hb_set_get_empty ()),
			      after  (glyphs_after  ? glyphs_after  : hb_set_get_empty ()),
			      output (glyphs_output ? glyphs_output : hb_set_get_empty ()),
			      recurse_func (nullptr),
			      recursed_lookups (hb_set_create ()),
			      nesting_level_left (nesting_level_left_) {}
  ~hb_collect_glyphs_context_t () { hb_set_destroy (recursed_lookups); }

  void set_recurse_func (recurse_func_t func) { recurse_func = func; }
};



template <typename set_t>
struct hb_collect_coverage_context_t :
       hb_dispatch_context_t<hb_collect_coverage_context_t<set_t>, const Coverage &>
{
  typedef const Coverage &return_t; // Stoopid that we have to dupe this here.
  template <typename T>
  return_t dispatch (const T &obj) { return obj.get_coverage (); }
  static return_t default_return_value () { return Null (Coverage); }
  bool stop_sublookup_iteration (return_t r) const
  {
    r.collect_coverage (set);
    return false;
  }

  hb_collect_coverage_context_t (set_t *set_) :
				   set (set_) {}

  set_t *set;
};


struct hb_ot_apply_context_t :
       hb_dispatch_context_t<hb_ot_apply_context_t, bool, HB_DEBUG_APPLY>
{
  struct matcher_t
  {
    matcher_t () :
	     lookup_props (0),
	     ignore_zwnj (false),
	     ignore_zwj (false),
	     mask (-1),
#define arg1(arg) (arg) /* Remove the macro to see why it's needed! */
	     syllable arg1(0),
#undef arg1
	     match_func (nullptr),
	     match_data (nullptr) {}

    typedef bool (*match_func_t) (hb_codepoint_t glyph_id, const HBUINT16 &value, const void *data);

    void set_ignore_zwnj (bool ignore_zwnj_) { ignore_zwnj = ignore_zwnj_; }
    void set_ignore_zwj (bool ignore_zwj_) { ignore_zwj = ignore_zwj_; }
    void set_lookup_props (unsigned int lookup_props_) { lookup_props = lookup_props_; }
    void set_mask (hb_mask_t mask_) { mask = mask_; }
    void set_syllable (uint8_t syllable_)  { syllable = syllable_; }
    void set_match_func (match_func_t match_func_,
			 const void *match_data_)
    { match_func = match_func_; match_data = match_data_; }

    enum may_match_t {
      MATCH_NO,
      MATCH_YES,
      MATCH_MAYBE
    };

    may_match_t may_match (const hb_glyph_info_t &info,
			   const HBUINT16        *glyph_data) const
    {
      if (!(info.mask & mask) ||
	  (syllable && syllable != info.syllable ()))
	return MATCH_NO;

      if (match_func)
	return match_func (info.codepoint, *glyph_data, match_data) ? MATCH_YES : MATCH_NO;

      return MATCH_MAYBE;
    }

    enum may_skip_t {
      SKIP_NO,
      SKIP_YES,
      SKIP_MAYBE
    };

    may_skip_t may_skip (const hb_ot_apply_context_t *c,
			 const hb_glyph_info_t       &info) const
    {
      if (!c->check_glyph_property (&info, lookup_props))
	return SKIP_YES;

      if (unlikely (_hb_glyph_info_is_default_ignorable_and_not_hidden (&info) &&
		    (ignore_zwnj || !_hb_glyph_info_is_zwnj (&info)) &&
		    (ignore_zwj || !_hb_glyph_info_is_zwj (&info))))
	return SKIP_MAYBE;

      return SKIP_NO;
    }

    protected:
    unsigned int lookup_props;
    bool ignore_zwnj;
    bool ignore_zwj;
    hb_mask_t mask;
    uint8_t syllable;
    match_func_t match_func;
    const void *match_data;
  };

  struct skipping_iterator_t
  {
    void init (hb_ot_apply_context_t *c_, bool context_match = false)
    {
      c = c_;
      match_glyph_data = nullptr;
      matcher.set_match_func (nullptr, nullptr);
      matcher.set_lookup_props (c->lookup_props);
      /* Ignore ZWNJ if we are matching GPOS, or matching GSUB context and asked to. */
      matcher.set_ignore_zwnj (c->table_index == 1 || (context_match && c->auto_zwnj));
      /* Ignore ZWJ if we are matching context, or asked to. */
      matcher.set_ignore_zwj  (context_match || c->auto_zwj);
      matcher.set_mask (context_match ? -1 : c->lookup_mask);
    }
    void set_lookup_props (unsigned int lookup_props)
    {
      matcher.set_lookup_props (lookup_props);
    }
    void set_match_func (matcher_t::match_func_t match_func_,
			 const void *match_data_,
			 const HBUINT16 glyph_data[])
    {
      matcher.set_match_func (match_func_, match_data_);
      match_glyph_data = glyph_data;
    }

    void reset (unsigned int start_index_,
		unsigned int num_items_)
    {
      idx = start_index_;
      num_items = num_items_;
      end = c->buffer->len;
      matcher.set_syllable (start_index_ == c->buffer->idx ? c->buffer->cur().syllable () : 0);
    }

    void reject ()
    {
      num_items++;
      if (match_glyph_data) match_glyph_data--;
    }

    matcher_t::may_skip_t
    may_skip (const hb_glyph_info_t &info) const
    { return matcher.may_skip (c, info); }

    bool next ()
    {
      assert (num_items > 0);
      while (idx + num_items < end)
      {
	idx++;
	const hb_glyph_info_t &info = c->buffer->info[idx];

	matcher_t::may_skip_t skip = matcher.may_skip (c, info);
	if (unlikely (skip == matcher_t::SKIP_YES))
	  continue;

	matcher_t::may_match_t match = matcher.may_match (info, match_glyph_data);
	if (match == matcher_t::MATCH_YES ||
	    (match == matcher_t::MATCH_MAYBE &&
	     skip == matcher_t::SKIP_NO))
	{
	  num_items--;
	  if (match_glyph_data) match_glyph_data++;
	  return true;
	}

	if (skip == matcher_t::SKIP_NO)
	  return false;
      }
      return false;
    }
    bool prev ()
    {
      assert (num_items > 0);
      while (idx > num_items - 1)
      {
	idx--;
	const hb_glyph_info_t &info = c->buffer->out_info[idx];

	matcher_t::may_skip_t skip = matcher.may_skip (c, info);
	if (unlikely (skip == matcher_t::SKIP_YES))
	  continue;

	matcher_t::may_match_t match = matcher.may_match (info, match_glyph_data);
	if (match == matcher_t::MATCH_YES ||
	    (match == matcher_t::MATCH_MAYBE &&
	     skip == matcher_t::SKIP_NO))
	{
	  num_items--;
	  if (match_glyph_data) match_glyph_data++;
	  return true;
	}

	if (skip == matcher_t::SKIP_NO)
	  return false;
      }
      return false;
    }

    unsigned int idx;
    protected:
    hb_ot_apply_context_t *c;
    matcher_t matcher;
    const HBUINT16 *match_glyph_data;

    unsigned int num_items;
    unsigned int end;
  };


  const char *get_name () { return "APPLY"; }
  typedef return_t (*recurse_func_t) (hb_ot_apply_context_t *c, unsigned int lookup_index);
  template <typename T>
  return_t dispatch (const T &obj) { return obj.apply (this); }
  static return_t default_return_value () { return false; }
  bool stop_sublookup_iteration (return_t r) const { return r; }
  return_t recurse (unsigned int sub_lookup_index)
  {
    if (unlikely (nesting_level_left == 0 || !recurse_func || buffer->max_ops-- <= 0))
      return default_return_value ();

    nesting_level_left--;
    bool ret = recurse_func (this, sub_lookup_index);
    nesting_level_left++;
    return ret;
  }

  skipping_iterator_t iter_input, iter_context;

  hb_font_t *font;
  hb_face_t *face;
  hb_buffer_t *buffer;
  recurse_func_t recurse_func;
  const GDEF &gdef;
  const VariationStore &var_store;

  hb_direction_t direction;
  hb_mask_t lookup_mask;
  unsigned int table_index; /* GSUB/GPOS */
  unsigned int lookup_index;
  unsigned int lookup_props;
  unsigned int nesting_level_left;

  bool has_glyph_classes;
  bool auto_zwnj;
  bool auto_zwj;
  bool random;

  uint32_t random_state;


  hb_ot_apply_context_t (unsigned int table_index_,
			 hb_font_t *font_,
			 hb_buffer_t *buffer_) :
			iter_input (), iter_context (),
			font (font_), face (font->face), buffer (buffer_),
			recurse_func (nullptr),
			gdef (
#ifndef HB_NO_OT_LAYOUT
			      *face->table.GDEF->table
#else
			      Null (GDEF)
#endif
			     ),
			var_store (gdef.get_var_store ()),
			direction (buffer_->props.direction),
			lookup_mask (1),
			table_index (table_index_),
			lookup_index ((unsigned int) -1),
			lookup_props (0),
			nesting_level_left (HB_MAX_NESTING_LEVEL),
			has_glyph_classes (gdef.has_glyph_classes ()),
			auto_zwnj (true),
			auto_zwj (true),
			random (false),
			random_state (1) { init_iters (); }

  void init_iters ()
  {
    iter_input.init (this, false);
    iter_context.init (this, true);
  }

  void set_lookup_mask (hb_mask_t mask) { lookup_mask = mask; init_iters (); }
  void set_auto_zwj (bool auto_zwj_) { auto_zwj = auto_zwj_; init_iters (); }
  void set_auto_zwnj (bool auto_zwnj_) { auto_zwnj = auto_zwnj_; init_iters (); }
  void set_random (bool random_) { random = random_; }
  void set_recurse_func (recurse_func_t func) { recurse_func = func; }
  void set_lookup_index (unsigned int lookup_index_) { lookup_index = lookup_index_; }
  void set_lookup_props (unsigned int lookup_props_) { lookup_props = lookup_props_; init_iters (); }

  uint32_t random_number ()
  {
    /* http://www.cplusplus.com/reference/random/minstd_rand/ */
    random_state = random_state * 48271 % 2147483647;
    return random_state;
  }

  bool match_properties_mark (hb_codepoint_t  glyph,
			      unsigned int    glyph_props,
			      unsigned int    match_props) const
  {
    /* If using mark filtering sets, the high short of
     * match_props has the set index.
     */
    if (match_props & LookupFlag::UseMarkFilteringSet)
      return gdef.mark_set_covers (match_props >> 16, glyph);

    /* The second byte of match_props has the meaning
     * "ignore marks of attachment type different than
     * the attachment type specified."
     */
    if (match_props & LookupFlag::MarkAttachmentType)
      return (match_props & LookupFlag::MarkAttachmentType) == (glyph_props & LookupFlag::MarkAttachmentType);

    return true;
  }

  bool check_glyph_property (const hb_glyph_info_t *info,
			     unsigned int  match_props) const
  {
    hb_codepoint_t glyph = info->codepoint;
    unsigned int glyph_props = _hb_glyph_info_get_glyph_props (info);

    /* Not covered, if, for example, glyph class is ligature and
     * match_props includes LookupFlags::IgnoreLigatures
     */
    if (glyph_props & match_props & LookupFlag::IgnoreFlags)
      return false;

    if (unlikely (glyph_props & HB_OT_LAYOUT_GLYPH_PROPS_MARK))
      return match_properties_mark (glyph, glyph_props, match_props);

    return true;
  }

  void _set_glyph_props (hb_codepoint_t glyph_index,
			  unsigned int class_guess = 0,
			  bool ligature = false,
			  bool component = false) const
  {
    unsigned int add_in = _hb_glyph_info_get_glyph_props (&buffer->cur()) &
			  HB_OT_LAYOUT_GLYPH_PROPS_PRESERVE;
    add_in |= HB_OT_LAYOUT_GLYPH_PROPS_SUBSTITUTED;
    if (ligature)
    {
      add_in |= HB_OT_LAYOUT_GLYPH_PROPS_LIGATED;
      /* In the only place that the MULTIPLIED bit is used, Uniscribe
       * seems to only care about the "last" transformation between
       * Ligature and Multiple substitutions.  Ie. if you ligate, expand,
       * and ligate again, it forgives the multiplication and acts as
       * if only ligation happened.  As such, clear MULTIPLIED bit.
       */
      add_in &= ~HB_OT_LAYOUT_GLYPH_PROPS_MULTIPLIED;
    }
    if (component)
      add_in |= HB_OT_LAYOUT_GLYPH_PROPS_MULTIPLIED;
    if (likely (has_glyph_classes))
      _hb_glyph_info_set_glyph_props (&buffer->cur(), add_in | gdef.get_glyph_props (glyph_index));
    else if (class_guess)
      _hb_glyph_info_set_glyph_props (&buffer->cur(), add_in | class_guess);
  }

  void replace_glyph (hb_codepoint_t glyph_index) const
  {
    _set_glyph_props (glyph_index);
    (void) buffer->replace_glyph (glyph_index);
  }
  void replace_glyph_inplace (hb_codepoint_t glyph_index) const
  {
    _set_glyph_props (glyph_index);
    buffer->cur().codepoint = glyph_index;
  }
  void replace_glyph_with_ligature (hb_codepoint_t glyph_index,
				    unsigned int class_guess) const
  {
    _set_glyph_props (glyph_index, class_guess, true);
    (void) buffer->replace_glyph (glyph_index);
  }
  void output_glyph_for_component (hb_codepoint_t glyph_index,
				   unsigned int class_guess) const
  {
    _set_glyph_props (glyph_index, class_guess, false, true);
    (void) buffer->output_glyph (glyph_index);
  }
};


struct hb_get_subtables_context_t :
       hb_dispatch_context_t<hb_get_subtables_context_t>
{
  template <typename Type>
  static inline bool apply_to (const void *obj, OT::hb_ot_apply_context_t *c)
  {
    const Type *typed_obj = (const Type *) obj;
    return typed_obj->apply (c);
  }

  typedef bool (*hb_apply_func_t) (const void *obj, OT::hb_ot_apply_context_t *c);

  struct hb_applicable_t
  {
    template <typename T>
    void init (const T &obj_, hb_apply_func_t apply_func_)
    {
      obj = &obj_;
      apply_func = apply_func_;
      digest.init ();
      obj_.get_coverage ().collect_coverage (&digest);
    }

    bool apply (OT::hb_ot_apply_context_t *c) const
    {
      return digest.may_have (c->buffer->cur().codepoint) && apply_func (obj, c);
    }

    private:
    const void *obj;
    hb_apply_func_t apply_func;
    hb_set_digest_t digest;
  };

  typedef hb_vector_t<hb_applicable_t> array_t;

  /* Dispatch interface. */
  template <typename T>
  return_t dispatch (const T &obj)
  {
    hb_applicable_t *entry = array.push();
    entry->init (obj, apply_to<T>);
    return hb_empty_t ();
  }
  static return_t default_return_value () { return hb_empty_t (); }

  hb_get_subtables_context_t (array_t &array_) :
			      array (array_) {}

  array_t &array;
};




typedef bool (*intersects_func_t) (const hb_set_t *glyphs, const HBUINT16 &value, const void *data);
typedef void (*intersected_glyphs_func_t) (const hb_set_t *glyphs, const void *data, unsigned value, hb_set_t *intersected_glyphs);
typedef void (*collect_glyphs_func_t) (hb_set_t *glyphs, const HBUINT16 &value, const void *data);
typedef bool (*match_func_t) (hb_codepoint_t glyph_id, const HBUINT16 &value, const void *data);

struct ContextClosureFuncs
{
  intersects_func_t intersects;
  intersected_glyphs_func_t intersected_glyphs;
};
struct ContextCollectGlyphsFuncs
{
  collect_glyphs_func_t collect;
};
struct ContextApplyFuncs
{
  match_func_t match;
};


static inline bool intersects_glyph (const hb_set_t *glyphs, const HBUINT16 &value, const void *data HB_UNUSED)
{
  return glyphs->has (value);
}
static inline bool intersects_class (const hb_set_t *glyphs, const HBUINT16 &value, const void *data)
{
  const ClassDef &class_def = *reinterpret_cast<const ClassDef *>(data);
  return class_def.intersects_class (glyphs, value);
}
static inline bool intersects_coverage (const hb_set_t *glyphs, const HBUINT16 &value, const void *data)
{
  const Offset16To<Coverage> &coverage = (const Offset16To<Coverage>&)value;
  return (data+coverage).intersects (glyphs);
}


static inline void intersected_glyph (const hb_set_t *glyphs HB_UNUSED, const void *data, unsigned value, hb_set_t *intersected_glyphs)
{
  unsigned g = reinterpret_cast<const HBUINT16 *>(data)[value];
  intersected_glyphs->add (g);
}
static inline void intersected_class_glyphs (const hb_set_t *glyphs, const void *data, unsigned value, hb_set_t *intersected_glyphs)
{
  const ClassDef &class_def = *reinterpret_cast<const ClassDef *>(data);
  class_def.intersected_class_glyphs (glyphs, value, intersected_glyphs);
}
static inline void intersected_coverage_glyphs (const hb_set_t *glyphs, const void *data, unsigned value, hb_set_t *intersected_glyphs)
{
  Offset16To<Coverage> coverage;
  coverage = value;
  (data+coverage).intersected_coverage_glyphs (glyphs, intersected_glyphs);
}


static inline bool array_is_subset_of (const hb_set_t *glyphs,
				       unsigned int count,
				       const HBUINT16 values[],
				       intersects_func_t intersects_func,
				       const void *intersects_data)
{
  for (const HBUINT16 &_ : + hb_iter (values, count))
    if (!intersects_func (glyphs, _, intersects_data)) return false;
  return true;
}


static inline void collect_glyph (hb_set_t *glyphs, const HBUINT16 &value, const void *data HB_UNUSED)
{
  glyphs->add (value);
}
static inline void collect_class (hb_set_t *glyphs, const HBUINT16 &value, const void *data)
{
  const ClassDef &class_def = *reinterpret_cast<const ClassDef *>(data);
  class_def.collect_class (glyphs, value);
}
static inline void collect_coverage (hb_set_t *glyphs, const HBUINT16 &value, const void *data)
{
  const Offset16To<Coverage> &coverage = (const Offset16To<Coverage>&)value;
  (data+coverage).collect_coverage (glyphs);
}
static inline void collect_array (hb_collect_glyphs_context_t *c HB_UNUSED,
				  hb_set_t *glyphs,
				  unsigned int count,
				  const HBUINT16 values[],
				  collect_glyphs_func_t collect_func,
				  const void *collect_data)
{
  return
  + hb_iter (values, count)
  | hb_apply ([&] (const HBUINT16 &_) { collect_func (glyphs, _, collect_data); })
  ;
}


static inline bool match_glyph (hb_codepoint_t glyph_id, const HBUINT16 &value, const void *data HB_UNUSED)
{
  return glyph_id == value;
}
static inline bool match_class (hb_codepoint_t glyph_id, const HBUINT16 &value, const void *data)
{
  const ClassDef &class_def = *reinterpret_cast<const ClassDef *>(data);
  return class_def.get_class (glyph_id) == value;
}
static inline bool match_coverage (hb_codepoint_t glyph_id, const HBUINT16 &value, const void *data)
{
  const Offset16To<Coverage> &coverage = (const Offset16To<Coverage>&)value;
  return (data+coverage).get_coverage (glyph_id) != NOT_COVERED;
}

static inline bool would_match_input (hb_would_apply_context_t *c,
				      unsigned int count, /* Including the first glyph (not matched) */
				      const HBUINT16 input[], /* Array of input values--start with second glyph */
				      match_func_t match_func,
				      const void *match_data)
{
  if (count != c->len)
    return false;

  for (unsigned int i = 1; i < count; i++)
    if (likely (!match_func (c->glyphs[i], input[i - 1], match_data)))
      return false;

  return true;
}
static inline bool match_input (hb_ot_apply_context_t *c,
				unsigned int count, /* Including the first glyph (not matched) */
				const HBUINT16 input[], /* Array of input values--start with second glyph */
				match_func_t match_func,
				const void *match_data,
				unsigned int *end_offset,
				unsigned int match_positions[HB_MAX_CONTEXT_LENGTH],
				unsigned int *p_total_component_count = nullptr)
{
  TRACE_APPLY (nullptr);

  if (unlikely (count > HB_MAX_CONTEXT_LENGTH)) return_trace (false);

  hb_buffer_t *buffer = c->buffer;

  hb_ot_apply_context_t::skipping_iterator_t &skippy_iter = c->iter_input;
  skippy_iter.reset (buffer->idx, count - 1);
  skippy_iter.set_match_func (match_func, match_data, input);

  /*
   * This is perhaps the trickiest part of OpenType...  Remarks:
   *
   * - If all components of the ligature were marks, we call this a mark ligature.
   *
   * - If there is no GDEF, and the ligature is NOT a mark ligature, we categorize
   *   it as a ligature glyph.
   *
   * - Ligatures cannot be formed across glyphs attached to different components
   *   of previous ligatures.  Eg. the sequence is LAM,SHADDA,LAM,FATHA,HEH, and
   *   LAM,LAM,HEH form a ligature, leaving SHADDA,FATHA next to eachother.
   *   However, it would be wrong to ligate that SHADDA,FATHA sequence.
   *   There are a couple of exceptions to this:
   *
   *   o If a ligature tries ligating with marks that belong to it itself, go ahead,
   *     assuming that the font designer knows what they are doing (otherwise it can
   *     break Indic stuff when a matra wants to ligate with a conjunct,
   *
   *   o If two marks want to ligate and they belong to different components of the
   *     same ligature glyph, and said ligature glyph is to be ignored according to
   *     mark-filtering rules, then allow.
   *     https://github.com/harfbuzz/harfbuzz/issues/545
   */

  unsigned int total_component_count = 0;
  total_component_count += _hb_glyph_info_get_lig_num_comps (&buffer->cur());

  unsigned int first_lig_id = _hb_glyph_info_get_lig_id (&buffer->cur());
  unsigned int first_lig_comp = _hb_glyph_info_get_lig_comp (&buffer->cur());

  enum {
    LIGBASE_NOT_CHECKED,
    LIGBASE_MAY_NOT_SKIP,
    LIGBASE_MAY_SKIP
  } ligbase = LIGBASE_NOT_CHECKED;

  match_positions[0] = buffer->idx;
  for (unsigned int i = 1; i < count; i++)
  {
    if (!skippy_iter.next ()) return_trace (false);

    match_positions[i] = skippy_iter.idx;

    unsigned int this_lig_id = _hb_glyph_info_get_lig_id (&buffer->info[skippy_iter.idx]);
    unsigned int this_lig_comp = _hb_glyph_info_get_lig_comp (&buffer->info[skippy_iter.idx]);

    if (first_lig_id && first_lig_comp)
    {
      /* If first component was attached to a previous ligature component,
       * all subsequent components should be attached to the same ligature
       * component, otherwise we shouldn't ligate them... */
      if (first_lig_id != this_lig_id || first_lig_comp != this_lig_comp)
      {
	/* ...unless, we are attached to a base ligature and that base
	 * ligature is ignorable. */
	if (ligbase == LIGBASE_NOT_CHECKED)
	{
	  bool found = false;
	  const auto *out = buffer->out_info;
	  unsigned int j = buffer->out_len;
	  while (j && _hb_glyph_info_get_lig_id (&out[j - 1]) == first_lig_id)
	  {
	    if (_hb_glyph_info_get_lig_comp (&out[j - 1]) == 0)
	    {
	      j--;
	      found = true;
	      break;
	    }
	    j--;
	  }

	  if (found && skippy_iter.may_skip (out[j]) == hb_ot_apply_context_t::matcher_t::SKIP_YES)
	    ligbase = LIGBASE_MAY_SKIP;
	  else
	    ligbase = LIGBASE_MAY_NOT_SKIP;
	}

	if (ligbase == LIGBASE_MAY_NOT_SKIP)
	  return_trace (false);
      }
    }
    else
    {
      /* If first component was NOT attached to a previous ligature component,
       * all subsequent components should also NOT be attached to any ligature
       * component, unless they are attached to the first component itself! */
      if (this_lig_id && this_lig_comp && (this_lig_id != first_lig_id))
	return_trace (false);
    }

    total_component_count += _hb_glyph_info_get_lig_num_comps (&buffer->info[skippy_iter.idx]);
  }

  *end_offset = skippy_iter.idx - buffer->idx + 1;

  if (p_total_component_count)
    *p_total_component_count = total_component_count;

  return_trace (true);
}
static inline bool ligate_input (hb_ot_apply_context_t *c,
				 unsigned int count, /* Including the first glyph */
				 const unsigned int match_positions[HB_MAX_CONTEXT_LENGTH], /* Including the first glyph */
				 unsigned int match_length,
				 hb_codepoint_t lig_glyph,
				 unsigned int total_component_count)
{
  TRACE_APPLY (nullptr);

  hb_buffer_t *buffer = c->buffer;

  buffer->merge_clusters (buffer->idx, buffer->idx + match_length);

  /* - If a base and one or more marks ligate, consider that as a base, NOT
   *   ligature, such that all following marks can still attach to it.
   *   https://github.com/harfbuzz/harfbuzz/issues/1109
   *
   * - If all components of the ligature were marks, we call this a mark ligature.
   *   If it *is* a mark ligature, we don't allocate a new ligature id, and leave
   *   the ligature to keep its old ligature id.  This will allow it to attach to
   *   a base ligature in GPOS.  Eg. if the sequence is: LAM,LAM,SHADDA,FATHA,HEH,
   *   and LAM,LAM,HEH for a ligature, they will leave SHADDA and FATHA with a
   *   ligature id and component value of 2.  Then if SHADDA,FATHA form a ligature
   *   later, we don't want them to lose their ligature id/component, otherwise
   *   GPOS will fail to correctly position the mark ligature on top of the
   *   LAM,LAM,HEH ligature.  See:
   *     https://bugzilla.gnome.org/show_bug.cgi?id=676343
   *
   * - If a ligature is formed of components that some of which are also ligatures
   *   themselves, and those ligature components had marks attached to *their*
   *   components, we have to attach the marks to the new ligature component
   *   positions!  Now *that*'s tricky!  And these marks may be following the
   *   last component of the whole sequence, so we should loop forward looking
   *   for them and update them.
   *
   *   Eg. the sequence is LAM,LAM,SHADDA,FATHA,HEH, and the font first forms a
   *   'calt' ligature of LAM,HEH, leaving the SHADDA and FATHA with a ligature
   *   id and component == 1.  Now, during 'liga', the LAM and the LAM-HEH ligature
   *   form a LAM-LAM-HEH ligature.  We need to reassign the SHADDA and FATHA to
   *   the new ligature with a component value of 2.
   *
   *   This in fact happened to a font...  See:
   *   https://bugzilla.gnome.org/show_bug.cgi?id=437633
   */

  bool is_base_ligature = _hb_glyph_info_is_base_glyph (&buffer->info[match_positions[0]]);
  bool is_mark_ligature = _hb_glyph_info_is_mark (&buffer->info[match_positions[0]]);
  for (unsigned int i = 1; i < count; i++)
    if (!_hb_glyph_info_is_mark (&buffer->info[match_positions[i]]))
    {
      is_base_ligature = false;
      is_mark_ligature = false;
      break;
    }
  bool is_ligature = !is_base_ligature && !is_mark_ligature;

  unsigned int klass = is_ligature ? HB_OT_LAYOUT_GLYPH_PROPS_LIGATURE : 0;
  unsigned int lig_id = is_ligature ? _hb_allocate_lig_id (buffer) : 0;
  unsigned int last_lig_id = _hb_glyph_info_get_lig_id (&buffer->cur());
  unsigned int last_num_components = _hb_glyph_info_get_lig_num_comps (&buffer->cur());
  unsigned int components_so_far = last_num_components;

  if (is_ligature)
  {
    _hb_glyph_info_set_lig_props_for_ligature (&buffer->cur(), lig_id, total_component_count);
    if (_hb_glyph_info_get_general_category (&buffer->cur()) == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK)
    {
      _hb_glyph_info_set_general_category (&buffer->cur(), HB_UNICODE_GENERAL_CATEGORY_OTHER_LETTER);
    }
  }
  c->replace_glyph_with_ligature (lig_glyph, klass);

  for (unsigned int i = 1; i < count; i++)
  {
    while (buffer->idx < match_positions[i] && buffer->successful)
    {
      if (is_ligature)
      {
	unsigned int this_comp = _hb_glyph_info_get_lig_comp (&buffer->cur());
	if (this_comp == 0)
	  this_comp = last_num_components;
	unsigned int new_lig_comp = components_so_far - last_num_components +
				    hb_min (this_comp, last_num_components);
	  _hb_glyph_info_set_lig_props_for_mark (&buffer->cur(), lig_id, new_lig_comp);
      }
      (void) buffer->next_glyph ();
    }

    last_lig_id = _hb_glyph_info_get_lig_id (&buffer->cur());
    last_num_components = _hb_glyph_info_get_lig_num_comps (&buffer->cur());
    components_so_far += last_num_components;

    /* Skip the base glyph */
    buffer->idx++;
  }

  if (!is_mark_ligature && last_lig_id)
  {
    /* Re-adjust components for any marks following. */
    for (unsigned i = buffer->idx; i < buffer->len; ++i)
    {
      if (last_lig_id != _hb_glyph_info_get_lig_id (&buffer->info[i])) break;

      unsigned this_comp = _hb_glyph_info_get_lig_comp (&buffer->info[i]);
      if (!this_comp) break;

      unsigned new_lig_comp = components_so_far - last_num_components +
			      hb_min (this_comp, last_num_components);
      _hb_glyph_info_set_lig_props_for_mark (&buffer->info[i], lig_id, new_lig_comp);
    }
  }
  return_trace (true);
}

static inline bool match_backtrack (hb_ot_apply_context_t *c,
				    unsigned int count,
				    const HBUINT16 backtrack[],
				    match_func_t match_func,
				    const void *match_data,
				    unsigned int *match_start)
{
  TRACE_APPLY (nullptr);

  hb_ot_apply_context_t::skipping_iterator_t &skippy_iter = c->iter_context;
  skippy_iter.reset (c->buffer->backtrack_len (), count);
  skippy_iter.set_match_func (match_func, match_data, backtrack);

  for (unsigned int i = 0; i < count; i++)
    if (!skippy_iter.prev ())
      return_trace (false);

  *match_start = skippy_iter.idx;

  return_trace (true);
}

static inline bool match_lookahead (hb_ot_apply_context_t *c,
				    unsigned int count,
				    const HBUINT16 lookahead[],
				    match_func_t match_func,
				    const void *match_data,
				    unsigned int offset,
				    unsigned int *end_index)
{
  TRACE_APPLY (nullptr);

  hb_ot_apply_context_t::skipping_iterator_t &skippy_iter = c->iter_context;
  skippy_iter.reset (c->buffer->idx + offset - 1, count);
  skippy_iter.set_match_func (match_func, match_data, lookahead);

  for (unsigned int i = 0; i < count; i++)
    if (!skippy_iter.next ())
      return_trace (false);

  *end_index = skippy_iter.idx + 1;

  return_trace (true);
}



struct LookupRecord
{
  LookupRecord* copy (hb_serialize_context_t *c,
		      const hb_map_t         *lookup_map) const
  {
    TRACE_SERIALIZE (this);
    auto *out = c->embed (*this);
    if (unlikely (!out)) return_trace (nullptr);

    out->lookupListIndex = hb_map_get (lookup_map, lookupListIndex);
    return_trace (out);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  HBUINT16	sequenceIndex;		/* Index into current glyph
					 * sequence--first glyph = 0 */
  HBUINT16	lookupListIndex;	/* Lookup to apply to that
					 * position--zero--based */
  public:
  DEFINE_SIZE_STATIC (4);
};

enum ContextFormat { SimpleContext = 1, ClassBasedContext = 2, CoverageBasedContext = 3 };

static void context_closure_recurse_lookups (hb_closure_context_t *c,
					     unsigned inputCount, const HBUINT16 input[],
					     unsigned lookupCount,
					     const LookupRecord lookupRecord[] /* Array of LookupRecords--in design order */,
					     unsigned value,
					     ContextFormat context_format,
					     const void *data,
					     intersected_glyphs_func_t intersected_glyphs_func)
{
  hb_set_t *covered_seq_indicies = hb_set_create ();
  for (unsigned int i = 0; i < lookupCount; i++)
  {
    unsigned seqIndex = lookupRecord[i].sequenceIndex;
    if (seqIndex >= inputCount) continue;

    hb_set_t *pos_glyphs = nullptr;

    if (hb_set_is_empty (covered_seq_indicies) || !hb_set_has (covered_seq_indicies, seqIndex))
    {
      pos_glyphs = hb_set_create ();
      if (seqIndex == 0)
      {
        switch (context_format) {
        case ContextFormat::SimpleContext:
          pos_glyphs->add (value);
          break;
        case ContextFormat::ClassBasedContext:
          intersected_glyphs_func (c->cur_intersected_glyphs, data, value, pos_glyphs);
          break;
        case ContextFormat::CoverageBasedContext:
          hb_set_set (pos_glyphs, c->cur_intersected_glyphs);
          break;
        }
      }
      else
      {
        const void *input_data = input;
        unsigned input_value = seqIndex - 1;
        if (context_format != ContextFormat::SimpleContext)
        {
          input_data = data;
          input_value = input[seqIndex - 1];
        }

        intersected_glyphs_func (c->glyphs, input_data, input_value, pos_glyphs);
      }
    }

    hb_set_add (covered_seq_indicies, seqIndex);
    if (pos_glyphs)
      c->push_cur_active_glyphs (pos_glyphs);

    unsigned endIndex = inputCount;
    if (context_format == ContextFormat::CoverageBasedContext)
      endIndex += 1;

    c->recurse (lookupRecord[i].lookupListIndex, covered_seq_indicies, seqIndex, endIndex);

    if (pos_glyphs) {
      c->pop_cur_done_glyphs ();
      hb_set_destroy (pos_glyphs);
    }
  }

  hb_set_destroy (covered_seq_indicies);
}

template <typename context_t>
static inline void recurse_lookups (context_t *c,
                                    unsigned int lookupCount,
                                    const LookupRecord lookupRecord[] /* Array of LookupRecords--in design order */)
{
  for (unsigned int i = 0; i < lookupCount; i++)
    c->recurse (lookupRecord[i].lookupListIndex);
}

static inline bool apply_lookup (hb_ot_apply_context_t *c,
				 unsigned int count, /* Including the first glyph */
				 unsigned int match_positions[HB_MAX_CONTEXT_LENGTH], /* Including the first glyph */
				 unsigned int lookupCount,
				 const LookupRecord lookupRecord[], /* Array of LookupRecords--in design order */
				 unsigned int match_length)
{
  TRACE_APPLY (nullptr);

  hb_buffer_t *buffer = c->buffer;
  int end;

  /* All positions are distance from beginning of *output* buffer.
   * Adjust. */
  {
    unsigned int bl = buffer->backtrack_len ();
    end = bl + match_length;

    int delta = bl - buffer->idx;
    /* Convert positions to new indexing. */
    for (unsigned int j = 0; j < count; j++)
      match_positions[j] += delta;
  }

  for (unsigned int i = 0; i < lookupCount && buffer->successful; i++)
  {
    unsigned int idx = lookupRecord[i].sequenceIndex;
    if (idx >= count)
      continue;

    /* Don't recurse to ourself at same position.
     * Note that this test is too naive, it doesn't catch longer loops. */
    if (unlikely (idx == 0 && lookupRecord[i].lookupListIndex == c->lookup_index))
      continue;

    if (unlikely (!buffer->move_to (match_positions[idx])))
      break;

    if (unlikely (buffer->max_ops <= 0))
      break;

    unsigned int orig_len = buffer->backtrack_len () + buffer->lookahead_len ();
    if (!c->recurse (lookupRecord[i].lookupListIndex))
      continue;

    unsigned int new_len = buffer->backtrack_len () + buffer->lookahead_len ();
    int delta = new_len - orig_len;

    if (!delta)
      continue;

    /* Recursed lookup changed buffer len.  Adjust.
     *
     * TODO:
     *
     * Right now, if buffer length increased by n, we assume n new glyphs
     * were added right after the current position, and if buffer length
     * was decreased by n, we assume n match positions after the current
     * one where removed.  The former (buffer length increased) case is
     * fine, but the decrease case can be improved in at least two ways,
     * both of which are significant:
     *
     *   - If recursed-to lookup is MultipleSubst and buffer length
     *     decreased, then it's current match position that was deleted,
     *     NOT the one after it.
     *
     *   - If buffer length was decreased by n, it does not necessarily
     *     mean that n match positions where removed, as there might
     *     have been marks and default-ignorables in the sequence.  We
     *     should instead drop match positions between current-position
     *     and current-position + n instead. Though, am not sure which
     *     one is better. Both cases have valid uses. Sigh.
     *
     * It should be possible to construct tests for both of these cases.
     */

    end += delta;
    if (end <= int (match_positions[idx]))
    {
      /* End might end up being smaller than match_positions[idx] if the recursed
       * lookup ended up removing many items, more than we have had matched.
       * Just never rewind end back and get out of here.
       * https://bugs.chromium.org/p/chromium/issues/detail?id=659496 */
      end = match_positions[idx];
      /* There can't be any further changes. */
      break;
    }

    unsigned int next = idx + 1; /* next now is the position after the recursed lookup. */

    if (delta > 0)
    {
      if (unlikely (delta + count > HB_MAX_CONTEXT_LENGTH))
	break;
    }
    else
    {
      /* NOTE: delta is negative. */
      delta = hb_max (delta, (int) next - (int) count);
      next -= delta;
    }

    /* Shift! */
    memmove (match_positions + next + delta, match_positions + next,
	     (count - next) * sizeof (match_positions[0]));
    next += delta;
    count += delta;

    /* Fill in new entries. */
    for (unsigned int j = idx + 1; j < next; j++)
      match_positions[j] = match_positions[j - 1] + 1;

    /* And fixup the rest. */
    for (; next < count; next++)
      match_positions[next] += delta;
  }

  (void) buffer->move_to (end);

  return_trace (true);
}



/* Contextual lookups */

struct ContextClosureLookupContext
{
  ContextClosureFuncs funcs;
  ContextFormat context_format;
  const void *intersects_data;
};

struct ContextCollectGlyphsLookupContext
{
  ContextCollectGlyphsFuncs funcs;
  const void *collect_data;
};

struct ContextApplyLookupContext
{
  ContextApplyFuncs funcs;
  const void *match_data;
};

static inline bool context_intersects (const hb_set_t *glyphs,
				       unsigned int inputCount, /* Including the first glyph (not matched) */
				       const HBUINT16 input[], /* Array of input values--start with second glyph */
				       ContextClosureLookupContext &lookup_context)
{
  return array_is_subset_of (glyphs,
			     inputCount ? inputCount - 1 : 0, input,
			     lookup_context.funcs.intersects, lookup_context.intersects_data);
}

static inline void context_closure_lookup (hb_closure_context_t *c,
					   unsigned int inputCount, /* Including the first glyph (not matched) */
					   const HBUINT16 input[], /* Array of input values--start with second glyph */
					   unsigned int lookupCount,
					   const LookupRecord lookupRecord[],
					   unsigned value, /* Index of first glyph in Coverage or Class value in ClassDef table */
					   ContextClosureLookupContext &lookup_context)
{
  if (context_intersects (c->glyphs,
			  inputCount, input,
			  lookup_context))
    context_closure_recurse_lookups (c,
				     inputCount, input,
				     lookupCount, lookupRecord,
				     value,
				     lookup_context.context_format,
				     lookup_context.intersects_data,
				     lookup_context.funcs.intersected_glyphs);
}

static inline void context_collect_glyphs_lookup (hb_collect_glyphs_context_t *c,
						  unsigned int inputCount, /* Including the first glyph (not matched) */
						  const HBUINT16 input[], /* Array of input values--start with second glyph */
						  unsigned int lookupCount,
						  const LookupRecord lookupRecord[],
						  ContextCollectGlyphsLookupContext &lookup_context)
{
  collect_array (c, c->input,
		 inputCount ? inputCount - 1 : 0, input,
		 lookup_context.funcs.collect, lookup_context.collect_data);
  recurse_lookups (c,
		   lookupCount, lookupRecord);
}

static inline bool context_would_apply_lookup (hb_would_apply_context_t *c,
					       unsigned int inputCount, /* Including the first glyph (not matched) */
					       const HBUINT16 input[], /* Array of input values--start with second glyph */
					       unsigned int lookupCount HB_UNUSED,
					       const LookupRecord lookupRecord[] HB_UNUSED,
					       ContextApplyLookupContext &lookup_context)
{
  return would_match_input (c,
			    inputCount, input,
			    lookup_context.funcs.match, lookup_context.match_data);
}
static inline bool context_apply_lookup (hb_ot_apply_context_t *c,
					 unsigned int inputCount, /* Including the first glyph (not matched) */
					 const HBUINT16 input[], /* Array of input values--start with second glyph */
					 unsigned int lookupCount,
					 const LookupRecord lookupRecord[],
					 ContextApplyLookupContext &lookup_context)
{
  unsigned int match_length = 0;
  unsigned int match_positions[HB_MAX_CONTEXT_LENGTH];
  return match_input (c,
		      inputCount, input,
		      lookup_context.funcs.match, lookup_context.match_data,
		      &match_length, match_positions)
      && (c->buffer->unsafe_to_break (c->buffer->idx, c->buffer->idx + match_length),
	  apply_lookup (c,
		       inputCount, match_positions,
		       lookupCount, lookupRecord,
		       match_length));
}

struct Rule
{
  bool intersects (const hb_set_t *glyphs, ContextClosureLookupContext &lookup_context) const
  {
    return context_intersects (glyphs,
			       inputCount, inputZ.arrayZ,
			       lookup_context);
  }

  void closure (hb_closure_context_t *c, unsigned value, ContextClosureLookupContext &lookup_context) const
  {
    if (unlikely (c->lookup_limit_exceeded ())) return;

    const UnsizedArrayOf<LookupRecord> &lookupRecord = StructAfter<UnsizedArrayOf<LookupRecord>>
						       (inputZ.as_array ((inputCount ? inputCount - 1 : 0)));
    context_closure_lookup (c,
			    inputCount, inputZ.arrayZ,
			    lookupCount, lookupRecord.arrayZ,
			    value, lookup_context);
  }

  void closure_lookups (hb_closure_lookups_context_t *c,
                        ContextClosureLookupContext &lookup_context) const
  {
    if (unlikely (c->lookup_limit_exceeded ())) return;
    if (!intersects (c->glyphs, lookup_context)) return;

    const UnsizedArrayOf<LookupRecord> &lookupRecord = StructAfter<UnsizedArrayOf<LookupRecord>>
						       (inputZ.as_array (inputCount ? inputCount - 1 : 0));
    recurse_lookups (c, lookupCount, lookupRecord.arrayZ);
  }

  void collect_glyphs (hb_collect_glyphs_context_t *c,
		       ContextCollectGlyphsLookupContext &lookup_context) const
  {
    const UnsizedArrayOf<LookupRecord> &lookupRecord = StructAfter<UnsizedArrayOf<LookupRecord>>
						       (inputZ.as_array (inputCount ? inputCount - 1 : 0));
    context_collect_glyphs_lookup (c,
				   inputCount, inputZ.arrayZ,
				   lookupCount, lookupRecord.arrayZ,
				   lookup_context);
  }

  bool would_apply (hb_would_apply_context_t *c,
		    ContextApplyLookupContext &lookup_context) const
  {
    const UnsizedArrayOf<LookupRecord> &lookupRecord = StructAfter<UnsizedArrayOf<LookupRecord>>
						       (inputZ.as_array (inputCount ? inputCount - 1 : 0));
    return context_would_apply_lookup (c,
				       inputCount, inputZ.arrayZ,
				       lookupCount, lookupRecord.arrayZ,
				       lookup_context);
  }

  bool apply (hb_ot_apply_context_t *c,
	      ContextApplyLookupContext &lookup_context) const
  {
    TRACE_APPLY (this);
    const UnsizedArrayOf<LookupRecord> &lookupRecord = StructAfter<UnsizedArrayOf<LookupRecord>>
						       (inputZ.as_array (inputCount ? inputCount - 1 : 0));
    return_trace (context_apply_lookup (c, inputCount, inputZ.arrayZ, lookupCount, lookupRecord.arrayZ, lookup_context));
  }

  bool serialize (hb_serialize_context_t *c,
		  const hb_map_t *input_mapping, /* old->new glyphid or class mapping */
		  const hb_map_t *lookup_map) const
  {
    TRACE_SERIALIZE (this);
    auto *out = c->start_embed (this);
    if (unlikely (!c->extend_min (out))) return_trace (false);

    out->inputCount = inputCount;
    out->lookupCount = lookupCount;

    const hb_array_t<const HBUINT16> input = inputZ.as_array (inputCount - 1);
    for (const auto org : input)
    {
      HBUINT16 d;
      d = input_mapping->get (org);
      c->copy (d);
    }

    const UnsizedArrayOf<LookupRecord> &lookupRecord = StructAfter<UnsizedArrayOf<LookupRecord>>
						       (inputZ.as_array ((inputCount ? inputCount - 1 : 0)));
    for (unsigned i = 0; i < (unsigned) lookupCount; i++)
      c->copy (lookupRecord[i], lookup_map);

    return_trace (true);
  }

  bool subset (hb_subset_context_t *c,
	       const hb_map_t *lookup_map,
	       const hb_map_t *klass_map = nullptr) const
  {
    TRACE_SUBSET (this);

    const hb_array_t<const HBUINT16> input = inputZ.as_array ((inputCount ? inputCount - 1 : 0));
    if (!input.length) return_trace (false);

    const hb_map_t *mapping = klass_map == nullptr ? c->plan->glyph_map : klass_map;
    if (!hb_all (input, mapping)) return_trace (false);
    return_trace (serialize (c->serializer, mapping, lookup_map));
  }

  public:
  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (inputCount.sanitize (c) &&
		  lookupCount.sanitize (c) &&
		  c->check_range (inputZ.arrayZ,
				  inputZ.item_size * (inputCount ? inputCount - 1 : 0) +
				  LookupRecord::static_size * lookupCount));
  }

  protected:
  HBUINT16	inputCount;		/* Total number of glyphs in input
					 * glyph sequence--includes the first
					 * glyph */
  HBUINT16	lookupCount;		/* Number of LookupRecords */
  UnsizedArrayOf<HBUINT16>
		inputZ;			/* Array of match inputs--start with
					 * second glyph */
/*UnsizedArrayOf<LookupRecord>
		lookupRecordX;*/	/* Array of LookupRecords--in
					 * design order */
  public:
  DEFINE_SIZE_ARRAY (4, inputZ);
};

struct RuleSet
{
  bool intersects (const hb_set_t *glyphs,
		   ContextClosureLookupContext &lookup_context) const
  {
    return
    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_map ([&] (const Rule &_) { return _.intersects (glyphs, lookup_context); })
    | hb_any
    ;
  }

  void closure (hb_closure_context_t *c, unsigned value,
		ContextClosureLookupContext &lookup_context) const
  {
    if (unlikely (c->lookup_limit_exceeded ())) return;

    return
    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const Rule &_) { _.closure (c, value, lookup_context); })
    ;
  }

  void closure_lookups (hb_closure_lookups_context_t *c,
                        ContextClosureLookupContext &lookup_context) const
  {
    if (unlikely (c->lookup_limit_exceeded ())) return;
    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const Rule &_) { _.closure_lookups (c, lookup_context); })
    ;
  }

  void collect_glyphs (hb_collect_glyphs_context_t *c,
		       ContextCollectGlyphsLookupContext &lookup_context) const
  {
    return
    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const Rule &_) { _.collect_glyphs (c, lookup_context); })
    ;
  }

  bool would_apply (hb_would_apply_context_t *c,
		    ContextApplyLookupContext &lookup_context) const
  {
    return
    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_map ([&] (const Rule &_) { return _.would_apply (c, lookup_context); })
    | hb_any
    ;
  }

  bool apply (hb_ot_apply_context_t *c,
	      ContextApplyLookupContext &lookup_context) const
  {
    TRACE_APPLY (this);
    return_trace (
    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_map ([&] (const Rule &_) { return _.apply (c, lookup_context); })
    | hb_any
    )
    ;
  }

  bool subset (hb_subset_context_t *c,
	       const hb_map_t *lookup_map,
	       const hb_map_t *klass_map = nullptr) const
  {
    TRACE_SUBSET (this);

    auto snap = c->serializer->snapshot ();
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);

    for (const Offset16To<Rule>& _ : rule)
    {
      if (!_) continue;
      auto *o = out->rule.serialize_append (c->serializer);
      if (unlikely (!o)) continue;

      auto o_snap = c->serializer->snapshot ();
      if (!o->serialize_subset (c, _, this, lookup_map, klass_map))
      {
	out->rule.pop ();
	c->serializer->revert (o_snap);
      }
    }

    bool ret = bool (out->rule);
    if (!ret) c->serializer->revert (snap);

    return_trace (ret);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (rule.sanitize (c, this));
  }

  protected:
  Array16OfOffset16To<Rule>
		rule;			/* Array of Rule tables
					 * ordered by preference */
  public:
  DEFINE_SIZE_ARRAY (2, rule);
};


struct ContextFormat1
{
  bool intersects (const hb_set_t *glyphs) const
  {
    struct ContextClosureLookupContext lookup_context = {
      {intersects_glyph, intersected_glyph},
      ContextFormat::SimpleContext,
      nullptr
    };

    return
    + hb_zip (this+coverage, ruleSet)
    | hb_filter (*glyphs, hb_first)
    | hb_map (hb_second)
    | hb_map (hb_add (this))
    | hb_map ([&] (const RuleSet &_) { return _.intersects (glyphs, lookup_context); })
    | hb_any
    ;
  }

  bool may_have_non_1to1 () const
  { return true; }

  void closure (hb_closure_context_t *c) const
  {
    c->cur_intersected_glyphs->clear ();
    get_coverage ().intersected_coverage_glyphs (c->parent_active_glyphs (), c->cur_intersected_glyphs);

    struct ContextClosureLookupContext lookup_context = {
      {intersects_glyph, intersected_glyph},
      ContextFormat::SimpleContext,
      nullptr
    };

    + hb_zip (this+coverage, hb_range ((unsigned) ruleSet.len))
    | hb_filter (c->parent_active_glyphs (), hb_first)
    | hb_map ([&](const hb_pair_t<hb_codepoint_t, unsigned> _) { return hb_pair_t<unsigned, const RuleSet&> (_.first, this+ruleSet[_.second]); })
    | hb_apply ([&] (const hb_pair_t<unsigned, const RuleSet&>& _) { _.second.closure (c, _.first, lookup_context); })
    ;
  }

  void closure_lookups (hb_closure_lookups_context_t *c) const
  {
    struct ContextClosureLookupContext lookup_context = {
      {intersects_glyph, intersected_glyph},
      ContextFormat::SimpleContext,
      nullptr
    };

    + hb_zip (this+coverage, ruleSet)
    | hb_filter (*c->glyphs, hb_first)
    | hb_map (hb_second)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const RuleSet &_) { _.closure_lookups (c, lookup_context); })
    ;
  }

  void collect_variation_indices (hb_collect_variation_indices_context_t *c) const {}

  void collect_glyphs (hb_collect_glyphs_context_t *c) const
  {
    (this+coverage).collect_coverage (c->input);

    struct ContextCollectGlyphsLookupContext lookup_context = {
      {collect_glyph},
      nullptr
    };

    + hb_iter (ruleSet)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const RuleSet &_) { _.collect_glyphs (c, lookup_context); })
    ;
  }

  bool would_apply (hb_would_apply_context_t *c) const
  {
    const RuleSet &rule_set = this+ruleSet[(this+coverage).get_coverage (c->glyphs[0])];
    struct ContextApplyLookupContext lookup_context = {
      {match_glyph},
      nullptr
    };
    return rule_set.would_apply (c, lookup_context);
  }

  const Coverage &get_coverage () const { return this+coverage; }

  bool apply (hb_ot_apply_context_t *c) const
  {
    TRACE_APPLY (this);
    unsigned int index = (this+coverage).get_coverage (c->buffer->cur().codepoint);
    if (likely (index == NOT_COVERED))
      return_trace (false);

    const RuleSet &rule_set = this+ruleSet[index];
    struct ContextApplyLookupContext lookup_context = {
      {match_glyph},
      nullptr
    };
    return_trace (rule_set.apply (c, lookup_context));
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    const hb_set_t &glyphset = *c->plan->glyphset_gsub ();
    const hb_map_t &glyph_map = *c->plan->glyph_map;

    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);
    out->format = format;

    const hb_map_t *lookup_map = c->table_tag == HB_OT_TAG_GSUB ? c->plan->gsub_lookups : c->plan->gpos_lookups;
    hb_sorted_vector_t<hb_codepoint_t> new_coverage;
    + hb_zip (this+coverage, ruleSet)
    | hb_filter (glyphset, hb_first)
    | hb_filter (subset_offset_array (c, out->ruleSet, this, lookup_map), hb_second)
    | hb_map (hb_first)
    | hb_map (glyph_map)
    | hb_sink (new_coverage)
    ;

    out->coverage.serialize_serialize (c->serializer, new_coverage.iter ());
    return_trace (bool (new_coverage));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (coverage.sanitize (c, this) && ruleSet.sanitize (c, this));
  }

  protected:
  HBUINT16	format;			/* Format identifier--format = 1 */
  Offset16To<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  Array16OfOffset16To<RuleSet>
		ruleSet;		/* Array of RuleSet tables
					 * ordered by Coverage Index */
  public:
  DEFINE_SIZE_ARRAY (6, ruleSet);
};


struct ContextFormat2
{
  bool intersects (const hb_set_t *glyphs) const
  {
    if (!(this+coverage).intersects (glyphs))
      return false;

    const ClassDef &class_def = this+classDef;

    struct ContextClosureLookupContext lookup_context = {
      {intersects_class, intersected_class_glyphs},
      ContextFormat::ClassBasedContext,
      &class_def
    };

    return
    + hb_iter (ruleSet)
    | hb_map (hb_add (this))
    | hb_enumerate
    | hb_map ([&] (const hb_pair_t<unsigned, const RuleSet &> p)
	      { return class_def.intersects_class (glyphs, p.first) &&
		       p.second.intersects (glyphs, lookup_context); })
    | hb_any
    ;
  }

  bool may_have_non_1to1 () const
  { return true; }

  void closure (hb_closure_context_t *c) const
  {
    if (!(this+coverage).intersects (c->glyphs))
      return;

    c->cur_intersected_glyphs->clear ();
    get_coverage ().intersected_coverage_glyphs (c->parent_active_glyphs (), c->cur_intersected_glyphs);

    const ClassDef &class_def = this+classDef;

    struct ContextClosureLookupContext lookup_context = {
      {intersects_class, intersected_class_glyphs},
      ContextFormat::ClassBasedContext,
      &class_def
    };

    return
    + hb_enumerate (ruleSet)
    | hb_filter ([&] (unsigned _)
		 { return class_def.intersects_class (c->cur_intersected_glyphs, _); },
		 hb_first)
    | hb_apply ([&] (const hb_pair_t<unsigned, const Offset16To<RuleSet>&> _)
                {
                  const RuleSet& rule_set = this+_.second;
                  rule_set.closure (c, _.first, lookup_context);
                })
    ;
  }

  void closure_lookups (hb_closure_lookups_context_t *c) const
  {
    if (!(this+coverage).intersects (c->glyphs))
      return;

    const ClassDef &class_def = this+classDef;

    struct ContextClosureLookupContext lookup_context = {
      {intersects_class, intersected_class_glyphs},
      ContextFormat::ClassBasedContext,
      &class_def
    };

    + hb_iter (ruleSet)
    | hb_map (hb_add (this))
    | hb_enumerate
    | hb_filter ([&] (const hb_pair_t<unsigned, const RuleSet &> p)
    { return class_def.intersects_class (c->glyphs, p.first); })
    | hb_map (hb_second)
    | hb_apply ([&] (const RuleSet & _)
    { _.closure_lookups (c, lookup_context); });
  }

  void collect_variation_indices (hb_collect_variation_indices_context_t *c) const {}

  void collect_glyphs (hb_collect_glyphs_context_t *c) const
  {
    (this+coverage).collect_coverage (c->input);

    const ClassDef &class_def = this+classDef;
    struct ContextCollectGlyphsLookupContext lookup_context = {
      {collect_class},
      &class_def
    };

    + hb_iter (ruleSet)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const RuleSet &_) { _.collect_glyphs (c, lookup_context); })
    ;
  }

  bool would_apply (hb_would_apply_context_t *c) const
  {
    const ClassDef &class_def = this+classDef;
    unsigned int index = class_def.get_class (c->glyphs[0]);
    const RuleSet &rule_set = this+ruleSet[index];
    struct ContextApplyLookupContext lookup_context = {
      {match_class},
      &class_def
    };
    return rule_set.would_apply (c, lookup_context);
  }

  const Coverage &get_coverage () const { return this+coverage; }

  bool apply (hb_ot_apply_context_t *c) const
  {
    TRACE_APPLY (this);
    unsigned int index = (this+coverage).get_coverage (c->buffer->cur().codepoint);
    if (likely (index == NOT_COVERED)) return_trace (false);

    const ClassDef &class_def = this+classDef;
    index = class_def.get_class (c->buffer->cur().codepoint);
    const RuleSet &rule_set = this+ruleSet[index];
    struct ContextApplyLookupContext lookup_context = {
      {match_class},
      &class_def
    };
    return_trace (rule_set.apply (c, lookup_context));
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);
    out->format = format;
    if (unlikely (!out->coverage.serialize_subset (c, coverage, this)))
      return_trace (false);

    hb_map_t klass_map;
    out->classDef.serialize_subset (c, classDef, this, &klass_map);

    const hb_map_t *lookup_map = c->table_tag == HB_OT_TAG_GSUB ? c->plan->gsub_lookups : c->plan->gpos_lookups;
    bool ret = true;
    int non_zero_index = 0, index = 0;
    for (const auto& _ : + hb_enumerate (ruleSet)
			 | hb_filter (klass_map, hb_first))
    {
      auto *o = out->ruleSet.serialize_append (c->serializer);
      if (unlikely (!o))
      {
	ret = false;
	break;
      }

      if (o->serialize_subset (c, _.second, this, lookup_map, &klass_map))
	non_zero_index = index;

      index++;
    }

    if (!ret) return_trace (ret);

    //prune empty trailing ruleSets
    --index;
    while (index > non_zero_index)
    {
      out->ruleSet.pop ();
      index--;
    }

    return_trace (bool (out->ruleSet));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (coverage.sanitize (c, this) && classDef.sanitize (c, this) && ruleSet.sanitize (c, this));
  }

  protected:
  HBUINT16	format;			/* Format identifier--format = 2 */
  Offset16To<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  Offset16To<ClassDef>
		classDef;		/* Offset to glyph ClassDef table--from
					 * beginning of table */
  Array16OfOffset16To<RuleSet>
		ruleSet;		/* Array of RuleSet tables
					 * ordered by class */
  public:
  DEFINE_SIZE_ARRAY (8, ruleSet);
};


struct ContextFormat3
{
  bool intersects (const hb_set_t *glyphs) const
  {
    if (!(this+coverageZ[0]).intersects (glyphs))
      return false;

    struct ContextClosureLookupContext lookup_context = {
      {intersects_coverage, intersected_coverage_glyphs},
      ContextFormat::CoverageBasedContext,
      this
    };
    return context_intersects (glyphs,
			       glyphCount, (const HBUINT16 *) (coverageZ.arrayZ + 1),
			       lookup_context);
  }

  bool may_have_non_1to1 () const
  { return true; }

  void closure (hb_closure_context_t *c) const
  {
    if (!(this+coverageZ[0]).intersects (c->glyphs))
      return;

    c->cur_intersected_glyphs->clear ();
    get_coverage ().intersected_coverage_glyphs (c->parent_active_glyphs (), c->cur_intersected_glyphs);

    const LookupRecord *lookupRecord = &StructAfter<LookupRecord> (coverageZ.as_array (glyphCount));
    struct ContextClosureLookupContext lookup_context = {
      {intersects_coverage, intersected_coverage_glyphs},
      ContextFormat::CoverageBasedContext,
      this
    };
    context_closure_lookup (c,
			    glyphCount, (const HBUINT16 *) (coverageZ.arrayZ + 1),
			    lookupCount, lookupRecord,
			    0, lookup_context);
  }

  void closure_lookups (hb_closure_lookups_context_t *c) const
  {
    if (!intersects (c->glyphs))
      return;
    const LookupRecord *lookupRecord = &StructAfter<LookupRecord> (coverageZ.as_array (glyphCount));
    recurse_lookups (c, lookupCount, lookupRecord);
  }

  void collect_variation_indices (hb_collect_variation_indices_context_t *c) const {}

  void collect_glyphs (hb_collect_glyphs_context_t *c) const
  {
    (this+coverageZ[0]).collect_coverage (c->input);

    const LookupRecord *lookupRecord = &StructAfter<LookupRecord> (coverageZ.as_array (glyphCount));
    struct ContextCollectGlyphsLookupContext lookup_context = {
      {collect_coverage},
      this
    };

    context_collect_glyphs_lookup (c,
				   glyphCount, (const HBUINT16 *) (coverageZ.arrayZ + 1),
				   lookupCount, lookupRecord,
				   lookup_context);
  }

  bool would_apply (hb_would_apply_context_t *c) const
  {
    const LookupRecord *lookupRecord = &StructAfter<LookupRecord> (coverageZ.as_array (glyphCount));
    struct ContextApplyLookupContext lookup_context = {
      {match_coverage},
      this
    };
    return context_would_apply_lookup (c,
				       glyphCount, (const HBUINT16 *) (coverageZ.arrayZ + 1),
				       lookupCount, lookupRecord,
				       lookup_context);
  }

  const Coverage &get_coverage () const { return this+coverageZ[0]; }

  bool apply (hb_ot_apply_context_t *c) const
  {
    TRACE_APPLY (this);
    unsigned int index = (this+coverageZ[0]).get_coverage (c->buffer->cur().codepoint);
    if (likely (index == NOT_COVERED)) return_trace (false);

    const LookupRecord *lookupRecord = &StructAfter<LookupRecord> (coverageZ.as_array (glyphCount));
    struct ContextApplyLookupContext lookup_context = {
      {match_coverage},
      this
    };
    return_trace (context_apply_lookup (c, glyphCount, (const HBUINT16 *) (coverageZ.arrayZ + 1), lookupCount, lookupRecord, lookup_context));
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);

    out->format = format;
    out->glyphCount = glyphCount;
    out->lookupCount = lookupCount;

    auto coverages = coverageZ.as_array (glyphCount);

    for (const Offset16To<Coverage>& offset : coverages)
    {
      /* TODO(subset) This looks like should not be necessary to write this way. */
      auto *o = c->serializer->allocate_size<Offset16To<Coverage>> (Offset16To<Coverage>::static_size);
      if (unlikely (!o)) return_trace (false);
      if (!o->serialize_subset (c, offset, this)) return_trace (false);
    }

    const LookupRecord *lookupRecord = &StructAfter<LookupRecord> (coverageZ.as_array (glyphCount));
    const hb_map_t *lookup_map = c->table_tag == HB_OT_TAG_GSUB ? c->plan->gsub_lookups : c->plan->gpos_lookups;
    for (unsigned i = 0; i < (unsigned) lookupCount; i++)
      c->serializer->copy (lookupRecord[i], lookup_map);

    return_trace (true);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (!c->check_struct (this)) return_trace (false);
    unsigned int count = glyphCount;
    if (!count) return_trace (false); /* We want to access coverageZ[0] freely. */
    if (!c->check_array (coverageZ.arrayZ, count)) return_trace (false);
    for (unsigned int i = 0; i < count; i++)
      if (!coverageZ[i].sanitize (c, this)) return_trace (false);
    const LookupRecord *lookupRecord = &StructAfter<LookupRecord> (coverageZ.as_array (glyphCount));
    return_trace (c->check_array (lookupRecord, lookupCount));
  }

  protected:
  HBUINT16	format;			/* Format identifier--format = 3 */
  HBUINT16	glyphCount;		/* Number of glyphs in the input glyph
					 * sequence */
  HBUINT16	lookupCount;		/* Number of LookupRecords */
  UnsizedArrayOf<Offset16To<Coverage>>
		coverageZ;		/* Array of offsets to Coverage
					 * table in glyph sequence order */
/*UnsizedArrayOf<LookupRecord>
		lookupRecordX;*/	/* Array of LookupRecords--in
					 * design order */
  public:
  DEFINE_SIZE_ARRAY (6, coverageZ);
};

struct Context
{
  template <typename context_t, typename ...Ts>
  typename context_t::return_t dispatch (context_t *c, Ts&&... ds) const
  {
    TRACE_DISPATCH (this, u.format);
    if (unlikely (!c->may_dispatch (this, &u.format))) return_trace (c->no_dispatch_return_value ());
    switch (u.format) {
    case 1: return_trace (c->dispatch (u.format1, hb_forward<Ts> (ds)...));
    case 2: return_trace (c->dispatch (u.format2, hb_forward<Ts> (ds)...));
    case 3: return_trace (c->dispatch (u.format3, hb_forward<Ts> (ds)...));
    default:return_trace (c->default_return_value ());
    }
  }

  protected:
  union {
  HBUINT16		format;		/* Format identifier */
  ContextFormat1	format1;
  ContextFormat2	format2;
  ContextFormat3	format3;
  } u;
};


/* Chaining Contextual lookups */

struct ChainContextClosureLookupContext
{
  ContextClosureFuncs funcs;
  ContextFormat context_format;
  const void *intersects_data[3];
};

struct ChainContextCollectGlyphsLookupContext
{
  ContextCollectGlyphsFuncs funcs;
  const void *collect_data[3];
};

struct ChainContextApplyLookupContext
{
  ContextApplyFuncs funcs;
  const void *match_data[3];
};

static inline bool chain_context_intersects (const hb_set_t *glyphs,
					     unsigned int backtrackCount,
					     const HBUINT16 backtrack[],
					     unsigned int inputCount, /* Including the first glyph (not matched) */
					     const HBUINT16 input[], /* Array of input values--start with second glyph */
					     unsigned int lookaheadCount,
					     const HBUINT16 lookahead[],
					     ChainContextClosureLookupContext &lookup_context)
{
  return array_is_subset_of (glyphs,
			     backtrackCount, backtrack,
			     lookup_context.funcs.intersects, lookup_context.intersects_data[0])
      && array_is_subset_of (glyphs,
			     inputCount ? inputCount - 1 : 0, input,
			     lookup_context.funcs.intersects, lookup_context.intersects_data[1])
      && array_is_subset_of (glyphs,
			     lookaheadCount, lookahead,
			     lookup_context.funcs.intersects, lookup_context.intersects_data[2]);
}

static inline void chain_context_closure_lookup (hb_closure_context_t *c,
						 unsigned int backtrackCount,
						 const HBUINT16 backtrack[],
						 unsigned int inputCount, /* Including the first glyph (not matched) */
						 const HBUINT16 input[], /* Array of input values--start with second glyph */
						 unsigned int lookaheadCount,
						 const HBUINT16 lookahead[],
						 unsigned int lookupCount,
						 const LookupRecord lookupRecord[],
						 unsigned value,
						 ChainContextClosureLookupContext &lookup_context)
{
  if (chain_context_intersects (c->glyphs,
				backtrackCount, backtrack,
				inputCount, input,
				lookaheadCount, lookahead,
				lookup_context))
    context_closure_recurse_lookups (c,
		     inputCount, input,
		     lookupCount, lookupRecord,
		     value,
		     lookup_context.context_format,
		     lookup_context.intersects_data[1],
		     lookup_context.funcs.intersected_glyphs);
}

static inline void chain_context_collect_glyphs_lookup (hb_collect_glyphs_context_t *c,
							unsigned int backtrackCount,
							const HBUINT16 backtrack[],
							unsigned int inputCount, /* Including the first glyph (not matched) */
							const HBUINT16 input[], /* Array of input values--start with second glyph */
							unsigned int lookaheadCount,
							const HBUINT16 lookahead[],
							unsigned int lookupCount,
							const LookupRecord lookupRecord[],
							ChainContextCollectGlyphsLookupContext &lookup_context)
{
  collect_array (c, c->before,
		 backtrackCount, backtrack,
		 lookup_context.funcs.collect, lookup_context.collect_data[0]);
  collect_array (c, c->input,
		 inputCount ? inputCount - 1 : 0, input,
		 lookup_context.funcs.collect, lookup_context.collect_data[1]);
  collect_array (c, c->after,
		 lookaheadCount, lookahead,
		 lookup_context.funcs.collect, lookup_context.collect_data[2]);
  recurse_lookups (c,
		   lookupCount, lookupRecord);
}

static inline bool chain_context_would_apply_lookup (hb_would_apply_context_t *c,
						     unsigned int backtrackCount,
						     const HBUINT16 backtrack[] HB_UNUSED,
						     unsigned int inputCount, /* Including the first glyph (not matched) */
						     const HBUINT16 input[], /* Array of input values--start with second glyph */
						     unsigned int lookaheadCount,
						     const HBUINT16 lookahead[] HB_UNUSED,
						     unsigned int lookupCount HB_UNUSED,
						     const LookupRecord lookupRecord[] HB_UNUSED,
						     ChainContextApplyLookupContext &lookup_context)
{
  return (c->zero_context ? !backtrackCount && !lookaheadCount : true)
      && would_match_input (c,
			    inputCount, input,
			    lookup_context.funcs.match, lookup_context.match_data[1]);
}

static inline bool chain_context_apply_lookup (hb_ot_apply_context_t *c,
					       unsigned int backtrackCount,
					       const HBUINT16 backtrack[],
					       unsigned int inputCount, /* Including the first glyph (not matched) */
					       const HBUINT16 input[], /* Array of input values--start with second glyph */
					       unsigned int lookaheadCount,
					       const HBUINT16 lookahead[],
					       unsigned int lookupCount,
					       const LookupRecord lookupRecord[],
					       ChainContextApplyLookupContext &lookup_context)
{
  unsigned int start_index = 0, match_length = 0, end_index = 0;
  unsigned int match_positions[HB_MAX_CONTEXT_LENGTH];
  return match_input (c,
		      inputCount, input,
		      lookup_context.funcs.match, lookup_context.match_data[1],
		      &match_length, match_positions)
      && match_backtrack (c,
			  backtrackCount, backtrack,
			  lookup_context.funcs.match, lookup_context.match_data[0],
			  &start_index)
      && match_lookahead (c,
			  lookaheadCount, lookahead,
			  lookup_context.funcs.match, lookup_context.match_data[2],
			  match_length, &end_index)
      && (c->buffer->unsafe_to_break_from_outbuffer (start_index, end_index),
	  apply_lookup (c,
			inputCount, match_positions,
			lookupCount, lookupRecord,
			match_length));
}

struct ChainRule
{
  bool intersects (const hb_set_t *glyphs, ChainContextClosureLookupContext &lookup_context) const
  {
    const HeadlessArrayOf<HBUINT16> &input = StructAfter<HeadlessArrayOf<HBUINT16>> (backtrack);
    const Array16Of<HBUINT16> &lookahead = StructAfter<Array16Of<HBUINT16>> (input);
    return chain_context_intersects (glyphs,
				     backtrack.len, backtrack.arrayZ,
				     input.lenP1, input.arrayZ,
				     lookahead.len, lookahead.arrayZ,
				     lookup_context);
  }

  void closure (hb_closure_context_t *c, unsigned value,
		ChainContextClosureLookupContext &lookup_context) const
  {
    if (unlikely (c->lookup_limit_exceeded ())) return;

    const HeadlessArrayOf<HBUINT16> &input = StructAfter<HeadlessArrayOf<HBUINT16>> (backtrack);
    const Array16Of<HBUINT16> &lookahead = StructAfter<Array16Of<HBUINT16>> (input);
    const Array16Of<LookupRecord> &lookup = StructAfter<Array16Of<LookupRecord>> (lookahead);
    chain_context_closure_lookup (c,
				  backtrack.len, backtrack.arrayZ,
				  input.lenP1, input.arrayZ,
				  lookahead.len, lookahead.arrayZ,
				  lookup.len, lookup.arrayZ,
				  value,
				  lookup_context);
  }

  void closure_lookups (hb_closure_lookups_context_t *c,
                        ChainContextClosureLookupContext &lookup_context) const
  {
    if (unlikely (c->lookup_limit_exceeded ())) return;
    if (!intersects (c->glyphs, lookup_context)) return;

    const HeadlessArrayOf<HBUINT16> &input = StructAfter<HeadlessArrayOf<HBUINT16>> (backtrack);
    const Array16Of<HBUINT16> &lookahead = StructAfter<Array16Of<HBUINT16>> (input);
    const Array16Of<LookupRecord> &lookup = StructAfter<Array16Of<LookupRecord>> (lookahead);
    recurse_lookups (c, lookup.len, lookup.arrayZ);
  }

  void collect_glyphs (hb_collect_glyphs_context_t *c,
		       ChainContextCollectGlyphsLookupContext &lookup_context) const
  {
    const HeadlessArrayOf<HBUINT16> &input = StructAfter<HeadlessArrayOf<HBUINT16>> (backtrack);
    const Array16Of<HBUINT16> &lookahead = StructAfter<Array16Of<HBUINT16>> (input);
    const Array16Of<LookupRecord> &lookup = StructAfter<Array16Of<LookupRecord>> (lookahead);
    chain_context_collect_glyphs_lookup (c,
					 backtrack.len, backtrack.arrayZ,
					 input.lenP1, input.arrayZ,
					 lookahead.len, lookahead.arrayZ,
					 lookup.len, lookup.arrayZ,
					 lookup_context);
  }

  bool would_apply (hb_would_apply_context_t *c,
		    ChainContextApplyLookupContext &lookup_context) const
  {
    const HeadlessArrayOf<HBUINT16> &input = StructAfter<HeadlessArrayOf<HBUINT16>> (backtrack);
    const Array16Of<HBUINT16> &lookahead = StructAfter<Array16Of<HBUINT16>> (input);
    const Array16Of<LookupRecord> &lookup = StructAfter<Array16Of<LookupRecord>> (lookahead);
    return chain_context_would_apply_lookup (c,
					     backtrack.len, backtrack.arrayZ,
					     input.lenP1, input.arrayZ,
					     lookahead.len, lookahead.arrayZ, lookup.len,
					     lookup.arrayZ, lookup_context);
  }

  bool apply (hb_ot_apply_context_t *c, ChainContextApplyLookupContext &lookup_context) const
  {
    TRACE_APPLY (this);
    const HeadlessArrayOf<HBUINT16> &input = StructAfter<HeadlessArrayOf<HBUINT16>> (backtrack);
    const Array16Of<HBUINT16> &lookahead = StructAfter<Array16Of<HBUINT16>> (input);
    const Array16Of<LookupRecord> &lookup = StructAfter<Array16Of<LookupRecord>> (lookahead);
    return_trace (chain_context_apply_lookup (c,
					      backtrack.len, backtrack.arrayZ,
					      input.lenP1, input.arrayZ,
					      lookahead.len, lookahead.arrayZ, lookup.len,
					      lookup.arrayZ, lookup_context));
  }

  template<typename Iterator,
	   hb_requires (hb_is_iterator (Iterator))>
  void serialize_array (hb_serialize_context_t *c,
			HBUINT16 len,
			Iterator it) const
  {
    c->copy (len);
    for (const auto g : it)
      c->copy ((HBUINT16) g);
  }

  ChainRule* copy (hb_serialize_context_t *c,
		   const hb_map_t *lookup_map,
		   const hb_map_t *backtrack_map,
		   const hb_map_t *input_map = nullptr,
		   const hb_map_t *lookahead_map = nullptr) const
  {
    TRACE_SERIALIZE (this);
    auto *out = c->start_embed (this);
    if (unlikely (!out)) return_trace (nullptr);

    const hb_map_t *mapping = backtrack_map;
    serialize_array (c, backtrack.len, + backtrack.iter ()
				       | hb_map (mapping));

    const HeadlessArrayOf<HBUINT16> &input = StructAfter<HeadlessArrayOf<HBUINT16>> (backtrack);
    if (input_map) mapping = input_map;
    serialize_array (c, input.lenP1, + input.iter ()
				     | hb_map (mapping));

    const Array16Of<HBUINT16> &lookahead = StructAfter<Array16Of<HBUINT16>> (input);
    if (lookahead_map) mapping = lookahead_map;
    serialize_array (c, lookahead.len, + lookahead.iter ()
				       | hb_map (mapping));

    const Array16Of<LookupRecord> &lookupRecord = StructAfter<Array16Of<LookupRecord>> (lookahead);

    HBUINT16* lookupCount = c->embed (&(lookupRecord.len));
    if (!lookupCount) return_trace (nullptr);

    for (unsigned i = 0; i < lookupRecord.len; i++)
    {
      if (!lookup_map->has (lookupRecord[i].lookupListIndex))
      {
        (*lookupCount)--;
        continue;
      }
      if (!c->copy (lookupRecord[i], lookup_map)) return_trace (nullptr);
    }

    return_trace (out);
  }

  bool subset (hb_subset_context_t *c,
	       const hb_map_t *lookup_map,
	       const hb_map_t *backtrack_map = nullptr,
	       const hb_map_t *input_map = nullptr,
	       const hb_map_t *lookahead_map = nullptr) const
  {
    TRACE_SUBSET (this);

    const HeadlessArrayOf<HBUINT16> &input = StructAfter<HeadlessArrayOf<HBUINT16>> (backtrack);
    const Array16Of<HBUINT16> &lookahead = StructAfter<Array16Of<HBUINT16>> (input);

    if (!backtrack_map)
    {
      const hb_set_t &glyphset = *c->plan->glyphset_gsub ();
      if (!hb_all (backtrack, glyphset) ||
	  !hb_all (input, glyphset) ||
	  !hb_all (lookahead, glyphset))
	return_trace (false);

      copy (c->serializer, lookup_map, c->plan->glyph_map);
    }
    else
    {
      if (!hb_all (backtrack, backtrack_map) ||
	  !hb_all (input, input_map) ||
	  !hb_all (lookahead, lookahead_map))
	return_trace (false);

      copy (c->serializer, lookup_map, backtrack_map, input_map, lookahead_map);
    }

    return_trace (true);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (!backtrack.sanitize (c)) return_trace (false);
    const HeadlessArrayOf<HBUINT16> &input = StructAfter<HeadlessArrayOf<HBUINT16>> (backtrack);
    if (!input.sanitize (c)) return_trace (false);
    const Array16Of<HBUINT16> &lookahead = StructAfter<Array16Of<HBUINT16>> (input);
    if (!lookahead.sanitize (c)) return_trace (false);
    const Array16Of<LookupRecord> &lookup = StructAfter<Array16Of<LookupRecord>> (lookahead);
    return_trace (lookup.sanitize (c));
  }

  protected:
  Array16Of<HBUINT16>
		backtrack;		/* Array of backtracking values
					 * (to be matched before the input
					 * sequence) */
  HeadlessArrayOf<HBUINT16>
		inputX;			/* Array of input values (start with
					 * second glyph) */
  Array16Of<HBUINT16>
		lookaheadX;		/* Array of lookahead values's (to be
					 * matched after the input sequence) */
  Array16Of<LookupRecord>
		lookupX;		/* Array of LookupRecords--in
					 * design order) */
  public:
  DEFINE_SIZE_MIN (8);
};

struct ChainRuleSet
{
  bool intersects (const hb_set_t *glyphs, ChainContextClosureLookupContext &lookup_context) const
  {
    return
    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_map ([&] (const ChainRule &_) { return _.intersects (glyphs, lookup_context); })
    | hb_any
    ;
  }
  void closure (hb_closure_context_t *c, unsigned value, ChainContextClosureLookupContext &lookup_context) const
  {
    if (unlikely (c->lookup_limit_exceeded ())) return;

    return
    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const ChainRule &_) { _.closure (c, value, lookup_context); })
    ;
  }

  void closure_lookups (hb_closure_lookups_context_t *c,
                        ChainContextClosureLookupContext &lookup_context) const
  {
    if (unlikely (c->lookup_limit_exceeded ())) return;

    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const ChainRule &_) { _.closure_lookups (c, lookup_context); })
    ;
  }

  void collect_glyphs (hb_collect_glyphs_context_t *c, ChainContextCollectGlyphsLookupContext &lookup_context) const
  {
    return
    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const ChainRule &_) { _.collect_glyphs (c, lookup_context); })
    ;
  }

  bool would_apply (hb_would_apply_context_t *c, ChainContextApplyLookupContext &lookup_context) const
  {
    return
    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_map ([&] (const ChainRule &_) { return _.would_apply (c, lookup_context); })
    | hb_any
    ;
  }

  bool apply (hb_ot_apply_context_t *c, ChainContextApplyLookupContext &lookup_context) const
  {
    TRACE_APPLY (this);
    return_trace (
    + hb_iter (rule)
    | hb_map (hb_add (this))
    | hb_map ([&] (const ChainRule &_) { return _.apply (c, lookup_context); })
    | hb_any
    )
    ;
  }

  bool subset (hb_subset_context_t *c,
	       const hb_map_t *lookup_map,
	       const hb_map_t *backtrack_klass_map = nullptr,
	       const hb_map_t *input_klass_map = nullptr,
	       const hb_map_t *lookahead_klass_map = nullptr) const
  {
    TRACE_SUBSET (this);

    auto snap = c->serializer->snapshot ();
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);

    for (const Offset16To<ChainRule>& _ : rule)
    {
      if (!_) continue;
      auto *o = out->rule.serialize_append (c->serializer);
      if (unlikely (!o)) continue;

      auto o_snap = c->serializer->snapshot ();
      if (!o->serialize_subset (c, _, this,
				lookup_map,
				backtrack_klass_map,
				input_klass_map,
				lookahead_klass_map))
      {
	out->rule.pop ();
	c->serializer->revert (o_snap);
      }
    }

    bool ret = bool (out->rule);
    if (!ret) c->serializer->revert (snap);

    return_trace (ret);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (rule.sanitize (c, this));
  }

  protected:
  Array16OfOffset16To<ChainRule>
		rule;			/* Array of ChainRule tables
					 * ordered by preference */
  public:
  DEFINE_SIZE_ARRAY (2, rule);
};

struct ChainContextFormat1
{
  bool intersects (const hb_set_t *glyphs) const
  {
    struct ChainContextClosureLookupContext lookup_context = {
      {intersects_glyph, intersected_glyph},
      ContextFormat::SimpleContext,
      {nullptr, nullptr, nullptr}
    };

    return
    + hb_zip (this+coverage, ruleSet)
    | hb_filter (*glyphs, hb_first)
    | hb_map (hb_second)
    | hb_map (hb_add (this))
    | hb_map ([&] (const ChainRuleSet &_) { return _.intersects (glyphs, lookup_context); })
    | hb_any
    ;
  }

  bool may_have_non_1to1 () const
  { return true; }

  void closure (hb_closure_context_t *c) const
  {
    c->cur_intersected_glyphs->clear ();
    get_coverage ().intersected_coverage_glyphs (c->parent_active_glyphs (), c->cur_intersected_glyphs);

    struct ChainContextClosureLookupContext lookup_context = {
      {intersects_glyph, intersected_glyph},
      ContextFormat::SimpleContext,
      {nullptr, nullptr, nullptr}
    };

    + hb_zip (this+coverage, hb_range ((unsigned) ruleSet.len))
    | hb_filter (c->parent_active_glyphs (), hb_first)
    | hb_map ([&](const hb_pair_t<hb_codepoint_t, unsigned> _) { return hb_pair_t<unsigned, const ChainRuleSet&> (_.first, this+ruleSet[_.second]); })
    | hb_apply ([&] (const hb_pair_t<unsigned, const ChainRuleSet&>& _) { _.second.closure (c, _.first, lookup_context); })
    ;
  }

  void closure_lookups (hb_closure_lookups_context_t *c) const
  {
    struct ChainContextClosureLookupContext lookup_context = {
      {intersects_glyph, intersected_glyph},
      ContextFormat::SimpleContext,
      {nullptr, nullptr, nullptr}
    };

    + hb_zip (this+coverage, ruleSet)
    | hb_filter (*c->glyphs, hb_first)
    | hb_map (hb_second)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const ChainRuleSet &_) { _.closure_lookups (c, lookup_context); })
    ;
  }

  void collect_variation_indices (hb_collect_variation_indices_context_t *c) const {}

  void collect_glyphs (hb_collect_glyphs_context_t *c) const
  {
    (this+coverage).collect_coverage (c->input);

    struct ChainContextCollectGlyphsLookupContext lookup_context = {
      {collect_glyph},
      {nullptr, nullptr, nullptr}
    };

    + hb_iter (ruleSet)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const ChainRuleSet &_) { _.collect_glyphs (c, lookup_context); })
    ;
  }

  bool would_apply (hb_would_apply_context_t *c) const
  {
    const ChainRuleSet &rule_set = this+ruleSet[(this+coverage).get_coverage (c->glyphs[0])];
    struct ChainContextApplyLookupContext lookup_context = {
      {match_glyph},
      {nullptr, nullptr, nullptr}
    };
    return rule_set.would_apply (c, lookup_context);
  }

  const Coverage &get_coverage () const { return this+coverage; }

  bool apply (hb_ot_apply_context_t *c) const
  {
    TRACE_APPLY (this);
    unsigned int index = (this+coverage).get_coverage (c->buffer->cur().codepoint);
    if (likely (index == NOT_COVERED)) return_trace (false);

    const ChainRuleSet &rule_set = this+ruleSet[index];
    struct ChainContextApplyLookupContext lookup_context = {
      {match_glyph},
      {nullptr, nullptr, nullptr}
    };
    return_trace (rule_set.apply (c, lookup_context));
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    const hb_set_t &glyphset = *c->plan->glyphset_gsub ();
    const hb_map_t &glyph_map = *c->plan->glyph_map;

    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);
    out->format = format;

    const hb_map_t *lookup_map = c->table_tag == HB_OT_TAG_GSUB ? c->plan->gsub_lookups : c->plan->gpos_lookups;
    hb_sorted_vector_t<hb_codepoint_t> new_coverage;
    + hb_zip (this+coverage, ruleSet)
    | hb_filter (glyphset, hb_first)
    | hb_filter (subset_offset_array (c, out->ruleSet, this, lookup_map), hb_second)
    | hb_map (hb_first)
    | hb_map (glyph_map)
    | hb_sink (new_coverage)
    ;

    out->coverage.serialize_serialize (c->serializer, new_coverage.iter ());
    return_trace (bool (new_coverage));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (coverage.sanitize (c, this) && ruleSet.sanitize (c, this));
  }

  protected:
  HBUINT16	format;			/* Format identifier--format = 1 */
  Offset16To<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  Array16OfOffset16To<ChainRuleSet>
		ruleSet;		/* Array of ChainRuleSet tables
					 * ordered by Coverage Index */
  public:
  DEFINE_SIZE_ARRAY (6, ruleSet);
};

struct ChainContextFormat2
{
  bool intersects (const hb_set_t *glyphs) const
  {
    if (!(this+coverage).intersects (glyphs))
      return false;

    const ClassDef &backtrack_class_def = this+backtrackClassDef;
    const ClassDef &input_class_def = this+inputClassDef;
    const ClassDef &lookahead_class_def = this+lookaheadClassDef;

    struct ChainContextClosureLookupContext lookup_context = {
      {intersects_class, intersected_class_glyphs},
      ContextFormat::ClassBasedContext,
      {&backtrack_class_def,
       &input_class_def,
       &lookahead_class_def}
    };

    return
    + hb_iter (ruleSet)
    | hb_map (hb_add (this))
    | hb_enumerate
    | hb_map ([&] (const hb_pair_t<unsigned, const ChainRuleSet &> p)
	      { return input_class_def.intersects_class (glyphs, p.first) &&
		       p.second.intersects (glyphs, lookup_context); })
    | hb_any
    ;
  }

  bool may_have_non_1to1 () const
  { return true; }

  void closure (hb_closure_context_t *c) const
  {
    if (!(this+coverage).intersects (c->glyphs))
      return;

    c->cur_intersected_glyphs->clear ();
    get_coverage ().intersected_coverage_glyphs (c->parent_active_glyphs (), c->cur_intersected_glyphs);

    const ClassDef &backtrack_class_def = this+backtrackClassDef;
    const ClassDef &input_class_def = this+inputClassDef;
    const ClassDef &lookahead_class_def = this+lookaheadClassDef;

    struct ChainContextClosureLookupContext lookup_context = {
      {intersects_class, intersected_class_glyphs},
      ContextFormat::ClassBasedContext,
      {&backtrack_class_def,
       &input_class_def,
       &lookahead_class_def}
    };

    return
    + hb_enumerate (ruleSet)
    | hb_filter ([&] (unsigned _)
		 { return input_class_def.intersects_class (c->cur_intersected_glyphs, _); },
		 hb_first)
    | hb_apply ([&] (const hb_pair_t<unsigned, const Offset16To<ChainRuleSet>&> _)
                {
                  const ChainRuleSet& chainrule_set = this+_.second;
                  chainrule_set.closure (c, _.first, lookup_context);
                })
    ;
  }

  void closure_lookups (hb_closure_lookups_context_t *c) const
  {
    if (!(this+coverage).intersects (c->glyphs))
      return;

    const ClassDef &backtrack_class_def = this+backtrackClassDef;
    const ClassDef &input_class_def = this+inputClassDef;
    const ClassDef &lookahead_class_def = this+lookaheadClassDef;

    struct ChainContextClosureLookupContext lookup_context = {
      {intersects_class, intersected_class_glyphs},
      ContextFormat::ClassBasedContext,
      {&backtrack_class_def,
       &input_class_def,
       &lookahead_class_def}
    };

    + hb_iter (ruleSet)
    | hb_map (hb_add (this))
    | hb_enumerate
    | hb_filter([&] (unsigned klass)
    { return input_class_def.intersects_class (c->glyphs, klass); }, hb_first)
    | hb_map (hb_second)
    | hb_apply ([&] (const ChainRuleSet &_)
    { _.closure_lookups (c, lookup_context); })
    ;
  }

  void collect_variation_indices (hb_collect_variation_indices_context_t *c) const {}

  void collect_glyphs (hb_collect_glyphs_context_t *c) const
  {
    (this+coverage).collect_coverage (c->input);

    const ClassDef &backtrack_class_def = this+backtrackClassDef;
    const ClassDef &input_class_def = this+inputClassDef;
    const ClassDef &lookahead_class_def = this+lookaheadClassDef;

    struct ChainContextCollectGlyphsLookupContext lookup_context = {
      {collect_class},
      {&backtrack_class_def,
       &input_class_def,
       &lookahead_class_def}
    };

    + hb_iter (ruleSet)
    | hb_map (hb_add (this))
    | hb_apply ([&] (const ChainRuleSet &_) { _.collect_glyphs (c, lookup_context); })
    ;
  }

  bool would_apply (hb_would_apply_context_t *c) const
  {
    const ClassDef &backtrack_class_def = this+backtrackClassDef;
    const ClassDef &input_class_def = this+inputClassDef;
    const ClassDef &lookahead_class_def = this+lookaheadClassDef;

    unsigned int index = input_class_def.get_class (c->glyphs[0]);
    const ChainRuleSet &rule_set = this+ruleSet[index];
    struct ChainContextApplyLookupContext lookup_context = {
      {match_class},
      {&backtrack_class_def,
       &input_class_def,
       &lookahead_class_def}
    };
    return rule_set.would_apply (c, lookup_context);
  }

  const Coverage &get_coverage () const { return this+coverage; }

  bool apply (hb_ot_apply_context_t *c) const
  {
    TRACE_APPLY (this);
    unsigned int index = (this+coverage).get_coverage (c->buffer->cur().codepoint);
    if (likely (index == NOT_COVERED)) return_trace (false);

    const ClassDef &backtrack_class_def = this+backtrackClassDef;
    const ClassDef &input_class_def = this+inputClassDef;
    const ClassDef &lookahead_class_def = this+lookaheadClassDef;

    index = input_class_def.get_class (c->buffer->cur().codepoint);
    const ChainRuleSet &rule_set = this+ruleSet[index];
    struct ChainContextApplyLookupContext lookup_context = {
      {match_class},
      {&backtrack_class_def,
       &input_class_def,
       &lookahead_class_def}
    };
    return_trace (rule_set.apply (c, lookup_context));
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);
    out->format = format;
    out->coverage.serialize_subset (c, coverage, this);

    hb_map_t backtrack_klass_map;
    hb_map_t input_klass_map;
    hb_map_t lookahead_klass_map;

    out->backtrackClassDef.serialize_subset (c, backtrackClassDef, this, &backtrack_klass_map);
    // TODO: subset inputClassDef based on glyphs survived in Coverage subsetting
    out->inputClassDef.serialize_subset (c, inputClassDef, this, &input_klass_map);
    out->lookaheadClassDef.serialize_subset (c, lookaheadClassDef, this, &lookahead_klass_map);

    if (unlikely (!c->serializer->propagate_error (backtrack_klass_map,
						   input_klass_map,
						   lookahead_klass_map)))
      return_trace (false);

    int non_zero_index = -1, index = 0;
    bool ret = true;
    const hb_map_t *lookup_map = c->table_tag == HB_OT_TAG_GSUB ? c->plan->gsub_lookups : c->plan->gpos_lookups;
    auto last_non_zero = c->serializer->snapshot ();
    for (const Offset16To<ChainRuleSet>& _ : + hb_enumerate (ruleSet)
					   | hb_filter (input_klass_map, hb_first)
					   | hb_map (hb_second))
    {
      auto *o = out->ruleSet.serialize_append (c->serializer);
      if (unlikely (!o))
      {
	ret = false;
	break;
      }
      if (o->serialize_subset (c, _, this,
			       lookup_map,
			       &backtrack_klass_map,
			       &input_klass_map,
			       &lookahead_klass_map))
      {
        last_non_zero = c->serializer->snapshot ();
	non_zero_index = index;
      }

      index++;
    }

    if (!ret) return_trace (ret);

    // prune empty trailing ruleSets
    if (index > non_zero_index) {
      c->serializer->revert (last_non_zero);
      out->ruleSet.len = non_zero_index + 1;
    }

    return_trace (bool (out->ruleSet));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (coverage.sanitize (c, this) &&
		  backtrackClassDef.sanitize (c, this) &&
		  inputClassDef.sanitize (c, this) &&
		  lookaheadClassDef.sanitize (c, this) &&
		  ruleSet.sanitize (c, this));
  }

  protected:
  HBUINT16	format;			/* Format identifier--format = 2 */
  Offset16To<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  Offset16To<ClassDef>
		backtrackClassDef;	/* Offset to glyph ClassDef table
					 * containing backtrack sequence
					 * data--from beginning of table */
  Offset16To<ClassDef>
		inputClassDef;		/* Offset to glyph ClassDef
					 * table containing input sequence
					 * data--from beginning of table */
  Offset16To<ClassDef>
		lookaheadClassDef;	/* Offset to glyph ClassDef table
					 * containing lookahead sequence
					 * data--from beginning of table */
  Array16OfOffset16To<ChainRuleSet>
		ruleSet;		/* Array of ChainRuleSet tables
					 * ordered by class */
  public:
  DEFINE_SIZE_ARRAY (12, ruleSet);
};

struct ChainContextFormat3
{
  bool intersects (const hb_set_t *glyphs) const
  {
    const Array16OfOffset16To<Coverage> &input = StructAfter<Array16OfOffset16To<Coverage>> (backtrack);

    if (!(this+input[0]).intersects (glyphs))
      return false;

    const Array16OfOffset16To<Coverage> &lookahead = StructAfter<Array16OfOffset16To<Coverage>> (input);
    struct ChainContextClosureLookupContext lookup_context = {
      {intersects_coverage, intersected_coverage_glyphs},
      ContextFormat::CoverageBasedContext,
      {this, this, this}
    };
    return chain_context_intersects (glyphs,
				     backtrack.len, (const HBUINT16 *) backtrack.arrayZ,
				     input.len, (const HBUINT16 *) input.arrayZ + 1,
				     lookahead.len, (const HBUINT16 *) lookahead.arrayZ,
				     lookup_context);
  }

  bool may_have_non_1to1 () const
  { return true; }

  void closure (hb_closure_context_t *c) const
  {
    const Array16OfOffset16To<Coverage> &input = StructAfter<Array16OfOffset16To<Coverage>> (backtrack);

    if (!(this+input[0]).intersects (c->glyphs))
      return;

    c->cur_intersected_glyphs->clear ();
    get_coverage ().intersected_coverage_glyphs (c->parent_active_glyphs (), c->cur_intersected_glyphs);

    const Array16OfOffset16To<Coverage> &lookahead = StructAfter<Array16OfOffset16To<Coverage>> (input);
    const Array16Of<LookupRecord> &lookup = StructAfter<Array16Of<LookupRecord>> (lookahead);
    struct ChainContextClosureLookupContext lookup_context = {
      {intersects_coverage, intersected_coverage_glyphs},
      ContextFormat::CoverageBasedContext,
      {this, this, this}
    };
    chain_context_closure_lookup (c,
				  backtrack.len, (const HBUINT16 *) backtrack.arrayZ,
				  input.len, (const HBUINT16 *) input.arrayZ + 1,
				  lookahead.len, (const HBUINT16 *) lookahead.arrayZ,
				  lookup.len, lookup.arrayZ,
				  0, lookup_context);
  }

  void closure_lookups (hb_closure_lookups_context_t *c) const
  {
    if (!intersects (c->glyphs))
      return;

    const Array16OfOffset16To<Coverage> &input = StructAfter<Array16OfOffset16To<Coverage>> (backtrack);
    const Array16OfOffset16To<Coverage> &lookahead = StructAfter<Array16OfOffset16To<Coverage>> (input);
    const Array16Of<LookupRecord> &lookup = StructAfter<Array16Of<LookupRecord>> (lookahead);
    recurse_lookups (c, lookup.len, lookup.arrayZ);
  }

  void collect_variation_indices (hb_collect_variation_indices_context_t *c) const {}

  void collect_glyphs (hb_collect_glyphs_context_t *c) const
  {
    const Array16OfOffset16To<Coverage> &input = StructAfter<Array16OfOffset16To<Coverage>> (backtrack);

    (this+input[0]).collect_coverage (c->input);

    const Array16OfOffset16To<Coverage> &lookahead = StructAfter<Array16OfOffset16To<Coverage>> (input);
    const Array16Of<LookupRecord> &lookup = StructAfter<Array16Of<LookupRecord>> (lookahead);
    struct ChainContextCollectGlyphsLookupContext lookup_context = {
      {collect_coverage},
      {this, this, this}
    };
    chain_context_collect_glyphs_lookup (c,
					 backtrack.len, (const HBUINT16 *) backtrack.arrayZ,
					 input.len, (const HBUINT16 *) input.arrayZ + 1,
					 lookahead.len, (const HBUINT16 *) lookahead.arrayZ,
					 lookup.len, lookup.arrayZ,
					 lookup_context);
  }

  bool would_apply (hb_would_apply_context_t *c) const
  {
    const Array16OfOffset16To<Coverage> &input = StructAfter<Array16OfOffset16To<Coverage>> (backtrack);
    const Array16OfOffset16To<Coverage> &lookahead = StructAfter<Array16OfOffset16To<Coverage>> (input);
    const Array16Of<LookupRecord> &lookup = StructAfter<Array16Of<LookupRecord>> (lookahead);
    struct ChainContextApplyLookupContext lookup_context = {
      {match_coverage},
      {this, this, this}
    };
    return chain_context_would_apply_lookup (c,
					     backtrack.len, (const HBUINT16 *) backtrack.arrayZ,
					     input.len, (const HBUINT16 *) input.arrayZ + 1,
					     lookahead.len, (const HBUINT16 *) lookahead.arrayZ,
					     lookup.len, lookup.arrayZ, lookup_context);
  }

  const Coverage &get_coverage () const
  {
    const Array16OfOffset16To<Coverage> &input = StructAfter<Array16OfOffset16To<Coverage>> (backtrack);
    return this+input[0];
  }

  bool apply (hb_ot_apply_context_t *c) const
  {
    TRACE_APPLY (this);
    const Array16OfOffset16To<Coverage> &input = StructAfter<Array16OfOffset16To<Coverage>> (backtrack);

    unsigned int index = (this+input[0]).get_coverage (c->buffer->cur().codepoint);
    if (likely (index == NOT_COVERED)) return_trace (false);

    const Array16OfOffset16To<Coverage> &lookahead = StructAfter<Array16OfOffset16To<Coverage>> (input);
    const Array16Of<LookupRecord> &lookup = StructAfter<Array16Of<LookupRecord>> (lookahead);
    struct ChainContextApplyLookupContext lookup_context = {
      {match_coverage},
      {this, this, this}
    };
    return_trace (chain_context_apply_lookup (c,
					      backtrack.len, (const HBUINT16 *) backtrack.arrayZ,
					      input.len, (const HBUINT16 *) input.arrayZ + 1,
					      lookahead.len, (const HBUINT16 *) lookahead.arrayZ,
					      lookup.len, lookup.arrayZ, lookup_context));
  }

  template<typename Iterator,
	   hb_requires (hb_is_iterator (Iterator))>
  bool serialize_coverage_offsets (hb_subset_context_t *c, Iterator it, const void* base) const
  {
    TRACE_SERIALIZE (this);
    auto *out = c->serializer->start_embed<Array16OfOffset16To<Coverage>> ();

    if (unlikely (!c->serializer->allocate_size<HBUINT16> (HBUINT16::static_size)))
      return_trace (false);

    for (auto& offset : it) {
      auto *o = out->serialize_append (c->serializer);
      if (unlikely (!o) || !o->serialize_subset (c, offset, base))
        return_trace (false);
    }

    return_trace (true);
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);

    auto *out = c->serializer->start_embed (this);
    if (unlikely (!out)) return_trace (false);
    if (unlikely (!c->serializer->embed (this->format))) return_trace (false);

    if (!serialize_coverage_offsets (c, backtrack.iter (), this))
      return_trace (false);

    const Array16OfOffset16To<Coverage> &input = StructAfter<Array16OfOffset16To<Coverage>> (backtrack);
    if (!serialize_coverage_offsets (c, input.iter (), this))
      return_trace (false);

    const Array16OfOffset16To<Coverage> &lookahead = StructAfter<Array16OfOffset16To<Coverage>> (input);
    if (!serialize_coverage_offsets (c, lookahead.iter (), this))
      return_trace (false);

    const Array16Of<LookupRecord> &lookupRecord = StructAfter<Array16Of<LookupRecord>> (lookahead);
    HBUINT16 lookupCount;
    lookupCount = lookupRecord.len;
    if (!c->serializer->copy (lookupCount)) return_trace (false);

    const hb_map_t *lookup_map = c->table_tag == HB_OT_TAG_GSUB ? c->plan->gsub_lookups : c->plan->gpos_lookups;
    for (unsigned i = 0; i < (unsigned) lookupCount; i++)
      if (!c->serializer->copy (lookupRecord[i], lookup_map)) return_trace (false);

    return_trace (true);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (!backtrack.sanitize (c, this)) return_trace (false);
    const Array16OfOffset16To<Coverage> &input = StructAfter<Array16OfOffset16To<Coverage>> (backtrack);
    if (!input.sanitize (c, this)) return_trace (false);
    if (!input.len) return_trace (false); /* To be consistent with Context. */
    const Array16OfOffset16To<Coverage> &lookahead = StructAfter<Array16OfOffset16To<Coverage>> (input);
    if (!lookahead.sanitize (c, this)) return_trace (false);
    const Array16Of<LookupRecord> &lookup = StructAfter<Array16Of<LookupRecord>> (lookahead);
    return_trace (lookup.sanitize (c));
  }

  protected:
  HBUINT16	format;			/* Format identifier--format = 3 */
  Array16OfOffset16To<Coverage>
		backtrack;		/* Array of coverage tables
					 * in backtracking sequence, in  glyph
					 * sequence order */
  Array16OfOffset16To<Coverage>
		inputX		;	/* Array of coverage
					 * tables in input sequence, in glyph
					 * sequence order */
  Array16OfOffset16To<Coverage>
		lookaheadX;		/* Array of coverage tables
					 * in lookahead sequence, in glyph
					 * sequence order */
  Array16Of<LookupRecord>
		lookupX;		/* Array of LookupRecords--in
					 * design order) */
  public:
  DEFINE_SIZE_MIN (10);
};

struct ChainContext
{
  template <typename context_t, typename ...Ts>
  typename context_t::return_t dispatch (context_t *c, Ts&&... ds) const
  {
    TRACE_DISPATCH (this, u.format);
    if (unlikely (!c->may_dispatch (this, &u.format))) return_trace (c->no_dispatch_return_value ());
    switch (u.format) {
    case 1: return_trace (c->dispatch (u.format1, hb_forward<Ts> (ds)...));
    case 2: return_trace (c->dispatch (u.format2, hb_forward<Ts> (ds)...));
    case 3: return_trace (c->dispatch (u.format3, hb_forward<Ts> (ds)...));
    default:return_trace (c->default_return_value ());
    }
  }

  protected:
  union {
  HBUINT16		format;	/* Format identifier */
  ChainContextFormat1	format1;
  ChainContextFormat2	format2;
  ChainContextFormat3	format3;
  } u;
};


template <typename T>
struct ExtensionFormat1
{
  unsigned int get_type () const { return extensionLookupType; }

  template <typename X>
  const X& get_subtable () const
  { return this + reinterpret_cast<const Offset32To<typename T::SubTable> &> (extensionOffset); }

  template <typename context_t, typename ...Ts>
  typename context_t::return_t dispatch (context_t *c, Ts&&... ds) const
  {
    TRACE_DISPATCH (this, format);
    if (unlikely (!c->may_dispatch (this, this))) return_trace (c->no_dispatch_return_value ());
    return_trace (get_subtable<typename T::SubTable> ().dispatch (c, get_type (), hb_forward<Ts> (ds)...));
  }

  void collect_variation_indices (hb_collect_variation_indices_context_t *c) const
  { dispatch (c); }

  /* This is called from may_dispatch() above with hb_sanitize_context_t. */
  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  extensionLookupType != T::SubTable::Extension);
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);

    auto *out = c->serializer->start_embed (this);
    if (unlikely (!out || !c->serializer->extend_min (out))) return_trace (false);

    out->format = format;
    out->extensionLookupType = extensionLookupType;

    const auto& src_offset =
        reinterpret_cast<const Offset32To<typename T::SubTable> &> (extensionOffset);
    auto& dest_offset =
        reinterpret_cast<Offset32To<typename T::SubTable> &> (out->extensionOffset);

    return_trace (dest_offset.serialize_subset (c, src_offset, this, get_type ()));
  }

  protected:
  HBUINT16	format;			/* Format identifier. Set to 1. */
  HBUINT16	extensionLookupType;	/* Lookup type of subtable referenced
					 * by ExtensionOffset (i.e. the
					 * extension subtable). */
  Offset32	extensionOffset;	/* Offset to the extension subtable,
					 * of lookup type subtable. */
  public:
  DEFINE_SIZE_STATIC (8);
};

template <typename T>
struct Extension
{
  unsigned int get_type () const
  {
    switch (u.format) {
    case 1: return u.format1.get_type ();
    default:return 0;
    }
  }
  template <typename X>
  const X& get_subtable () const
  {
    switch (u.format) {
    case 1: return u.format1.template get_subtable<typename T::SubTable> ();
    default:return Null (typename T::SubTable);
    }
  }

  // Specialization of dispatch for subset. dispatch() normally just
  // dispatches to the sub table this points too, but for subset
  // we need to run subset on this subtable too.
  template <typename ...Ts>
  typename hb_subset_context_t::return_t dispatch (hb_subset_context_t *c, Ts&&... ds) const
  {
    switch (u.format) {
    case 1: return u.format1.subset (c);
    default: return c->default_return_value ();
    }
  }

  template <typename context_t, typename ...Ts>
  typename context_t::return_t dispatch (context_t *c, Ts&&... ds) const
  {
    TRACE_DISPATCH (this, u.format);
    if (unlikely (!c->may_dispatch (this, &u.format))) return_trace (c->no_dispatch_return_value ());
    switch (u.format) {
    case 1: return_trace (u.format1.dispatch (c, hb_forward<Ts> (ds)...));
    default:return_trace (c->default_return_value ());
    }
  }

  protected:
  union {
  HBUINT16		format;		/* Format identifier */
  ExtensionFormat1<T>	format1;
  } u;
};


/*
 * GSUB/GPOS Common
 */

struct hb_ot_layout_lookup_accelerator_t
{
  template <typename TLookup>
  void init (const TLookup &lookup)
  {
    digest.init ();
    lookup.collect_coverage (&digest);

    subtables.init ();
    OT::hb_get_subtables_context_t c_get_subtables (subtables);
    lookup.dispatch (&c_get_subtables);
  }
  void fini () { subtables.fini (); }

  bool may_have (hb_codepoint_t g) const
  { return digest.may_have (g); }

  bool apply (hb_ot_apply_context_t *c) const
  {
    for (unsigned int i = 0; i < subtables.length; i++)
      if (subtables[i].apply (c))
	return true;
    return false;
  }

  private:
  hb_set_digest_t digest;
  hb_get_subtables_context_t::array_t subtables;
};

struct GSUBGPOS
{
  bool has_data () const { return version.to_int (); }
  unsigned int get_script_count () const
  { return (this+scriptList).len; }
  const Tag& get_script_tag (unsigned int i) const
  { return (this+scriptList).get_tag (i); }
  unsigned int get_script_tags (unsigned int start_offset,
				unsigned int *script_count /* IN/OUT */,
				hb_tag_t     *script_tags /* OUT */) const
  { return (this+scriptList).get_tags (start_offset, script_count, script_tags); }
  const Script& get_script (unsigned int i) const
  { return (this+scriptList)[i]; }
  bool find_script_index (hb_tag_t tag, unsigned int *index) const
  { return (this+scriptList).find_index (tag, index); }

  unsigned int get_feature_count () const
  { return (this+featureList).len; }
  hb_tag_t get_feature_tag (unsigned int i) const
  { return i == Index::NOT_FOUND_INDEX ? HB_TAG_NONE : (this+featureList).get_tag (i); }
  unsigned int get_feature_tags (unsigned int start_offset,
				 unsigned int *feature_count /* IN/OUT */,
				 hb_tag_t     *feature_tags /* OUT */) const
  { return (this+featureList).get_tags (start_offset, feature_count, feature_tags); }
  const Feature& get_feature (unsigned int i) const
  { return (this+featureList)[i]; }
  bool find_feature_index (hb_tag_t tag, unsigned int *index) const
  { return (this+featureList).find_index (tag, index); }

  unsigned int get_lookup_count () const
  { return (this+lookupList).len; }
  const Lookup& get_lookup (unsigned int i) const
  { return (this+lookupList)[i]; }

  bool find_variations_index (const int *coords, unsigned int num_coords,
			      unsigned int *index) const
  {
#ifdef HB_NO_VAR
    *index = FeatureVariations::NOT_FOUND_INDEX;
    return false;
#endif
    return (version.to_int () >= 0x00010001u ? this+featureVars : Null (FeatureVariations))
	    .find_index (coords, num_coords, index);
  }
  const Feature& get_feature_variation (unsigned int feature_index,
					unsigned int variations_index) const
  {
#ifndef HB_NO_VAR
    if (FeatureVariations::NOT_FOUND_INDEX != variations_index &&
	version.to_int () >= 0x00010001u)
    {
      const Feature *feature = (this+featureVars).find_substitute (variations_index,
								   feature_index);
      if (feature)
	return *feature;
    }
#endif
    return get_feature (feature_index);
  }

  void feature_variation_collect_lookups (const hb_set_t *feature_indexes,
					  hb_set_t       *lookup_indexes /* OUT */) const
  {
#ifndef HB_NO_VAR
    if (version.to_int () >= 0x00010001u)
      (this+featureVars).collect_lookups (feature_indexes, lookup_indexes);
#endif
  }

  template <typename TLookup>
  void closure_lookups (hb_face_t      *face,
			const hb_set_t *glyphs,
			hb_set_t       *lookup_indexes /* IN/OUT */) const
  {
    hb_set_t visited_lookups, inactive_lookups;
    OT::hb_closure_lookups_context_t c (face, glyphs, &visited_lookups, &inactive_lookups);

    for (unsigned lookup_index : + hb_iter (lookup_indexes))
      reinterpret_cast<const TLookup &> (get_lookup (lookup_index)).closure_lookups (&c, lookup_index);

    hb_set_union (lookup_indexes, &visited_lookups);
    hb_set_subtract (lookup_indexes, &inactive_lookups);
  }

  void prune_langsys (const hb_map_t *duplicate_feature_map,
                      hb_hashmap_t<unsigned, hb_set_t *, (unsigned)-1, nullptr> *script_langsys_map,
                      hb_set_t       *new_feature_indexes /* OUT */) const
  {
    hb_prune_langsys_context_t c (this, script_langsys_map, duplicate_feature_map, new_feature_indexes);

    unsigned count = get_script_count ();
    for (unsigned script_index = 0; script_index < count; script_index++)
    {
      const Script& s = get_script (script_index);
      s.prune_langsys (&c, script_index);
    }
  }

  template <typename TLookup>
  bool subset (hb_subset_layout_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->subset_context->serializer->embed (*this);
    if (unlikely (!out)) return_trace (false);

    typedef LookupOffsetList<TLookup> TLookupList;
    reinterpret_cast<Offset16To<TLookupList> &> (out->lookupList)
	.serialize_subset (c->subset_context,
			   reinterpret_cast<const Offset16To<TLookupList> &> (lookupList),
			   this,
			   c);

    reinterpret_cast<Offset16To<RecordListOfFeature> &> (out->featureList)
	.serialize_subset (c->subset_context,
			   reinterpret_cast<const Offset16To<RecordListOfFeature> &> (featureList),
			   this,
			   c);

    out->scriptList.serialize_subset (c->subset_context,
				      scriptList,
				      this,
				      c);

#ifndef HB_NO_VAR
    if (version.to_int () >= 0x00010001u)
    {
      bool ret = out->featureVars.serialize_subset (c->subset_context, featureVars, this, c);
      if (!ret)
      {
	out->version.major = 1;
	out->version.minor = 0;
      }
    }
#endif

    return_trace (true);
  }

  void find_duplicate_features (const hb_map_t *lookup_indices,
                                const hb_set_t *feature_indices,
                                hb_map_t *duplicate_feature_map /* OUT */) const
  {
    //find out duplicate features after subset
    unsigned prev = 0xFFFFu;
    for (unsigned i : feature_indices->iter ())
    {
      if (prev == 0xFFFFu)
      {
        duplicate_feature_map->set (i, i);
        prev = i;
        continue;
      }

      hb_tag_t t = get_feature_tag (i);
      hb_tag_t prev_t = get_feature_tag (prev);
      if (t != prev_t)
      {
        duplicate_feature_map->set (i, i);
        prev = i;
        continue;
      }

      const Feature& f = get_feature (i);
      const Feature& prev_f = get_feature (prev);

      auto f_iter =
      + hb_iter (f.lookupIndex)
      | hb_filter (lookup_indices)
      ;

      auto prev_iter =
      + hb_iter (prev_f.lookupIndex)
      | hb_filter (lookup_indices)
      ;

      if (f_iter.len () != prev_iter.len ())
      {
        duplicate_feature_map->set (i, i);
        prev = i;
        continue;
      }

      bool is_equal = true;
      for (auto _ : + hb_zip (f_iter, prev_iter))
        if (_.first != _.second) { is_equal = false; break; }

      if (is_equal == true) duplicate_feature_map->set (i, prev);
      else
      {
        duplicate_feature_map->set (i, i);
        prev = i;
      }
    }
  }

  void prune_features (const hb_map_t *lookup_indices, /* IN */
		       hb_set_t       *feature_indices /* IN/OUT */) const
  {
#ifndef HB_NO_VAR
    // This is the set of feature indices which have alternate versions defined
    // if the FeatureVariation's table and the alternate version(s) intersect the
    // set of lookup indices.
    hb_set_t alternate_feature_indices;
    if (version.to_int () >= 0x00010001u)
      (this+featureVars).closure_features (lookup_indices, &alternate_feature_indices);
    if (unlikely (alternate_feature_indices.in_error()))
    {
      feature_indices->err ();
      return;
    }
#endif

    for (unsigned i : feature_indices->iter())
    {
      const Feature& f = get_feature (i);
      hb_tag_t tag =  get_feature_tag (i);
      if (tag == HB_TAG ('p', 'r', 'e', 'f'))
        // Note: Never ever drop feature 'pref', even if it's empty.
        // HarfBuzz chooses shaper for Khmer based on presence of this
        // feature.	See thread at:
	// http://lists.freedesktop.org/archives/harfbuzz/2012-November/002660.html
        continue;

      if (f.featureParams.is_null ()
	  && !f.intersects_lookup_indexes (lookup_indices)
#ifndef HB_NO_VAR
          && !alternate_feature_indices.has (i)
#endif
	  )
	feature_indices->del (i);
    }
  }

  unsigned int get_size () const
  {
    return min_size +
	   (version.to_int () >= 0x00010001u ? featureVars.static_size : 0);
  }

  template <typename TLookup>
  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    typedef List16OfOffset16To<TLookup> TLookupList;
    if (unlikely (!(version.sanitize (c) &&
		    likely (version.major == 1) &&
		    scriptList.sanitize (c, this) &&
		    featureList.sanitize (c, this) &&
		    reinterpret_cast<const Offset16To<TLookupList> &> (lookupList).sanitize (c, this))))
      return_trace (false);

#ifndef HB_NO_VAR
    if (unlikely (!(version.to_int () < 0x00010001u || featureVars.sanitize (c, this))))
      return_trace (false);
#endif

    return_trace (true);
  }

  template <typename T>
  struct accelerator_t
  {
    void init (hb_face_t *face)
    {
      this->table = hb_sanitize_context_t ().reference_table<T> (face);
      if (unlikely (this->table->is_blocklisted (this->table.get_blob (), face)))
      {
	hb_blob_destroy (this->table.get_blob ());
	this->table = hb_blob_get_empty ();
      }

      this->lookup_count = table->get_lookup_count ();

      this->accels = (hb_ot_layout_lookup_accelerator_t *) hb_calloc (this->lookup_count, sizeof (hb_ot_layout_lookup_accelerator_t));
      if (unlikely (!this->accels))
      {
	this->lookup_count = 0;
	this->table.destroy ();
	this->table = hb_blob_get_empty ();
      }

      for (unsigned int i = 0; i < this->lookup_count; i++)
	this->accels[i].init (table->get_lookup (i));
    }

    void fini ()
    {
      for (unsigned int i = 0; i < this->lookup_count; i++)
	this->accels[i].fini ();
      hb_free (this->accels);
      this->table.destroy ();
    }

    hb_blob_ptr_t<T> table;
    unsigned int lookup_count;
    hb_ot_layout_lookup_accelerator_t *accels;
  };

  protected:
  FixedVersion<>version;	/* Version of the GSUB/GPOS table--initially set
				 * to 0x00010000u */
  Offset16To<ScriptList>
		scriptList;	/* ScriptList table */
  Offset16To<FeatureList>
		featureList;	/* FeatureList table */
  Offset16To<LookupList>
		lookupList;	/* LookupList table */
  Offset32To<FeatureVariations>
		featureVars;	/* Offset to Feature Variations
				   table--from beginning of table
				 * (may be NULL).  Introduced
				 * in version 0x00010001. */
  public:
  DEFINE_SIZE_MIN (10);
};


} /* namespace OT */


#endif /* HB_OT_LAYOUT_GSUBGPOS_HH */
