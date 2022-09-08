/* This Source Code Form is subject to the terms of the Mozilla Public *
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWebProgressRequest.h"

#include "nsIURI.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS(RemoteWebProgressRequest, nsIRequest, nsIChannel,
                  nsIClassifiedChannel)

// nsIChannel methods

NS_IMETHODIMP RemoteWebProgressRequest::GetOriginalURI(nsIURI** aOriginalURI) {
  NS_ENSURE_ARG_POINTER(aOriginalURI);
  NS_ADDREF(*aOriginalURI = mOriginalURI);
  return NS_OK;
}

NS_IMETHODIMP RemoteWebProgressRequest::SetOriginalURI(nsIURI* aOriginalURI) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetURI(nsIURI** aURI) {
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ADDREF(*aURI = mURI);
  return NS_OK;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetOwner(nsISupports** aOwner) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::SetOwner(nsISupports* aOwner) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetNotificationCallbacks(
    nsIInterfaceRequestor** aNotificationCallbacks) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::SetNotificationCallbacks(
    nsIInterfaceRequestor* aNotificationCallbacks) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetSecurityInfo(
    nsISupports** aSecurityInfo) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetContentType(
    nsACString& aContentType) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::SetContentType(
    const nsACString& aContentType) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetContentCharset(
    nsACString& aContentCharset) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::SetContentCharset(
    const nsACString& aContentCharset) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetContentLength(
    int64_t* aContentLength) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::SetContentLength(
    int64_t aContentLength) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::Open(nsIInputStream** _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::AsyncOpen(
    nsIStreamListener* aListener) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetContentDisposition(
    uint32_t* aContentDisposition) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::SetContentDisposition(
    uint32_t aContentDisposition) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetContentDispositionFilename(
    nsAString& aContentDispositionFilename) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::SetContentDispositionFilename(
    const nsAString& aContentDispositionFilename) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetContentDispositionHeader(
    nsACString& aContentDispositionHeader) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetLoadInfo(nsILoadInfo** aLoadInfo) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::SetLoadInfo(nsILoadInfo* aLoadInfo) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetIsDocument(bool* aIsDocument) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIClassifiedChannel methods

NS_IMETHODIMP RemoteWebProgressRequest::SetMatchedInfo(
    const nsACString& aList, const nsACString& aProvider,
    const nsACString& aFullHash) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetMatchedList(
    nsACString& aMatchedList) {
  aMatchedList = mMatchedList;
  return NS_OK;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetMatchedProvider(
    nsACString& aMatchedProvider) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetMatchedFullHash(
    nsACString& aMatchedFullHash) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::SetMatchedTrackingInfo(
    const nsTArray<nsCString>& aLists, const nsTArray<nsCString>& aFullHashes) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetMatchedTrackingLists(
    nsTArray<nsCString>& aLists) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetMatchedTrackingFullHashes(
    nsTArray<nsCString>& aFullHashes) {
  return NS_ERROR_NOT_IMPLEMENTED;
}
// nsIRequest methods

NS_IMETHODIMP RemoteWebProgressRequest::GetName(nsACString& aName) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::IsPending(bool* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetStatus(nsresult* aStatus) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::SetCanceledReason(
    const nsACString& aReason) {
  return SetCanceledReasonImpl(aReason);
}

NS_IMETHODIMP RemoteWebProgressRequest::GetCanceledReason(nsACString& aReason) {
  return GetCanceledReasonImpl(aReason);
}

NS_IMETHODIMP RemoteWebProgressRequest::CancelWithReason(
    nsresult aStatus, const nsACString& aReason) {
  return CancelWithReasonImpl(aStatus, aReason);
}

NS_IMETHODIMP RemoteWebProgressRequest::Cancel(nsresult aStatus) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetCanceled(bool* aCanceled) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::Suspend(void) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::Resume(void) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetLoadGroup(
    nsILoadGroup** aLoadGroup) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::SetLoadGroup(nsILoadGroup* aLoadGroup) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetLoadFlags(nsLoadFlags* aLoadFlags) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::GetTRRMode(
    nsIRequest::TRRMode* aTRRMode) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::SetTRRMode(
    nsIRequest::TRRMode aTRRMode) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP RemoteWebProgressRequest::SetLoadFlags(nsLoadFlags aLoadFlags) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteWebProgressRequest::IsThirdPartyTrackingResource(
    bool* aIsTrackingResource) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteWebProgressRequest::IsThirdPartySocialTrackingResource(
    bool* aIsThirdPartySocialTrackingResource) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteWebProgressRequest::GetClassificationFlags(
    uint32_t* aClassificationFlags) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteWebProgressRequest::GetFirstPartyClassificationFlags(
    uint32_t* aClassificationFlags) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RemoteWebProgressRequest::GetThirdPartyClassificationFlags(
    uint32_t* aClassificationFlags) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

}  // namespace mozilla::dom
