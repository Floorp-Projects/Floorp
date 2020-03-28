/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHTTPSOnlyStreamListener.h"
#include "nsHTTPSOnlyUtils.h"
#include "nsCOMPtr.h"
#include "nsIChannel.h"
#include "nsIRequest.h"
#include "nsITransportSecurityInfo.h"
#include "nsIURI.h"
#include "nsPrintfCString.h"

NS_IMPL_ISUPPORTS(nsHTTPSOnlyStreamListener, nsIStreamListener,
                  nsIRequestObserver)

nsHTTPSOnlyStreamListener::nsHTTPSOnlyStreamListener(
    nsIStreamListener* aListener) {
  mListener = aListener;
}

NS_IMETHODIMP
nsHTTPSOnlyStreamListener::OnDataAvailable(nsIRequest* aRequest,
                                           nsIInputStream* aInputStream,
                                           uint64_t aOffset, uint32_t aCount) {
  return mListener->OnDataAvailable(aRequest, aInputStream, aOffset, aCount);
}

NS_IMETHODIMP
nsHTTPSOnlyStreamListener::OnStartRequest(nsIRequest* request) {
  return mListener->OnStartRequest(request);
}

NS_IMETHODIMP
nsHTTPSOnlyStreamListener::OnStopRequest(nsIRequest* request,
                                         nsresult aStatus) {
  // If the request failed we'll log it to the console with the error-code
  if (NS_FAILED(aStatus)) {
    nsresult rv;
    // Try to query for the channel-object
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request, &rv);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
      uint32_t innerWindowId = loadInfo->GetInnerWindowID();

      nsCOMPtr<nsIURI> uri;
      rv = channel->GetURI(getter_AddRefs(uri));
      if (NS_SUCCEEDED(rv)) {
        // Logging URI as well as Module- and Error-Code
        AutoTArray<nsString, 2> params = {
            NS_ConvertUTF8toUTF16(uri->GetSpecOrDefault()),
            NS_ConvertUTF8toUTF16(nsPrintfCString("M%u-C%u",
                                                  NS_ERROR_GET_MODULE(aStatus),
                                                  NS_ERROR_GET_CODE(aStatus)))};
        nsHTTPSOnlyUtils::LogLocalizedString(
            "HTTPSOnlyFailedRequest", params, nsIScriptError::errorFlag,
            innerWindowId, !!loadInfo->GetOriginAttributes().mPrivateBrowsingId,
            uri);
      }
    }
  }

  return mListener->OnStopRequest(request, aStatus);
}
