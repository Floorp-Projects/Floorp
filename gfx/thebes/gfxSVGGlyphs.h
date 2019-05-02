/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_SVG_GLYPHS_WRAPPER_H
#define GFX_SVG_GLYPHS_WRAPPER_H

#include "gfxFontUtils.h"
#include "mozilla/gfx/2D.h"
#include "nsString.h"
#include "nsClassHashtable.h"
#include "nsBaseHashtable.h"
#include "nsHashKeys.h"
#include "gfxPattern.h"
#include "mozilla/gfx/UserData.h"
#include "mozilla/SVGContextPaint.h"
#include "nsRefreshDriver.h"

class nsIContentViewer;
class gfxSVGGlyphs;

namespace mozilla {
class PresShell;
class SVGContextPaint;
namespace dom {
class Document;
class Element;
}  // namespace dom
}  // namespace mozilla

/**
 * Wraps an SVG document contained in the SVG table of an OpenType font.
 * There may be multiple SVG documents in an SVG table which we lazily parse
 *   so we have an instance of this class for every document in the SVG table
 *   which contains a glyph ID which has been used
 * Finds and looks up elements contained in the SVG document which have glyph
 *   mappings to be drawn by gfxSVGGlyphs
 */
class gfxSVGGlyphsDocument final : public nsAPostRefreshObserver {
  typedef mozilla::dom::Element Element;

 public:
  gfxSVGGlyphsDocument(const uint8_t* aBuffer, uint32_t aBufLen,
                       gfxSVGGlyphs* aSVGGlyphs);

  Element* GetGlyphElement(uint32_t aGlyphId);

  ~gfxSVGGlyphsDocument();

  void DidRefresh() override;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 private:
  nsresult ParseDocument(const uint8_t* aBuffer, uint32_t aBufLen);

  nsresult SetupPresentation();

  void FindGlyphElements(Element* aElement);

  void InsertGlyphId(Element* aGlyphElement);

  // Weak so as not to create a cycle. mOwner owns us so this can't dangle.
  gfxSVGGlyphs* mOwner;
  RefPtr<mozilla::dom::Document> mDocument;
  nsCOMPtr<nsIContentViewer> mViewer;
  RefPtr<mozilla::PresShell> mPresShell;

  nsBaseHashtable<nsUint32HashKey, Element*, Element*> mGlyphIdMap;

  nsCString mSVGGlyphsDocumentURI;
};

/**
 * Used by |gfxFontEntry| to represent the SVG table of an OpenType font.
 * Handles lazy parsing of the SVG documents in the table, looking up SVG glyphs
 *   and rendering SVG glyphs.
 * Each |gfxFontEntry| owns at most one |gfxSVGGlyphs| instance.
 */
class gfxSVGGlyphs {
 private:
  typedef mozilla::dom::Element Element;

 public:
  /**
   * @param aSVGTable The SVG table from the OpenType font
   *
   * The gfxSVGGlyphs object takes over ownership of the blob references
   * that are passed in, and will hb_blob_destroy() them when finished;
   * the caller should -not- destroy these references.
   */
  gfxSVGGlyphs(hb_blob_t* aSVGTable, gfxFontEntry* aFontEntry);

  /**
   * Releases our references to the SVG table and cleans up everything else.
   */
  ~gfxSVGGlyphs();

  /**
   * This is called when the refresh driver has ticked.
   */
  void DidRefresh();

  /**
   * Find the |gfxSVGGlyphsDocument| containing an SVG glyph for |aGlyphId|.
   * If |aGlyphId| does not map to an SVG document, return null.
   * If a |gfxSVGGlyphsDocument| has not been created for the document, create
   * one.
   */
  gfxSVGGlyphsDocument* FindOrCreateGlyphsDocument(uint32_t aGlyphId);

  /**
   * Return true iff there is an SVG glyph for |aGlyphId|
   */
  bool HasSVGGlyph(uint32_t aGlyphId);

  /**
   * Render the SVG glyph for |aGlyphId|
   * @param aContextPaint Information on text context paints.
   *   See |SVGContextPaint|.
   */
  void RenderGlyph(gfxContext* aContext, uint32_t aGlyphId,
                   mozilla::SVGContextPaint* aContextPaint);

  /**
   * Get the extents for the SVG glyph associated with |aGlyphId|
   * @param aSVGToAppSpace The matrix mapping the SVG glyph space to the
   *   target context space
   */
  bool GetGlyphExtents(uint32_t aGlyphId, const gfxMatrix& aSVGToAppSpace,
                       gfxRect* aResult);

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 private:
  Element* GetGlyphElement(uint32_t aGlyphId);

  nsClassHashtable<nsUint32HashKey, gfxSVGGlyphsDocument> mGlyphDocs;
  nsBaseHashtable<nsUint32HashKey, Element*, Element*> mGlyphIdMap;

  hb_blob_t* mSVGData;

  // pointer to the font entry that owns this gfxSVGGlyphs object
  gfxFontEntry* MOZ_NON_OWNING_REF mFontEntry;

  const struct Header {
    mozilla::AutoSwap_PRUint16 mVersion;
    mozilla::AutoSwap_PRUint32 mDocIndexOffset;
    mozilla::AutoSwap_PRUint32 mColorPalettesOffset;
  } * mHeader;

  struct IndexEntry {
    mozilla::AutoSwap_PRUint16 mStartGlyph;
    mozilla::AutoSwap_PRUint16 mEndGlyph;
    mozilla::AutoSwap_PRUint32 mDocOffset;
    mozilla::AutoSwap_PRUint32 mDocLength;
  };

  const struct DocIndex {
    mozilla::AutoSwap_PRUint16 mNumEntries;
    IndexEntry mEntries[1]; /* actual length = mNumEntries */
  } * mDocIndex;

  static int CompareIndexEntries(const void* _a, const void* _b);
};

/**
 * XXX This is a complete hack and should die (see bug 1291494).
 *
 * This class is used when code fails to pass through an SVGContextPaint from
 * the context in which we are painting.  In that case we create one of these
 * as a fallback and have it wrap the gfxContext's current gfxPattern and
 * pretend that that is the paint context's fill pattern.  In some contexts
 * that will be the case, in others it will not.  As we convert more code to
 * Moz2D the less likely it is that this hack will work.  It will also make
 * converting to Moz2D harder.
 */
class SimpleTextContextPaint : public mozilla::SVGContextPaint {
 private:
  static const mozilla::gfx::Color sZero;

  static gfxMatrix SetupDeviceToPatternMatrix(gfxPattern* aPattern,
                                              const gfxMatrix& aCTM) {
    if (!aPattern) {
      return gfxMatrix();
    }
    gfxMatrix deviceToUser = aCTM;
    if (!deviceToUser.Invert()) {
      return gfxMatrix(0, 0, 0, 0, 0, 0);  // singular
    }
    return deviceToUser * aPattern->GetMatrix();
  }

 public:
  SimpleTextContextPaint(gfxPattern* aFillPattern, gfxPattern* aStrokePattern,
                         const gfxMatrix& aCTM)
      : mFillPattern(aFillPattern ? aFillPattern : new gfxPattern(sZero)),
        mStrokePattern(aStrokePattern ? aStrokePattern
                                      : new gfxPattern(sZero)) {
    mFillMatrix = SetupDeviceToPatternMatrix(aFillPattern, aCTM);
    mStrokeMatrix = SetupDeviceToPatternMatrix(aStrokePattern, aCTM);
  }

  already_AddRefed<gfxPattern> GetFillPattern(
      const DrawTarget* aDrawTarget, float aOpacity, const gfxMatrix& aCTM,
      imgDrawingParams& aImgParams) override {
    if (mFillPattern) {
      mFillPattern->SetMatrix(aCTM * mFillMatrix);
    }
    RefPtr<gfxPattern> fillPattern = mFillPattern;
    return fillPattern.forget();
  }

  already_AddRefed<gfxPattern> GetStrokePattern(
      const DrawTarget* aDrawTarget, float aOpacity, const gfxMatrix& aCTM,
      imgDrawingParams& aImgParams) override {
    if (mStrokePattern) {
      mStrokePattern->SetMatrix(aCTM * mStrokeMatrix);
    }
    RefPtr<gfxPattern> strokePattern = mStrokePattern;
    return strokePattern.forget();
  }

  float GetFillOpacity() const override { return mFillPattern ? 1.0f : 0.0f; }

  float GetStrokeOpacity() const override {
    return mStrokePattern ? 1.0f : 0.0f;
  }

 private:
  RefPtr<gfxPattern> mFillPattern;
  RefPtr<gfxPattern> mStrokePattern;

  // Device space to pattern space transforms
  gfxMatrix mFillMatrix;
  gfxMatrix mStrokeMatrix;
};

#endif
