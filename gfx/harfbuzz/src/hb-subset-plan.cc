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
 * Google Author(s): Garret Rieger, Roderick Sheeter
 */

#include "hb-subset-private.hh"

#include "hb-subset-plan.hh"
#include "hb-ot-cmap-table.hh"
#include "hb-ot-glyf-table.hh"

static int
_hb_codepoint_t_cmp (const void *pa, const void *pb)
{
  hb_codepoint_t a = * (hb_codepoint_t *) pa;
  hb_codepoint_t b = * (hb_codepoint_t *) pb;

  return a < b ? -1 : a > b ? +1 : 0;
}

hb_bool_t
hb_subset_plan_new_gid_for_codepoint (hb_subset_plan_t *plan,
                                      hb_codepoint_t codepoint,
                                      hb_codepoint_t *new_gid)
{
  // TODO actual map, delete this garbage.
  for (unsigned int i = 0; i < plan->codepoints.len; i++)
  {
    if (plan->codepoints[i] != codepoint) continue;
    if (!hb_subset_plan_new_gid_for_old_id(plan, plan->gids_to_retain[i], new_gid))
    {
      return false;
    }
    return true;
  }
  return false;
}

hb_bool_t
hb_subset_plan_new_gid_for_old_id (hb_subset_plan_t *plan,
                                   hb_codepoint_t old_gid,
                                   hb_codepoint_t *new_gid)
{
  // the index in old_gids is the new gid; only up to codepoints.len are valid
  for (unsigned int i = 0; i < plan->gids_to_retain_sorted.len; i++)
  {
    if (plan->gids_to_retain_sorted[i] == old_gid)
    {
      *new_gid = i;
      return true;
    }
  }
  return false;
}

hb_bool_t
hb_subset_plan_add_table (hb_subset_plan_t *plan,
                          hb_tag_t tag,
                          hb_blob_t *contents)
{
  hb_blob_t *source_blob = plan->source->reference_table (tag);
  DEBUG_MSG(SUBSET, nullptr, "add table %c%c%c%c, dest %d bytes, source %d bytes", HB_UNTAG(tag), hb_blob_get_length (contents), hb_blob_get_length (source_blob));
  hb_blob_destroy (source_blob);
  return hb_subset_face_add_table(plan->dest, tag, contents);
}

static void
_populate_codepoints (hb_set_t *input_codepoints,
                      hb_prealloced_array_t<hb_codepoint_t>& plan_codepoints)
{
  plan_codepoints.alloc (hb_set_get_population (input_codepoints));
  hb_codepoint_t cp = -1;
  while (hb_set_next (input_codepoints, &cp)) {
    hb_codepoint_t *wr = plan_codepoints.push();
    *wr = cp;
  }
  plan_codepoints.qsort (_hb_codepoint_t_cmp);
}

static void
_add_gid_and_children (const OT::glyf::accelerator_t &glyf,
		       hb_codepoint_t gid,
		       hb_set_t *gids_to_retain)
{
  if (hb_set_has (gids_to_retain, gid))
    // Already visited this gid, ignore.
    return;

  hb_set_add (gids_to_retain, gid);

  OT::glyf::CompositeGlyphHeader::Iterator composite;
  if (glyf.get_composite (gid, &composite))
  {
    do
    {
      _add_gid_and_children (glyf, (hb_codepoint_t) composite.current->glyphIndex, gids_to_retain);
    } while (composite.move_to_next());
  }
}

static void
_populate_gids_to_retain (hb_face_t *face,
                          hb_prealloced_array_t<hb_codepoint_t>& codepoints,
                          hb_prealloced_array_t<hb_codepoint_t>& old_gids,
                          hb_prealloced_array_t<hb_codepoint_t>& old_gids_sorted)
{
  OT::cmap::accelerator_t cmap;
  OT::glyf::accelerator_t glyf;
  cmap.init (face);
  glyf.init (face);

  hb_auto_array_t<unsigned int> bad_indices;

  old_gids.alloc (codepoints.len);
  for (unsigned int i = 0; i < codepoints.len; i++)
  {
    hb_codepoint_t gid;
    if (!cmap.get_nominal_glyph (codepoints[i], &gid))
    {
      gid = -1;
      *(bad_indices.push ()) = i;
    }
    *(old_gids.push ()) = gid;
  }

  /* Generally there shouldn't be any */
  while (bad_indices.len > 0)
  {
    unsigned int i = bad_indices[bad_indices.len - 1];
    bad_indices.pop ();
    DEBUG_MSG(SUBSET, nullptr, "Drop U+%04X; no gid", codepoints[i]);
    codepoints.remove (i);
    old_gids.remove (i);
  }

  // Populate a full set of glyphs to retain by adding all referenced
  // composite glyphs.
  // TODO expand with glyphs reached by G*
  hb_set_t * all_gids_to_retain = hb_set_create ();
  _add_gid_and_children (glyf, 0, all_gids_to_retain);
  for (unsigned int i = 0; i < old_gids.len; i++)
    _add_gid_and_children (glyf, old_gids[i], all_gids_to_retain);

  // Transfer to a sorted list.
  old_gids_sorted.alloc (hb_set_get_population (all_gids_to_retain));
  hb_codepoint_t gid = HB_SET_VALUE_INVALID;
  while (hb_set_next (all_gids_to_retain, &gid))
    *(old_gids_sorted.push ()) = gid;

  hb_set_destroy (all_gids_to_retain);
  glyf.fini ();
  cmap.fini ();
}

/**
 * hb_subset_plan_create:
 * Computes a plan for subsetting the supplied face according
 * to a provide profile and input. The plan describes
 * which tables and glyphs should be retained.
 *
 * Return value: New subset plan.
 *
 * Since: 1.7.5
 **/
hb_subset_plan_t *
hb_subset_plan_create (hb_face_t           *face,
                       hb_subset_profile_t *profile,
                       hb_subset_input_t   *input)
{
  hb_subset_plan_t *plan = hb_object_create<hb_subset_plan_t> ();

  plan->codepoints.init();
  plan->gids_to_retain.init();
  plan->gids_to_retain_sorted.init();
  plan->source = hb_face_reference (face);
  plan->dest = hb_subset_face_create ();
  plan->drop_hints = input->drop_hints;

  _populate_codepoints (input->unicodes, plan->codepoints);
  _populate_gids_to_retain (face,
                            plan->codepoints,
                            plan->gids_to_retain,
                            plan->gids_to_retain_sorted);

  return plan;
}

/**
 * hb_subset_plan_destroy:
 *
 * Since: 1.7.5
 **/
void
hb_subset_plan_destroy (hb_subset_plan_t *plan)
{
  if (!hb_object_destroy (plan)) return;

  plan->codepoints.finish ();
  plan->gids_to_retain.finish ();
  plan->gids_to_retain_sorted.finish ();

  hb_face_destroy (plan->source);
  hb_face_destroy (plan->dest);

  free (plan);
}
