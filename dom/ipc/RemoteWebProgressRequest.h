/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteWebProgressRequest_h
#define mozilla_dom_RemoteWebProgressRequest_h

#include "nsIChannel.h"
#include "nsIClassifiedChannel.h"
#include "nsIRemoteWebProgressRequest.h"

namespace mozilla {
namespace dom {

class RemoteWebProgressRequest final : public nsIRemoteWebProgressRequest,
                                       public nsIChannel,
                                       public nsIClassifiedChannel {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREMOTEWEBPROGRESSREQUEST
  NS_DECL_NSICHANNEL
  NS_DECL_NSICLASSIFIEDCHANNEL
  NS_DECL_NSIREQUEST

  RemoteWebProgressRequest()
      : mURI(nullptr), mOriginalURI(nullptr), mMatchedList(VoidCString()) {}

  RemoteWebProgressRequest(nsIURI* aURI, nsIURI* aOriginalURI,
                           const nsACString& aMatchedList,
                           const Maybe<uint64_t>& aMaybeElapsedLoadTimeMS)
      : mURI(aURI),
        mOriginalURI(aOriginalURI),
        mMatchedList(aMatchedList),
        mMaybeElapsedLoadTimeMS(aMaybeElapsedLoadTimeMS) {}

 protected:
  ~RemoteWebProgressRequest() = default;

 private:
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIURI> mOriginalURI;
  nsCString mMatchedList;

  // This field is only Some(...) when the RemoteWebProgressRequest
  // is created at a time that the document whose progress is being
  // described by this request is top level and its status changes
  // from loading to completely loaded.
  // See BrowserChild::OnStateChange.
  Maybe<uint64_t> mMaybeElapsedLoadTimeMS;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_RemoteWebProgressRequest_h
