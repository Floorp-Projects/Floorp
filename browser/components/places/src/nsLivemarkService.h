/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Places.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Annie Sullivan <annie.sullivan@gmail.com> (original author)
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

#include "nsILivemarkService.h"
#include "nsIStringBundle.h"
#include "nsIAnnotationService.h"
#include "nsNavHistory.h"
#include "nsBrowserCompsCID.h"
#include "nsILoadGroup.h"

// Constants for livemark annotations
#define LMANNO_FEEDURI     "livemark/feedURI"
#define LMANNO_SITEURI     "livemark/siteURI"
#define LMANNO_EXPIRATION  "livemark/expiration"

class nsLivemarkService : public nsILivemarkService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBOOKMARKSCONTAINER
  NS_DECL_NSILIVEMARKSERVICE

  nsLivemarkService();
  nsresult Init();

  // These functions are called by the livemarks feed loader
  // to set the livemark children.
  nsresult DeleteLivemarkChildren(PRInt64 aLivemarkFolderId);
  nsresult InsertLivemarkChild(PRInt64 aLivemarkFolderId, 
                               nsIURI *aURI,
                               const nsAString &aTitle);
  nsresult InsertLivemarkLoadingItem(PRInt64 aFolder);
  nsresult InsertLivemarkFailedItem(PRInt64 aFolder);

  struct LivemarkInfo {
    PRInt64 folderId;
    nsCOMPtr<nsIURI> folderURI;
    nsCOMPtr<nsIURI> feedURI;
    PRBool locked;
    // Keep track of the load group that contains the channel we're using
    // to load this livemark.  This allows the load to be cancelled if
    // necessary.  The load group automatically adds redirect channels, so
    // cancelling the load group cancels everything.
    nsCOMPtr<nsILoadGroup> loadGroup;

    LivemarkInfo(PRInt64 aFolderId, nsIURI *aFolderURI, nsIURI *aFeedURI)
      : folderId(aFolderId), folderURI(aFolderURI), feedURI(aFeedURI),
        locked(PR_FALSE) { }

    void AddRef() { ++mRefCnt; }
    void Release() { if (--mRefCnt == 0) delete this; }

  private:
    nsAutoRefCnt mRefCnt;
  };

private:
  static nsLivemarkService *sInstance;

  ~nsLivemarkService();

  // remove me when there is better query initialization
  nsNavHistory* History() { return nsNavHistory::GetHistoryService(); }

  // For localized "livemark loading...", "livemark failed to load", 
  // etc. messages
  nsCOMPtr<nsIStringBundle> mBundle;
  nsXPIDLString mLivemarkLoading;
  nsXPIDLString mLivemarkFailed;

  nsCOMPtr<nsIAnnotationService> mAnnotationService;
  nsCOMPtr<nsINavBookmarksService> mBookmarksService;

  // The list of livemarks is stored in this array
  nsTArray< nsRefPtr<LivemarkInfo> > mLivemarks;

  // Livemarks are updated on a timer.
  nsCOMPtr<nsITimer> mTimer;
  static void FireTimer(nsITimer* aTimer, void* aClosure);
  nsresult UpdateLivemarkChildren(PRInt32 aLivemarkIndex);
};
