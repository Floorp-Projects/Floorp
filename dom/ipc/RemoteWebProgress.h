/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteWebProgress_h
#define mozilla_dom_RemoteWebProgress_h

#include "nsIRemoteWebProgress.h"

namespace mozilla {
namespace dom {

class RemoteWebProgress final : public nsIRemoteWebProgress {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(RemoteWebProgress)

  NS_DECL_NSIWEBPROGRESS
  NS_DECL_NSIREMOTEWEBPROGRESS

  RemoteWebProgress()
      : mManager(nullptr),
        mOuterDOMWindowID(0),
        mInnerDOMWindowID(0),
        mLoadType(0),
        mIsLoadingDocument(false),
        mIsTopLevel(false) {}

  RemoteWebProgress(nsIWebProgress* aManager, uint64_t aOuterDOMWindowID,
                    uint64_t aInnerDOMWindowID, uint32_t aLoadType,
                    bool aIsLoadingDocument, bool aIsTopLevel)
      : mManager(aManager),
        mOuterDOMWindowID(aOuterDOMWindowID),
        mInnerDOMWindowID(aInnerDOMWindowID),
        mLoadType(aLoadType),
        mIsLoadingDocument(aIsLoadingDocument),
        mIsTopLevel(aIsTopLevel) {}

 private:
  virtual ~RemoteWebProgress() = default;

  nsCOMPtr<nsIWebProgress> mManager;

  uint64_t mOuterDOMWindowID;
  uint64_t mInnerDOMWindowID;
  uint32_t mLoadType;
  bool mIsLoadingDocument;
  bool mIsTopLevel;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_RemoteWebProgress_h
