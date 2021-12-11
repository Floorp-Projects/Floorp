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
 * Google Author(s): Garret Rieger, Rod Sheeter, Behdad Esfahbod
 */

#include "hb-subset.hh"
#include "hb-set.hh"

/**
 * hb_subset_input_create_or_fail:
 *
 * Creates a new subset input object.
 *
 * Return value: (transfer full): New subset input, or %NULL if failed. Destroy
 * with hb_subset_input_destroy().
 *
 * Since: 1.8.0
 **/
hb_subset_input_t *
hb_subset_input_create_or_fail (void)
{
  hb_subset_input_t *input = hb_object_create<hb_subset_input_t>();

  if (unlikely (!input))
    return nullptr;

  for (auto& set : input->sets_iter ())
    set = hb_set_create ();

  if (input->in_error ())
  {
    hb_subset_input_destroy (input);
    return nullptr;
  }

  input->flags = HB_SUBSET_FLAGS_DEFAULT;

  hb_set_add_range (input->sets.name_ids, 0, 6);
  hb_set_add (input->sets.name_languages, 0x0409);

  hb_tag_t default_drop_tables[] = {
    // Layout disabled by default
    HB_TAG ('m', 'o', 'r', 'x'),
    HB_TAG ('m', 'o', 'r', 't'),
    HB_TAG ('k', 'e', 'r', 'x'),
    HB_TAG ('k', 'e', 'r', 'n'),

    // Copied from fontTools:
    HB_TAG ('B', 'A', 'S', 'E'),
    HB_TAG ('J', 'S', 'T', 'F'),
    HB_TAG ('D', 'S', 'I', 'G'),
    HB_TAG ('E', 'B', 'D', 'T'),
    HB_TAG ('E', 'B', 'L', 'C'),
    HB_TAG ('E', 'B', 'S', 'C'),
    HB_TAG ('S', 'V', 'G', ' '),
    HB_TAG ('P', 'C', 'L', 'T'),
    HB_TAG ('L', 'T', 'S', 'H'),
    // Graphite tables
    HB_TAG ('F', 'e', 'a', 't'),
    HB_TAG ('G', 'l', 'a', 't'),
    HB_TAG ('G', 'l', 'o', 'c'),
    HB_TAG ('S', 'i', 'l', 'f'),
    HB_TAG ('S', 'i', 'l', 'l'),
  };
  input->sets.drop_tables->add_array (default_drop_tables, ARRAY_LENGTH (default_drop_tables));

  hb_tag_t default_no_subset_tables[] = {
    HB_TAG ('a', 'v', 'a', 'r'),
    HB_TAG ('f', 'v', 'a', 'r'),
    HB_TAG ('g', 'a', 's', 'p'),
    HB_TAG ('c', 'v', 't', ' '),
    HB_TAG ('f', 'p', 'g', 'm'),
    HB_TAG ('p', 'r', 'e', 'p'),
    HB_TAG ('V', 'D', 'M', 'X'),
    HB_TAG ('D', 'S', 'I', 'G'),
    HB_TAG ('M', 'V', 'A', 'R'),
    HB_TAG ('c', 'v', 'a', 'r'),
    HB_TAG ('S', 'T', 'A', 'T'),
  };
  input->sets.no_subset_tables->add_array (default_no_subset_tables,
                                         ARRAY_LENGTH (default_no_subset_tables));

  //copied from _layout_features_groups in fonttools
  hb_tag_t default_layout_features[] = {
    // default shaper
    // common
    HB_TAG ('r', 'v', 'r', 'n'),
    HB_TAG ('c', 'c', 'm', 'p'),
    HB_TAG ('l', 'i', 'g', 'a'),
    HB_TAG ('l', 'o', 'c', 'l'),
    HB_TAG ('m', 'a', 'r', 'k'),
    HB_TAG ('m', 'k', 'm', 'k'),
    HB_TAG ('r', 'l', 'i', 'g'),

    //fractions
    HB_TAG ('f', 'r', 'a', 'c'),
    HB_TAG ('n', 'u', 'm', 'r'),
    HB_TAG ('d', 'n', 'o', 'm'),

    //horizontal
    HB_TAG ('c', 'a', 'l', 't'),
    HB_TAG ('c', 'l', 'i', 'g'),
    HB_TAG ('c', 'u', 'r', 's'),
    HB_TAG ('k', 'e', 'r', 'n'),
    HB_TAG ('r', 'c', 'l', 't'),

    //vertical
    HB_TAG ('v', 'a', 'l', 't'),
    HB_TAG ('v', 'e', 'r', 't'),
    HB_TAG ('v', 'k', 'r', 'n'),
    HB_TAG ('v', 'p', 'a', 'l'),
    HB_TAG ('v', 'r', 't', '2'),

    //ltr
    HB_TAG ('l', 't', 'r', 'a'),
    HB_TAG ('l', 't', 'r', 'm'),

    //rtl
    HB_TAG ('r', 't', 'l', 'a'),
    HB_TAG ('r', 't', 'l', 'm'),

    //Complex shapers
    //arabic
    HB_TAG ('i', 'n', 'i', 't'),
    HB_TAG ('m', 'e', 'd', 'i'),
    HB_TAG ('f', 'i', 'n', 'a'),
    HB_TAG ('i', 's', 'o', 'l'),
    HB_TAG ('m', 'e', 'd', '2'),
    HB_TAG ('f', 'i', 'n', '2'),
    HB_TAG ('f', 'i', 'n', '3'),
    HB_TAG ('c', 's', 'w', 'h'),
    HB_TAG ('m', 's', 'e', 't'),
    HB_TAG ('s', 't', 'c', 'h'),

    //hangul
    HB_TAG ('l', 'j', 'm', 'o'),
    HB_TAG ('v', 'j', 'm', 'o'),
    HB_TAG ('t', 'j', 'm', 'o'),

    //tibetan
    HB_TAG ('a', 'b', 'v', 's'),
    HB_TAG ('b', 'l', 'w', 's'),
    HB_TAG ('a', 'b', 'v', 'm'),
    HB_TAG ('b', 'l', 'w', 'm'),

    //indic
    HB_TAG ('n', 'u', 'k', 't'),
    HB_TAG ('a', 'k', 'h', 'n'),
    HB_TAG ('r', 'p', 'h', 'f'),
    HB_TAG ('r', 'k', 'r', 'f'),
    HB_TAG ('p', 'r', 'e', 'f'),
    HB_TAG ('b', 'l', 'w', 'f'),
    HB_TAG ('h', 'a', 'l', 'f'),
    HB_TAG ('a', 'b', 'v', 'f'),
    HB_TAG ('p', 's', 't', 'f'),
    HB_TAG ('c', 'f', 'a', 'r'),
    HB_TAG ('v', 'a', 't', 'u'),
    HB_TAG ('c', 'j', 'c', 't'),
    HB_TAG ('i', 'n', 'i', 't'),
    HB_TAG ('p', 'r', 'e', 's'),
    HB_TAG ('a', 'b', 'v', 's'),
    HB_TAG ('b', 'l', 'w', 's'),
    HB_TAG ('p', 's', 't', 's'),
    HB_TAG ('h', 'a', 'l', 'n'),
    HB_TAG ('d', 'i', 's', 't'),
    HB_TAG ('a', 'b', 'v', 'm'),
    HB_TAG ('b', 'l', 'w', 'm'),
  };

  input->sets.layout_features->add_array (default_layout_features, ARRAY_LENGTH (default_layout_features));

  if (input->in_error ())
  {
    hb_subset_input_destroy (input);
    return nullptr;
  }
  return input;
}

/**
 * hb_subset_input_reference: (skip)
 * @input: a #hb_subset_input_t object.
 *
 * Increases the reference count on @input.
 *
 * Return value: @input.
 *
 * Since: 1.8.0
 **/
hb_subset_input_t *
hb_subset_input_reference (hb_subset_input_t *input)
{
  return hb_object_reference (input);
}

/**
 * hb_subset_input_destroy:
 * @input: a #hb_subset_input_t object.
 *
 * Decreases the reference count on @input, and if it reaches zero, destroys
 * @input, freeing all memory.
 *
 * Since: 1.8.0
 **/
void
hb_subset_input_destroy (hb_subset_input_t *input)
{
  if (!hb_object_destroy (input)) return;

  for (hb_set_t* set : input->sets_iter ())
    hb_set_destroy (set);

  hb_free (input);
}

/**
 * hb_subset_input_unicode_set:
 * @input: a #hb_subset_input_t object.
 *
 * Gets the set of Unicode code points to retain, the caller should modify the
 * set as needed.
 *
 * Return value: (transfer none): pointer to the #hb_set_t of Unicode code
 * points.
 *
 * Since: 1.8.0
 **/
HB_EXTERN hb_set_t *
hb_subset_input_unicode_set (hb_subset_input_t *input)
{
  return input->sets.unicodes;
}

/**
 * hb_subset_input_glyph_set:
 * @input: a #hb_subset_input_t object.
 *
 * Gets the set of glyph IDs to retain, the caller should modify the set as
 * needed.
 *
 * Return value: (transfer none): pointer to the #hb_set_t of glyph IDs.
 *
 * Since: 1.8.0
 **/
HB_EXTERN hb_set_t *
hb_subset_input_glyph_set (hb_subset_input_t *input)
{
  return input->sets.glyphs;
}

/**
 * hb_subset_input_nameid_set:
 * @input: a #hb_subset_input_t object.
 *
 * Gets the set of name table name IDs to retain, the caller should modify the
 * set as needed.
 *
 * Return value: (transfer none): pointer to the #hb_set_t of name IDs.
 *
 * Since: 2.9.0
 **/
HB_EXTERN hb_set_t *
hb_subset_input_nameid_set (hb_subset_input_t *input)
{
  return input->sets.name_ids;
}

/**
 * hb_subset_input_namelangid_set:
 * @input: a #hb_subset_input_t object.
 *
 * Gets the set of name table language IDs to retain, the caller should modify
 * the set as needed.
 *
 * Return value: (transfer none): pointer to the #hb_set_t of language IDs.
 *
 * Since: 2.9.0
 **/
HB_EXTERN hb_set_t *
hb_subset_input_namelangid_set (hb_subset_input_t *input)
{
  return input->sets.name_languages;
}


/**
 * hb_subset_input_layout_features_set:
 * @input: a #hb_subset_input_t object.
 *
 * Gets the set of layout feature tags to retain, the caller should modify the
 * set as needed.
 *
 * Return value: (transfer none): pointer to the #hb_set_t of feature tags.
 *
 * Since: 2.9.0
 **/
HB_EXTERN hb_set_t *
hb_subset_input_layout_features_set (hb_subset_input_t *input)
{
  return input->sets.layout_features;
}

/**
 * hb_subset_input_drop_tables_set:
 * @input: a #hb_subset_input_t object.
 *
 * Gets the set of table tags to drop, the caller should modify the set as
 * needed.
 *
 * Return value: (transfer none): pointer to the #hb_set_t of table tags.
 *
 * Since: 2.9.0
 **/
HB_EXTERN hb_set_t *
hb_subset_input_drop_tables_set (hb_subset_input_t *input)
{
  return input->sets.drop_tables;
}

/**
 * hb_subset_input_set:
 * @input: a #hb_subset_input_t object.
 * @set_type: a #hb_subset_sets_t set type.
 *
 * Gets the set of the specified type.
 *
 * Return value: (transfer none): pointer to the #hb_set_t of the specified type.
 *
 * Since: 2.9.1
 **/
HB_EXTERN hb_set_t *
hb_subset_input_set (hb_subset_input_t *input, hb_subset_sets_t set_type)
{
  return input->sets_iter () [set_type];
}

/**
 * hb_subset_input_no_subset_tables_set:
 * @input: a #hb_subset_input_t object.
 *
 * Gets the set of table tags which specifies tables that should not be
 * subsetted, the caller should modify the set as needed.
 *
 * Return value: (transfer none): pointer to the #hb_set_t of table tags.
 *
 * Since: 2.9.0
 **/
HB_EXTERN hb_set_t *
hb_subset_input_no_subset_tables_set (hb_subset_input_t *input)
{
  return input->sets.no_subset_tables;
}


/**
 * hb_subset_input_get_flags:
 * @input: a #hb_subset_input_t object.
 *
 * Gets all of the subsetting flags in the input object.
 *
 * Return value: the subsetting flags bit field.
 *
 * Since: 2.9.0
 **/
HB_EXTERN hb_subset_flags_t
hb_subset_input_get_flags (hb_subset_input_t *input)
{
  return (hb_subset_flags_t) input->flags;
}

/**
 * hb_subset_input_set_flags:
 * @input: a #hb_subset_input_t object.
 * @value: bit field of flags
 *
 * Sets all of the flags in the input object to the values specified by the bit
 * field.
 *
 * Since: 2.9.0
 **/
HB_EXTERN void
hb_subset_input_set_flags (hb_subset_input_t *input,
			   unsigned value)
{
  input->flags = (hb_subset_flags_t) value;
}

/**
 * hb_subset_input_set_user_data: (skip)
 * @input: a #hb_subset_input_t object.
 * @key: The user-data key to set
 * @data: A pointer to the user data
 * @destroy: (nullable): A callback to call when @data is not needed anymore
 * @replace: Whether to replace an existing data with the same key
 *
 * Attaches a user-data key/data pair to the given subset input object.
 *
 * Return value: %true if success, %false otherwise
 *
 * Since: 2.9.0
 **/
hb_bool_t
hb_subset_input_set_user_data (hb_subset_input_t  *input,
			       hb_user_data_key_t *key,
			       void *		   data,
			       hb_destroy_func_t   destroy,
			       hb_bool_t	   replace)
{
  return hb_object_set_user_data (input, key, data, destroy, replace);
}

/**
 * hb_subset_input_get_user_data: (skip)
 * @input: a #hb_subset_input_t object.
 * @key: The user-data key to query
 *
 * Fetches the user data associated with the specified key,
 * attached to the specified subset input object.
 *
 * Return value: (transfer none): A pointer to the user data
 *
 * Since: 2.9.0
 **/
void *
hb_subset_input_get_user_data (const hb_subset_input_t *input,
			       hb_user_data_key_t     *key)
{
  return hb_object_get_user_data (input, key);
}


static void set_flag_value (hb_subset_input_t *input, hb_subset_flags_t flag, hb_bool_t value)
{
  hb_subset_input_set_flags (input,
                             value
                             ? hb_subset_input_get_flags (input) | flag
                             : hb_subset_input_get_flags (input) & ~flag);
}

void
hb_subset_input_set_drop_hints (hb_subset_input_t *subset_input,
				hb_bool_t drop_hints)
{
  return set_flag_value (subset_input,
                         HB_SUBSET_FLAGS_NO_HINTING,
                         drop_hints);
}

hb_bool_t
hb_subset_input_get_drop_hints (hb_subset_input_t *subset_input)
{
  return (bool) (hb_subset_input_get_flags (subset_input) & HB_SUBSET_FLAGS_NO_HINTING);
}

void
hb_subset_input_set_desubroutinize (hb_subset_input_t *subset_input,
				    hb_bool_t desubroutinize)
{
  return set_flag_value (subset_input,
                         HB_SUBSET_FLAGS_DESUBROUTINIZE,
                         desubroutinize);
}

hb_bool_t
hb_subset_input_get_desubroutinize (hb_subset_input_t *subset_input)
{
  return (bool) (hb_subset_input_get_flags (subset_input) & HB_SUBSET_FLAGS_DESUBROUTINIZE);
}

void
hb_subset_input_set_retain_gids (hb_subset_input_t *subset_input,
				 hb_bool_t retain_gids)
{
  return set_flag_value (subset_input,
                         HB_SUBSET_FLAGS_RETAIN_GIDS,
                         retain_gids);
}

hb_bool_t
hb_subset_input_get_retain_gids (hb_subset_input_t *subset_input)
{
  return (bool) (hb_subset_input_get_flags (subset_input) & HB_SUBSET_FLAGS_RETAIN_GIDS);
}

void
hb_subset_input_set_name_legacy (hb_subset_input_t *subset_input,
				 hb_bool_t name_legacy)
{
  return set_flag_value (subset_input,
                         HB_SUBSET_FLAGS_NAME_LEGACY,
                         name_legacy);
}

hb_bool_t
hb_subset_input_get_name_legacy (hb_subset_input_t *subset_input)
{
  return (bool) (hb_subset_input_get_flags (subset_input) & HB_SUBSET_FLAGS_NAME_LEGACY);
}

void
hb_subset_input_set_overlaps_flag (hb_subset_input_t *subset_input,
                                   hb_bool_t overlaps_flag)
{
  return set_flag_value (subset_input,
                         HB_SUBSET_FLAGS_SET_OVERLAPS_FLAG,
                         overlaps_flag);
}

hb_bool_t
hb_subset_input_get_overlaps_flag (hb_subset_input_t *subset_input)
{
  return (bool) (hb_subset_input_get_flags (subset_input) & HB_SUBSET_FLAGS_SET_OVERLAPS_FLAG);
}

void
hb_subset_input_set_notdef_outline (hb_subset_input_t *subset_input,
                                    hb_bool_t notdef_outline)
{
  return set_flag_value (subset_input,
                         HB_SUBSET_FLAGS_NOTDEF_OUTLINE,
                         notdef_outline);
}

hb_bool_t
hb_subset_input_get_notdef_outline (hb_subset_input_t *subset_input)
{
  return (bool) (hb_subset_input_get_flags (subset_input) & HB_SUBSET_FLAGS_NOTDEF_OUTLINE);
}

void
hb_subset_input_set_no_prune_unicode_ranges (hb_subset_input_t *subset_input,
                                             hb_bool_t no_prune_unicode_ranges)
{
  return set_flag_value (subset_input,
                         HB_SUBSET_FLAGS_NO_PRUNE_UNICODE_RANGES,
                         no_prune_unicode_ranges);
}


hb_bool_t
hb_subset_input_get_no_prune_unicode_ranges (hb_subset_input_t *subset_input)
{
  return (bool) (hb_subset_input_get_flags (subset_input) & HB_SUBSET_FLAGS_NO_PRUNE_UNICODE_RANGES);
}
