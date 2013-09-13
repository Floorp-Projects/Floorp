/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxSVGGlyphs.h"

#include "nscore.h"
#include "nsError.h"
#include "nsAutoPtr.h"
#include "nsIParser.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNodeList.h"
#include "nsString.h"
#include "nsIDocument.h"
#include "nsICategoryManager.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIContentViewer.h"
#include "nsIStreamListener.h"
#include "nsServiceManagerUtils.h"
#include "nsIPresShell.h"
#include "nsQueryFrame.h"
#include "nsIContentSink.h"
#include "nsXMLContentSink.h"
#include "nsNetUtil.h"
#include "nsIInputStream.h"
#include "nsStringStream.h"
#include "nsStreamUtils.h"
#include "nsIPrincipal.h"
#include "mozilla/dom/Element.h"
#include "nsSVGUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsContentUtils.h"
#include "gfxFont.h"
#include "nsSMILAnimationController.h"
#include "harfbuzz/hb.h"

#define SVG_CONTENT_TYPE NS_LITERAL_CSTRING("image/svg+xml")
#define UTF8_CHARSET NS_LITERAL_CSTRING("utf-8")

using namespace mozilla;

typedef mozilla::dom::Element Element;

mozilla::gfx::UserDataKey gfxTextObjectPaint::sUserDataKey;

const float gfxSVGGlyphs::SVG_UNITS_PER_EM = 1000.0f;

const gfxRGBA SimpleTextObjectPaint::sZero = gfxRGBA(0.0f, 0.0f, 0.0f, 0.0f);

gfxSVGGlyphs::gfxSVGGlyphs(hb_blob_t *aSVGTable, gfxFontEntry *aFontEntry)
    : mSVGData(aSVGTable)
    , mFontEntry(aFontEntry)
{
    unsigned int length;
    const char* svgData = hb_blob_get_data(mSVGData, &length);
    mHeader = reinterpret_cast<const Header*>(svgData);
    mDocIndex = nullptr;

    if (sizeof(Header) <= length && uint16_t(mHeader->mVersion) == 0 &&
        uint64_t(mHeader->mDocIndexOffset) + 2 <= length) {
        const DocIndex* docIndex = reinterpret_cast<const DocIndex*>
            (svgData + mHeader->mDocIndexOffset);
        // Limit the number of documents to avoid overflow
        if (uint64_t(mHeader->mDocIndexOffset) + 2 +
                uint16_t(docIndex->mNumEntries) * sizeof(IndexEntry) <= length) {
            mDocIndex = docIndex;
        }
    }
}

gfxSVGGlyphs::~gfxSVGGlyphs()
{
    hb_blob_destroy(mSVGData);
}

void
gfxSVGGlyphs::DidRefresh()
{
    mFontEntry->NotifyGlyphsChanged();
}

/*
 * Comparison operator for finding a range containing a given glyph ID. Simply
 *   checks whether |key| is less (greater) than every element of |range|, in
 *   which case return |key| < |range| (|key| > |range|). Otherwise |key| is in
 *   |range|, in which case return equality.
 * The total ordering here is guaranteed by
 *   (1) the index ranges being disjoint; and
 *   (2) the (sole) key always being a singleton, so intersection => containment
 *       (note that this is wrong if we have more than one intersection or two 
 *        sets intersecting of size > 1 -- so... don't do that)
 */
/* static */ int
gfxSVGGlyphs::CompareIndexEntries(const void *aKey, const void *aEntry)
{
    const uint32_t key = *(uint32_t*)aKey;
    const IndexEntry *entry = (const IndexEntry*)aEntry;

    if (key < uint16_t(entry->mStartGlyph)) {
        return -1;
    }
    if (key > uint16_t(entry->mEndGlyph)) {
        return 1;
    }
    return 0;
}

gfxSVGGlyphsDocument *
gfxSVGGlyphs::FindOrCreateGlyphsDocument(uint32_t aGlyphId)
{
    if (!mDocIndex) {
        // Invalid table
        return nullptr;
    }

    IndexEntry *entry = (IndexEntry*)bsearch(&aGlyphId, mDocIndex->mEntries,
                                             uint16_t(mDocIndex->mNumEntries),
                                             sizeof(IndexEntry),
                                             CompareIndexEntries);
    if (!entry) {
        return nullptr;
    }

    gfxSVGGlyphsDocument *result = mGlyphDocs.Get(entry->mDocOffset);

    if (!result) {
        unsigned int length;
        const uint8_t *data = (const uint8_t*)hb_blob_get_data(mSVGData, &length);
        if (entry->mDocOffset > 0 &&
            uint64_t(mHeader->mDocIndexOffset) + entry->mDocOffset + entry->mDocLength <= length) {
            result = new gfxSVGGlyphsDocument(data + mHeader->mDocIndexOffset + entry->mDocOffset,
                                              entry->mDocLength, this);
            mGlyphDocs.Put(entry->mDocOffset, result);
        }
    }

    return result;
}

nsresult
gfxSVGGlyphsDocument::SetupPresentation()
{
    nsCOMPtr<nsICategoryManager> catMan = do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
    nsXPIDLCString contractId;
    nsresult rv = catMan->GetCategoryEntry("Gecko-Content-Viewers", "image/svg+xml", getter_Copies(contractId));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory = do_GetService(contractId);
    NS_ASSERTION(docLoaderFactory, "Couldn't get DocumentLoaderFactory");

    nsCOMPtr<nsIContentViewer> viewer;
    rv = docLoaderFactory->CreateInstanceForDocument(nullptr, mDocument, nullptr, getter_AddRefs(viewer));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = viewer->Init(nullptr, nsIntRect(0, 0, 1000, 1000));
    if (NS_SUCCEEDED(rv)) {
        rv = viewer->Open(nullptr, nullptr);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    nsCOMPtr<nsIPresShell> presShell;
    rv = viewer->GetPresShell(getter_AddRefs(presShell));
    NS_ENSURE_SUCCESS(rv, rv);
    nsPresContext* presContext = presShell->GetPresContext();
    presContext->SetIsGlyph(true);

    if (!presShell->DidInitialize()) {
        nsRect rect = presContext->GetVisibleArea();
        rv = presShell->Initialize(rect.width, rect.height);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    mDocument->FlushPendingNotifications(Flush_Layout);

    nsSMILAnimationController* controller = mDocument->GetAnimationController();
    if (controller) {
      controller->Resume(nsSMILTimeContainer::PAUSE_IMAGE);
    }
    mDocument->SetImagesNeedAnimating(true);

    mViewer = viewer;
    mPresShell = presShell;
    mPresShell->AddPostRefreshObserver(this);

    return NS_OK;
}

void
gfxSVGGlyphsDocument::DidRefresh()
{
    mOwner->DidRefresh();
}

/**
 * Walk the DOM tree to find all glyph elements and insert them into the lookup
 * table
 * @param aElem The element to search from
 */
void
gfxSVGGlyphsDocument::FindGlyphElements(Element *aElem)
{
    for (nsIContent *child = aElem->GetLastChild(); child;
            child = child->GetPreviousSibling()) {
        if (!child->IsElement()) {
            continue;
        }
        FindGlyphElements(child->AsElement());
    }

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

    return nsSVGUtils::PaintSVGGlyph(glyph, aContext, aDrawMode, aObjectPaint);
}

bool
gfxSVGGlyphs::GetGlyphExtents(uint32_t aGlyphId, const gfxMatrix& aSVGToAppSpace,
                              gfxRect *aResult)
{
    Element *glyph = mGlyphIdMap.Get(aGlyphId);
    NS_ASSERTION(glyph, "No glyph element. Should check with HasSVGGlyph() first!");

    return nsSVGUtils::GetSVGGlyphExtents(glyph, aSVGToAppSpace, aResult);
}

Element *
gfxSVGGlyphs::GetGlyphElement(uint32_t aGlyphId)
{
    Element *elem;

    if (!mGlyphIdMap.Get(aGlyphId, &elem)) {
        elem = nullptr;
        if (gfxSVGGlyphsDocument *set = FindOrCreateGlyphsDocument(aGlyphId)) {
            elem = set->GetGlyphElement(aGlyphId);
        }
        mGlyphIdMap.Put(aGlyphId, elem);
    }

    return elem;
}

bool
gfxSVGGlyphs::HasSVGGlyph(uint32_t aGlyphId)
{
    return !!GetGlyphElement(aGlyphId);
}

Element *
gfxSVGGlyphsDocument::GetGlyphElement(uint32_t aGlyphId)
{
    return mGlyphIdMap.Get(aGlyphId);
}

gfxSVGGlyphsDocument::gfxSVGGlyphsDocument(const uint8_t *aBuffer,
                                           uint32_t aBufLen,
                                           gfxSVGGlyphs *aSVGGlyphs)
    : mOwner(aSVGGlyphs)
{
    ParseDocument(aBuffer, aBufLen);
    if (!mDocument) {
        NS_WARNING("Could not parse SVG glyphs document");
        return;
    }

    Element *root = mDocument->GetRootElement();
    if (!root) {
        NS_WARNING("Could not parse SVG glyphs document");
        return;
    }

    nsresult rv = SetupPresentation();
    if (NS_FAILED(rv)) {
        NS_WARNING("Couldn't setup presentation for SVG glyphs document");
        return;
    }

    FindGlyphElements(root);
}

gfxSVGGlyphsDocument::~gfxSVGGlyphsDocument()
{
    if (mDocument) {
        nsSMILAnimationController* controller = mDocument->GetAnimationController();
        if (controller) {
            controller->Pause(nsSMILTimeContainer::PAUSE_PAGEHIDE);
        }
    }
    if (mPresShell) {
        mPresShell->RemovePostRefreshObserver(this);
    }
    if (mViewer) {
        mViewer->Destroy();
    }
}

static nsresult
CreateBufferedStream(const uint8_t *aBuffer, uint32_t aBufLen,
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

nsresult
gfxSVGGlyphsDocument::ParseDocument(const uint8_t *aBuffer, uint32_t aBufLen)
{
    // Mostly pulled from nsDOMParser::ParseFromStream

    nsCOMPtr<nsIInputStream> stream;
    nsresult rv = CreateBufferedStream(aBuffer, aBufLen, stream);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> uri;
    nsHostObjectProtocolHandler::GenerateURIString(NS_LITERAL_CSTRING(FONTTABLEURI_SCHEME),
                                                   mSVGGlyphsDocumentURI);
 
    rv = NS_NewURI(getter_AddRefs(uri), mSVGGlyphsDocumentURI);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrincipal> principal;
    nsContentUtils::GetSecurityManager()->
        GetNoAppCodebasePrincipal(uri, getter_AddRefs(principal));

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

    nsCOMPtr<nsIDocument> document(do_QueryInterface(domDoc));
    if (!document) {
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewInputStreamChannel(getter_AddRefs(channel), uri, nullptr /* stream */,
                                  SVG_CONTENT_TYPE, UTF8_CHARSET);
    NS_ENSURE_SUCCESS(rv, rv);

    channel->SetOwner(principal);

    // Set this early because various decisions during page-load depend on it.
    document->SetIsBeingUsedAsImage();
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

    document.swap(mDocument);

    return NS_OK;
}

void
gfxSVGGlyphsDocument::InsertGlyphId(Element *aGlyphElement)
{
    nsAutoString glyphIdStr;
    static const uint32_t glyphPrefixLength = 5;
    // The maximum glyph ID is 65535 so the maximum length of the numeric part
    // is 5.
    if (!aGlyphElement->GetAttr(kNameSpaceID_None, nsGkAtoms::id, glyphIdStr) ||
        !StringBeginsWith(glyphIdStr, NS_LITERAL_STRING("glyph")) ||
        glyphIdStr.Length() > glyphPrefixLength + 5) {
        return;
    }

    uint32_t id = 0;
    for (uint32_t i = glyphPrefixLength; i < glyphIdStr.Length(); ++i) {
      PRUnichar ch = glyphIdStr.CharAt(i);
      if (ch < '0' || ch > '9') {
        return;
      }
      if (ch == '0' && i == glyphPrefixLength) {
        return;
      }
      id = id * 10 + (ch - '0');
    }

    mGlyphIdMap.Put(id, aGlyphElement);
}

void
gfxTextObjectPaint::InitStrokeGeometry(gfxContext *aContext,
                                       float devUnitsPerSVGUnit)
{
    mStrokeWidth = aContext->CurrentLineWidth() / devUnitsPerSVGUnit;
    aContext->CurrentDash(mDashes, &mDashOffset);
    for (uint32_t i = 0; i < mDashes.Length(); i++) {
        mDashes[i] /= devUnitsPerSVGUnit;
    }
    mDashOffset /= devUnitsPerSVGUnit;
}
