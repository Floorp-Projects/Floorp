/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Edwin Flores <eflores@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef GFX_SVG_GLYPHS_WRAPPER_H
#define GFX_SVG_GLYPHS_WRAPPER_H

#include "gfxFontUtils.h"
#include "nsString.h"
#include "nsIDocument.h"
#include "nsAutoPtr.h"
#include "nsIContentViewer.h"
#include "nsIPresShell.h"
#include "nsClassHashtable.h"
#include "nsBaseHashtable.h"
#include "nsHashKeys.h"
#include "gfxPattern.h"
#include "gfxFont.h"
#include "mozilla/gfx/UserData.h"


/**
 * Wraps an SVG document contained in the SVG table of an OpenType font.
 * There may be multiple SVG documents in an SVG table which we lazily parse
 *   so we have an instance of this class for every document in the SVG table
 *   which contains a glyph ID which has been used
 * Finds and looks up elements contained in the SVG document which have glyph
 *   mappings to be drawn by gfxSVGGlyphs
 */
class gfxSVGGlyphsDocument
{
    typedef mozilla::dom::Element Element;
    typedef gfxFont::DrawMode DrawMode;

public:
    gfxSVGGlyphsDocument(const uint8_t *aBuffer, uint32_t aBufLen,
                         hb_blob_t *aCmapTable);

    Element *GetGlyphElement(uint32_t aGlyphId);

    ~gfxSVGGlyphsDocument() {
        if (mViewer) {
            mViewer->Destroy();
        }
    }

private:
    nsresult ParseDocument(const uint8_t *aBuffer, uint32_t aBufLen);

    nsresult SetupPresentation();

    void FindGlyphElements(Element *aElement, hb_blob_t *aCmapTable);

    void InsertGlyphId(Element *aGlyphElement);
    void InsertGlyphChar(Element *aGlyphElement, hb_blob_t *aCmapTable);

    nsCOMPtr<nsIDocument> mDocument;
    nsCOMPtr<nsIContentViewer> mViewer;
    nsCOMPtr<nsIPresShell> mPresShell;

    nsBaseHashtable<nsUint32HashKey, Element*, Element*> mGlyphIdMap;
};

/**
 * Used by |gfxFontEntry| to represent the SVG table of an OpenType font.
 * Handles lazy parsing of the SVG documents in the table, looking up SVG glyphs
 *   and rendering SVG glyphs.
 * Each |gfxFontEntry| owns at most one |gfxSVGGlyphs| instance.
 */
class gfxSVGGlyphs
{
private:
    typedef mozilla::dom::Element Element;
    typedef gfxFont::DrawMode DrawMode;

public:
    static const float SVG_UNITS_PER_EM;

    /**
     * @param aSVGTable The SVG table from the OpenType font
     * @param aCmapTable The CMAP table from the OpenType font
     *
     * The gfxSVGGlyphs object takes over ownership of the blob references
     * that are passed in, and will hb_blob_destroy() them when finished;
     * the caller should -not- destroy these references.
     */
    gfxSVGGlyphs(hb_blob_t *aSVGTable, hb_blob_t *aCmapTable);

    /**
     * Releases our references to the SVG and cmap tables.
     */
    ~gfxSVGGlyphs();

    /**
     * Find the |gfxSVGGlyphsDocument| containing an SVG glyph for |aGlyphId|.
     * If |aGlyphId| does not map to an SVG document, return null.
     * If a |gfxSVGGlyphsDocument| has not been created for the document, create one.
     */
    gfxSVGGlyphsDocument *FindOrCreateGlyphsDocument(uint32_t aGlyphId);

    /**
     * Return true iff there is an SVG glyph for |aGlyphId|
     */
    bool HasSVGGlyph(uint32_t aGlyphId);

    /**
     * Render the SVG glyph for |aGlyphId|
     * @param aDrawMode Whether to fill or stroke or both; see gfxFont::DrawMode
     * @param aObjectPaint Information on outer text object paints.
     *   See |gfxTextObjectPaint|.
     */
    bool RenderGlyph(gfxContext *aContext, uint32_t aGlyphId, DrawMode aDrawMode,
                     gfxTextObjectPaint *aObjectPaint);

    /**
     * Get the extents for the SVG glyph associated with |aGlyphId|
     * @param aSVGToAppSpace The matrix mapping the SVG glyph space to the
     *   target context space
     */
    bool GetGlyphExtents(uint32_t aGlyphId, const gfxMatrix& aSVGToAppSpace,
                         gfxRect *aResult);

private:
    Element *GetGlyphElement(uint32_t aGlyphId);

    nsClassHashtable<nsUint32HashKey, gfxSVGGlyphsDocument> mGlyphDocs;
    nsBaseHashtable<nsUint32HashKey, Element*, Element*> mGlyphIdMap;

    hb_blob_t *mSVGData;
    hb_blob_t *mCmapData;

    const struct Header {
        mozilla::AutoSwap_PRUint16 mVersion;
        mozilla::AutoSwap_PRUint16 mIndexLength;
    } *mHeader;

    const struct IndexEntry {
        mozilla::AutoSwap_PRUint16 mStartGlyph;
        mozilla::AutoSwap_PRUint16 mEndGlyph;
        mozilla::AutoSwap_PRUint32 mDocOffset;
        mozilla::AutoSwap_PRUint32 mDocLength;
    } *mIndex;

    static int CompareIndexEntries(const void *_a, const void *_b);
};

/**
 * Used for trickling down paint information through to SVG glyphs.
 * Will be extended in later patch.
 */
class gfxTextObjectPaint
{
protected:
    gfxTextObjectPaint() { }

public:
    static mozilla::gfx::UserDataKey sUserDataKey;

    /*
     * Get outer text object pattern with the specified opacity value.
     * This lets us inherit paints and paint opacities (i.e. fill/stroke and
     * fill-opacity/stroke-opacity) separately.
     */
    virtual already_AddRefed<gfxPattern> GetFillPattern(float aOpacity,
                                                        const gfxMatrix& aCTM) = 0;
    virtual already_AddRefed<gfxPattern> GetStrokePattern(float aOpacity,
                                                          const gfxMatrix& aCTM) = 0;

    virtual float GetFillOpacity() { return 1.0f; }
    virtual float GetStrokeOpacity() { return 1.0f; }

    void InitStrokeGeometry(gfxContext *aContext,
                            float devUnitsPerSVGUnit);

    FallibleTArray<gfxFloat>& GetStrokeDashArray() {
        return mDashes;
    }

    gfxFloat GetStrokeDashOffset() {
        return mDashOffset;
    }

    gfxFloat GetStrokeWidth() {
        return mStrokeWidth;
    }

    already_AddRefed<gfxPattern> GetFillPattern(const gfxMatrix& aCTM) {
        return GetFillPattern(GetFillOpacity(), aCTM);
    }

    already_AddRefed<gfxPattern> GetStrokePattern(const gfxMatrix& aCTM) {
        return GetStrokePattern(GetStrokeOpacity(), aCTM);
    }

    virtual ~gfxTextObjectPaint() { }

private:
    FallibleTArray<gfxFloat> mDashes;
    gfxFloat mDashOffset;
    gfxFloat mStrokeWidth;
};

/**
 * For passing in patterns where the outer text object has no separate pattern
 * opacity value.
 */
class SimpleTextObjectPaint : public gfxTextObjectPaint
{
private:
    static const gfxRGBA sZero;

public:
    static gfxMatrix SetupDeviceToPatternMatrix(gfxPattern *aPattern,
                                                const gfxMatrix& aCTM)
    {
        if (!aPattern) {
            return gfxMatrix();
        }
        gfxMatrix deviceToUser = aCTM;
        deviceToUser.Invert();
        return deviceToUser * aPattern->GetMatrix();
    }

    SimpleTextObjectPaint(gfxPattern *aFillPattern, gfxPattern *aStrokePattern,
                          const gfxMatrix& aCTM) :
        mFillPattern(aFillPattern ? aFillPattern : new gfxPattern(sZero)),
        mStrokePattern(aStrokePattern ? aStrokePattern : new gfxPattern(sZero))
    {
        mFillMatrix = SetupDeviceToPatternMatrix(aFillPattern, aCTM);
        mStrokeMatrix = SetupDeviceToPatternMatrix(aStrokePattern, aCTM);
    }

    already_AddRefed<gfxPattern> GetFillPattern(float aOpacity,
                                                const gfxMatrix& aCTM) {
        if (mFillPattern) {
            mFillPattern->SetMatrix(aCTM * mFillMatrix);
        }
        nsRefPtr<gfxPattern> fillPattern = mFillPattern;
        return fillPattern.forget();
    }

    already_AddRefed<gfxPattern> GetStrokePattern(float aOpacity,
                                                  const gfxMatrix& aCTM) {
        if (mStrokePattern) {
            mStrokePattern->SetMatrix(aCTM * mStrokeMatrix);
        }
        nsRefPtr<gfxPattern> strokePattern = mStrokePattern;
        return strokePattern.forget();
    }

    float GetFillOpacity() {
        return mFillPattern ? 1.0f : 0.0f;
    }

    float GetStrokeOpacity() {
        return mStrokePattern ? 1.0f : 0.0f;
    }

private:
    nsRefPtr<gfxPattern> mFillPattern;
    nsRefPtr<gfxPattern> mStrokePattern;

    // Device space to pattern space transforms
    gfxMatrix mFillMatrix;
    gfxMatrix mStrokeMatrix;
};

#endif
