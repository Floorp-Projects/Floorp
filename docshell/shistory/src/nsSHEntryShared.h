
#ifndef nsSHEntryShared_h__
#define nsSHEntryShared_h__

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsIBFCacheEntry.h"
#include "nsIMutationObserver.h"
#include "nsExpirationTracker.h"
#include "nsRect.h"

class nsSHEntry;
class nsISHEntry;
class nsIDocument;
class nsIContentViewer;
class nsIDocShellTreeItem;
class nsILayoutHistoryState;
class nsISupportsArray;
class nsDocShellEditorData;

// A document may have multiple SHEntries, either due to hash navigations or
// calls to history.pushState.  SHEntries corresponding to the same document
// share many members; in particular, they share state related to the
// back/forward cache.
//
// nsSHEntryShared is the vehicle for this sharing.
class nsSHEntryShared : public nsIBFCacheEntry,
                        public nsIMutationObserver
{
  public:
    static void Startup();
    static void Shutdown();

    nsSHEntryShared();
    ~nsSHEntryShared();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIMUTATIONOBSERVER
    NS_DECL_NSIBFCACHEENTRY

  private:
    friend class nsSHEntry;

    friend class HistoryTracker;
    friend class nsExpirationTracker<nsSHEntryShared, 3>;
    nsExpirationState *GetExpirationState() { return &mExpirationState; }

    static already_AddRefed<nsSHEntryShared> Duplicate(nsSHEntryShared *aEntry);
    void SetDocIdentifier(PRUint64 aDocIdentifier);

    void RemoveFromExpirationTracker();
    void Expire();
    nsresult SyncPresentationState();
    void DropPresentationState();

    nsresult SetContentViewer(nsIContentViewer *aViewer);

    // See nsISHEntry.idl for an explanation of these members.

    // These members are copied by nsSHEntryShared::Duplicate().  If you add a
    // member here, be sure to update the Duplicate() implementation.
    PRUint64                        mDocShellID;
    nsCOMArray<nsIDocShellTreeItem> mChildShells;
    nsCOMPtr<nsISupports>           mOwner;
    nsISHEntry*                     mParent;
    nsCString                       mContentType;
    PRPackedBool                    mIsFrameNavigation;
    PRPackedBool                    mSaveLayoutState;
    PRPackedBool                    mSticky;
    PRPackedBool                    mDynamicallyCreated;
    nsCOMPtr<nsISupports>           mCacheKey;
    PRUint32                        mLastTouched;

    // These members aren't copied by nsSHEntryShared::Duplicate() because
    // they're specific to a particular content viewer.
    nsCOMPtr<nsIContentViewer>      mContentViewer;
    nsCOMPtr<nsIDocument>           mDocument;
    nsCOMPtr<nsILayoutHistoryState> mLayoutHistoryState;
    PRPackedBool                    mExpired;
    nsCOMPtr<nsISupports>           mWindowState;
    nsIntRect                       mViewerBounds;
    nsCOMPtr<nsISupportsArray>      mRefreshURIList;
    nsExpirationState               mExpirationState;
    nsAutoPtr<nsDocShellEditorData> mEditorData;
};

#endif
