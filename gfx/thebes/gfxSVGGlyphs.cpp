/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxSVGGlyphs.h"

#include "mozilla/SVGContextPaint.h"
#include "nsError.h"
#include "nsString.h"
#include "nsIDocument.h"
#include "nsICategoryManager.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIContentViewer.h"
#include "nsIStreamListener.h"
#include "nsServiceManagerUtils.h"
#include "nsIPresShell.h"
#include "nsNetUtil.h"
#include "NullPrincipal.h"
#include "nsIInputStream.h"
#include "nsStringStream.h"
#include "nsStreamUtils.h"
#include "nsIPrincipal.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/SVGDocument.h"
#include "mozilla/LoadInfo.h"
#include "nsSVGUtils.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsContentUtils.h"
#include "gfxFont.h"
#include "nsSMILAnimationController.h"
#include "gfxContext.h"
#include "harfbuzz/hb.h"
#include "mozilla/dom/ImageTracker.h"

#define SVG_CONTENT_TYPE NS_LITERAL_CSTRING("image/svg+xml")
#define UTF8_CHARSET NS_LITERAL_CSTRING("utf-8")

using namespace mozilla;

typedef mozilla::dom::Element Element;

/* static */ const Color SimpleTextContextPaint::sZero = Color();

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
    nsCString contractId;
    nsresult rv = catMan->GetCategoryEntry("Gecko-Content-Viewers", "image/svg+xml", getter_Copies(contractId));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory =
      do_GetService(contractId.get());
    NS_ASSERTION(docLoaderFactory, "Couldn't get DocumentLoaderFactory");

    nsCOMPtr<nsIContentViewer> viewer;
    rv = docLoaderFactory->CreateInstanceForDocument(nullptr, mDocument, nullptr, getter_AddRefs(viewer));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = viewer->Init(nullptr, gfx::IntRect(0, 0, 1000, 1000));
    if (NS_SUCCEEDED(rv)) {
        rv = viewer->Open(nullptr, nullptr);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    nsCOMPtr<nsIPresShell> presShell;
    rv = viewer->GetPresShell(getter_AddRefs(presShell));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!presShell->DidInitialize()) {
        rv = presShell->Initialize();
        NS_ENSURE_SUCCESS(rv, rv);
    }

    mDocument->FlushPendingNotifications(FlushType::Layout);

    if (mDocument->HasAnimationController()) {
      mDocument->GetAnimationController()
               ->Resume(nsSMILTimeContainer::PAUSE_IMAGE);
    }
    mDocument->ImageTracker()->SetAnimatingState(true);

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
 * @return true iff rendering succeeded
 */
void
gfxSVGGlyphs::RenderGlyph(gfxContext *aContext, uint32_t aGlyphId,
                          SVGContextPaint* aContextPaint)
{
    gfxContextAutoSaveRestore aContextRestorer(aContext);

    Element* glyph = mGlyphIdMap.Get(aGlyphId);
    MOZ_ASSERT(glyph, "No glyph element. Should check with HasSVGGlyph() first!");

    AutoSetRestoreSVGContextPaint autoSetRestore(
        *aContextPaint, *glyph->OwnerDoc()->AsSVGDocument());

    nsSVGUtils::PaintSVGGlyph(glyph, aContext);
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

size_t
gfxSVGGlyphs::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
    // We don't include the size of mSVGData here, because (depending on the
    // font backend implementation) it will either wrap a block of data owned
    // by the system (and potentially shared), or a table that's in our font
    // table cache and therefore already counted.
    size_t result = aMallocSizeOf(this)
                    + mGlyphDocs.ShallowSizeOfExcludingThis(aMallocSizeOf)
                    + mGlyphIdMap.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (auto iter = mGlyphDocs.ConstIter(); !iter.Done(); iter.Next()) {
        result += iter.Data()->SizeOfIncludingThis(aMallocSizeOf);
    }
    return result;
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
        mDocument->OnPageHide(false, nullptr);
    }
    if (mPresShell) {
        mPresShell->RemovePostRefreshObserver(this);
    }
    if (mViewer) {
        mViewer->Close(nullptr);
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
        rv = NS_NewBufferedInputStream(getter_AddRefs(aBufferedStream),
                                       stream.forget(), 4096);
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
                                                   nullptr,
                                                   mSVGGlyphsDocumentURI);

    rv = NS_NewURI(getter_AddRefs(uri), mSVGGlyphsDocumentURI);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrincipal> principal = NullPrincipal::CreateWithoutOriginAttributes();

    nsCOMPtr<nsIDocument> document;
    rv = NS_NewDOMDocument(getter_AddRefs(document),
                           EmptyString(),   // aNamespaceURI
                           EmptyString(),   // aQualifiedName
                           nullptr,          // aDoctype
                           uri, uri, principal,
                           false,           // aLoadedAsData
                           nullptr,          // aEventObject
                           DocumentFlavorSVG);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewInputStreamChannel(getter_AddRefs(channel),
                                  uri,
                                  nullptr, //aStream
                                  principal,
                                  nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL,
                                  nsIContentPolicy::TYPE_OTHER,
                                  SVG_CONTENT_TYPE,
                                  UTF8_CHARSET);
    NS_ENSURE_SUCCESS(rv, rv);

    // Set this early because various decisions during page-load depend on it.
    document->SetIsBeingUsedAsImage();
    document->SetIsSVGGlyphsDocument();
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
      char16_t ch = glyphIdStr.CharAt(i);
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

size_t
gfxSVGGlyphsDocument::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
    return aMallocSizeOf(this)
           + mGlyphIdMap.ShallowSizeOfExcludingThis(aMallocSizeOf)
           + mSVGGlyphsDocumentURI.SizeOfExcludingThisIfUnshared(aMallocSizeOf);

}
