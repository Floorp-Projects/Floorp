/*
 * Copyright © 2015  Ebrahim Byagowi
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
 */

#define HB_SHAPER directwrite
#include "hb-shaper-impl-private.hh"

#include <dwrite.h>

#include "hb-directwrite.h"

#include "hb-open-file-private.hh"
#include "hb-ot-name-table.hh"
#include "hb-ot-tag.h"


#ifndef HB_DEBUG_DIRECTWRITE
#define HB_DEBUG_DIRECTWRITE (HB_DEBUG+0)
#endif

HB_SHAPER_DATA_ENSURE_DECLARE(directwrite, face)
HB_SHAPER_DATA_ENSURE_DECLARE(directwrite, font)

/*
* shaper face data
*/

struct hb_directwrite_shaper_face_data_t {
  HANDLE fh;
  wchar_t face_name[LF_FACESIZE];
};

/* face_name should point to a wchar_t[LF_FACESIZE] object. */
static void
_hb_generate_unique_face_name(wchar_t *face_name, unsigned int *plen)
{
  /* We'll create a private name for the font from a UUID using a simple,
  * somewhat base64-like encoding scheme */
  const char *enc = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";
  UUID id;
  UuidCreate ((UUID*)&id);
  ASSERT_STATIC (2 + 3 * (16 / 2) < LF_FACESIZE);
  unsigned int name_str_len = 0;
  face_name[name_str_len++] = 'F';
  face_name[name_str_len++] = '_';
  unsigned char *p = (unsigned char *)&id;
  for (unsigned int i = 0; i < 16; i += 2)
  {
    /* Spread the 16 bits from two bytes of the UUID across three chars of face_name,
    * using the bits in groups of 5,5,6 to select chars from enc.
    * This will generate 24 characters; with the 'F_' prefix we already provided,
    * the name will be 26 chars (plus the NUL terminator), so will always fit within
    * face_name (LF_FACESIZE = 32). */
    face_name[name_str_len++] = enc[p[i] >> 3];
    face_name[name_str_len++] = enc[((p[i] << 2) | (p[i + 1] >> 6)) & 0x1f];
    face_name[name_str_len++] = enc[p[i + 1] & 0x3f];
  }
  face_name[name_str_len] = 0;
  if (plen)
    *plen = name_str_len;
}

/* Destroys blob. */
static hb_blob_t *
_hb_rename_font(hb_blob_t *blob, wchar_t *new_name)
{
  /* Create a copy of the font data, with the 'name' table replaced by a
   * table that names the font with our private F_* name created above.
   * For simplicity, we just append a new 'name' table and update the
   * sfnt directory; the original table is left in place, but unused.
   *
   * The new table will contain just 5 name IDs: family, style, unique,
   * full, PS. All of them point to the same name data with our unique name.
   */

  blob = OT::Sanitizer<OT::OpenTypeFontFile>::sanitize (blob);

  unsigned int length, new_length, name_str_len;
  const char *orig_sfnt_data = hb_blob_get_data (blob, &length);

  _hb_generate_unique_face_name (new_name, &name_str_len);

  static const uint16_t name_IDs[] = { 1, 2, 3, 4, 6 };

  unsigned int name_table_length = OT::name::min_size +
    ARRAY_LENGTH(name_IDs) * OT::NameRecord::static_size +
    name_str_len * 2; /* for name data in UTF16BE form */
  unsigned int name_table_offset = (length + 3) & ~3;

  new_length = name_table_offset + ((name_table_length + 3) & ~3);
  void *new_sfnt_data = calloc(1, new_length);
  if (!new_sfnt_data)
  {
    hb_blob_destroy (blob);
    return NULL;
  }

  memcpy(new_sfnt_data, orig_sfnt_data, length);

  OT::name &name = OT::StructAtOffset<OT::name> (new_sfnt_data, name_table_offset);
  name.format.set (0);
  name.count.set (ARRAY_LENGTH (name_IDs));
  name.stringOffset.set (name.get_size());
  for (unsigned int i = 0; i < ARRAY_LENGTH (name_IDs); i++)
  {
    OT::NameRecord &record = name.nameRecord[i];
    record.platformID.set(3);
    record.encodingID.set(1);
    record.languageID.set(0x0409u); /* English */
    record.nameID.set(name_IDs[i]);
    record.length.set(name_str_len * 2);
    record.offset.set(0);
  }

  /* Copy string data from new_name, converting wchar_t to UTF16BE. */
  unsigned char *p = &OT::StructAfter<unsigned char>(name);
  for (unsigned int i = 0; i < name_str_len; i++)
  {
    *p++ = new_name[i] >> 8;
    *p++ = new_name[i] & 0xff;
  }

  /* Adjust name table entry to point to new name table */
  const OT::OpenTypeFontFile &file = *(OT::OpenTypeFontFile *) (new_sfnt_data);
  unsigned int face_count = file.get_face_count ();
  for (unsigned int face_index = 0; face_index < face_count; face_index++)
  {
    /* Note: doing multiple edits (ie. TTC) can be unsafe.  There may be
    * toe-stepping.  But we don't really care. */
    const OT::OpenTypeFontFace &face = file.get_face (face_index);
    unsigned int index;
    if (face.find_table_index (HB_OT_TAG_name, &index))
    {
      OT::TableRecord &record = const_cast<OT::TableRecord &> (face.get_table (index));
      record.checkSum.set_for_data (&name, name_table_length);
      record.offset.set (name_table_offset);
      record.length.set (name_table_length);
    }
    else if (face_index == 0) /* Fail if first face doesn't have 'name' table. */
    {
      free (new_sfnt_data);
      hb_blob_destroy (blob);
      return NULL;
    }
  }

  /* The checkSumAdjustment field in the 'head' table is now wrong,
  * but that doesn't actually seem to cause any problems so we don't
  * bother. */

  hb_blob_destroy (blob);
  return hb_blob_create ((const char *)new_sfnt_data, new_length,
    HB_MEMORY_MODE_WRITABLE, NULL, free);
}

hb_directwrite_shaper_face_data_t *
_hb_directwrite_shaper_face_data_create(hb_face_t *face)
{
  hb_directwrite_shaper_face_data_t *data = (hb_directwrite_shaper_face_data_t *)calloc(1, sizeof (hb_directwrite_shaper_face_data_t));
  if (unlikely (!data))
    return NULL;

  hb_blob_t *blob = hb_face_reference_blob (face);
  if (unlikely (!hb_blob_get_length (blob)))
    DEBUG_MSG(DIRECTWRITE, face, "Face has empty blob");

  blob = _hb_rename_font (blob, data->face_name);
  if (unlikely (!blob))
  {
    free(data);
    return NULL;
  }

  DWORD num_fonts_installed;
  data->fh = AddFontMemResourceEx ((void *)hb_blob_get_data(blob, NULL),
    hb_blob_get_length (blob),
    0, &num_fonts_installed);
  if (unlikely (!data->fh))
  {
    DEBUG_MSG (DIRECTWRITE, face, "Face AddFontMemResourceEx() failed");
    free (data);
    return NULL;
  }

  return data;
}

void
_hb_directwrite_shaper_face_data_destroy(hb_directwrite_shaper_face_data_t *data)
{
  RemoveFontMemResourceEx(data->fh);
  free(data);
}


/*
 * shaper font data
 */

struct hb_directwrite_shaper_font_data_t {
  HDC hdc;
  LOGFONTW log_font;
  HFONT hfont;
};

static bool
populate_log_font (LOGFONTW  *lf,
       hb_font_t *font)
{
  memset (lf, 0, sizeof (*lf));
  lf->lfHeight = -font->y_scale;
  lf->lfCharSet = DEFAULT_CHARSET;

  hb_face_t *face = font->face;
  hb_directwrite_shaper_face_data_t *face_data = HB_SHAPER_DATA_GET (face);

  memcpy (lf->lfFaceName, face_data->face_name, sizeof (lf->lfFaceName));

  return true;
}

hb_directwrite_shaper_font_data_t *
_hb_directwrite_shaper_font_data_create (hb_font_t *font)
{
  if (unlikely (!hb_directwrite_shaper_face_data_ensure (font->face))) return NULL;

  hb_directwrite_shaper_font_data_t *data = (hb_directwrite_shaper_font_data_t *) calloc (1, sizeof (hb_directwrite_shaper_font_data_t));
  if (unlikely (!data))
    return NULL;

  data->hdc = GetDC (NULL);

  if (unlikely (!populate_log_font (&data->log_font, font))) {
    DEBUG_MSG (DIRECTWRITE, font, "Font populate_log_font() failed");
    _hb_directwrite_shaper_font_data_destroy (data);
    return NULL;
  }

  data->hfont = CreateFontIndirectW (&data->log_font);
  if (unlikely (!data->hfont)) {
    DEBUG_MSG (DIRECTWRITE, font, "Font CreateFontIndirectW() failed");
    _hb_directwrite_shaper_font_data_destroy (data);
     return NULL;
  }

  if (!SelectObject (data->hdc, data->hfont)) {
    DEBUG_MSG (DIRECTWRITE, font, "Font SelectObject() failed");
    _hb_directwrite_shaper_font_data_destroy (data);
     return NULL;
  }

  return data;
}

void
_hb_directwrite_shaper_font_data_destroy (hb_directwrite_shaper_font_data_t *data)
{
  if (data->hdc)
    ReleaseDC (NULL, data->hdc);
  if (data->hfont)
    DeleteObject (data->hfont);
  free (data);
}

LOGFONTW *
hb_directwrite_font_get_logfontw (hb_font_t *font)
{
  if (unlikely (!hb_directwrite_shaper_font_data_ensure (font))) return NULL;
  hb_directwrite_shaper_font_data_t *font_data =  HB_SHAPER_DATA_GET (font);
  return &font_data->log_font;
}

HFONT
hb_directwrite_font_get_hfont (hb_font_t *font)
{
  if (unlikely (!hb_directwrite_shaper_font_data_ensure (font))) return NULL;
  hb_directwrite_shaper_font_data_t *font_data =  HB_SHAPER_DATA_GET (font);
  return font_data->hfont;
}


/*
 * shaper shape_plan data
 */

struct hb_directwrite_shaper_shape_plan_data_t {};

hb_directwrite_shaper_shape_plan_data_t *
_hb_directwrite_shaper_shape_plan_data_create (hb_shape_plan_t    *shape_plan HB_UNUSED,
               const hb_feature_t *user_features HB_UNUSED,
               unsigned int        num_user_features HB_UNUSED)
{
  return (hb_directwrite_shaper_shape_plan_data_t *) HB_SHAPER_DATA_SUCCEEDED;
}

void
_hb_directwrite_shaper_shape_plan_data_destroy (hb_directwrite_shaper_shape_plan_data_t *data HB_UNUSED)
{
}

// Most of here TextAnalysis is originally written by Bas Schouten for Mozilla project
// but now is relicensed to MIT for HarfBuzz use
class TextAnalysis
  : public IDWriteTextAnalysisSource, public IDWriteTextAnalysisSink
{
public:

  IFACEMETHOD(QueryInterface)(IID const& iid, OUT void** ppObject) { return S_OK; }
  IFACEMETHOD_(ULONG, AddRef)() { return 1; }
  IFACEMETHOD_(ULONG, Release)() { return 1; }

  // A single contiguous run of characters containing the same analysis 
  // results.
  struct Run
  {
    UINT32 mTextStart;   // starting text position of this run
    UINT32 mTextLength;  // number of contiguous code units covered
    UINT32 mGlyphStart;  // starting glyph in the glyphs array
    UINT32 mGlyphCount;  // number of glyphs associated with this run of 
    // text
    DWRITE_SCRIPT_ANALYSIS mScript;
    UINT8 mBidiLevel;
    bool mIsSideways;

    inline bool ContainsTextPosition(UINT32 aTextPosition) const
    {
      return aTextPosition >= mTextStart
        && aTextPosition <  mTextStart + mTextLength;
    }

    Run *nextRun;
  };

public:
  TextAnalysis(const wchar_t* text,
    UINT32 textLength,
    const wchar_t* localeName,
    DWRITE_READING_DIRECTION readingDirection)
    : mText(text)
    , mTextLength(textLength)
    , mLocaleName(localeName)
    , mReadingDirection(readingDirection)
    , mCurrentRun(NULL) { };

  ~TextAnalysis() {
    // delete runs, except mRunHead which is part of the TextAnalysis object
    for (Run *run = mRunHead.nextRun; run;) {
      Run *origRun = run;
      run = run->nextRun;
      delete origRun;
    }
  }

  STDMETHODIMP GenerateResults(IDWriteTextAnalyzer* textAnalyzer,
    Run **runHead) {
    // Analyzes the text using the script analyzer and returns
    // the result as a series of runs.

    HRESULT hr = S_OK;

    // Initially start out with one result that covers the entire range.
    // This result will be subdivided by the analysis processes.
    mRunHead.mTextStart = 0;
    mRunHead.mTextLength = mTextLength;
    mRunHead.mBidiLevel =
      (mReadingDirection == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    mRunHead.nextRun = NULL;
    mCurrentRun = &mRunHead;

    // Call each of the analyzers in sequence, recording their results.
    if (SUCCEEDED(hr = textAnalyzer->AnalyzeScript(this,
      0,
      mTextLength,
      this))) {
      *runHead = &mRunHead;
    }

    return hr;
  }

  // IDWriteTextAnalysisSource implementation

  IFACEMETHODIMP GetTextAtPosition(UINT32 textPosition,
    OUT WCHAR const** textString,
    OUT UINT32* textLength)
  {
    if (textPosition >= mTextLength) {
      // No text at this position, valid query though.
      *textString = NULL;
      *textLength = 0;
    }
    else {
      *textString = mText + textPosition;
      *textLength = mTextLength - textPosition;
    }
    return S_OK;
  }

  IFACEMETHODIMP GetTextBeforePosition(UINT32 textPosition,
    OUT WCHAR const** textString,
    OUT UINT32* textLength)
  {
    if (textPosition == 0 || textPosition > mTextLength) {
      // Either there is no text before here (== 0), or this
      // is an invalid position. The query is considered valid thouh.
      *textString = NULL;
      *textLength = 0;
    }
    else {
      *textString = mText;
      *textLength = textPosition;
    }
    return S_OK;
  }

  IFACEMETHODIMP_(DWRITE_READING_DIRECTION)
    GetParagraphReadingDirection() { return mReadingDirection; }

  IFACEMETHODIMP GetLocaleName(UINT32 textPosition,
    UINT32* textLength,
    WCHAR const** localeName) {
    return S_OK;
  }

  IFACEMETHODIMP
    GetNumberSubstitution(UINT32 textPosition,
    OUT UINT32* textLength,
    OUT IDWriteNumberSubstitution** numberSubstitution)
  {
    // We do not support number substitution.
    *numberSubstitution = NULL;
    *textLength = mTextLength - textPosition;

    return S_OK;
  }

  // IDWriteTextAnalysisSink implementation

  IFACEMETHODIMP
    SetScriptAnalysis(UINT32 textPosition,
    UINT32 textLength,
    DWRITE_SCRIPT_ANALYSIS const* scriptAnalysis)
  {
    SetCurrentRun(textPosition);
    SplitCurrentRun(textPosition);
    while (textLength > 0) {
      Run *run = FetchNextRun(&textLength);
      run->mScript = *scriptAnalysis;
    }

    return S_OK;
  }

  IFACEMETHODIMP
    SetLineBreakpoints(UINT32 textPosition,
    UINT32 textLength,
    const DWRITE_LINE_BREAKPOINT* lineBreakpoints) { return S_OK; }

  IFACEMETHODIMP SetBidiLevel(UINT32 textPosition,
    UINT32 textLength,
    UINT8 explicitLevel,
    UINT8 resolvedLevel) { return S_OK; }

  IFACEMETHODIMP
    SetNumberSubstitution(UINT32 textPosition,
    UINT32 textLength,
    IDWriteNumberSubstitution* numberSubstitution) { return S_OK; }

protected:
  Run *FetchNextRun(IN OUT UINT32* textLength)
  {
    // Used by the sink setters, this returns a reference to the next run.
    // Position and length are adjusted to now point after the current run
    // being returned.

    Run *origRun = mCurrentRun;
    // Split the tail if needed (the length remaining is less than the
    // current run's size).
    if (*textLength < mCurrentRun->mTextLength) {
      SplitCurrentRun(mCurrentRun->mTextStart + *textLength);
    }
    else {
      // Just advance the current run.
      mCurrentRun = mCurrentRun->nextRun;
    }
    *textLength -= origRun->mTextLength;

    // Return a reference to the run that was just current.
    return origRun;
  }

  void SetCurrentRun(UINT32 textPosition)
  {
    // Move the current run to the given position.
    // Since the analyzers generally return results in a forward manner,
    // this will usually just return early. If not, find the
    // corresponding run for the text position.

    if (mCurrentRun && mCurrentRun->ContainsTextPosition(textPosition)) {
      return;
    }

    for (Run *run = &mRunHead; run; run = run->nextRun) {
      if (run->ContainsTextPosition(textPosition)) {
        mCurrentRun = run;
        return;
      }
    }
    //NS_NOTREACHED("We should always be able to find the text position in one \
            //                of our runs");
  }

  void SplitCurrentRun(UINT32 splitPosition)
  {
    if (!mCurrentRun) {
      //NS_ASSERTION(false, "SplitCurrentRun called without current run.");
      // Shouldn't be calling this when no current run is set!
      return;
    }
    // Split the current run.
    if (splitPosition <= mCurrentRun->mTextStart) {
      // No need to split, already the start of a run
      // or before it. Usually the first.
      return;
    }
    Run *newRun = new Run;

    *newRun = *mCurrentRun;

    // Insert the new run in our linked list.
    newRun->nextRun = mCurrentRun->nextRun;
    mCurrentRun->nextRun = newRun;

    // Adjust runs' text positions and lengths.
    UINT32 splitPoint = splitPosition - mCurrentRun->mTextStart;
    newRun->mTextStart += splitPoint;
    newRun->mTextLength -= splitPoint;
    mCurrentRun->mTextLength = splitPoint;
    mCurrentRun = newRun;
  }

protected:
  // Input
  // (weak references are fine here, since this class is a transient
  //  stack-based helper that doesn't need to copy data)
  UINT32 mTextLength;
  const WCHAR* mText;
  const WCHAR* mLocaleName;
  DWRITE_READING_DIRECTION mReadingDirection;

  // Current processing state.
  Run *mCurrentRun;

  // Output is a list of runs starting here
  Run  mRunHead;
};


/*
 * shaper
 */

hb_bool_t
_hb_directwrite_shape(hb_shape_plan_t    *shape_plan,
  hb_font_t          *font,
  hb_buffer_t        *buffer,
  const hb_feature_t *features,
  unsigned int        num_features)
{
  hb_face_t *face = font->face;
  hb_directwrite_shaper_face_data_t *face_data = HB_SHAPER_DATA_GET (face);
  hb_directwrite_shaper_font_data_t *font_data = HB_SHAPER_DATA_GET (font);

  // factory probably should be cached
  IDWriteFactory* dwriteFactory;
  DWriteCreateFactory(
    DWRITE_FACTORY_TYPE_SHARED,
    __uuidof(IDWriteFactory),
    reinterpret_cast<IUnknown**>(&dwriteFactory)
    );

  IDWriteGdiInterop *gdiInterop;
  dwriteFactory->GetGdiInterop (&gdiInterop);
  IDWriteFontFace* fontFace;
  gdiInterop->CreateFontFaceFromHdc (font_data->hdc, &fontFace);

  IDWriteTextAnalyzer* analyzer;
  dwriteFactory->CreateTextAnalyzer (&analyzer);

  unsigned int scratch_size;
  hb_buffer_t::scratch_buffer_t *scratch = buffer->get_scratch_buffer (&scratch_size);
#define ALLOCATE_ARRAY(Type, name, len) \
  Type *name = (Type *) scratch; \
  { \
    unsigned int _consumed = DIV_CEIL ((len) * sizeof (Type), sizeof (*scratch)); \
    assert (_consumed <= scratch_size); \
    scratch += _consumed; \
    scratch_size -= _consumed; \
  }

#define utf16_index() var1.u32

  ALLOCATE_ARRAY(WCHAR, pchars, buffer->len * 2);

  unsigned int chars_len = 0;
  for (unsigned int i = 0; i < buffer->len; i++)
  {
    hb_codepoint_t c = buffer->info[i].codepoint;
    buffer->info[i].utf16_index() = chars_len;
    if (likely(c <= 0xFFFFu))
      pchars[chars_len++] = c;
    else if (unlikely(c > 0x10FFFFu))
      pchars[chars_len++] = 0xFFFDu;
    else {
      pchars[chars_len++] = 0xD800u + ((c - 0x10000u) >> 10);
      pchars[chars_len++] = 0xDC00u + ((c - 0x10000u) & ((1 << 10) - 1));
    }
  }

  ALLOCATE_ARRAY(WORD, log_clusters, chars_len);
  if (num_features)
  {
    /* Need log_clusters to assign features. */
    chars_len = 0;
    for (unsigned int i = 0; i < buffer->len; i++)
    {
      hb_codepoint_t c = buffer->info[i].codepoint;
      unsigned int cluster = buffer->info[i].cluster;
      log_clusters[chars_len++] = cluster;
      if (hb_in_range(c, 0x10000u, 0x10FFFFu))
        log_clusters[chars_len++] = cluster; /* Surrogates. */
    }
  }

  HRESULT hr;
  // TODO: Handle TEST_DISABLE_OPTIONAL_LIGATURES

  DWRITE_READING_DIRECTION readingDirection = buffer->props.direction ? 
    DWRITE_READING_DIRECTION_RIGHT_TO_LEFT :
    DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;

  /*
  * There's an internal 16-bit limit on some things inside the analyzer,
  * but we never attempt to shape a word longer than 64K characters
  * in a single gfxShapedWord, so we cannot exceed that limit.
  */
  UINT32 length = buffer->len;

  TextAnalysis analysis(pchars, length, NULL, readingDirection);
  TextAnalysis::Run *runHead;
  hr = analysis.GenerateResults(analyzer, &runHead);

  if (FAILED(hr)) {
    //NS_WARNING("Analyzer failed to generate results.");
    return false;
  }

  UINT32 maxGlyphs = 3 * length / 2 + 16;

#define INITIAL_GLYPH_SIZE 400
  UINT16* clusters = (UINT16*)malloc(INITIAL_GLYPH_SIZE * sizeof(UINT16));
  UINT16* glyphs = (UINT16*)malloc(INITIAL_GLYPH_SIZE * sizeof(UINT16));
  DWRITE_SHAPING_TEXT_PROPERTIES* textProperties = (DWRITE_SHAPING_TEXT_PROPERTIES*)
    malloc(INITIAL_GLYPH_SIZE * sizeof(DWRITE_SHAPING_TEXT_PROPERTIES));
  DWRITE_SHAPING_GLYPH_PROPERTIES* glyphProperties = (DWRITE_SHAPING_GLYPH_PROPERTIES*)
    malloc(INITIAL_GLYPH_SIZE * sizeof(DWRITE_SHAPING_GLYPH_PROPERTIES));

  UINT32 actualGlyphs;

  bool backward = HB_DIRECTION_IS_BACKWARD(buffer->props.direction);

  wchar_t lang[4];
  mbstowcs(lang, hb_language_to_string(buffer->props.language), 4);
  hr = analyzer->GetGlyphs(pchars, length,
    fontFace, FALSE,
    buffer->props.direction,
    &runHead->mScript, (const wchar_t*)lang, NULL, NULL, NULL, 0,
    maxGlyphs, clusters, textProperties,
    glyphs, glyphProperties, &actualGlyphs);

  if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) {
    free(clusters);
    free(glyphs);
    free(textProperties);
    free(glyphProperties);

    clusters = (UINT16*)malloc(INITIAL_GLYPH_SIZE * sizeof(UINT16));
    glyphs = (UINT16*)malloc(INITIAL_GLYPH_SIZE * sizeof(UINT16));
    textProperties = (DWRITE_SHAPING_TEXT_PROPERTIES*)
      malloc(INITIAL_GLYPH_SIZE * sizeof(DWRITE_SHAPING_TEXT_PROPERTIES));
    glyphProperties = (DWRITE_SHAPING_GLYPH_PROPERTIES*)
      malloc(INITIAL_GLYPH_SIZE * sizeof(DWRITE_SHAPING_GLYPH_PROPERTIES));

    hr = analyzer->GetGlyphs(pchars, length,
      fontFace, FALSE,
      buffer->props.direction,
      &runHead->mScript, (const wchar_t*)lang, NULL, NULL, NULL, 0,
      maxGlyphs, clusters, textProperties,
      glyphs, glyphProperties, &actualGlyphs);
  }
  if (FAILED(hr)) {
    //NS_WARNING("Analyzer failed to get glyphs.");
    return false;
  }

  FLOAT advances[400];
  DWRITE_GLYPH_OFFSET offsets[400];


  /* The -2 in the following is to compensate for possible
   * alignment needed after the WORD array.  sizeof(WORD) == 2. */
  unsigned int glyphs_size = (scratch_size * sizeof (int)-2)
    / (sizeof (WORD) +
    4 + // sizeof (SCRIPT_GLYPHPROP) +
    sizeof (int) +
    8 + // sizeof (GOFFSET) +
    sizeof (uint32_t));
  ALLOCATE_ARRAY(uint32_t, vis_clusters, glyphs_size);

#undef ALLOCATE_ARRAY

  hr = analyzer->GetGlyphPlacements(pchars,
    clusters,
    textProperties,
    length,
    glyphs,
    glyphProperties,
    actualGlyphs,
    fontFace,
    face->get_upem(),
    FALSE,
    FALSE,
    &runHead->mScript,
    NULL,
    NULL,
    NULL,
    0,
    advances,
    offsets);

  if (FAILED(hr)) {
    //NS_WARNING("Analyzer failed to get glyph placements.");
    return false;
  }

  unsigned int glyphs_len = actualGlyphs;

  /* Ok, we've got everything we need, now compose output buffer,
   * very, *very*, carefully! */

  /* Calculate visual-clusters.  That's what we ship. */
  for (unsigned int i = 0; i < glyphs_len; i++)
    vis_clusters[i] = -1;
  for (unsigned int i = 0; i < buffer->len; i++) {
    uint32_t *p = &vis_clusters[log_clusters[buffer->info[i].utf16_index()]];
    //*p = MIN (*p, buffer->info[i].cluster);
  }
  for (unsigned int i = 1; i < glyphs_len; i++)
    if (vis_clusters[i] == -1)
      vis_clusters[i] = vis_clusters[i - 1];

#undef utf16_index

  //if (unlikely (!buffer->ensure (glyphs_len)))
  //  FAIL ("Buffer in error");

#undef FAIL

  /* Set glyph infos */
  buffer->len = 0;
  for (unsigned int i = 0; i < glyphs_len; i++)
  {
    hb_glyph_info_t *info = &buffer->info[buffer->len++];

    info->codepoint = glyphs[i];
    info->cluster = vis_clusters[i];

    /* The rest is crap.  Let's store position info there for now. */
    info->mask = advances[i];
    info->var1.u32 = offsets[i].ascenderOffset;
    info->var2.u32 = -offsets[i].advanceOffset;
  }

  free(clusters);
  free(glyphs);
  free(textProperties);
  free(glyphProperties);

  /* Set glyph positions */
  buffer->clear_positions ();
  for (unsigned int i = 0; i < glyphs_len; i++)
  {
    hb_glyph_info_t *info = &buffer->info[i];
    hb_glyph_position_t *pos = &buffer->pos[i];

    /* TODO vertical */
    pos->x_advance = info->mask;
    pos->x_offset = backward ? -info->var1.u32 : info->var1.u32;
    pos->y_offset = info->var2.u32;
  }

  if (backward)
    hb_buffer_reverse (buffer);

  /* Wow, done! */
  return true;
}
