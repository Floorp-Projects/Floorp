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

  void ContextDiscarded();

 private:
  virtual ~BrowsingContextWebProgress() = default;

  void UpdateAndNotifyListeners(
      uint32_t aFlag,
      const std::function<void(nsIWebProgressListener*)>& aCallback);

  using ListenerArray = nsAutoTObserverArray<ListenerInfo, 4>;
  ListenerArray mListenerInfoList;

  // This indicates whether we are currently suspending onStateChange top level
  // STATE_START events for a document. We start suspending whenever we receive
  // the first STATE_START event with the matching flags (see
  // ::RecvOnStateChange for details), until we get a corresponding STATE_STOP
  // event. In the meantime, if there other onStateChange events, this flag does
  // not affect them. We do this to avoid duplicate onStateChange STATE_START
  // events that happen during process switch. With this flag, we allow
  // onStateChange STATE_START event from the old BrowserParent, but not the
  // same event from the new BrowserParent during a process switch.
  bool mAwaitingStop = false;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_BrowsingContextWebProgress_h
