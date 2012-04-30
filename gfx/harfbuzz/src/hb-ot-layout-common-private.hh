/*
 * Copyright © 2007,2008,2009  Red Hat, Inc.
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

#ifndef HB_OT_LAYOUT_COMMON_PRIVATE_HH
#define HB_OT_LAYOUT_COMMON_PRIVATE_HH

#include "hb-ot-layout-private.hh"
#include "hb-open-type-private.hh"


#define NO_CONTEXT		((unsigned int) 0x110000)
#define NOT_COVERED		((unsigned int) 0x110000)
#define MAX_NESTING_LEVEL	8



/*
 *
 * OpenType Layout Common Table Formats
 *
 */


/*
 * Script, ScriptList, LangSys, Feature, FeatureList, Lookup, LookupList
 */

template <typename Type>
struct Record
{
  inline int cmp (hb_tag_t a) const {
    return tag.cmp (a);
  }

  inline bool sanitize (hb_sanitize_context_t *c, void *base) {
    TRACE_SANITIZE ();
    return c->check_struct (this)
	&& offset.sanitize (c, base);
  }

  Tag		tag;		/* 4-byte Tag identifier */
  OffsetTo<Type>
		offset;		/* Offset from beginning of object holding
				 * the Record */
  public:
  DEFINE_SIZE_STATIC (6);
};

template <typename Type>
struct RecordArrayOf : SortedArrayOf<Record<Type> > {
  inline const Tag& get_tag (unsigned int i) const
  {
    /* We cheat slightly and don't define separate Null objects
     * for Record types.  Instead, we return the correct Null(Tag)
     * here. */
    if (unlikely (i >= this->len)) return Null(Tag);
    return (*this)[i].tag;
  }
  inline unsigned int get_tags (unsigned int start_offset,
				unsigned int *record_count /* IN/OUT */,
				hb_tag_t     *record_tags /* OUT */) const
  {
    if (record_count) {
      const Record<Type> *arr = this->sub_array (start_offset, record_count);
      unsigned int count = *record_count;
      for (unsigned int i = 0; i < count; i++)
	record_tags[i] = arr[i].tag;
    }
    return this->len;
  }
  inline bool find_index (hb_tag_t tag, unsigned int *index) const
  {
    int i = this->search (tag);
    if (i != -1) {
        if (index) *index = i;
        return true;
    } else {
      if (index) *index = Index::NOT_FOUND_INDEX;
      return false;
    }
  }
};

template <typename Type>
struct RecordListOf : RecordArrayOf<Type>
{
  inline const Type& operator [] (unsigned int i) const
  { return this+RecordArrayOf<Type>::operator [](i).offset; }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return RecordArrayOf<Type>::sanitize (c, this);
  }
};


struct RangeRecord
{
  inline int cmp (hb_codepoint_t g) const {
    hb_codepoint_t a = start, b = end;
    return g < a ? -1 : g <= b ? 0 : +1 ;
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return c->check_struct (this);
  }

  GlyphID	start;		/* First GlyphID in the range */
  GlyphID	end;		/* Last GlyphID in the range */
  USHORT	value;		/* Value */
  public:
  DEFINE_SIZE_STATIC (6);
};
DEFINE_NULL_DATA (RangeRecord, "\000\001");


struct IndexArray : ArrayOf<Index>
{
  inline unsigned int get_indexes (unsigned int start_offset,
				   unsigned int *_count /* IN/OUT */,
				   unsigned int *_indexes /* OUT */) const
  {
    if (_count) {
      const USHORT *arr = this->sub_array (start_offset, _count);
      unsigned int count = *_count;
      for (unsigned int i = 0; i < count; i++)
	_indexes[i] = arr[i];
    }
    return this->len;
  }
};


struct Script;
struct LangSys;
struct Feature;


struct LangSys
{
  inline unsigned int get_feature_count (void) const
  { return featureIndex.len; }
  inline hb_tag_t get_feature_index (unsigned int i) const
  { return featureIndex[i]; }
  inline unsigned int get_feature_indexes (unsigned int start_offset,
					   unsigned int *feature_count /* IN/OUT */,
					   unsigned int *feature_indexes /* OUT */) const
  { return featureIndex.get_indexes (start_offset, feature_count, feature_indexes); }

  inline bool has_required_feature (void) const { return reqFeatureIndex != 0xffff; }
  inline unsigned int get_required_feature_index (void) const
  {
    if (reqFeatureIndex == 0xffff)
      return Index::NOT_FOUND_INDEX;
   return reqFeatureIndex;;
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return c->check_struct (this)
	&& featureIndex.sanitize (c);
  }

  Offset	lookupOrder;	/* = Null (reserved for an offset to a
				 * reordering table) */
  USHORT	reqFeatureIndex;/* Index of a feature required for this
				 * language system--if no required features
				 * = 0xFFFF */
  IndexArray	featureIndex;	/* Array of indices into the FeatureList */
  public:
  DEFINE_SIZE_ARRAY (6, featureIndex);
};
DEFINE_NULL_DATA (LangSys, "\0\0\xFF\xFF");


struct Script
{
  inline unsigned int get_lang_sys_count (void) const
  { return langSys.len; }
  inline const Tag& get_lang_sys_tag (unsigned int i) const
  { return langSys.get_tag (i); }
  inline unsigned int get_lang_sys_tags (unsigned int start_offset,
					 unsigned int *lang_sys_count /* IN/OUT */,
					 hb_tag_t     *lang_sys_tags /* OUT */) const
  { return langSys.get_tags (start_offset, lang_sys_count, lang_sys_tags); }
  inline const LangSys& get_lang_sys (unsigned int i) const
  {
    if (i == Index::NOT_FOUND_INDEX) return get_default_lang_sys ();
    return this+langSys[i].offset;
  }
  inline bool find_lang_sys_index (hb_tag_t tag, unsigned int *index) const
  { return langSys.find_index (tag, index); }

  inline bool has_default_lang_sys (void) const { return defaultLangSys != 0; }
  inline const LangSys& get_default_lang_sys (void) const { return this+defaultLangSys; }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return defaultLangSys.sanitize (c, this)
	&& langSys.sanitize (c, this);
  }

  private:
  OffsetTo<LangSys>
		defaultLangSys;	/* Offset to DefaultLangSys table--from
				 * beginning of Script table--may be Null */
  RecordArrayOf<LangSys>
		langSys;	/* Array of LangSysRecords--listed
				 * alphabetically by LangSysTag */
  public:
  DEFINE_SIZE_ARRAY (4, langSys);
};

typedef RecordListOf<Script> ScriptList;


struct Feature
{
  inline unsigned int get_lookup_count (void) const
  { return lookupIndex.len; }
  inline hb_tag_t get_lookup_index (unsigned int i) const
  { return lookupIndex[i]; }
  inline unsigned int get_lookup_indexes (unsigned int start_index,
					  unsigned int *lookup_count /* IN/OUT */,
					  unsigned int *lookup_tags /* OUT */) const
  { return lookupIndex.get_indexes (start_index, lookup_count, lookup_tags); }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return c->check_struct (this)
	&& lookupIndex.sanitize (c);
  }

  Offset	featureParams;	/* Offset to Feature Parameters table (if one
				 * has been defined for the feature), relative
				 * to the beginning of the Feature Table; = Null
				 * if not required */
  IndexArray	 lookupIndex;	/* Array of LookupList indices */
  public:
  DEFINE_SIZE_ARRAY (4, lookupIndex);
};

typedef RecordListOf<Feature> FeatureList;


struct LookupFlag : USHORT
{
  enum Flags {
    RightToLeft		= 0x0001u,
    IgnoreBaseGlyphs	= 0x0002u,
    IgnoreLigatures	= 0x0004u,
    IgnoreMarks		= 0x0008u,
    IgnoreFlags		= 0x000Eu,
    UseMarkFilteringSet	= 0x0010u,
    Reserved		= 0x00E0u,
    MarkAttachmentType	= 0xFF00u
  };
  public:
  DEFINE_SIZE_STATIC (2);
};

struct Lookup
{
  inline unsigned int get_subtable_count (void) const { return subTable.len; }

  inline unsigned int get_type (void) const { return lookupType; }

  /* lookup_props is a 32-bit integer where the lower 16-bit is LookupFlag and
   * higher 16-bit is mark-filtering-set if the lookup uses one.
   * Not to be confused with glyph_props which is very similar. */
  inline uint32_t get_props (void) const
  {
    unsigned int flag = lookupFlag;
    if (unlikely (flag & LookupFlag::UseMarkFilteringSet))
    {
      const USHORT &markFilteringSet = StructAfter<USHORT> (subTable);
      flag += (markFilteringSet << 16);
    }
    return flag;
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    /* Real sanitize of the subtables is done by GSUB/GPOS/... */
    if (!(c->check_struct (this)
       && subTable.sanitize (c))) return false;
    if (unlikely (lookupFlag & LookupFlag::UseMarkFilteringSet))
    {
      USHORT &markFilteringSet = StructAfter<USHORT> (subTable);
      if (!markFilteringSet.sanitize (c)) return false;
    }
    return true;
  }

  USHORT	lookupType;		/* Different enumerations for GSUB and GPOS */
  USHORT	lookupFlag;		/* Lookup qualifiers */
  ArrayOf<Offset>
		subTable;		/* Array of SubTables */
  USHORT	markFilteringSetX[VAR];	/* Index (base 0) into GDEF mark glyph sets
					 * structure. This field is only present if bit
					 * UseMarkFilteringSet of lookup flags is set. */
  public:
  DEFINE_SIZE_ARRAY2 (6, subTable, markFilteringSetX);
};

typedef OffsetListOf<Lookup> LookupList;


/*
 * Coverage Table
 */

struct CoverageFormat1
{
  friend struct Coverage;

  private:
  inline unsigned int get_coverage (hb_codepoint_t glyph_id) const
  {
    int i = glyphArray.search (glyph_id);
    if (i != -1)
        return i;
    return NOT_COVERED;
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return glyphArray.sanitize (c);
  }

  private:
  USHORT	coverageFormat;	/* Format identifier--format = 1 */
  SortedArrayOf<GlyphID>
		glyphArray;	/* Array of GlyphIDs--in numerical order */
  public:
  DEFINE_SIZE_ARRAY (4, glyphArray);
};

struct CoverageFormat2
{
  friend struct Coverage;

  private:
  inline unsigned int get_coverage (hb_codepoint_t glyph_id) const
  {
    int i = rangeRecord.search (glyph_id);
    if (i != -1) {
      const RangeRecord &range = rangeRecord[i];
      return (unsigned int) range.value + (glyph_id - range.start);
    }
    return NOT_COVERED;
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return rangeRecord.sanitize (c);
  }

  private:
  USHORT	coverageFormat;	/* Format identifier--format = 2 */
  SortedArrayOf<RangeRecord>
		rangeRecord;	/* Array of glyph ranges--ordered by
				 * Start GlyphID. rangeCount entries
				 * long */
  public:
  DEFINE_SIZE_ARRAY (4, rangeRecord);
};

struct Coverage
{
  inline unsigned int operator () (hb_codepoint_t glyph_id) const { return get_coverage (glyph_id); }

  inline unsigned int get_coverage (hb_codepoint_t glyph_id) const
  {
    switch (u.format) {
    case 1: return u.format1.get_coverage(glyph_id);
    case 2: return u.format2.get_coverage(glyph_id);
    default:return NOT_COVERED;
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
  CoverageFormat1	format1;
  CoverageFormat2	format2;
  } u;
  public:
  DEFINE_SIZE_UNION (2, format);
};


/*
 * Class Definition Table
 */

struct ClassDefFormat1
{
  friend struct ClassDef;

  private:
  inline unsigned int get_class (hb_codepoint_t glyph_id) const
  {
    if ((unsigned int) (glyph_id - startGlyph) < classValue.len)
      return classValue[glyph_id - startGlyph];
    return 0;
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return c->check_struct (this)
	&& classValue.sanitize (c);
  }

  USHORT	classFormat;		/* Format identifier--format = 1 */
  GlyphID	startGlyph;		/* First GlyphID of the classValueArray */
  ArrayOf<USHORT>
		classValue;		/* Array of Class Values--one per GlyphID */
  public:
  DEFINE_SIZE_ARRAY (6, classValue);
};

struct ClassDefFormat2
{
  friend struct ClassDef;

  private:
  inline unsigned int get_class (hb_codepoint_t glyph_id) const
  {
    int i = rangeRecord.search (glyph_id);
    if (i != -1)
      return rangeRecord[i].value;
    return 0;
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return rangeRecord.sanitize (c);
  }

  USHORT	classFormat;	/* Format identifier--format = 2 */
  SortedArrayOf<RangeRecord>
		rangeRecord;	/* Array of glyph ranges--ordered by
				 * Start GlyphID */
  public:
  DEFINE_SIZE_ARRAY (4, rangeRecord);
};

struct ClassDef
{
  inline unsigned int operator () (hb_codepoint_t glyph_id) const { return get_class (glyph_id); }

  inline unsigned int get_class (hb_codepoint_t glyph_id) const
  {
    switch (u.format) {
    case 1: return u.format1.get_class(glyph_id);
    case 2: return u.format2.get_class(glyph_id);
    default:return 0;
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
  ClassDefFormat1	format1;
  ClassDefFormat2	format2;
  } u;
  public:
  DEFINE_SIZE_UNION (2, format);
};


/*
 * Device Tables
 */

struct Device
{

  inline hb_position_t get_x_delta (hb_font_t *font) const
  { return get_delta (font->x_ppem, font->x_scale); }

  inline hb_position_t get_y_delta (hb_font_t *font) const
  { return get_delta (font->y_ppem, font->y_scale); }

  inline int get_delta (unsigned int ppem, int scale) const
  {
    if (!ppem) return 0;

    int pixels = get_delta_pixels (ppem);

    if (!pixels) return 0;

    return pixels * (int64_t) scale / ppem;
  }


  inline int get_delta_pixels (unsigned int ppem_size) const
  {
    unsigned int f = deltaFormat;
    if (unlikely (f < 1 || f > 3))
      return 0;

    if (ppem_size < startSize || ppem_size > endSize)
      return 0;

    unsigned int s = ppem_size - startSize;

    unsigned int byte = deltaValue[s >> (4 - f)];
    unsigned int bits = (byte >> (16 - (((s & ((1 << (4 - f)) - 1)) + 1) << f)));
    unsigned int mask = (0xFFFF >> (16 - (1 << f)));

    int delta = bits & mask;

    if ((unsigned int) delta >= ((mask + 1) >> 1))
      delta -= mask + 1;

    return delta;
  }

  inline unsigned int get_size (void) const
  {
    unsigned int f = deltaFormat;
    if (unlikely (f < 1 || f > 3 || startSize > endSize)) return 3 * USHORT::static_size;
    return USHORT::static_size * (4 + ((endSize - startSize) >> (4 - f)));
  }

  inline bool sanitize (hb_sanitize_context_t *c) {
    TRACE_SANITIZE ();
    return c->check_struct (this)
	&& c->check_range (this, this->get_size ());
  }

  private:
  USHORT	startSize;		/* Smallest size to correct--in ppem */
  USHORT	endSize;		/* Largest size to correct--in ppem */
  USHORT	deltaFormat;		/* Format of DeltaValue array data: 1, 2, or 3
					 * 1	Signed 2-bit value, 8 values per uint16
					 * 2	Signed 4-bit value, 4 values per uint16
					 * 3	Signed 8-bit value, 2 values per uint16
					 */
  USHORT	deltaValue[VAR];	/* Array of compressed data */
  public:
  DEFINE_SIZE_ARRAY (6, deltaValue);
};



#endif /* HB_OT_LAYOUT_COMMON_PRIVATE_HH */
