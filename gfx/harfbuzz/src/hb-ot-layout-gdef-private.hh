/*
 * Copyright (C) 2007,2008,2009  Red Hat, Inc.
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

#ifndef HB_OT_LAYOUT_GDEF_PRIVATE_HH
#define HB_OT_LAYOUT_GDEF_PRIVATE_HH

#include "hb-ot-layout-common-private.hh"

#include "hb-font-private.hh"


/*
 * Attachment List Table
 */

typedef ArrayOf<USHORT> AttachPoint;	/* Array of contour point indices--in
					 * increasing numerical order */

struct AttachList
{
  inline unsigned int get_attach_points (hb_codepoint_t glyph_id,
					 unsigned int start_offset,
					 unsigned int *point_count /* IN/OUT */,
					 unsigned int *point_array /* OUT */) const
  {
    unsigned int index = (this+coverage) (glyph_id);
    if (index == NOT_COVERED)
    {
      if (point_count)
	*point_count = 0;
      return 0;
    }

    const AttachPoint &points = this+attachPoint[index];

    if (point_count) {
      const USHORT *array = points.sub_array (start_offset, point_count);
      unsigned int count = *point_count;
      for (unsigned int i = 0; i < count; i++)
	point_array[i] = array[i];
    }

    return points.len;
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return coverage.sanitize (c, this)
	&& attachPoint.sanitize (c, this);
  }

  private:
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table -- from
					 * beginning of AttachList table */
  OffsetArrayOf<AttachPoint>
		attachPoint;		/* Array of AttachPoint tables
					 * in Coverage Index order */
  public:
  DEFINE_SIZE_ARRAY (4, attachPoint);
};

/*
 * Ligature Caret Table
 */

struct CaretValueFormat1
{
  friend struct CaretValue;

  private:
  inline int get_caret_value (hb_ot_layout_context_t *c, hb_codepoint_t glyph_id HB_UNUSED) const
  {
    /* TODO vertical */
    return c->scale_x (coordinate);
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return c->check_struct (this);
  }

  private:
  USHORT	caretValueFormat;	/* Format identifier--format = 1 */
  SHORT		coordinate;		/* X or Y value, in design units */
  public:
  DEFINE_SIZE_STATIC (4);
};

struct CaretValueFormat2
{
  friend struct CaretValue;

  private:
  inline int get_caret_value (hb_ot_layout_context_t *c, hb_codepoint_t glyph_id) const
  {
    /* TODO vertical */
    hb_position_t x, y;
    if (hb_font_get_contour_point (c->font, c->face, caretValuePoint, glyph_id, &x, &y))
      return x;
    else
      return 0;
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return c->check_struct (this);
  }

  private:
  USHORT	caretValueFormat;	/* Format identifier--format = 2 */
  USHORT	caretValuePoint;	/* Contour point index on glyph */
  public:
  DEFINE_SIZE_STATIC (4);
};

struct CaretValueFormat3
{
  friend struct CaretValue;

  inline int get_caret_value (hb_ot_layout_context_t *c, hb_codepoint_t glyph_id HB_UNUSED) const
  {
    /* TODO vertical */
    return c->scale_x (coordinate) + ((this+deviceTable).get_x_delta (c));
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return c->check_struct (this)
	&& deviceTable.sanitize (c, this);
  }

  private:
  USHORT	caretValueFormat;	/* Format identifier--format = 3 */
  SHORT		coordinate;		/* X or Y value, in design units */
  OffsetTo<Device>
		deviceTable;		/* Offset to Device table for X or Y
					 * value--from beginning of CaretValue
					 * table */
  public:
  DEFINE_SIZE_STATIC (6);
};

struct CaretValue
{
  inline int get_caret_value (hb_ot_layout_context_t *c, hb_codepoint_t glyph_id) const
  {
    switch (u.format) {
    case 1: return u.format1.get_caret_value (c, glyph_id);
    case 2: return u.format2.get_caret_value (c, glyph_id);
    case 3: return u.format3.get_caret_value (c, glyph_id);
    default:return 0;
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
  CaretValueFormat1	format1;
  CaretValueFormat2	format2;
  CaretValueFormat3	format3;
  } u;
  public:
  DEFINE_SIZE_UNION (2, format);
};

struct LigGlyph
{
  inline unsigned int get_lig_carets (hb_ot_layout_context_t *c,
				      hb_codepoint_t glyph_id,
				      unsigned int start_offset,
				      unsigned int *caret_count /* IN/OUT */,
				      int *caret_array /* OUT */) const
  {
    if (caret_count) {
      const OffsetTo<CaretValue> *array = carets.sub_array (start_offset, caret_count);
      unsigned int count = *caret_count;
      for (unsigned int i = 0; i < count; i++)
	caret_array[i] = (this+array[i]).get_caret_value (c, glyph_id);
    }

    return carets.len;
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return carets.sanitize (c, this);
  }

  private:
  OffsetArrayOf<CaretValue>
		carets;			/* Offset array of CaretValue tables
					 * --from beginning of LigGlyph table
					 * --in increasing coordinate order */
  public:
  DEFINE_SIZE_ARRAY (2, carets);
};

struct LigCaretList
{
  inline unsigned int get_lig_carets (hb_ot_layout_context_t *c,
				      hb_codepoint_t glyph_id,
				      unsigned int start_offset,
				      unsigned int *caret_count /* IN/OUT */,
				      int *caret_array /* OUT */) const
  {
    unsigned int index = (this+coverage) (glyph_id);
    if (index == NOT_COVERED)
    {
      if (caret_count)
	*caret_count = 0;
      return 0;
    }
    const LigGlyph &lig_glyph = this+ligGlyph[index];
    return lig_glyph.get_lig_carets (c, glyph_id, start_offset, caret_count, caret_array);
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return coverage.sanitize (c, this)
	&& ligGlyph.sanitize (c, this);
  }

  private:
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of LigCaretList table */
  OffsetArrayOf<LigGlyph>
		ligGlyph;		/* Array of LigGlyph tables
					 * in Coverage Index order */
  public:
  DEFINE_SIZE_ARRAY (4, ligGlyph);
};


struct MarkGlyphSetsFormat1
{
  inline bool covers (unsigned int set_index, hb_codepoint_t glyph_id) const
  { return (this+coverage[set_index]).get_coverage (glyph_id) != NOT_COVERED; }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return coverage.sanitize (c, this);
  }

  private:
  USHORT	format;			/* Format identifier--format = 1 */
  LongOffsetArrayOf<Coverage>
		coverage;		/* Array of long offsets to mark set
					 * coverage tables */
  public:
  DEFINE_SIZE_ARRAY (4, coverage);
};

struct MarkGlyphSets
{
  inline bool covers (unsigned int set_index, hb_codepoint_t glyph_id) const
  {
    switch (u.format) {
    case 1: return u.format1.covers (set_index, glyph_id);
    default:return false;
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
  MarkGlyphSetsFormat1	format1;
  } u;
  public:
  DEFINE_SIZE_UNION (2, format);
};


/*
 * GDEF
 */

struct GDEF
{
  static const hb_tag_t Tag	= HB_OT_TAG_GDEF;

  enum {
    UnclassifiedGlyph	= 0,
    BaseGlyph		= 1,
    LigatureGlyph	= 2,
    MarkGlyph		= 3,
    ComponentGlyph	= 4
  };

  inline bool has_glyph_classes () const { return glyphClassDef != 0; }
  inline hb_ot_layout_class_t get_glyph_class (hb_codepoint_t glyph) const
  { return (this+glyphClassDef).get_class (glyph); }

  inline bool has_mark_attachment_types () const { return markAttachClassDef != 0; }
  inline hb_ot_layout_class_t get_mark_attachment_type (hb_codepoint_t glyph) const
  { return (this+markAttachClassDef).get_class (glyph); }

  inline bool has_attach_points () const { return attachList != 0; }
  inline unsigned int get_attach_points (hb_codepoint_t glyph_id,
					 unsigned int start_offset,
					 unsigned int *point_count /* IN/OUT */,
					 unsigned int *point_array /* OUT */) const
  { return (this+attachList).get_attach_points (glyph_id, start_offset, point_count, point_array); }

  inline bool has_lig_carets () const { return ligCaretList != 0; }
  inline unsigned int get_lig_carets (hb_ot_layout_context_t *c,
				      hb_codepoint_t glyph_id,
				      unsigned int start_offset,
				      unsigned int *caret_count /* IN/OUT */,
				      int *caret_array /* OUT */) const
  { return (this+ligCaretList).get_lig_carets (c, glyph_id, start_offset, caret_count, caret_array); }

  inline bool has_mark_sets () const { return version >= 0x00010002 && markGlyphSetsDef[0] != 0; }
  inline bool mark_set_covers (unsigned int set_index, hb_codepoint_t glyph_id) const
  { return version >= 0x00010002 && (this+markGlyphSetsDef[0]).covers (set_index, glyph_id); }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return version.sanitize (c) && likely (version.major == 1)
	&& glyphClassDef.sanitize (c, this)
	&& attachList.sanitize (c, this)
	&& ligCaretList.sanitize (c, this)
	&& markAttachClassDef.sanitize (c, this)
	&& (version < 0x00010002 || markGlyphSetsDef[0].sanitize (c, this));
  }

  private:
  FixedVersion	version;		/* Version of the GDEF table--currently
					 * 0x00010002 */
  OffsetTo<ClassDef>
		glyphClassDef;		/* Offset to class definition table
					 * for glyph type--from beginning of
					 * GDEF header (may be Null) */
  OffsetTo<AttachList>
		attachList;		/* Offset to list of glyphs with
					 * attachment points--from beginning
					 * of GDEF header (may be Null) */
  OffsetTo<LigCaretList>
		ligCaretList;		/* Offset to list of positioning points
					 * for ligature carets--from beginning
					 * of GDEF header (may be Null) */
  OffsetTo<ClassDef>
		markAttachClassDef;	/* Offset to class definition table for
					 * mark attachment type--from beginning
					 * of GDEF header (may be Null) */
  OffsetTo<MarkGlyphSets>
		markGlyphSetsDef[VAR];	/* Offset to the table of mark set
					 * definitions--from beginning of GDEF
					 * header (may be NULL).  Introduced
					 * in version 00010002. */
  public:
  DEFINE_SIZE_ARRAY (12, markGlyphSetsDef);
};


#endif /* HB_OT_LAYOUT_GDEF_PRIVATE_HH */
