/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BrowsingContextWebProgress_h
#define mozilla_dom_BrowsingContextWebProgress_h

#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsTObserverArray.h"
#include "nsWeakReference.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/BounceTrackingState.h"

namespace mozilla::dom {

class CanonicalBrowsingContext;

/// Object acting as the nsIWebProgress instance for a BrowsingContext over its
/// lifetime.
///
/// An active toplevel CanonicalBrowsingContext will always have a
/// BrowsingContextWebProgress, which will be moved between contexts as
/// BrowsingContextGroup-changing loads are performed.
///
/// Subframes will only have a `BrowsingContextWebProgress` if they are loaded
/// in a content process, and will use the nsDocShell instead if they are loaded
/// in the parent process, as parent process documents cannot have or be
/// out-of-process iframes.
class BrowsingContextWebProgress final : public nsIWebProgress,
                                         public nsIWebProgressListener {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(BrowsingContextWebProgress,
                                           nsIWebProgress)
  NS_DECL_NSIWEBPROGRESS
  NS_DECL_NSIWEBPROGRESSLISTENER

  explicit BrowsingContextWebProgress(
      CanonicalBrowsingContext* aBrowsingContext);

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
  void ContextReplaced(CanonicalBrowsingContext* aNewContext);

  void SetLoadType(uint32_t aLoadType) { mLoadType = aLoadType; }

  already_AddRefed<BounceTrackingState> GetBounceTrackingState();

 private:
  virtual ~BrowsingContextWebProgress();

  void UpdateAndNotifyListeners(
      uint32_t aFlag,
      const std::function<void(nsIWebProgressListener*)>& aCallback);

  using ListenerArray = nsAutoTObserverArray<ListenerInfo, 4>;
  ListenerArray mListenerInfoList;

  // The current BrowsingContext which owns this BrowsingContextWebProgress.
  // This context may change during navigations and may not be fully attached at
  // all times.
  RefPtr<CanonicalBrowsingContext> mCurrentBrowsingContext;

  // The current request being actively loaded by the BrowsingContext. Only set
  // while mIsLoadingDocument is true, and is used to fire STATE_STOP
  // notifications if the BrowsingContext is discarded while the load is
  // ongoing.
  nsCOMPtr<nsIRequest> mLoadingDocumentRequest;

  // The most recent load type observed for this BrowsingContextWebProgress.
  uint32_t mLoadType = 0;

  // Are we currently in the process of loading a document? This is true between
  // the `STATE_START` notification from content and the `STATE_STOP`
  // notification being received. Duplicate `STATE_START` events may be
  // discarded while loading a document to avoid noise caused by process
  // switches.
  bool mIsLoadingDocument = false;

  RefPtr<mozilla::BounceTrackingState> mBounceTrackingState;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_BrowsingContextWebProgress_h
