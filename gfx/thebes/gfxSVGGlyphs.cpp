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

#include "gfxSVGGlyphs.h"

#include "nscore.h"
#include "nsError.h"
#include "nsAutoPtr.h"
#include "nsIParser.h"
#include "nsIDOMNodeList.h"
#include "nsString.h"
#include "nsIDocument.h"
#include "nsPrintfCString.h"
#include "nsICategoryManager.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIContentViewer.h"
#include "nsIStreamListener.h"
#include "nsServiceManagerUtils.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsQueryFrame.h"
#include "nsIContentSink.h"
#include "nsXMLContentSink.h"
#include "nsNetUtil.h"
#include "nsIInputStream.h"
#include "nsContentUtils.h"
#include "nsStringStream.h"
#include "nsStreamUtils.h"
#include "nsIPrincipal.h"
#include "Element.h"

#define SVG_CONTENT_TYPE NS_LITERAL_CSTRING("image/svg+xml")
#define UTF8_CHARSET NS_LITERAL_CSTRING("utf-8")


mozilla::gfx::UserDataKey gfxTextObjectPaint::sUserDataKey;

const float gfxSVGGlyphs::SVG_UNITS_PER_EM = 1000.0f;

const gfxRGBA SimpleTextObjectPaint::sZero = gfxRGBA(0.0f, 0.0f, 0.0f, 0.0f);

/* static */ gfxSVGGlyphs*
gfxSVGGlyphs::ParseFromBuffer(uint8_t *aBuffer, uint32_t aBufLen)
{
    nsCOMPtr<nsIDocument> doc;
    nsresult rv = ParseDocument(aBuffer, aBufLen, getter_AddRefs(doc));
    NS_ENSURE_SUCCESS(rv, nullptr);

    doc->SetIsBeingUsedAsImage();

    nsAutoPtr<gfxSVGGlyphs> result(new gfxSVGGlyphs());

    nsCOMPtr<nsICategoryManager> catMan = do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
    nsXPIDLCString contractId;
    rv = catMan->GetCategoryEntry("Gecko-Content-Viewers", "image/svg+xml", getter_Copies(contractId));
    NS_ENSURE_SUCCESS(rv, nullptr);

    nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory = do_GetService(contractId);
    NS_ASSERTION(docLoaderFactory, "Couldn't get DocumentLoaderFactory");

    nsCOMPtr<nsIContentViewer> viewer;
    rv = docLoaderFactory->CreateInstanceForDocument(nullptr, doc, nullptr, getter_AddRefs(viewer));
    NS_ENSURE_SUCCESS(rv, nullptr);

    rv = viewer->Init(nullptr, nsIntRect(0, 0, 1000, 1000));
    if (NS_SUCCEEDED(rv)) {
        rv = viewer->Open(nullptr, nullptr);
        NS_ENSURE_SUCCESS(rv, nullptr);
    }

    nsCOMPtr<nsIPresShell> presShell;
    rv = viewer->GetPresShell(getter_AddRefs(presShell));
    NS_ENSURE_SUCCESS(rv, nullptr);
    presShell->GetPresContext()->SetIsGlyph(true);

    if (!presShell->DidInitialReflow()) {
        nsRect rect = presShell->GetPresContext()->GetVisibleArea();
        rv = presShell->InitialReflow(rect.width, rect.height);
        NS_ENSURE_SUCCESS(rv, nullptr);
    }

    doc->FlushPendingNotifications(Flush_Layout);

    result->mViewer = viewer;
    result->mPresShell = presShell;
    result->mDocument = doc;

    return result.forget();
}

/**
 * Initialize the glyphid -> glyph map.
 * @param aFontEntry The owning font entry. Used for resolving glyphchar attributes.
 * @param aCmapTable Buffer containing the raw cmap table data
 */
bool
gfxSVGGlyphs::Init(const gfxFontEntry *aFontEntry,
                   const FallibleTArray<uint8_t> &aCmapTable)
{
    NS_ASSERTION(mDocument, "Document not set");

    if (!mGlyphIdMap.Init()) {
        return false;
    }

    Element *svgRoot = mDocument->GetRootElement();
    if (!svgRoot) {
        return false;
    }

    FindGlyphElements(svgRoot, aFontEntry, aCmapTable);

    return true;
}

/**
 * Walk the DOM tree to find all glyph elements and insert them into the lookup
 * table
 * @param aElem The element to search from
 * @param aFontEntry The font entry owning this document, used for resolving
 *     glyphchar attributes.
 * @param aCmapTable Buffer containing the raw cmap table data
 */
void
gfxSVGGlyphs::FindGlyphElements(Element *aElem,
                                const gfxFontEntry *aFontEntry,
                                const FallibleTArray<uint8_t> &aCmapTable)
{
    for (nsIContent *child = aElem->GetLastChild(); child;
            child = child->GetPreviousSibling()) {
        if (!child->IsElement()) {
            continue;
        }
        FindGlyphElements(child->AsElement(), aFontEntry, aCmapTable);
    }

    InsertGlyphChar(aElem, aFontEntry, aCmapTable);
    InsertGlyphId(aElem);
}

/**
 * If there exists an SVG glyph with the specified glyph id, render it and return true
 * If no such glyph exists, or in the case of an error return false
 * @param aContext The thebes aContext to draw to
 * @param aGlyphId The glyph id
 * @param aDrawMode Whether to fill or stroke or both (see |gfxFont::DrawMode|)
 * @return true iff rendering succeeded
 */
bool
gfxSVGGlyphs::RenderGlyph(gfxContext *aContext, uint32_t aGlyphId,
                          DrawMode aDrawMode, gfxTextObjectPaint *aObjectPaint)
{
    if (aDrawMode == gfxFont::GLYPH_PATH) {
        return false;
    }

    gfxContextAutoSaveRestore aContextRestorer(aContext);

    Element *glyph = mGlyphIdMap.Get(aGlyphId);
    NS_ASSERTION(glyph, "No glyph element. Should check with HasSVGGlyph() first!");

    return nsContentUtils::PaintSVGGlyph(glyph, aContext, aDrawMode, aObjectPaint);
}

bool
gfxSVGGlyphs::GetGlyphExtents(uint32_t aGlyphId, const gfxMatrix& aSVGToAppSpace,
                              gfxRect *aResult)
{
    Element *glyph = mGlyphIdMap.Get(aGlyphId);
    NS_ASSERTION(glyph, "No glyph element. Should check with HasSVGGlyph() first!");

    return nsContentUtils::GetSVGGlyphExtents(glyph, aSVGToAppSpace, aResult);
}

bool
gfxSVGGlyphs::HasSVGGlyph(uint32_t aGlyphId)
{
    Element *glyph = mGlyphIdMap.Get(aGlyphId);
    return !!glyph;
}

nsresult
CreateBufferedStream(uint8_t *aBuffer, uint32_t aBufLen,
                     nsCOMPtr<nsIInputStream> &aResult)
{
    nsCOMPtr<nsIInputStream> stream;
    nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream),
                                        reinterpret_cast<const char *>(aBuffer),
                                        aBufLen, NS_ASSIGNMENT_DEPEND);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIInputStream> aBufferedStream;
    if (!NS_InputStreamIsBuffered(stream)) {
        rv = NS_NewBufferedInputStream(getter_AddRefs(aBufferedStream), stream, 4096);
        NS_ENSURE_SUCCESS(rv, rv);
        stream = aBufferedStream;
    }

    aResult = stream;

    return NS_OK;
}

/* static */ nsresult
gfxSVGGlyphs::ParseDocument(uint8_t *aBuffer, uint32_t aBufLen,
                            nsIDocument **aResult)
{
    // Mostly pulled from nsDOMParser::ParseFromStream

    *aResult = nullptr;

    nsCOMPtr<nsIInputStream> stream;
    nsresult rv = CreateBufferedStream(aBuffer, aBufLen, stream);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrincipal> principal =
        do_CreateInstance("@mozilla.org/nullprincipal;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> uri;
    principal->GetURI(getter_AddRefs(uri));

    nsCOMPtr<nsIDOMDocument> domDoc;
    rv = NS_NewDOMDocument(getter_AddRefs(domDoc),
                           EmptyString(),   // aNamespaceURI
                           EmptyString(),   // aQualifiedName
                           nullptr,          // aDoctype
                           uri, uri, principal,
                           false,           // aLoadedAsData
                           nullptr,          // aEventObject
                           DocumentFlavorSVG);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewInputStreamChannel(getter_AddRefs(channel), uri, nullptr /* stream */,
                                  SVG_CONTENT_TYPE, UTF8_CHARSET);
    NS_ENSURE_SUCCESS(rv, rv);

    channel->SetOwner(principal);

    nsCOMPtr<nsIDocument> document(do_QueryInterface(domDoc));
    if (!document) {
        return NS_ERROR_FAILURE;
    }
    document->SetReadyStateInternal(nsIDocument::READYSTATE_UNINITIALIZED);

    nsCOMPtr<nsIStreamListener> listener;
    rv = document->StartDocumentLoad("external-resource", channel,
                                     nullptr,    // aLoadGroup
                                     nullptr,    // aContainer
                                     getter_AddRefs(listener),
                                     true /* aReset */);
    if (NS_FAILED(rv) || !listener) {
        return NS_ERROR_FAILURE;
    }

    document->SetBaseURI(uri);
    document->SetPrincipal(principal);

    rv = listener->OnStartRequest(channel, nullptr /* aContext */);
    if (NS_FAILED(rv)) {
        channel->Cancel(rv);
    }

    nsresult status;
    channel->GetStatus(&status);
    if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(status)) {
        rv = listener->OnDataAvailable(channel, nullptr /* aContext */, stream, 0, aBufLen);
        if (NS_FAILED(rv)) {
            channel->Cancel(rv);
        }
        channel->GetStatus(&status);
    }

    rv = listener->OnStopRequest(channel, nullptr /* aContext */, status);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    document.swap(*aResult);

    return NS_OK;
}

void
gfxSVGGlyphs::InsertGlyphId(Element *aGlyphElement)
{
    nsAutoString glyphIdStr;
    if (!aGlyphElement->GetAttr(kNameSpaceID_None, nsGkAtoms::glyphid, glyphIdStr)) {
        return;
    }

    nsresult rv;
    uint32_t glyphId = glyphIdStr.ToInteger(&rv, kRadix10);

    if (NS_FAILED(rv)) {
        return;
    }

    mGlyphIdMap.Put(glyphId, aGlyphElement);
}

void
gfxSVGGlyphs::InsertGlyphChar(Element *aGlyphElement, 
                              const gfxFontEntry *aFontEntry,
                              const FallibleTArray<uint8_t> &aCmapTable)
{
    nsAutoString glyphChar;
    if (!aGlyphElement->GetAttr(kNameSpaceID_None, nsGkAtoms::glyphchar, glyphChar)) {
        return;
    }

    uint32_t varSelector;

    switch (glyphChar.Length()) {
        case 0:
            NS_WARNING("glyphchar is empty");
            return;
        case 1:
            varSelector = 0;
            break;
        case 2:
            if (gfxFontUtils::IsVarSelector(glyphChar.CharAt(1))) {
                varSelector = glyphChar.CharAt(1);
                break;
            }
        default:
            NS_WARNING("glyphchar contains more than one character");
            return;
    }

    uint32_t glyphId = gfxFontUtils::MapCharToGlyph(aCmapTable.Elements(),
                                                    aCmapTable.Length(),
                                                    glyphChar.CharAt(0),
                                                    varSelector);

    if (glyphId) {
        mGlyphIdMap.Put(glyphId, aGlyphElement);
    }
}

