/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteWebProgress_h
#define mozilla_dom_RemoteWebProgress_h

#include "nsIWebProgress.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {

class RemoteWebProgress final : public nsIWebProgress {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBPROGRESS

  RemoteWebProgress()
      : mLoadType(0), mIsLoadingDocument(false), mIsTopLevel(false) {}

  RemoteWebProgress(uint32_t aLoadType, bool aIsLoadingDocument,
                    bool aIsTopLevel)
      : mLoadType(aLoadType),
        mIsLoadingDocument(aIsLoadingDocument),
        mIsTopLevel(aIsTopLevel) {}

 private:
  virtual ~RemoteWebProgress() = default;

  uint32_t mLoadType;
  bool mIsLoadingDocument;
  bool mIsTopLevel;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_RemoteWebProgress_h
