/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebBrowserPersistDocumentChild.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/dom/Document.h"
#include "nsIInputStream.h"
#include "WebBrowserPersistLocalDocument.h"
#include "WebBrowserPersistResourcesChild.h"
#include "WebBrowserPersistSerializeChild.h"
#include "mozilla/StaticPrefs_fission.h"
#include "mozilla/net/CookieJarSettings.h"

namespace mozilla {

WebBrowserPersistDocumentChild::WebBrowserPersistDocumentChild() = default;

WebBrowserPersistDocumentChild::~WebBrowserPersistDocumentChild() = default;

void WebBrowserPersistDocumentChild::Start(dom::Document* aDocument) {
  RefPtr<WebBrowserPersistLocalDocument> doc;
  if (aDocument) {
    doc = new WebBrowserPersistLocalDocument(aDocument);
  }
  Start(doc);
}

void WebBrowserPersistDocumentChild::Start(
    nsIWebBrowserPersistDocument* aDocument) {
  MOZ_ASSERT(!mDocument);
  if (!aDocument) {
    SendInitFailure(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsIPrincipal> principal;
  nsCOMPtr<nsIReferrerInfo> referrerInfo;
  nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
  WebBrowserPersistDocumentAttrs attrs;
  nsCOMPtr<nsIInputStream> postDataStream;
#define ENSURE(e)          \
  do {                     \
    nsresult rv = (e);     \
    if (NS_FAILED(rv)) {   \
      SendInitFailure(rv); \
      return;              \
    }                      \
  } while (0)
  ENSURE(aDocument->GetIsPrivate(&(attrs.isPrivate())));
  ENSURE(aDocument->GetDocumentURI(attrs.documentURI()));
  ENSURE(aDocument->GetBaseURI(attrs.baseURI()));
  ENSURE(aDocument->GetContentType(attrs.contentType()));
  ENSURE(aDocument->GetCharacterSet(attrs.characterSet()));
  ENSURE(aDocument->GetTitle(attrs.title()));
  ENSURE(aDocument->GetContentDisposition(attrs.contentDisposition()));

  attrs.sessionHistoryCacheKey() = aDocument->GetCacheKey();

  ENSURE(aDocument->GetPersistFlags(&(attrs.persistFlags())));

  ENSURE(aDocument->GetPrincipal(getter_AddRefs(principal)));
  ENSURE(ipc::PrincipalToPrincipalInfo(principal, &(attrs.principal())));

  ENSURE(aDocument->GetReferrerInfo(getter_AddRefs(referrerInfo)));
  attrs.referrerInfo() = referrerInfo;

  ENSURE(aDocument->GetCookieJarSettings(getter_AddRefs(cookieJarSettings)));
  net::CookieJarSettings::Cast(cookieJarSettings)
      ->Serialize(attrs.cookieJarSettings());

  ENSURE(aDocument->GetPostData(getter_AddRefs(postDataStream)));
#undef ENSURE

  Maybe<mozilla::ipc::IPCStream> stream;
  mozilla::ipc::SerializeIPCStream(postDataStream.forget(), stream,
                                   /* aAllowLazy */ false);

  mDocument = aDocument;
  SendAttributes(attrs, stream);
}

mozilla::ipc::IPCResult WebBrowserPersistDocumentChild::RecvSetPersistFlags(
    const uint32_t& aNewFlags) {
  mDocument->SetPersistFlags(aNewFlags);
  return IPC_OK();
}

PWebBrowserPersistResourcesChild*
WebBrowserPersistDocumentChild::AllocPWebBrowserPersistResourcesChild() {
  auto* actor = new WebBrowserPersistResourcesChild();
  NS_ADDREF(actor);
  return actor;
}

mozilla::ipc::IPCResult
WebBrowserPersistDocumentChild::RecvPWebBrowserPersistResourcesConstructor(
    PWebBrowserPersistResourcesChild* aActor) {
  RefPtr<WebBrowserPersistResourcesChild> visitor =
      static_cast<WebBrowserPersistResourcesChild*>(aActor);
  nsresult rv = mDocument->ReadResources(visitor);
  if (NS_FAILED(rv)) {
    // This is a sync failure on the child side but an async
    // failure on the parent side -- it already got NS_OK from
    // ReadResources, so the error has to be reported via the
    // visitor instead.
    visitor->EndVisit(mDocument, rv);
  }
  return IPC_OK();
}

bool WebBrowserPersistDocumentChild::DeallocPWebBrowserPersistResourcesChild(
    PWebBrowserPersistResourcesChild* aActor) {
  auto* castActor = static_cast<WebBrowserPersistResourcesChild*>(aActor);
  NS_RELEASE(castActor);
  return true;
}

PWebBrowserPersistSerializeChild*
WebBrowserPersistDocumentChild::AllocPWebBrowserPersistSerializeChild(
    const WebBrowserPersistURIMap& aMap,
    const nsACString& aRequestedContentType, const uint32_t& aEncoderFlags,
    const uint32_t& aWrapColumn) {
  auto* actor = new WebBrowserPersistSerializeChild(aMap);
  NS_ADDREF(actor);
  return actor;
}

mozilla::ipc::IPCResult
WebBrowserPersistDocumentChild::RecvPWebBrowserPersistSerializeConstructor(
    PWebBrowserPersistSerializeChild* aActor,
    const WebBrowserPersistURIMap& aMap,
    const nsACString& aRequestedContentType, const uint32_t& aEncoderFlags,
    const uint32_t& aWrapColumn) {
  auto* castActor = static_cast<WebBrowserPersistSerializeChild*>(aActor);
  // This actor performs the roles of: completion, URI map, and output stream.
  nsresult rv =
      mDocument->WriteContent(castActor, castActor, aRequestedContentType,
                              aEncoderFlags, aWrapColumn, castActor);
  if (NS_FAILED(rv)) {
    castActor->OnFinish(mDocument, castActor, aRequestedContentType, rv);
  }
  return IPC_OK();
}

bool WebBrowserPersistDocumentChild::DeallocPWebBrowserPersistSerializeChild(
    PWebBrowserPersistSerializeChild* aActor) {
  auto* castActor = static_cast<WebBrowserPersistSerializeChild*>(aActor);
  NS_RELEASE(castActor);
  return true;
}

}  // namespace mozilla
