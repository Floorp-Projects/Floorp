/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "URLMainThread.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Blob.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/Unused.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {

/* static */
void URLMainThread::CreateObjectURL(const GlobalObject& aGlobal, Blob& aBlob,
                                    nsAString& aResult, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsIPrincipal> principal =
      nsContentUtils::ObjectPrincipal(aGlobal.Get());

  nsAutoCString url;
  aRv = BlobURLProtocolHandler::AddDataEntry(aBlob.Impl(), principal, url);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  global->RegisterHostObjectURI(url);
  CopyASCIItoUTF16(url, aResult);
}

/* static */
void URLMainThread::CreateObjectURL(const GlobalObject& aGlobal,
                                    MediaSource& aSource, nsAString& aResult,
                                    ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIPrincipal> principal =
      nsContentUtils::ObjectPrincipal(aGlobal.Get());

  nsAutoCString url;
  aRv = BlobURLProtocolHandler::AddDataEntry(&aSource, principal, url);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsCOMPtr<nsIRunnable> revocation = NS_NewRunnableFunction(
      "dom::URLMainThread::CreateObjectURL",
      [url] { BlobURLProtocolHandler::RemoveDataEntry(url); });

  nsContentUtils::RunInStableState(revocation.forget());

  CopyASCIItoUTF16(url, aResult);
}

/* static */
void URLMainThread::RevokeObjectURL(const GlobalObject& aGlobal,
                                    const nsAString& aURL, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsIPrincipal* principal = nsContentUtils::ObjectPrincipal(aGlobal.Get());

  NS_LossyConvertUTF16toASCII asciiurl(aURL);

  nsIPrincipal* urlPrincipal =
      BlobURLProtocolHandler::GetDataEntryPrincipal(asciiurl);

  if (urlPrincipal && principal->Subsumes(urlPrincipal)) {
    global->UnregisterHostObjectURI(asciiurl);
    BlobURLProtocolHandler::RemoveDataEntry(asciiurl);
  }
}

/* static */
bool URLMainThread::IsValidURL(const GlobalObject& aGlobal,
                               const nsAString& aURL, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_LossyConvertUTF16toASCII asciiurl(aURL);
  return BlobURLProtocolHandler::HasDataEntry(asciiurl);
}

}  // namespace dom
}  // namespace mozilla
