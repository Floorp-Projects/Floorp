/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebBrowserPersistRemoteDocument.h"
#include "WebBrowserPersistDocumentParent.h"
#include "WebBrowserPersistResourcesParent.h"
#include "WebBrowserPersistSerializeParent.h"
#include "mozilla/Unused.h"
#include "mozilla/ipc/BackgroundUtils.h"

#include "nsIPrincipal.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(WebBrowserPersistRemoteDocument,
                  nsIWebBrowserPersistDocument)

WebBrowserPersistRemoteDocument
::WebBrowserPersistRemoteDocument(WebBrowserPersistDocumentParent* aActor,
                                  const Attrs& aAttrs,
                                  nsIInputStream* aPostData)
: mActor(aActor)
, mAttrs(aAttrs)
, mPostData(aPostData)
{
  nsresult rv;
  mPrincipal = ipc::PrincipalInfoToPrincipal(mAttrs.principal(), &rv);
}

WebBrowserPersistRemoteDocument::~WebBrowserPersistRemoteDocument()
{
    if (mActor) {
        Unused << WebBrowserPersistDocumentParent::Send__delete__(mActor);
        // That will call mActor->ActorDestroy, which calls this->ActorDestroy
        // (whether or not the IPC send succeeds).
    }
    MOZ_ASSERT(!mActor);
}

void
WebBrowserPersistRemoteDocument::ActorDestroy(void)
{
    mActor = nullptr;
}

NS_IMETHODIMP
WebBrowserPersistRemoteDocument::GetIsPrivate(bool* aIsPrivate)
{
    *aIsPrivate = mAttrs.isPrivate();
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistRemoteDocument::GetDocumentURI(nsACString& aURISpec)
{
    aURISpec = mAttrs.documentURI();
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistRemoteDocument::GetBaseURI(nsACString& aURISpec)
{
    aURISpec = mAttrs.baseURI();
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistRemoteDocument::GetContentType(nsACString& aContentType)
{
    aContentType = mAttrs.contentType();
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistRemoteDocument::GetCharacterSet(nsACString& aCharSet)
{
    aCharSet = mAttrs.characterSet();
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistRemoteDocument::GetTitle(nsAString& aTitle)
{
    aTitle = mAttrs.title();
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistRemoteDocument::GetReferrer(nsAString& aReferrer)
{
    aReferrer = mAttrs.referrer();
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistRemoteDocument::GetContentDisposition(nsAString& aDisp)
{
    aDisp = mAttrs.contentDisposition();
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistRemoteDocument::GetCacheKey(uint32_t* aCacheKey)
{
    *aCacheKey = mAttrs.cacheKey();
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistRemoteDocument::GetPersistFlags(uint32_t* aFlags)
{
    *aFlags = mAttrs.persistFlags();
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistRemoteDocument::SetPersistFlags(uint32_t aFlags)
{
    if (!mActor) {
        return NS_ERROR_FAILURE;
    }
    if (!mActor->SendSetPersistFlags(aFlags)) {
        return NS_ERROR_FAILURE;
    }
    mAttrs.persistFlags() = aFlags;
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistRemoteDocument::GetPostData(nsIInputStream** aStream)
{
    nsCOMPtr<nsIInputStream> stream = mPostData;
    stream.forget(aStream);
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistRemoteDocument::GetPrincipal(nsIPrincipal** aPrincipal)
{
    nsCOMPtr<nsIPrincipal> nodePrincipal = mPrincipal;
    nodePrincipal.forget(aPrincipal);
    return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistRemoteDocument::ReadResources(nsIWebBrowserPersistResourceVisitor* aVisitor)
{
    if (!mActor) {
        return NS_ERROR_FAILURE;
    }
    RefPtr<WebBrowserPersistResourcesParent> subActor =
        new WebBrowserPersistResourcesParent(this, aVisitor);
    return mActor->SendPWebBrowserPersistResourcesConstructor(
        subActor.forget().take())
        ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
WebBrowserPersistRemoteDocument::WriteContent(
    nsIOutputStream* aStream,
    nsIWebBrowserPersistURIMap* aMap,
    const nsACString& aRequestedContentType,
    uint32_t aEncoderFlags,
    uint32_t aWrapColumn,
    nsIWebBrowserPersistWriteCompletion* aCompletion)
{
    if (!mActor) {
        return NS_ERROR_FAILURE;
    }

    nsresult rv;
    WebBrowserPersistURIMap map;
    uint32_t numMappedURIs;
    if (aMap) {
        rv = aMap->GetTargetBaseURI(map.targetBaseURI());
        NS_ENSURE_SUCCESS(rv, rv);
        rv = aMap->GetNumMappedURIs(&numMappedURIs);
        NS_ENSURE_SUCCESS(rv, rv);
        for (uint32_t i = 0; i < numMappedURIs; ++i) {
            WebBrowserPersistURIMapEntry& nextEntry =
                *(map.mapURIs().AppendElement());
            rv = aMap->GetURIMapping(i, nextEntry.mapFrom(), nextEntry.mapTo());
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    auto* subActor = new WebBrowserPersistSerializeParent(this,
                                                          aStream,
                                                          aCompletion);
    nsCString requestedContentType(aRequestedContentType); // Sigh.
    return mActor->SendPWebBrowserPersistSerializeConstructor(
        subActor, map, requestedContentType, aEncoderFlags, aWrapColumn)
        ? NS_OK : NS_ERROR_FAILURE;
}

} // namespace mozilla
