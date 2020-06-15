/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BrowsingContextWebProgress_h
#define mozilla_dom_BrowsingContextWebProgress_h

#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsTObserverArray.h"
#include "nsWeakReference.h"

namespace mozilla {
namespace dom {

class BrowsingContextWebProgress final : public nsIWebProgress,
                                         public nsIWebProgressListener {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBPROGRESS
  NS_DECL_NSIWEBPROGRESSLISTENER

  BrowsingContextWebProgress() = default;

  struct ListenerInfo {
    ListenerInfo(nsIWeakReference* aListener, unsigned long aNotifyMask)
        : mWeakListener(aListener), mNotifyMask(aNotifyMask) {}

    bool operator==(const ListenerInfo& aOther) const {
      return mWeakListener == aOther.mWeakListener;
    }
    bool operator==(const nsWeakPtr& aOther) const {
      return mWeakListener == aOther;
    }

    // Weak pointer for the nsIWebProgressListener...
    nsWeakPtr mWeakListener;

    // Mask indicating which notifications the listener wants to receive.
    unsigned long mNotifyMask;
  };

 private:
  virtual ~BrowsingContextWebProgress() = default;

  void UpdateAndNotifyListeners(
      nsIWebProgress* aWebProgress, uint32_t aFlag,
      const std::function<void(nsIWebProgressListener*)>& aCallback);

  using ListenerArray = nsAutoTObserverArray<ListenerInfo, 4>;
  ListenerArray mListenerInfoList;

  uint32_t mLoadType = 0;
  bool mIsLoadingDocument = false;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_BrowsingContextWebProgress_h
