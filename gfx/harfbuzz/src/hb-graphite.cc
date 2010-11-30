/*
 * Copyright (C) 2009  Martin Hosken
 * Copyright (C) 2009  SIL International
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

#include <graphite/GrClient.h>
#include <graphite/ITextSource.h>
#include <graphite/GrData.h>
#include <graphite/GrConstants.h>
#include <graphite/Segment.h>
#include "hb-buffer-private.hh"
#include "hb-font-private.h"
#include "hb-graphite.h"
#include <map>

HB_BEGIN_DECLS


namespace TtfUtil
{
extern int FontAscent(const void *pOS2);
extern int FontDescent(const void *pOS2);
extern int DesignUnits(const void *pHead);
extern bool FontOs2Style(const void *pOS2, bool &fBold, bool &fItalic);
}

typedef struct _featureSetting {
    unsigned int id;
    int value;
} featureSetting;

class HbGrBufferTextSrc : public gr::ITextSource
{
public:
  HbGrBufferTextSrc(hb_buffer_t *buff, hb_feature_t *feats, unsigned int num_features)
  {
    hb_feature_t *aFeat = feats;
    featureSetting *aNewFeat;

    buffer = hb_buffer_reference(buff);
    features = new featureSetting[num_features];
    nFeatures = num_features;
    aNewFeat = features;
    for (unsigned int i = 0; i < num_features; i++, aFeat++, aNewFeat++)
    {
        aNewFeat->id = aFeat->tag;
        aNewFeat->value = aFeat->value;
    }
  };
  ~HbGrBufferTextSrc() { hb_buffer_destroy(buffer); delete[] features; };
  virtual gr::UtfType utfEncodingForm() { return gr::kutf32; };
  virtual size_t getLength() { return buffer->len; };
  virtual size_t fetch(gr::toffset ichMin, size_t cch, gr::utf32 * prgchBuffer)
  {
    assert(cch <= buffer->len);
    if (cch > buffer->len)
      return 0;
    for (unsigned int i = ichMin; i < ichMin + cch; i++)
      prgchBuffer[i - ichMin] = buffer->info[i].codepoint;
    return (cch - ichMin);
  };
  virtual size_t fetch(gr::toffset ichMin, size_t cch, gr::utf16 * prgchBuffer) { return 0 ;};
  virtual size_t fetch(gr::toffset ichMin, size_t cch, gr::utf8 * prgchBuffer) { return 0; };
  virtual bool getRightToLeft(gr::toffset ich)
  { return hb_buffer_get_direction(buffer) == HB_DIRECTION_RTL; };
  virtual unsigned int getDirectionDepth(gr::toffset ich)
  { return hb_buffer_get_direction(buffer) == HB_DIRECTION_RTL ? 1 : 0; };
  virtual float getVerticalOffset(gr::toffset ich) { return 0; };
  virtual gr::isocode getLanguage(gr::toffset ich)
  {
    gr::isocode aLang;
    char *p = (char *)(buffer->language);
    int i;
    for (i = 0; i < 4; i++)
    {
      if (p != NULL)
        aLang.rgch[i] = *p;
      else
        aLang.rgch[i] = 0;
      if (p && *p)
        p++;
    }
    return aLang;
  }

  virtual std::pair<gr::toffset, gr::toffset> propertyRange(gr::toffset ich)
  { return std::pair<gr::toffset, gr::toffset>(0, buffer->len); };
  virtual size_t getFontFeatures(gr::toffset ich, gr::FeatureSetting * prgfset)
  {
    featureSetting *aFeat = features;
    for (unsigned int i = 0; i < nFeatures; i++, aFeat++, prgfset++)
    {
      prgfset->id = aFeat->id;
      prgfset->value = aFeat->value;
    }
    return nFeatures;
  }
  virtual bool sameSegment(gr::toffset ich1, gr::toffset ich2) {return true; };

private:
  hb_buffer_t   *buffer;
  featureSetting   *features;
  unsigned int nFeatures;
};

class HbGrFont : public gr::Font
{
public:
  HbGrFont(hb_font_t *font, hb_face_t *face) : gr::Font()
  { m_font = hb_font_reference(font); m_face = hb_face_reference(face); initfont(); };
  ~HbGrFont()
  {
    std::map<hb_tag_t,hb_blob_t *>::iterator p = m_blobs.begin();
    while (p != m_blobs.end())
    { hb_blob_destroy((p++)->second); }
    hb_font_destroy(m_font);
    hb_face_destroy(m_face);
  };
  HbGrFont (const HbGrFont &font) : gr::Font(font)
  {
    *this = font;
    m_blobs = std::map<hb_tag_t, hb_blob_t *>(font.m_blobs);
    std::map<hb_tag_t,hb_blob_t *>::iterator p=m_blobs.begin();
    while (p != m_blobs.end()) { hb_blob_reference((*p++).second); }
    hb_font_reference(m_font);
    hb_face_reference(m_face);
  };
  virtual HbGrFont *copyThis() { return new HbGrFont(*this); };
  virtual bool bold() { return m_bold; };
  virtual bool italic() { return m_italic; };
  virtual float ascent() { float asc; getFontMetrics(&asc, NULL, NULL); return asc; };
  virtual float descent() { float desc; getFontMetrics(NULL, &desc, NULL); return desc; };
  virtual float height()
  { float asc, desc; getFontMetrics(&asc, &desc, NULL); return (asc + desc); };
  virtual unsigned int getDPIx() { return m_font->x_ppem; };
  virtual unsigned int getDPIy() { return m_font->y_ppem; };
  virtual const void *getTable(gr::fontTableId32 tableID, size_t *pcbsize)
  {
    hb_blob_t *blob;
    std::map<hb_tag_t,hb_blob_t *>::iterator p=m_blobs.find((hb_tag_t)tableID);
    if (p == m_blobs.end())
    {
      blob = hb_face_get_table(m_face, (hb_tag_t)tableID);
      m_blobs[(hb_tag_t)tableID] = blob;
    }
    else
    { blob = p->second; }

    const char *res = hb_blob_lock(blob);
    if (pcbsize)
      *pcbsize = hb_blob_get_length(blob);
    hb_blob_unlock(blob);
    return (const void *)res;
  }

  virtual void getFontMetrics(float *pAscent, float *pDescent, float *pEmSquare)
  {
    if (pAscent) *pAscent = 1. * m_ascent * m_font->y_ppem / m_emsquare;
    if (pDescent) *pDescent = 1. * m_descent * m_font->y_ppem / m_emsquare;
    if (pEmSquare) *pEmSquare = m_font->x_scale;
  }
  virtual void getGlyphPoint(gr::gid16 glyphID, unsigned int pointNum, gr::Point &pointReturn)
  {
    hb_position_t x, y;
    hb_font_get_contour_point(m_font, m_face, pointNum, glyphID, &x, &y);
    pointReturn.x = (float)x;
    pointReturn.y = (float)y;
  }

  virtual void getGlyphMetrics(gr::gid16 glyphID, gr::Rect &boundingBox, gr::Point &advances)
  {
    hb_glyph_metrics_t metrics;
    hb_font_get_glyph_metrics(m_font, m_face, glyphID, &metrics);
    boundingBox.top = (metrics.y_offset + metrics.height);
    boundingBox.bottom = metrics.y_offset;
    boundingBox.left = metrics.x_offset;
    boundingBox.right = (metrics.x_offset + metrics.width);
    advances.x = metrics.x_advance;
    advances.y = metrics.y_advance;
//    fprintf (stderr, "%d: (%d, %d, %d, %d)+(%d, %d)\n", glyphID, metrics.x_offset, metrics.y_offset, metrics.width, metrics.height, metrics.x_advance, metrics.y_advance);
  }

private:
  HB_INTERNAL void initfont();

  hb_font_t *m_font;
  hb_face_t *m_face;
  float m_ascent;
  float m_descent;
  float m_emsquare;
  bool m_bold;
  bool m_italic;
  std::map<hb_tag_t, hb_blob_t *> m_blobs;
};

void HbGrFont::initfont()
{
  const void *pOS2 = getTable(gr::kttiOs2, NULL);
  const void *pHead = getTable(gr::kttiHead, NULL);
  TtfUtil::FontOs2Style(pOS2, m_bold, m_italic);
  m_ascent = static_cast<float>(TtfUtil::FontAscent(pOS2));
  m_descent = static_cast<float>(TtfUtil::FontDescent(pOS2));
  m_emsquare = static_cast<float>(TtfUtil::DesignUnits(pHead));
}

void
hb_graphite_shape (hb_font_t    *font,
		   hb_face_t    *face,
		   hb_buffer_t  *buffer,
		   hb_feature_t *features,
		   unsigned int  num_features)
{
  /* create text source */
  HbGrBufferTextSrc   textSrc(buffer, features, num_features);

  /* create grfont */
  HbGrFont          grfont(font, face);

  /* create segment */
  int *firsts;
  bool *flags;
  int numChars;
  int numGlyphs;
  gr::LayoutEnvironment layout;
  std::pair<gr::GlyphIterator, gr::GlyphIterator>glyph_range;
  gr::GlyphIterator iGlyph;
  hb_codepoint_t *glyph_infos, *pGlyph;
  hb_glyph_position_t *pPosition;
  int cGlyph = 0;
  int cChar = 0;

  layout.setStartOfLine(0);
  layout.setEndOfLine(0);
  layout.setDumbFallback(true);
  layout.setJustifier(NULL);
  layout.setRightToLeft(false);

  gr::RangeSegment pSegment(&grfont, &textSrc, &layout, (gr::toffset)0,
        static_cast<gr::toffset>(buffer->len), (gr::Segment *)NULL);

  /* fill in buffer from segment */
  _hb_buffer_clear_output(buffer);
  pSegment.getUniscribeClusters(NULL, 0, &numChars, NULL, 0, &numGlyphs);
  firsts = new int[numChars];
  flags = new bool[numGlyphs];
  glyph_infos = new hb_codepoint_t[numGlyphs];
  hb_buffer_ensure(buffer, numGlyphs);
  pSegment.getUniscribeClusters(firsts, numChars, NULL, flags, numGlyphs, NULL);
  glyph_range = pSegment.glyphs();
  for (pGlyph = glyph_infos, iGlyph = glyph_range.first; iGlyph != glyph_range.second;
        iGlyph++, pGlyph++)
  { *pGlyph = iGlyph->glyphID(); }

  while (cGlyph < numGlyphs)
  {
    if (flags[cGlyph])
    {
        int oldcChar = cChar++;
        int oldcGlyph = cGlyph++;
        while (cChar < numChars && firsts[cChar] == firsts[oldcChar]) cChar++;
        while (cGlyph < numGlyphs && !flags[cGlyph]) cGlyph++;
        _hb_buffer_add_output_glyphs(buffer, cChar - oldcChar, cGlyph - oldcGlyph,
                glyph_infos + oldcGlyph, 0xFFFF, 0xFFFF);
    }
    else
    { cGlyph++; }   /* This should never happen */
  }

  float curradvx = 0., curradvy = 0.;
  for (pPosition = hb_buffer_get_glyph_positions(buffer), iGlyph = glyph_range.first;
        iGlyph != glyph_range.second; pPosition++, iGlyph++)
  {
    pPosition->x_offset = iGlyph->origin() - curradvx;
    pPosition->y_offset = iGlyph->yOffset() - curradvy;
    pPosition->x_advance = pPosition->x_offset + iGlyph->advanceWidth();
    pPosition->y_advance = pPosition->y_offset + iGlyph->advanceHeight();
    if (pPosition->x_advance < 0 && iGlyph->logicalIndex() != iGlyph->attachedClusterBase()->logicalIndex())
        pPosition->x_advance = 0;
    curradvx += pPosition->x_advance;
    curradvy += pPosition->y_advance;
//    fprintf(stderr, "%d@(%f, %f)+(%f, %f)\n", iGlyph->glyphID(), iGlyph->origin(), iGlyph->yOffset(), iGlyph->advanceWidth(), iGlyph->advanceHeight());
  }

  delete[] glyph_infos;
  delete[] firsts;
  delete[] flags;
}


HB_END_DECLS
