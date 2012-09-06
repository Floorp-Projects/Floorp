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
#include "nsBaseHashtable.h"
#include "nsHashKeys.h"
#include "nsIDocument.h"
#include "gfxPattern.h"
#include "gfxFont.h"
#include "mozilla/gfx/UserData.h"


/**
 * Used by gfxFontEntry for OpenType fonts which contains SVG tables to parse
 * the SVG document and store and render SVG glyphs
 */
class gfxSVGGlyphs {
private:
    typedef mozilla::dom::Element Element;
    typedef gfxFont::DrawMode DrawMode;

public:
    static const float SVG_UNITS_PER_EM;

    static gfxSVGGlyphs* ParseFromBuffer(uint8_t *aBuffer, uint32_t aBufLen);

    bool HasSVGGlyph(uint32_t aGlyphId);

    bool RenderGlyph(gfxContext *aContext, uint32_t aGlyphId, DrawMode aDrawMode,
                     gfxTextObjectPaint *aObjectPaint);

    bool GetGlyphExtents(uint32_t aGlyphId, const gfxMatrix& aSVGToAppSpace,
                         gfxRect *aResult);

    bool Init(const gfxFontEntry *aFont,
              const FallibleTArray<uint8_t> &aCmapTable);

    ~gfxSVGGlyphs() {
        mViewer->Destroy();
    }

private:
    gfxSVGGlyphs() {
    }

    static nsresult ParseDocument(uint8_t *aBuffer, uint32_t aBufLen,
                                  nsIDocument **aResult);

    void FindGlyphElements(Element *aElement, const gfxFontEntry *aFontEntry,
                           const FallibleTArray<uint8_t> &aCmapTable);

    void InsertGlyphId(Element *aGlyphElement);
    void InsertGlyphChar(Element *aGlyphElement, const gfxFontEntry *aFontEntry,
                         const FallibleTArray<uint8_t> &aCmapTable);
    
    nsCOMPtr<nsIDocument> mDocument;
    nsCOMPtr<nsIContentViewer> mViewer;
    nsCOMPtr<nsIPresShell> mPresShell;

    nsBaseHashtable<nsUint32HashKey, Element*, Element*> mGlyphIdMap;
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
    virtual already_AddRefed<gfxPattern> GetFillPattern(float aOpacity) = 0;
    virtual already_AddRefed<gfxPattern> GetStrokePattern(float aOpacity) = 0;

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

    already_AddRefed<gfxPattern> GetFillPattern() {
        return GetFillPattern(GetFillOpacity());
    }

    already_AddRefed<gfxPattern> GetStrokePattern() {
        return GetStrokePattern(GetStrokeOpacity());
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
    SimpleTextObjectPaint(gfxPattern *aFillPattern, gfxPattern *aStrokePattern) :
        mFillPattern(aFillPattern ? aFillPattern : new gfxPattern(sZero)),
        mStrokePattern(aStrokePattern ? aStrokePattern : new gfxPattern(sZero)),
        mFillMatrix(aFillPattern ? aFillPattern->GetMatrix() : gfxMatrix()),
        mStrokeMatrix(aStrokePattern ? aStrokePattern->GetMatrix() : gfxMatrix())
    {
    }

    already_AddRefed<gfxPattern> GetFillPattern(float aOpacity) {
        if (mFillPattern) {
            mFillPattern->SetMatrix(mFillMatrix);
        }
        nsRefPtr<gfxPattern> fillPattern = mFillPattern;
        return fillPattern.forget();
    }

    already_AddRefed<gfxPattern> GetStrokePattern(float aOpacity) {
        if (mStrokePattern) {
            mStrokePattern->SetMatrix(mStrokeMatrix);
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

    gfxMatrix mFillMatrix;
    gfxMatrix mStrokeMatrix;
};

#endif
