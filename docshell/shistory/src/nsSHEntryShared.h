/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.org code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * 
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Justin Lebar <justin.lebar@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsSHEntryShared_h__
#define nsSHEntryShared_h__

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsIBFCacheEntry.h"
#include "nsIMutationObserver.h"
#include "nsExpirationTracker.h"
#include "nsRect.h"
#include "nsString.h"

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
    nsCString                       mContentType;
    bool                            mIsFrameNavigation;
    bool                            mSaveLayoutState;
    bool                            mSticky;
    bool                            mDynamicallyCreated;
    nsCOMPtr<nsISupports>           mCacheKey;
    PRUint32                        mLastTouched;

    // These members aren't copied by nsSHEntryShared::Duplicate() because
    // they're specific to a particular content viewer.
    PRUint64                        mID;
    nsCOMPtr<nsIContentViewer>      mContentViewer;
    nsCOMPtr<nsIDocument>           mDocument;
    nsCOMPtr<nsILayoutHistoryState> mLayoutHistoryState;
    bool                            mExpired;
    nsCOMPtr<nsISupports>           mWindowState;
    nsIntRect                       mViewerBounds;
    nsCOMPtr<nsISupportsArray>      mRefreshURIList;
    nsExpirationState               mExpirationState;
    nsAutoPtr<nsDocShellEditorData> mEditorData;
};

#endif
