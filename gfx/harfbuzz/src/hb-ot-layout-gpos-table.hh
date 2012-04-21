/*
 * Copyright © 2007,2008,2009,2010  Red Hat, Inc.
 * Copyright © 2010  Google, Inc.
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

#ifndef HB_OT_LAYOUT_GPOS_TABLE_HH
#define HB_OT_LAYOUT_GPOS_TABLE_HH

#include "hb-ot-layout-gsubgpos-private.hh"



/* buffer **position** var allocations */
#define attach_lookback() var.u16[0] /* number of glyphs to go back to attach this glyph to its base */
#define cursive_chain() var.i16[1] /* character to which this connects, may be positive or negative */


/* Shared Tables: ValueRecord, Anchor Table, and MarkArray */

typedef USHORT Value;

typedef Value ValueRecord[VAR];

struct ValueFormat : USHORT
{
  enum Flags {
    xPlacement	= 0x0001,	/* Includes horizontal adjustment for placement */
    yPlacement	= 0x0002,	/* Includes vertical adjustment for placement */
    xAdvance	= 0x0004,	/* Includes horizontal adjustment for advance */
    yAdvance	= 0x0008,	/* Includes vertical adjustment for advance */
    xPlaDevice	= 0x0010,	/* Includes horizontal Device table for placement */
    yPlaDevice	= 0x0020,	/* Includes vertical Device table for placement */
    xAdvDevice	= 0x0040,	/* Includes horizontal Device table for advance */
    yAdvDevice	= 0x0080,	/* Includes vertical Device table for advance */
    ignored	= 0x0F00,	/* Was used in TrueType Open for MM fonts */
    reserved	= 0xF000,	/* For future use */

    devices	= 0x00F0	/* Mask for having any Device table */
  };

/* All fields are options.  Only those available advance the value pointer. */
#if 0
  SHORT		xPlacement;		/* Horizontal adjustment for
					 * placement--in design units */
  SHORT		yPlacement;		/* Vertical adjustment for
					 * placement--in design units */
  SHORT		xAdvance;		/* Horizontal adjustment for
					 * advance--in design units (only used
					 * for horizontal writing) */
  SHORT		yAdvance;		/* Vertical adjustment for advance--in
					 * design units (only used for vertical
					 * writing) */
  Offset	xPlaDevice;		/* Offset to Device table for
					 * horizontal placement--measured from
					 * beginning of PosTable (may be NULL) */
  Offset	yPlaDevice;		/* Offset to Device table for vertical
					 * placement--measured from beginning
					 * of PosTable (may be NULL) */
  Offset	xAdvDevice;		/* Offset to Device table for
					 * horizontal advance--measured from
					 * beginning of PosTable (may be NULL) */
  Offset	yAdvDevice;		/* Offset to Device table for vertical
					 * advance--measured from beginning of
					 * PosTable (may be NULL) */
#endif

  inline unsigned int get_len (void) const
  { return _hb_popcount32 ((unsigned int) *this); }
  inline unsigned int get_size (void) const
  { return get_len () * Value::static_size; }

  void apply_value (hb_font_t            *font,
		    hb_direction_t        direction,
		    const void           *base,
		    const Value          *values,
		    hb_glyph_position_t  &glyph_pos) const
  {
    unsigned int x_ppem, y_ppem;
    unsigned int format = *this;
    hb_bool_t horizontal = HB_DIRECTION_IS_HORIZONTAL (direction);

    if (!format) return;

    if (format & xPlacement) glyph_pos.x_offset  += font->em_scale_x (get_short (values++));
    if (format & yPlacement) glyph_pos.y_offset  += font->em_scale_y (get_short (values++));
    if (format & xAdvance) {
      if (likely (horizontal)) glyph_pos.x_advance += font->em_scale_x (get_short (values++)); else values++;
    }
    /* y_advance values grow downward but font-space grows upward, hence negation */
    if (format & yAdvance) {
      if (unlikely (!horizontal)) glyph_pos.y_advance -= font->em_scale_y (get_short (values++)); else values++;
    }

    if (!has_device ()) return;

    x_ppem = font->x_ppem;
    y_ppem = font->y_ppem;

    if (!x_ppem && !y_ppem) return;

    /* pixel -> fractional pixel */
    if (format & xPlaDevice) {
      if (x_ppem) glyph_pos.x_offset  += (base + get_device (values++)).get_x_delta (font); else values++;
    }
    if (format & yPlaDevice) {
      if (y_ppem) glyph_pos.y_offset  += (base + get_device (values++)).get_y_delta (font); else values++;
    }
    if (format & xAdvDevice) {
      if (horizontal && x_ppem) glyph_pos.x_advance += (base + get_device (values++)).get_x_delta (font); else values++;
    }
    if (format & yAdvDevice) {
      /* y_advance values grow downward but font-space grows upward, hence negation */
      if (!horizontal && y_ppem) glyph_pos.y_advance -= (base + get_device (values++)).get_y_delta (font); else values++;
    }
  }

  private:
  inline bool sanitize_value_devices (hb_sanitize_context_t *c, void *base, Value *values) {
    unsigned int format = *this;

    if (format & xPlacement) values++;
    if (format & yPlacement) values++;
    if (format & xAdvance)   values++;
    if (format & yAdvance)   values++;

    if ((format & xPlaDevice) && !get_device (values++).sanitize (c, base)) return false;
    if ((format & yPlaDevice) && !get_device (values++).sanitize (c, base)) return false;
    if ((format & xAdvDevice) && !get_device (values++).sanitize (c, base)) return false;
    if ((format & yAdvDevice) && !get_device (values++).sanitize (c, base)) return false;

    return true;
  }

  static inline OffsetTo<Device>& get_device (Value* value)
  { return *CastP<OffsetTo<Device> > (value); }
  static inline const OffsetTo<Device>& get_device (const Value* value)
  { return *CastP<OffsetTo<Device> > (value); }

  static inline const SHORT& get_short (const Value* value)
  { return *CastP<SHORT> (value); }

  public:

  inline bool has_device (void) const {
    unsigned int format = *this;
    return (format & devices) != 0;
  }

  inline bool sanitize_value (hb_sanitize_context_t *c, void *base, Value *values) {
    TRACE_SANITIZE ();
    return c->check_range (values, get_size ())
	&& (!has_device () || sanitize_value_devices (c, base, values));
  }

  inline bool sanitize_values (hb_sanitize_context_t *c, void *base, Value *values, unsigned int count) {
    TRACE_SANITIZE ();
    unsigned int len = get_len ();

    if (!c->check_array (values, get_size (), count)) return false;

    if (!has_device ()) return true;

    for (unsigned int i = 0; i < count; i++) {
      if (!sanitize_value_devices (c, base, values))
        return false;
      values += len;
    }

    return true;
  }

  /* Just sanitize referenced Device tables.  Doesn't check the values themselves. */
  inline bool sanitize_values_stride_unsafe (hb_sanitize_context_t *c, void *base, Value *values, unsigned int count, unsigned int stride) {
    TRACE_SANITIZE ();

    if (!has_device ()) return true;

    for (unsigned int i = 0; i < count; i++) {
      if (!sanitize_value_devices (c, base, values))
        return false;
      values += stride;
    }

    return true;
  }
};


struct AnchorFormat1
{
  friend struct Anchor;

  private:
  inline void get_anchor (hb_font_t *font, hb_codepoint_t glyph_id HB_UNUSED,
			  hb_position_t *x, hb_position_t *y) const
  {
      *x = font->em_scale_x (xCoordinate);
      *y = font->em_scale_y (yCoordinate);
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return c->check_struct (this);
  }

  private:
  USHORT	format;			/* Format identifier--format = 1 */
  SHORT		xCoordinate;		/* Horizontal value--in design units */
  SHORT		yCoordinate;		/* Vertical value--in design units */
  public:
  DEFINE_SIZE_STATIC (6);
};

struct AnchorFormat2
{
  friend struct Anchor;

  private:
  inline void get_anchor (hb_font_t *font, hb_codepoint_t glyph_id,
			  hb_position_t *x, hb_position_t *y) const
  {
      unsigned int x_ppem = font->x_ppem;
      unsigned int y_ppem = font->y_ppem;
      hb_position_t cx, cy;
      hb_bool_t ret = false;

      if (x_ppem || y_ppem)
	ret = hb_font_get_glyph_contour_point_for_origin (font, glyph_id, anchorPoint, HB_DIRECTION_LTR, &cx, &cy);
      *x = x_ppem && ret ? cx : font->em_scale_x (xCoordinate);
      *y = y_ppem && ret ? cy : font->em_scale_y (yCoordinate);
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return c->check_struct (this);
  }

  private:
  USHORT	format;			/* Format identifier--format = 2 */
  SHORT		xCoordinate;		/* Horizontal value--in design units */
  SHORT		yCoordinate;		/* Vertical value--in design units */
  USHORT	anchorPoint;		/* Index to glyph contour point */
  public:
  DEFINE_SIZE_STATIC (8);
};

struct AnchorFormat3
{
  friend struct Anchor;

  private:
  inline void get_anchor (hb_font_t *font, hb_codepoint_t glyph_id HB_UNUSED,
			  hb_position_t *x, hb_position_t *y) const
  {
      *x = font->em_scale_x (xCoordinate);
      *y = font->em_scale_y (yCoordinate);

      if (font->x_ppem)
	*x += (this+xDeviceTable).get_x_delta (font);
      if (font->y_ppem)
	*y += (this+yDeviceTable).get_x_delta (font);
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return c->check_struct (this)
	&& xDeviceTable.sanitize (c, this)
	&& yDeviceTable.sanitize (c, this);
  }

  private:
  USHORT	format;			/* Format identifier--format = 3 */
  SHORT		xCoordinate;		/* Horizontal value--in design units */
  SHORT		yCoordinate;		/* Vertical value--in design units */
  OffsetTo<Device>
		xDeviceTable;		/* Offset to Device table for X
					 * coordinate-- from beginning of
					 * Anchor table (may be NULL) */
  OffsetTo<Device>
		yDeviceTable;		/* Offset to Device table for Y
					 * coordinate-- from beginning of
					 * Anchor table (may be NULL) */
  public:
  DEFINE_SIZE_STATIC (10);
};

struct Anchor
{
  inline void get_anchor (hb_font_t *font, hb_codepoint_t glyph_id,
			  hb_position_t *x, hb_position_t *y) const
  {
    *x = *y = 0;
    switch (u.format) {
    case 1: u.format1.get_anchor (font, glyph_id, x, y); return;
    case 2: u.format2.get_anchor (font, glyph_id, x, y); return;
    case 3: u.format3.get_anchor (font, glyph_id, x, y); return;
    default:						 return;
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
  AnchorFormat1		format1;
  AnchorFormat2		format2;
  AnchorFormat3		format3;
  } u;
  public:
  DEFINE_SIZE_UNION (2, format);
};


struct AnchorMatrix
{
  inline const Anchor& get_anchor (unsigned int row, unsigned int col, unsigned int cols) const {
    if (unlikely (row >= rows || col >= cols)) return Null(Anchor);
    return this+matrix[row * cols + col];
  }

  inline bool sanitize (hb_sanitize_context_t *c, unsigned int cols) {
    TRACE_SANITIZE ();
    if (!c->check_struct (this)) return false;
    if (unlikely (rows > 0 && cols >= ((unsigned int) -1) / rows)) return false;
    unsigned int count = rows * cols;
    if (!c->check_array (matrix, matrix[0].static_size, count)) return false;
    for (unsigned int i = 0; i < count; i++)
      if (!matrix[i].sanitize (c, this)) return false;
    return true;
  }

  USHORT	rows;			/* Number of rows */
  private:
  OffsetTo<Anchor>
		matrix[VAR];		/* Matrix of offsets to Anchor tables--
					 * from beginning of AnchorMatrix table */
  public:
  DEFINE_SIZE_ARRAY (2, matrix);
};


struct MarkRecord
{
  friend struct MarkArray;

  inline bool sanitize (hb_sanitize_context_t *c, void *base) {
    TRACE_SANITIZE ();
    return c->check_struct (this)
	&& markAnchor.sanitize (c, base);
  }

  private:
  USHORT	klass;			/* Class defined for this mark */
  OffsetTo<Anchor>
		markAnchor;		/* Offset to Anchor table--from
					 * beginning of MarkArray table */
  public:
  DEFINE_SIZE_STATIC (4);
};

struct MarkArray : ArrayOf<MarkRecord>	/* Array of MarkRecords--in Coverage order */
{
  inline bool apply (hb_apply_context_t *c,
		     unsigned int mark_index, unsigned int glyph_index,
		     const AnchorMatrix &anchors, unsigned int class_count,
		     unsigned int glyph_pos) const
  {
    TRACE_APPLY ();
    const MarkRecord &record = ArrayOf<MarkRecord>::operator[](mark_index);
    unsigned int mark_class = record.klass;

    const Anchor& mark_anchor = this + record.markAnchor;
    const Anchor& glyph_anchor = anchors.get_anchor (glyph_index, mark_class, class_count);

    hb_position_t mark_x, mark_y, base_x, base_y;

    mark_anchor.get_anchor (c->font, c->buffer->info[c->buffer->idx].codepoint, &mark_x, &mark_y);
    glyph_anchor.get_anchor (c->font, c->buffer->info[glyph_pos].codepoint, &base_x, &base_y);

    hb_glyph_position_t &o = c->buffer->pos[c->buffer->idx];
    o.x_offset = base_x - mark_x;
    o.y_offset = base_y - mark_y;
    o.attach_lookback() = c->buffer->idx - glyph_pos;

    c->buffer->idx++;
    return true;
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return ArrayOf<MarkRecord>::sanitize (c, this);
  }
};


/* Lookups */

struct SinglePosFormat1
{
  friend struct SinglePos;

  private:
  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY ();
    unsigned int index = (this+coverage) (c->buffer->info[c->buffer->idx].codepoint);
    if (likely (index == NOT_COVERED))
      return false;

    valueFormat.apply_value (c->font, c->direction, this,
			     values, c->buffer->pos[c->buffer->idx]);

    c->buffer->idx++;
    return true;
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return c->check_struct (this)
	&& coverage.sanitize (c, this)
	&& valueFormat.sanitize_value (c, this, values);
  }

  private:
  USHORT	format;			/* Format identifier--format = 1 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of subtable */
  ValueFormat	valueFormat;		/* Defines the types of data in the
					 * ValueRecord */
  ValueRecord	values;			/* Defines positioning
					 * value(s)--applied to all glyphs in
					 * the Coverage table */
  public:
  DEFINE_SIZE_ARRAY (6, values);
};

struct SinglePosFormat2
{
  friend struct SinglePos;

  private:
  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY ();
    unsigned int index = (this+coverage) (c->buffer->info[c->buffer->idx].codepoint);
    if (likely (index == NOT_COVERED))
      return false;

    if (likely (index >= valueCount))
      return false;

    valueFormat.apply_value (c->font, c->direction, this,
			     &values[index * valueFormat.get_len ()],
			     c->buffer->pos[c->buffer->idx]);

    c->buffer->idx++;
    return true;
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return c->check_struct (this)
	&& coverage.sanitize (c, this)
	&& valueFormat.sanitize_values (c, this, values, valueCount);
  }

  private:
  USHORT	format;			/* Format identifier--format = 2 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of subtable */
  ValueFormat	valueFormat;		/* Defines the types of data in the
					 * ValueRecord */
  USHORT	valueCount;		/* Number of ValueRecords */
  ValueRecord	values;			/* Array of ValueRecords--positioning
					 * values applied to glyphs */
  public:
  DEFINE_SIZE_ARRAY (8, values);
};

struct SinglePos
{
  friend struct PosLookupSubTable;

  private:
  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY ();
    switch (u.format) {
    case 1: return u.format1.apply (c);
    case 2: return u.format2.apply (c);
    default:return false;
    }
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    if (!u.format.sanitize (c)) return false;
    switch (u.format) {
    case 1: return u.format1.sanitize (c);
    case 2: return u.format2.sanitize (c);
    default:return true;
    }
  }

  private:
  union {
  USHORT		format;		/* Format identifier */
  SinglePosFormat1	format1;
  SinglePosFormat2	format2;
  } u;
};


struct PairValueRecord
{
  friend struct PairSet;

  private:
  GlyphID	secondGlyph;		/* GlyphID of second glyph in the
					 * pair--first glyph is listed in the
					 * Coverage table */
  ValueRecord	values;			/* Positioning data for the first glyph
					 * followed by for second glyph */
  public:
  DEFINE_SIZE_ARRAY (2, values);
};

struct PairSet
{
  friend struct PairPosFormat1;

  inline bool apply (hb_apply_context_t *c,
		     const ValueFormat *valueFormats,
		     unsigned int pos) const
  {
    TRACE_APPLY ();
    unsigned int len1 = valueFormats[0].get_len ();
    unsigned int len2 = valueFormats[1].get_len ();
    unsigned int record_size = USHORT::static_size * (1 + len1 + len2);

    unsigned int count = len;
    const PairValueRecord *record = CastP<PairValueRecord> (array);
    for (unsigned int i = 0; i < count; i++)
    {
      if (c->buffer->info[pos].codepoint == record->secondGlyph)
      {
	valueFormats[0].apply_value (c->font, c->direction, this,
				     &record->values[0], c->buffer->pos[c->buffer->idx]);
	valueFormats[1].apply_value (c->font, c->direction, this,
				     &record->values[len1], c->buffer->pos[pos]);
	if (len2)
	  pos++;
	c->buffer->idx = pos;
	return true;
      }
      record = &StructAtOffset<PairValueRecord> (record, record_size);
    }

    return false;
  }

  struct sanitize_closure_t {
    void *base;
    ValueFormat *valueFormats;
    unsigned int len1; /* valueFormats[0].get_len() */
    unsigned int stride; /* 1 + len1 + len2 */
  };

  inline bool sanitize (hb_sanitize_context_t *c, const sanitize_closure_t *closure) {
    TRACE_SANITIZE ();
    if (!(c->check_struct (this)
       && c->check_array (array, USHORT::static_size * closure->stride, len))) return false;

    unsigned int count = len;
    PairValueRecord *record = CastP<PairValueRecord> (array);
    return closure->valueFormats[0].sanitize_values_stride_unsafe (c, closure->base, &record->values[0], count, closure->stride)
	&& closure->valueFormats[1].sanitize_values_stride_unsafe (c, closure->base, &record->values[closure->len1], count, closure->stride);
  }

  private:
  USHORT	len;			/* Number of PairValueRecords */
  USHORT	array[VAR];		/* Array of PairValueRecords--ordered
					 * by GlyphID of the second glyph */
  public:
  DEFINE_SIZE_ARRAY (2, array);
};

struct PairPosFormat1
{
  friend struct PairPos;

  private:
  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY ();
    hb_apply_context_t::mark_skipping_forward_iterator_t skippy_iter (c, c->buffer->idx, 1);
    if (skippy_iter.has_no_chance ())
      return false;

    unsigned int index = (this+coverage) (c->buffer->info[c->buffer->idx].codepoint);
    if (likely (index == NOT_COVERED))
      return false;

    if (!skippy_iter.next ())
      return false;

    return (this+pairSet[index]).apply (c, &valueFormat1, skippy_iter.idx);
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();

    unsigned int len1 = valueFormat1.get_len ();
    unsigned int len2 = valueFormat2.get_len ();
    PairSet::sanitize_closure_t closure = {
      this,
      &valueFormat1,
      len1,
      1 + len1 + len2
    };

    return c->check_struct (this)
	&& coverage.sanitize (c, this)
	&& pairSet.sanitize (c, this, &closure);
  }

  private:
  USHORT	format;			/* Format identifier--format = 1 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of subtable */
  ValueFormat	valueFormat1;		/* Defines the types of data in
					 * ValueRecord1--for the first glyph
					 * in the pair--may be zero (0) */
  ValueFormat	valueFormat2;		/* Defines the types of data in
					 * ValueRecord2--for the second glyph
					 * in the pair--may be zero (0) */
  OffsetArrayOf<PairSet>
		pairSet;		/* Array of PairSet tables
					 * ordered by Coverage Index */
  public:
  DEFINE_SIZE_ARRAY (10, pairSet);
};

struct PairPosFormat2
{
  friend struct PairPos;

  private:
  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY ();
    hb_apply_context_t::mark_skipping_forward_iterator_t skippy_iter (c, c->buffer->idx, 1);
    if (skippy_iter.has_no_chance ())
      return false;

    unsigned int index = (this+coverage) (c->buffer->info[c->buffer->idx].codepoint);
    if (likely (index == NOT_COVERED))
      return false;

    if (!skippy_iter.next ())
      return false;

    unsigned int len1 = valueFormat1.get_len ();
    unsigned int len2 = valueFormat2.get_len ();
    unsigned int record_len = len1 + len2;

    unsigned int klass1 = (this+classDef1) (c->buffer->info[c->buffer->idx].codepoint);
    unsigned int klass2 = (this+classDef2) (c->buffer->info[skippy_iter.idx].codepoint);
    if (unlikely (klass1 >= class1Count || klass2 >= class2Count))
      return false;

    const Value *v = &values[record_len * (klass1 * class2Count + klass2)];
    valueFormat1.apply_value (c->font, c->direction, this,
			      v, c->buffer->pos[c->buffer->idx]);
    valueFormat2.apply_value (c->font, c->direction, this,
			      v + len1, c->buffer->pos[skippy_iter.idx]);

    c->buffer->idx = skippy_iter.idx;
    if (len2)
      c->buffer->idx++;

    return true;
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    if (!(c->check_struct (this)
       && coverage.sanitize (c, this)
       && classDef1.sanitize (c, this)
       && classDef2.sanitize (c, this))) return false;

    unsigned int len1 = valueFormat1.get_len ();
    unsigned int len2 = valueFormat2.get_len ();
    unsigned int stride = len1 + len2;
    unsigned int record_size = valueFormat1.get_size () + valueFormat2.get_size ();
    unsigned int count = (unsigned int) class1Count * (unsigned int) class2Count;
    return c->check_array (values, record_size, count) &&
	   valueFormat1.sanitize_values_stride_unsafe (c, this, &values[0], count, stride) &&
	   valueFormat2.sanitize_values_stride_unsafe (c, this, &values[len1], count, stride);
  }

  private:
  USHORT	format;			/* Format identifier--format = 2 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of subtable */
  ValueFormat	valueFormat1;		/* ValueRecord definition--for the
					 * first glyph of the pair--may be zero
					 * (0) */
  ValueFormat	valueFormat2;		/* ValueRecord definition--for the
					 * second glyph of the pair--may be
					 * zero (0) */
  OffsetTo<ClassDef>
		classDef1;		/* Offset to ClassDef table--from
					 * beginning of PairPos subtable--for
					 * the first glyph of the pair */
  OffsetTo<ClassDef>
		classDef2;		/* Offset to ClassDef table--from
					 * beginning of PairPos subtable--for
					 * the second glyph of the pair */
  USHORT	class1Count;		/* Number of classes in ClassDef1
					 * table--includes Class0 */
  USHORT	class2Count;		/* Number of classes in ClassDef2
					 * table--includes Class0 */
  ValueRecord	values;			/* Matrix of value pairs:
					 * class1-major, class2-minor,
					 * Each entry has value1 and value2 */
  public:
  DEFINE_SIZE_ARRAY (16, values);
};

struct PairPos
{
  friend struct PosLookupSubTable;

  private:
  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY ();
    switch (u.format) {
    case 1: return u.format1.apply (c);
    case 2: return u.format2.apply (c);
    default:return false;
    }
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    if (!u.format.sanitize (c)) return false;
    switch (u.format) {
    case 1: return u.format1.sanitize (c);
    case 2: return u.format2.sanitize (c);
    default:return true;
    }
  }

  private:
  union {
  USHORT		format;		/* Format identifier */
  PairPosFormat1	format1;
  PairPosFormat2	format2;
  } u;
};


struct EntryExitRecord
{
  friend struct CursivePosFormat1;

  inline bool sanitize (hb_sanitize_context_t *c, void *base) {
    TRACE_SANITIZE ();
    return entryAnchor.sanitize (c, base)
	&& exitAnchor.sanitize (c, base);
  }

  private:
  OffsetTo<Anchor>
		entryAnchor;		/* Offset to EntryAnchor table--from
					 * beginning of CursivePos
					 * subtable--may be NULL */
  OffsetTo<Anchor>
		exitAnchor;		/* Offset to ExitAnchor table--from
					 * beginning of CursivePos
					 * subtable--may be NULL */
  public:
  DEFINE_SIZE_STATIC (4);
};

struct CursivePosFormat1
{
  friend struct CursivePos;

  private:
  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY ();

    /* We don't handle mark glyphs here. */
    if (c->property & HB_OT_LAYOUT_GLYPH_CLASS_MARK)
      return false;

    hb_apply_context_t::mark_skipping_forward_iterator_t skippy_iter (c, c->buffer->idx, 1);
    if (skippy_iter.has_no_chance ())
      return false;

    const EntryExitRecord &this_record = entryExitRecord[(this+coverage) (c->buffer->info[c->buffer->idx].codepoint)];
    if (!this_record.exitAnchor)
      return false;

    if (!skippy_iter.next ())
      return false;

    const EntryExitRecord &next_record = entryExitRecord[(this+coverage) (c->buffer->info[skippy_iter.idx].codepoint)];
    if (!next_record.entryAnchor)
      return false;

    unsigned int i = c->buffer->idx;
    unsigned int j = skippy_iter.idx;

    hb_position_t entry_x, entry_y, exit_x, exit_y;
    (this+this_record.exitAnchor).get_anchor (c->font, c->buffer->info[i].codepoint, &exit_x, &exit_y);
    (this+next_record.entryAnchor).get_anchor (c->font, c->buffer->info[j].codepoint, &entry_x, &entry_y);

    hb_glyph_position_t *pos = c->buffer->pos;

    hb_position_t d;
    /* Main-direction adjustment */
    switch (c->direction) {
      case HB_DIRECTION_LTR:
	pos[i].x_advance  =  exit_x + pos[i].x_offset;

	d = entry_x + pos[j].x_offset;
	pos[j].x_advance -= d;
	pos[j].x_offset  -= d;
	break;
      case HB_DIRECTION_RTL:
	d = exit_x + pos[i].x_offset;
	pos[i].x_advance -= d;
	pos[i].x_offset  -= d;

	pos[j].x_advance  =  entry_x + pos[j].x_offset;
	break;
      case HB_DIRECTION_TTB:
	pos[i].y_advance  =  exit_y + pos[i].y_offset;

	d = entry_y + pos[j].y_offset;
	pos[j].y_advance -= d;
	pos[j].y_offset  -= d;
	break;
      case HB_DIRECTION_BTT:
	d = exit_y + pos[i].y_offset;
	pos[i].y_advance -= d;
	pos[i].y_offset  -= d;

	pos[j].y_advance  =  entry_y;
	break;
      case HB_DIRECTION_INVALID:
      default:
	break;
    }

    /* Cross-direction adjustment */
    if  (c->lookup_props & LookupFlag::RightToLeft) {
      pos[i].cursive_chain() = j - i;
      if (likely (HB_DIRECTION_IS_HORIZONTAL (c->direction)))
	pos[i].y_offset = entry_y - exit_y;
      else
	pos[i].x_offset = entry_x - exit_x;
    } else {
      pos[j].cursive_chain() = i - j;
      if (likely (HB_DIRECTION_IS_HORIZONTAL (c->direction)))
	pos[j].y_offset = exit_y - entry_y;
      else
	pos[j].x_offset = exit_x - entry_x;
    }

    c->buffer->idx = j;
    return true;
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return coverage.sanitize (c, this)
	&& entryExitRecord.sanitize (c, this);
  }

  private:
  USHORT	format;			/* Format identifier--format = 1 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of subtable */
  ArrayOf<EntryExitRecord>
		entryExitRecord;	/* Array of EntryExit records--in
					 * Coverage Index order */
  public:
  DEFINE_SIZE_ARRAY (6, entryExitRecord);
};

struct CursivePos
{
  friend struct PosLookupSubTable;

  private:
  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY ();
    switch (u.format) {
    case 1: return u.format1.apply (c);
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
  CursivePosFormat1	format1;
  } u;
};


typedef AnchorMatrix BaseArray;		/* base-major--
					 * in order of BaseCoverage Index--,
					 * mark-minor--
					 * ordered by class--zero-based. */

struct MarkBasePosFormat1
{
  friend struct MarkBasePos;

  private:
  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY ();
    unsigned int mark_index = (this+markCoverage) (c->buffer->info[c->buffer->idx].codepoint);
    if (likely (mark_index == NOT_COVERED))
      return false;

    /* now we search backwards for a non-mark glyph */
    unsigned int property;
    hb_apply_context_t::mark_skipping_backward_iterator_t skippy_iter (c, c->buffer->idx, 1);
    if (!skippy_iter.prev (&property, LookupFlag::IgnoreMarks))
      return false;

    /* The following assertion is too strong, so we've disabled it. */
    if (!(property & HB_OT_LAYOUT_GLYPH_CLASS_BASE_GLYPH))
    {/*return false;*/}

    unsigned int base_index = (this+baseCoverage) (c->buffer->info[skippy_iter.idx].codepoint);
    if (base_index == NOT_COVERED)
      return false;

    return (this+markArray).apply (c, mark_index, base_index, this+baseArray, classCount, skippy_iter.idx);
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return c->check_struct (this)
        && markCoverage.sanitize (c, this)
	&& baseCoverage.sanitize (c, this)
	&& markArray.sanitize (c, this)
	&& baseArray.sanitize (c, this, (unsigned int) classCount);
  }

  private:
  USHORT	format;			/* Format identifier--format = 1 */
  OffsetTo<Coverage>
		markCoverage;		/* Offset to MarkCoverage table--from
					 * beginning of MarkBasePos subtable */
  OffsetTo<Coverage>
		baseCoverage;		/* Offset to BaseCoverage table--from
					 * beginning of MarkBasePos subtable */
  USHORT	classCount;		/* Number of classes defined for marks */
  OffsetTo<MarkArray>
		markArray;		/* Offset to MarkArray table--from
					 * beginning of MarkBasePos subtable */
  OffsetTo<BaseArray>
		baseArray;		/* Offset to BaseArray table--from
					 * beginning of MarkBasePos subtable */
  public:
  DEFINE_SIZE_STATIC (12);
};

struct MarkBasePos
{
  friend struct PosLookupSubTable;

  private:
  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY ();
    switch (u.format) {
    case 1: return u.format1.apply (c);
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
  MarkBasePosFormat1	format1;
  } u;
};


typedef AnchorMatrix LigatureAttach;	/* component-major--
					 * in order of writing direction--,
					 * mark-minor--
					 * ordered by class--zero-based. */

typedef OffsetListOf<LigatureAttach> LigatureArray;
					/* Array of LigatureAttach
					 * tables ordered by
					 * LigatureCoverage Index */

struct MarkLigPosFormat1
{
  friend struct MarkLigPos;

  private:
  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY ();
    unsigned int mark_index = (this+markCoverage) (c->buffer->info[c->buffer->idx].codepoint);
    if (likely (mark_index == NOT_COVERED))
      return false;

    /* now we search backwards for a non-mark glyph */
    unsigned int property;
    hb_apply_context_t::mark_skipping_backward_iterator_t skippy_iter (c, c->buffer->idx, 1);
    if (!skippy_iter.prev (&property, LookupFlag::IgnoreMarks))
      return false;

    /* The following assertion is too strong, so we've disabled it. */
    if (!(property & HB_OT_LAYOUT_GLYPH_CLASS_LIGATURE))
    {/*return false;*/}

    unsigned int j = skippy_iter.idx;
    unsigned int lig_index = (this+ligatureCoverage) (c->buffer->info[j].codepoint);
    if (lig_index == NOT_COVERED)
      return false;

    const LigatureArray& lig_array = this+ligatureArray;
    const LigatureAttach& lig_attach = lig_array[lig_index];

    /* Find component to attach to */
    unsigned int comp_count = lig_attach.rows;
    if (unlikely (!comp_count))
      return false;
    unsigned int comp_index;
    /* We must now check whether the ligature ID of the current mark glyph
     * is identical to the ligature ID of the found ligature.  If yes, we
     * can directly use the component index.  If not, we attach the mark
     * glyph to the last component of the ligature. */
    if (c->buffer->info[j].lig_id() && c->buffer->info[j].lig_id() == c->buffer->info[c->buffer->idx].lig_id() && c->buffer->info[c->buffer->idx].lig_comp())
    {
      comp_index = c->buffer->info[c->buffer->idx].lig_comp() - 1;
      if (comp_index >= comp_count)
	comp_index = comp_count - 1;
    }
    else
      comp_index = comp_count - 1;

    return (this+markArray).apply (c, mark_index, comp_index, lig_attach, classCount, j);
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return c->check_struct (this)
        && markCoverage.sanitize (c, this)
	&& ligatureCoverage.sanitize (c, this)
	&& markArray.sanitize (c, this)
	&& ligatureArray.sanitize (c, this, (unsigned int) classCount);
  }

  private:
  USHORT	format;			/* Format identifier--format = 1 */
  OffsetTo<Coverage>
		markCoverage;		/* Offset to Mark Coverage table--from
					 * beginning of MarkLigPos subtable */
  OffsetTo<Coverage>
		ligatureCoverage;	/* Offset to Ligature Coverage
					 * table--from beginning of MarkLigPos
					 * subtable */
  USHORT	classCount;		/* Number of defined mark classes */
  OffsetTo<MarkArray>
		markArray;		/* Offset to MarkArray table--from
					 * beginning of MarkLigPos subtable */
  OffsetTo<LigatureArray>
		ligatureArray;		/* Offset to LigatureArray table--from
					 * beginning of MarkLigPos subtable */
  public:
  DEFINE_SIZE_STATIC (12);
};

struct MarkLigPos
{
  friend struct PosLookupSubTable;

  private:
  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY ();
    switch (u.format) {
    case 1: return u.format1.apply (c);
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
  MarkLigPosFormat1	format1;
  } u;
};


typedef AnchorMatrix Mark2Array;	/* mark2-major--
					 * in order of Mark2Coverage Index--,
					 * mark1-minor--
					 * ordered by class--zero-based. */

struct MarkMarkPosFormat1
{
  friend struct MarkMarkPos;

  private:
  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY ();
    unsigned int mark1_index = (this+mark1Coverage) (c->buffer->info[c->buffer->idx].codepoint);
    if (likely (mark1_index == NOT_COVERED))
      return false;

    /* now we search backwards for a suitable mark glyph until a non-mark glyph */
    unsigned int property;
    hb_apply_context_t::mark_skipping_backward_iterator_t skippy_iter (c, c->buffer->idx, 1);
    if (!skippy_iter.prev (&property))
      return false;

    if (!(property & HB_OT_LAYOUT_GLYPH_CLASS_MARK))
      return false;

    unsigned int j = skippy_iter.idx;

    /* Two marks match only if they belong to the same base, or same component
     * of the same ligature.  That is, the component numbers must match, and
     * if those are non-zero, the ligid number should also match. */
    if ((c->buffer->info[j].lig_comp() != c->buffer->info[c->buffer->idx].lig_comp()) ||
	(c->buffer->info[j].lig_comp() && c->buffer->info[j].lig_id() != c->buffer->info[c->buffer->idx].lig_id()))
      return false;

    unsigned int mark2_index = (this+mark2Coverage) (c->buffer->info[j].codepoint);
    if (mark2_index == NOT_COVERED)
      return false;

    return (this+mark1Array).apply (c, mark1_index, mark2_index, this+mark2Array, classCount, j);
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return c->check_struct (this)
	&& mark1Coverage.sanitize (c, this)
	&& mark2Coverage.sanitize (c, this)
	&& mark1Array.sanitize (c, this)
	&& mark2Array.sanitize (c, this, (unsigned int) classCount);
  }

  private:
  USHORT	format;			/* Format identifier--format = 1 */
  OffsetTo<Coverage>
		mark1Coverage;		/* Offset to Combining Mark1 Coverage
					 * table--from beginning of MarkMarkPos
					 * subtable */
  OffsetTo<Coverage>
		mark2Coverage;		/* Offset to Combining Mark2 Coverage
					 * table--from beginning of MarkMarkPos
					 * subtable */
  USHORT	classCount;		/* Number of defined mark classes */
  OffsetTo<MarkArray>
		mark1Array;		/* Offset to Mark1Array table--from
					 * beginning of MarkMarkPos subtable */
  OffsetTo<Mark2Array>
		mark2Array;		/* Offset to Mark2Array table--from
					 * beginning of MarkMarkPos subtable */
  public:
  DEFINE_SIZE_STATIC (12);
};

struct MarkMarkPos
{
  friend struct PosLookupSubTable;

  private:
  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY ();
    switch (u.format) {
    case 1: return u.format1.apply (c);
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
  MarkMarkPosFormat1	format1;
  } u;
};


static inline bool position_lookup (hb_apply_context_t *c, unsigned int lookup_index);

struct ContextPos : Context
{
  friend struct PosLookupSubTable;

  private:
  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY ();
    return Context::apply (c, position_lookup);
  }
};

struct ChainContextPos : ChainContext
{
  friend struct PosLookupSubTable;

  private:
  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY ();
    return ChainContext::apply (c, position_lookup);
  }
};


struct ExtensionPos : Extension
{
  friend struct PosLookupSubTable;

  private:
  inline const struct PosLookupSubTable& get_subtable (void) const
  {
    unsigned int offset = get_offset ();
    if (unlikely (!offset)) return Null(PosLookupSubTable);
    return StructAtOffset<PosLookupSubTable> (this, offset);
  }

  inline bool apply (hb_apply_context_t *c) const;

  inline bool sanitize (hb_sanitize_context_t *c);
};



/*
 * PosLookup
 */


struct PosLookupSubTable
{
  friend struct PosLookup;

  enum Type {
    Single		= 1,
    Pair		= 2,
    Cursive		= 3,
    MarkBase		= 4,
    MarkLig		= 5,
    MarkMark		= 6,
    Context		= 7,
    ChainContext	= 8,
    Extension		= 9
  };

  inline bool apply (hb_apply_context_t *c, unsigned int lookup_type) const
  {
    TRACE_APPLY ();
    switch (lookup_type) {
    case Single:		return u.single.apply (c);
    case Pair:			return u.pair.apply (c);
    case Cursive:		return u.cursive.apply (c);
    case MarkBase:		return u.markBase.apply (c);
    case MarkLig:		return u.markLig.apply (c);
    case MarkMark:		return u.markMark.apply (c);
    case Context:		return u.c.apply (c);
    case ChainContext:		return u.chainContext.apply (c);
    case Extension:		return u.extension.apply (c);
    default:return false;
    }
  }

  inline bool sanitize (hb_sanitize_context_t *c, unsigned int lookup_type) {
    TRACE_SANITIZE ();
    switch (lookup_type) {
    case Single:		return u.single.sanitize (c);
    case Pair:			return u.pair.sanitize (c);
    case Cursive:		return u.cursive.sanitize (c);
    case MarkBase:		return u.markBase.sanitize (c);
    case MarkLig:		return u.markLig.sanitize (c);
    case MarkMark:		return u.markMark.sanitize (c);
    case Context:		return u.c.sanitize (c);
    case ChainContext:		return u.chainContext.sanitize (c);
    case Extension:		return u.extension.sanitize (c);
    default:return true;
    }
  }

  private:
  union {
  USHORT		sub_format;
  SinglePos		single;
  PairPos		pair;
  CursivePos		cursive;
  MarkBasePos		markBase;
  MarkLigPos		markLig;
  MarkMarkPos		markMark;
  ContextPos		c;
  ChainContextPos	chainContext;
  ExtensionPos		extension;
  } u;
  public:
  DEFINE_SIZE_UNION (2, sub_format);
};


struct PosLookup : Lookup
{
  inline const PosLookupSubTable& get_subtable (unsigned int i) const
  { return this+CastR<OffsetArrayOf<PosLookupSubTable> > (subTable)[i]; }

  inline bool apply_once (hb_apply_context_t *c) const
  {
    unsigned int lookup_type = get_type ();

    if (!_hb_ot_layout_check_glyph_property (c->face, &c->buffer->info[c->buffer->idx], c->lookup_props, &c->property))
      return false;

    for (unsigned int i = 0; i < get_subtable_count (); i++)
      if (get_subtable (i).apply (c, lookup_type))
	return true;

    return false;
  }

   inline bool apply_string (hb_font_t   *font,
			     hb_buffer_t *buffer,
			     hb_mask_t    mask) const
  {
    bool ret = false;

    if (unlikely (!buffer->len))
      return false;

    hb_apply_context_t c (font, font->face, buffer, mask, *this);

    buffer->idx = 0;
    while (buffer->idx < buffer->len)
    {
      if ((buffer->info[buffer->idx].mask & mask) && apply_once (&c))
	ret = true;
      else
	buffer->idx++;
    }

    return ret;
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    if (unlikely (!Lookup::sanitize (c))) return false;
    OffsetArrayOf<PosLookupSubTable> &list = CastR<OffsetArrayOf<PosLookupSubTable> > (subTable);
    return list.sanitize (c, this, get_type ());
  }
};

typedef OffsetListOf<PosLookup> PosLookupList;

/*
 * GPOS -- The Glyph Positioning Table
 */

struct GPOS : GSUBGPOS
{
  static const hb_tag_t Tag	= HB_OT_TAG_GPOS;

  inline const PosLookup& get_lookup (unsigned int i) const
  { return CastR<PosLookup> (GSUBGPOS::get_lookup (i)); }

  inline bool position_lookup (hb_font_t    *font,
			       hb_buffer_t  *buffer,
			       unsigned int  lookup_index,
			       hb_mask_t     mask) const
  { return get_lookup (lookup_index).apply_string (font, buffer, mask); }

  static inline void position_start (hb_buffer_t *buffer);
  static inline void position_finish (hb_buffer_t *buffer);

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    if (unlikely (!GSUBGPOS::sanitize (c))) return false;
    OffsetTo<PosLookupList> &list = CastR<OffsetTo<PosLookupList> > (lookupList);
    return list.sanitize (c, this);
  }
  public:
  DEFINE_SIZE_STATIC (10);
};


static void
fix_cursive_minor_offset (hb_glyph_position_t *pos, unsigned int i, hb_direction_t direction)
{
    unsigned int j = pos[i].cursive_chain();
    if (likely (!j))
      return;

    j += i;

    pos[i].cursive_chain() = 0;

    fix_cursive_minor_offset (pos, j, direction);

    if (HB_DIRECTION_IS_HORIZONTAL (direction))
      pos[i].y_offset += pos[j].y_offset;
    else
      pos[i].x_offset += pos[j].x_offset;
}

static void
fix_mark_attachment (hb_glyph_position_t *pos, unsigned int i, hb_direction_t direction)
{
  if (likely (!(pos[i].attach_lookback())))
    return;

  unsigned int j = i - pos[i].attach_lookback();

  pos[i].x_advance = 0;
  pos[i].y_advance = 0;
  pos[i].x_offset += pos[j].x_offset;
  pos[i].y_offset += pos[j].y_offset;

  if (HB_DIRECTION_IS_FORWARD (direction))
    for (unsigned int k = j; k < i; k++) {
      pos[i].x_offset -= pos[k].x_advance;
      pos[i].y_offset -= pos[k].y_advance;
    }
  else
    for (unsigned int k = j + 1; k < i + 1; k++) {
      pos[i].x_offset += pos[k].x_advance;
      pos[i].y_offset += pos[k].y_advance;
    }
}

void
GPOS::position_start (hb_buffer_t *buffer)
{
  buffer->clear_positions ();

  unsigned int count = buffer->len;
  for (unsigned int i = 0; i < count; i++)
    buffer->pos[i].attach_lookback() = buffer->pos[i].cursive_chain() = 0;
}

void
GPOS::position_finish (hb_buffer_t *buffer)
{
  unsigned int len;
  hb_glyph_position_t *pos = hb_buffer_get_glyph_positions (buffer, &len);
  hb_direction_t direction = buffer->props.direction;

  /* Handle cursive connections */
  for (unsigned int i = 0; i < len; i++)
    fix_cursive_minor_offset (pos, i, direction);

  /* Handle attachments */
  for (unsigned int i = 0; i < len; i++)
    fix_mark_attachment (pos, i, direction);

  HB_BUFFER_DEALLOCATE_VAR (buffer, lig_comp);
  HB_BUFFER_DEALLOCATE_VAR (buffer, lig_id);
  HB_BUFFER_DEALLOCATE_VAR (buffer, props_cache);
}


/* Out-of-class implementation for methods recursing */

inline bool ExtensionPos::apply (hb_apply_context_t *c) const
{
  TRACE_APPLY ();
  return get_subtable ().apply (c, get_type ());
}

inline bool ExtensionPos::sanitize (hb_sanitize_context_t *c)
{
  TRACE_SANITIZE ();
  if (unlikely (!Extension::sanitize (c))) return false;
  unsigned int offset = get_offset ();
  if (unlikely (!offset)) return true;
  return StructAtOffset<PosLookupSubTable> (this, offset).sanitize (c, get_type ());
}

static inline bool position_lookup (hb_apply_context_t *c, unsigned int lookup_index)
{
  const GPOS &gpos = *(c->face->ot_layout->gpos);
  const PosLookup &l = gpos.get_lookup (lookup_index);

  if (unlikely (c->nesting_level_left == 0))
    return false;

  if (unlikely (c->context_length < 1))
    return false;

  hb_apply_context_t new_c (*c, l);
  return l.apply_once (&new_c);
}


#undef attach_lookback
#undef cursive_chain



#endif /* HB_OT_LAYOUT_GPOS_TABLE_HH */
