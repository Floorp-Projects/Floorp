/*
 * Copyright © 2015  Google, Inc.
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
 * Google Author(s): Behdad Esfahbod, Garret Rieger, Roderick Sheeter
 */

#ifndef HB_OT_GLYF_TABLE_HH
#define HB_OT_GLYF_TABLE_HH

#include "hb-open-type.hh"
#include "hb-ot-head-table.hh"

namespace OT {


/*
 * loca -- Index to Location
 * https://docs.microsoft.com/en-us/typography/opentype/spec/loca
 */
#define HB_OT_TAG_loca HB_TAG('l','o','c','a')


struct loca
{
  friend struct glyf;

  static constexpr hb_tag_t tableTag = HB_OT_TAG_loca;

  bool sanitize (hb_sanitize_context_t *c HB_UNUSED) const
  {
    TRACE_SANITIZE (this);
    return_trace (true);
  }

  protected:
  UnsizedArrayOf<HBUINT8>	dataZ;		/* Location data. */
  public:
  DEFINE_SIZE_MIN (0); /* In reality, this is UNBOUNDED() type; but since we always
			* check the size externally, allow Null() object of it by
			* defining it _MIN instead. */
};


/*
 * glyf -- TrueType Glyph Data
 * https://docs.microsoft.com/en-us/typography/opentype/spec/glyf
 */
#define HB_OT_TAG_glyf HB_TAG('g','l','y','f')


struct glyf
{
  static constexpr hb_tag_t tableTag = HB_OT_TAG_glyf;

  bool sanitize (hb_sanitize_context_t *c HB_UNUSED) const
  {
    TRACE_SANITIZE (this);
    /* We don't check for anything specific here.  The users of the
     * struct do all the hard work... */
    return_trace (true);
  }

  template<typename Iterator,
	   hb_requires (hb_is_source_of (Iterator, unsigned int))>
  static bool
  _add_loca_and_head (hb_subset_plan_t * plan, Iterator padded_offsets)
  {
    unsigned max_offset = + padded_offsets | hb_reduce(hb_add, 0);
    unsigned num_offsets = padded_offsets.len () + 1;
    bool use_short_loca = max_offset < 0x1FFFF;
    unsigned entry_size = use_short_loca ? 2 : 4;
    char *loca_prime_data = (char *) calloc (entry_size, num_offsets);

    if (unlikely (!loca_prime_data)) return false;

    DEBUG_MSG(SUBSET, nullptr, "loca entry_size %d num_offsets %d max_offset %d size %d", entry_size, num_offsets, max_offset, entry_size * num_offsets);

    if (use_short_loca)
      _write_loca (padded_offsets, 1, hb_array ((HBUINT16*) loca_prime_data, num_offsets));
    else
      _write_loca (padded_offsets, 0, hb_array ((HBUINT32*) loca_prime_data, num_offsets));

    hb_blob_t * loca_blob = hb_blob_create (loca_prime_data,
					    entry_size * num_offsets,
					    HB_MEMORY_MODE_WRITABLE,
					    loca_prime_data,
					    free);

    bool result = plan->add_table (HB_OT_TAG_loca, loca_blob)
		  && _add_head_and_set_loca_version(plan, use_short_loca);

    hb_blob_destroy (loca_blob);
    return result;
  }

  template<typename IteratorIn, typename IteratorOut,
	   hb_requires (hb_is_source_of (IteratorIn, unsigned int)),
	   hb_requires (hb_is_sink_of (IteratorOut, unsigned))>
  static void
  _write_loca (IteratorIn it, unsigned right_shift, IteratorOut dest)
  {
    unsigned int offset = 0;
    dest << 0;
    + it
    | hb_map ([=, &offset] (unsigned int padded_size) {
      offset += padded_size;
      DEBUG_MSG(SUBSET, nullptr, "loca entry offset %d", offset);
      return offset >> right_shift;
    })
    | hb_sink (dest)
    ;
  }

  // requires source of SubsetGlyph complains the identifier isn't declared
  template <typename Iterator>
  bool serialize(hb_serialize_context_t *c,
		 Iterator it,
		 const hb_subset_plan_t *plan)
  {
    TRACE_SERIALIZE (this);

    + it
    | hb_apply ([=] (const SubsetGlyph& _) { _.serialize (c, plan); })
    ;

    return_trace (true);
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);

    glyf *glyf_prime = c->serializer->start_embed <glyf> ();
    if (unlikely (!c->serializer->check_success (glyf_prime))) return_trace (false);

    // Byte region(s) per glyph to output
    // unpadded, hints removed if so requested
    // If we fail to process a glyph we produce an empty (0-length) glyph
    hb_vector_t<SubsetGlyph> glyphs;
    _populate_subset_glyphs (c->plan, &glyphs);

    glyf_prime->serialize (c->serializer, hb_iter (glyphs), c->plan);

    auto padded_offsets =
    + hb_iter (glyphs)
    | hb_map (&SubsetGlyph::padded_size)
    ;

    if (c->serializer->in_error ()) return_trace (false);
    return_trace (c->serializer->check_success (_add_loca_and_head (c->plan, padded_offsets)));
  }

  template <typename SubsetGlyph>
  void
  _populate_subset_glyphs (const hb_subset_plan_t * plan,
			   hb_vector_t<SubsetGlyph> * glyphs /* OUT */) const
  {
    OT::glyf::accelerator_t glyf;
    glyf.init (plan->source);

    + hb_range (plan->num_output_glyphs ())
    | hb_map ([&] (hb_codepoint_t new_gid) {
      SubsetGlyph subset_glyph;
      subset_glyph.new_gid = new_gid;

      // should never fail: all old gids should be mapped
      if (!plan->old_gid_for_new_gid (new_gid, &subset_glyph.old_gid)) return subset_glyph;

      subset_glyph.source_glyph = glyf.bytes_for_glyph ((const char *) this, subset_glyph.old_gid);
      if (plan->drop_hints) subset_glyph.drop_hints (glyf);
      else subset_glyph.dest_start = subset_glyph.source_glyph;

      return subset_glyph;
    })
    | hb_sink (glyphs)
    ;

    glyf.fini();
  }

  static void
  _fix_component_gids (const hb_subset_plan_t *plan,
		       hb_bytes_t glyph)
  {
    OT::glyf::CompositeGlyphHeader::Iterator iterator;
    if (OT::glyf::CompositeGlyphHeader::get_iterator (&glyph,
						      glyph.length,
						      &iterator))
    {
      do
      {
	hb_codepoint_t new_gid;
	if (!plan->new_gid_for_old_gid (iterator.current->glyphIndex,
					&new_gid))
	  continue;
	((OT::glyf::CompositeGlyphHeader *) iterator.current)->glyphIndex = new_gid;
      } while (iterator.move_to_next ());
    }
  }

  static void
  _zero_instruction_length (hb_bytes_t glyph)
  {
    const GlyphHeader &glyph_header = StructAtOffset<GlyphHeader> (&glyph, 0);
    int16_t num_contours = (int16_t) glyph_header.numberOfContours;
    if (num_contours <= 0) return;  // only for simple glyphs

    const HBUINT16 &instruction_length = StructAtOffset<HBUINT16> (&glyph, GlyphHeader::static_size + 2 * num_contours);
    (HBUINT16 &) instruction_length = 0;
  }

  static bool _remove_composite_instruction_flag (hb_bytes_t glyph)
  {
    const GlyphHeader &glyph_header = StructAtOffset<GlyphHeader> (&glyph, 0);
    if (glyph_header.numberOfContours >= 0) return true;  // only for composites

    /* remove WE_HAVE_INSTRUCTIONS from flags in dest */
    OT::glyf::CompositeGlyphHeader::Iterator composite_it;
    if (unlikely (!OT::glyf::CompositeGlyphHeader::get_iterator (&glyph, glyph.length, &composite_it))) return false;
    const OT::glyf::CompositeGlyphHeader *composite_header;
    do {
      composite_header = composite_it.current;
      OT::HBUINT16 *flags = const_cast<OT::HBUINT16 *> (&composite_header->flags);
      *flags = (uint16_t) *flags & ~OT::glyf::CompositeGlyphHeader::WE_HAVE_INSTRUCTIONS;
    } while (composite_it.move_to_next ());
    return true;
}

  static bool
  _add_head_and_set_loca_version (hb_subset_plan_t *plan, bool use_short_loca)
  {
    hb_blob_t *head_blob = hb_sanitize_context_t ().reference_table<head> (plan->source);
    hb_blob_t *head_prime_blob = hb_blob_copy_writable_or_fail (head_blob);
    hb_blob_destroy (head_blob);

    if (unlikely (!head_prime_blob))
      return false;

    head *head_prime = (head *) hb_blob_get_data_writable (head_prime_blob, nullptr);
    head_prime->indexToLocFormat = use_short_loca ? 0 : 1;
    bool success = plan->add_table (HB_OT_TAG_head, head_prime_blob);

    hb_blob_destroy (head_prime_blob);
    return success;
  }

  struct GlyphHeader
  {
    HBINT16		numberOfContours;	/* If the number of contours is
						 * greater than or equal to zero,
						 * this is a simple glyph; if negative,
						 * this is a composite glyph. */
    FWORD		xMin;			/* Minimum x for coordinate data. */
    FWORD		yMin;			/* Minimum y for coordinate data. */
    FWORD		xMax;			/* Maximum x for coordinate data. */
    FWORD		yMax;			/* Maximum y for coordinate data. */

    DEFINE_SIZE_STATIC (10);
  };

  struct CompositeGlyphHeader
  {
    enum composite_glyph_flag_t {
      ARG_1_AND_2_ARE_WORDS =      0x0001,
      ARGS_ARE_XY_VALUES =         0x0002,
      ROUND_XY_TO_GRID =           0x0004,
      WE_HAVE_A_SCALE =            0x0008,
      MORE_COMPONENTS =            0x0020,
      WE_HAVE_AN_X_AND_Y_SCALE =   0x0040,
      WE_HAVE_A_TWO_BY_TWO =       0x0080,
      WE_HAVE_INSTRUCTIONS =       0x0100,
      USE_MY_METRICS =             0x0200,
      OVERLAP_COMPOUND =           0x0400,
      SCALED_COMPONENT_OFFSET =    0x0800,
      UNSCALED_COMPONENT_OFFSET =  0x1000
    };

    HBUINT16 flags;
    GlyphID  glyphIndex;

    unsigned int get_size () const
    {
      unsigned int size = min_size;
      // arg1 and 2 are int16
      if (flags & ARG_1_AND_2_ARE_WORDS) size += 4;
      // arg1 and 2 are int8
      else size += 2;

      // One x 16 bit (scale)
      if (flags & WE_HAVE_A_SCALE) size += 2;
      // Two x 16 bit (xscale, yscale)
      else if (flags & WE_HAVE_AN_X_AND_Y_SCALE) size += 4;
      // Four x 16 bit (xscale, scale01, scale10, yscale)
      else if (flags & WE_HAVE_A_TWO_BY_TWO) size += 8;

      return size;
    }

    // TODO rewrite using new iterator framework if possible
    struct Iterator
    {
      const char *glyph_start;
      const char *glyph_end;
      const CompositeGlyphHeader *current;

      bool move_to_next ()
      {
	if (current->flags & CompositeGlyphHeader::MORE_COMPONENTS)
	{
	  const CompositeGlyphHeader *possible =
	    &StructAfter<CompositeGlyphHeader, CompositeGlyphHeader> (*current);
	  if (!in_range (possible))
	    return false;
	  current = possible;
	  return true;
	}
	return false;
      }

      bool in_range (const CompositeGlyphHeader *composite) const
      {
	return (const char *) composite >= glyph_start
	  && ((const char *) composite + CompositeGlyphHeader::min_size) <= glyph_end
	  && ((const char *) composite + composite->get_size ()) <= glyph_end;
      }
    };

    static bool get_iterator (const char * glyph_data,
			      unsigned int length,
			      CompositeGlyphHeader::Iterator *iterator /* OUT */)
    {
      if (length < GlyphHeader::static_size)
	return false; /* Empty glyph; zero extents. */

      const GlyphHeader &glyph_header = StructAtOffset<GlyphHeader> (glyph_data, 0);
      if (glyph_header.numberOfContours < 0)
      {
	const CompositeGlyphHeader *possible =
	  &StructAfter<CompositeGlyphHeader, GlyphHeader> (glyph_header);

	iterator->glyph_start = glyph_data;
	iterator->glyph_end = (const char *) glyph_data + length;
	if (!iterator->in_range (possible))
	  return false;
	iterator->current = possible;
	return true;
      }

      return false;
    }

    DEFINE_SIZE_MIN (4);
  };

  struct accelerator_t
  {
    void init (hb_face_t *face)
    {
      memset (this, 0, sizeof (accelerator_t));

      const OT::head &head = *face->table.head;
      if (head.indexToLocFormat > 1 || head.glyphDataFormat != 0)
	/* Unknown format.  Leave num_glyphs=0, that takes care of disabling us. */
	return;
      short_offset = 0 == head.indexToLocFormat;

      loca_table = hb_sanitize_context_t ().reference_table<loca> (face);
      glyf_table = hb_sanitize_context_t ().reference_table<glyf> (face);

      num_glyphs = hb_max (1u, loca_table.get_length () / (short_offset ? 2 : 4)) - 1;
    }

    void fini ()
    {
      loca_table.destroy ();
      glyf_table.destroy ();
    }

    /*
     * Returns true if the referenced glyph is a valid glyph and a composite glyph.
     * If true is returned a pointer to the composite glyph will be written into
     * composite.
     */
    bool get_composite (hb_codepoint_t glyph,
			CompositeGlyphHeader::Iterator *composite /* OUT */) const
    {
      if (unlikely (!num_glyphs))
	return false;

      unsigned int start_offset, end_offset;
      if (!get_offsets (glyph, &start_offset, &end_offset))
	return false; /* glyph not found */

      return CompositeGlyphHeader::get_iterator ((const char *) this->glyf_table + start_offset,
						 end_offset - start_offset,
						 composite);
    }

    enum simple_glyph_flag_t {
      FLAG_ON_CURVE = 0x01,
      FLAG_X_SHORT = 0x02,
      FLAG_Y_SHORT = 0x04,
      FLAG_REPEAT = 0x08,
      FLAG_X_SAME = 0x10,
      FLAG_Y_SAME = 0x20,
      FLAG_RESERVED1 = 0x40,
      FLAG_RESERVED2 = 0x80
    };

    /* based on FontTools _g_l_y_f.py::trim */
    bool remove_padding (unsigned int start_offset,
			 unsigned int *end_offset) const
    {
      if (*end_offset - start_offset < GlyphHeader::static_size) return true;

      const char *glyph = ((const char *) glyf_table) + start_offset;
      const char * const glyph_end = glyph + (*end_offset - start_offset);
      const GlyphHeader &glyph_header = StructAtOffset<GlyphHeader> (glyph, 0);
      int16_t num_contours = (int16_t) glyph_header.numberOfContours;

      if (num_contours < 0)
	/* Trimming for composites not implemented.
	 * If removing hints it falls out of that. */
	return true;
      else if (num_contours > 0)
      {
	/* simple glyph w/contours, possibly trimmable */
	glyph += GlyphHeader::static_size + 2 * num_contours;

	if (unlikely (glyph + 2 >= glyph_end)) return false;
	uint16_t nCoordinates = (uint16_t) StructAtOffset<HBUINT16> (glyph - 2, 0) + 1;
	uint16_t nInstructions = (uint16_t) StructAtOffset<HBUINT16> (glyph, 0);

	glyph += 2 + nInstructions;
	if (unlikely (glyph + 2 >= glyph_end)) return false;

	unsigned int coordBytes = 0;
	unsigned int coordsWithFlags = 0;
	while (glyph < glyph_end)
	{
	  uint8_t flag = (uint8_t) *glyph;
	  glyph++;

	  unsigned int repeat = 1;
	  if (flag & FLAG_REPEAT)
	  {
	    if (glyph >= glyph_end)
	    {
	      DEBUG_MSG(SUBSET, nullptr, "Bad flag");
	      return false;
	    }
	    repeat = ((uint8_t) *glyph) + 1;
	    glyph++;
	  }

	  unsigned int xBytes, yBytes;
	  xBytes = yBytes = 0;
	  if (flag & FLAG_X_SHORT) xBytes = 1;
	  else if ((flag & FLAG_X_SAME) == 0) xBytes = 2;

	  if (flag & FLAG_Y_SHORT) yBytes = 1;
	  else if ((flag & FLAG_Y_SAME) == 0) yBytes = 2;

	  coordBytes += (xBytes + yBytes) * repeat;
	  coordsWithFlags += repeat;
	  if (coordsWithFlags >= nCoordinates)
	    break;
	}

	if (coordsWithFlags != nCoordinates)
	{
	  DEBUG_MSG(SUBSET, nullptr, "Expect %d coords to have flags, got flags for %d", nCoordinates, coordsWithFlags);
	  return false;
	}
	glyph += coordBytes;

	if (glyph < glyph_end)
	  *end_offset -= glyph_end - glyph;
      }
      return true;
    }

    bool get_offsets (hb_codepoint_t  glyph,
		      unsigned int   *start_offset /* OUT */,
		      unsigned int   *end_offset   /* OUT */) const
    {
      if (unlikely (glyph >= num_glyphs))
	return false;

      if (short_offset)
      {
	const HBUINT16 *offsets = (const HBUINT16 *) loca_table->dataZ.arrayZ;
	*start_offset = 2 * offsets[glyph];
	*end_offset   = 2 * offsets[glyph + 1];
      }
      else
      {
	const HBUINT32 *offsets = (const HBUINT32 *) loca_table->dataZ.arrayZ;

	*start_offset = offsets[glyph];
	*end_offset   = offsets[glyph + 1];
      }

      if (*start_offset > *end_offset || *end_offset > glyf_table.get_length ())
	return false;

      return true;
    }

    bool get_instruction_length (hb_bytes_t glyph,
				 unsigned int * length /* OUT */) const
    {
      /* Empty glyph; no instructions. */
      if (glyph.length < GlyphHeader::static_size)
      {
	*length = 0;
	// only 0 byte glyphs are healthy when missing GlyphHeader
	return glyph.length == 0;
      }
      const GlyphHeader &glyph_header = StructAtOffset<GlyphHeader> (&glyph, 0);
      int16_t num_contours = (int16_t) glyph_header.numberOfContours;
      if (num_contours < 0)
      {
	unsigned int start = glyph.length;
	unsigned int end = glyph.length;
	unsigned int glyph_offset = &glyph - glyf_table;
	CompositeGlyphHeader::Iterator composite_it;
	if (unlikely (!CompositeGlyphHeader::get_iterator (&glyph, glyph.length, &composite_it))) return false;
	const CompositeGlyphHeader *last;
	do {
	  last = composite_it.current;
	} while (composite_it.move_to_next ());

	if ((uint16_t) last->flags & CompositeGlyphHeader::WE_HAVE_INSTRUCTIONS)
	  start = ((char *) last - (char *) glyf_table->dataZ.arrayZ) + last->get_size () - glyph_offset;
	if (unlikely (start > end))
	{
	  DEBUG_MSG(SUBSET, nullptr, "Invalid instruction offset, %d is outside %d byte buffer", start, glyph.length);
	  return false;
	}
	*length = end - start;
      }
      else
      {
	unsigned int instruction_length_offset = GlyphHeader::static_size + 2 * num_contours;
	if (unlikely (instruction_length_offset + 2 > glyph.length))
	{
	  DEBUG_MSG(SUBSET, nullptr, "Glyph size is too short, missing field instructionLength.");
	  return false;
	}

	const HBUINT16 &instruction_length = StructAtOffset<HBUINT16> (&glyph, instruction_length_offset);
	if (unlikely (instruction_length_offset + instruction_length > glyph.length)) // Out of bounds of the current glyph
	{
	  DEBUG_MSG(SUBSET, nullptr, "The instructions array overruns the glyph's boundaries.");
	  return false;
	}
	*length = (uint16_t) instruction_length;
      }
      return true;
    }

    bool get_extents (hb_codepoint_t glyph, hb_glyph_extents_t *extents) const
    {
      unsigned int start_offset, end_offset;
      if (!get_offsets (glyph, &start_offset, &end_offset))
	return false;

      if (end_offset - start_offset < GlyphHeader::static_size)
	return true; /* Empty glyph; zero extents. */

      const GlyphHeader &glyph_header = StructAtOffset<GlyphHeader> (glyf_table, start_offset);

      extents->x_bearing = hb_min (glyph_header.xMin, glyph_header.xMax);
      extents->y_bearing = hb_max (glyph_header.yMin, glyph_header.yMax);
      extents->width     = hb_max (glyph_header.xMin, glyph_header.xMax) - extents->x_bearing;
      extents->height    = hb_min (glyph_header.yMin, glyph_header.yMax) - extents->y_bearing;

      return true;
    }

  hb_bytes_t bytes_for_glyph (const char * glyf, hb_codepoint_t gid)
  {
    unsigned int start_offset, end_offset;
    if (unlikely (!(get_offsets (gid, &start_offset, &end_offset) &&
      remove_padding (start_offset, &end_offset))))
    {
      DEBUG_MSG(SUBSET, nullptr, "Unable to get offset or remove padding for %d", gid);
      return hb_bytes_t ();
    }
    hb_bytes_t glyph = hb_bytes_t (glyf + start_offset, end_offset - start_offset);
    if (glyph.length == 0) return glyph;
    if (unlikely (glyph.length < GlyphHeader::static_size))
    {
      DEBUG_MSG(SUBSET, nullptr, "Glyph size smaller than minimum header %d", gid);
      return hb_bytes_t ();
    }
    return glyph;
  }

    private:
    bool short_offset;
    unsigned int num_glyphs;
    hb_blob_ptr_t<loca> loca_table;
    hb_blob_ptr_t<glyf> glyf_table;
  };


  struct SubsetGlyph
  {
    hb_codepoint_t new_gid;
    hb_codepoint_t old_gid;
    hb_bytes_t source_glyph;
    hb_bytes_t dest_start;  // region of source_glyph to copy first
    hb_bytes_t dest_end;    // region of source_glyph to copy second


  bool serialize (hb_serialize_context_t *c,
		  const hb_subset_plan_t *plan) const
  {
    TRACE_SERIALIZE (this);

    hb_bytes_t dest_glyph = dest_start.copy(c);
    dest_glyph = hb_bytes_t (&dest_glyph, dest_glyph.length + dest_end.copy(c).length);
    unsigned int pad_length = padding ();
    DEBUG_MSG(SUBSET, nullptr, "serialize %d byte glyph, width %d pad %d", dest_glyph.length, dest_glyph.length  + pad_length, pad_length);

    HBUINT8 pad;
    pad = 0;
    while (pad_length > 0)
    {
      c->embed(pad);
      pad_length--;
    }

    if (dest_glyph.length)
    {
      _fix_component_gids (plan, dest_glyph);
      if (plan->drop_hints)
      {
	_zero_instruction_length (dest_glyph);
	c->check_success (_remove_composite_instruction_flag (dest_glyph));
      }
    }

    return_trace (true);
  }

    void drop_hints (const OT::glyf::accelerator_t& glyf)
    {
      if (source_glyph.length == 0) return;

      unsigned int instruction_length = 0;
      if (!glyf.get_instruction_length (source_glyph, &instruction_length))
      {
	 DEBUG_MSG(SUBSET, nullptr, "Unable to read instruction length for new_gid %d", new_gid);
	return ;
      }

      const GlyphHeader& header = StructAtOffset<GlyphHeader> (&source_glyph, 0);
      int16_t num_contours = (int16_t) header.numberOfContours;
      DEBUG_MSG(SUBSET, nullptr, "new_gid %d (%d contours) drop %d instruction bytes from %d byte source glyph", new_gid, num_contours, instruction_length, source_glyph.length);
      if (num_contours < 0)
      {
	// composite, just chop instructions off the end
	dest_start = hb_bytes_t (&source_glyph, source_glyph.length - instruction_length);
      }
      else
      {
	// simple glyph
	dest_start = hb_bytes_t (&source_glyph, GlyphHeader::static_size + 2 * header.numberOfContours + 2);
	dest_end = hb_bytes_t (&source_glyph + dest_start.length + instruction_length,
			       source_glyph.length - dest_start.length - instruction_length);
DEBUG_MSG(SUBSET, nullptr, "source_len %d start len %d instruction_len %d end len %d", source_glyph.length, dest_start.length, instruction_length, dest_end.length);
      }
    }

    unsigned int length () const
    {
      return dest_start.length + dest_end.length;
    }

    // pad to 2 to ensure 2-byte loca will be ok
    unsigned int padding () const
    {
      return length () % 2;
    }

    unsigned int padded_size () const
    {
      return length () + padding ();
    }
  };

  protected:
  UnsizedArrayOf<HBUINT8>	dataZ;		/* Glyphs data. */
  public:
  DEFINE_SIZE_MIN (0); /* In reality, this is UNBOUNDED() type; but since we always
			* check the size externally, allow Null() object of it by
			* defining it _MIN instead. */
};

struct glyf_accelerator_t : glyf::accelerator_t {};

} /* namespace OT */


#endif /* HB_OT_GLYF_TABLE_HH */
