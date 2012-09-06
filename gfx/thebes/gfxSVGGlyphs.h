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


/**
 * Used by gfxFontEntry for OpenType fonts which contains SVG tables to parse
 * the SVG document and store and render SVG glyphs
 */
class gfxSVGGlyphs {
private:
    typedef mozilla::dom::Element Element;
    typedef gfxFont::DrawMode DrawMode;

public:
    static gfxSVGGlyphs* ParseFromBuffer(uint8_t *aBuffer, uint32_t aBufLen);

    bool HasSVGGlyph(uint32_t aGlyphId);

    bool RenderGlyph(gfxContext *aContext, uint32_t aGlyphId, DrawMode aDrawMode);

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

#endif
