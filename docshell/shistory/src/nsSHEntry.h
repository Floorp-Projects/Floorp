/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Radha Kulkarni <radha@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsSHEntry_h
#define nsSHEntry_h

// Helper Classes
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsString.h"

// Interfaces needed
#include "nsIInputStream.h"
#include "nsISHEntry.h"
#include "nsISHContainer.h"
#include "nsIURI.h"
#include "nsIHistoryEntry.h"

class nsSHEntryShared;

class nsSHEntry : public nsISHEntry,
                  public nsISHContainer,
                  public nsISHEntryInternal
{
public: 
  nsSHEntry();
  nsSHEntry(const nsSHEntry &other);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIHISTORYENTRY
  NS_DECL_NSISHENTRY
  NS_DECL_NSISHENTRYINTERNAL
  NS_DECL_NSISHCONTAINER

  void DropPresentationState();

  static nsresult Startup();
  static void Shutdown();
  
private:
  ~nsSHEntry();

  // We share the state in here with other SHEntries which correspond to the
  // same document.
  nsRefPtr<nsSHEntryShared> mShared;

  // See nsSHEntry.idl for comments on these members.
  nsCOMPtr<nsIURI>         mURI;
  nsCOMPtr<nsIURI>         mReferrerURI;
  nsString                 mTitle;
  nsCOMPtr<nsIInputStream> mPostData;
  PRUint32                 mLoadType;
  PRUint32                 mID;
  PRInt32                  mScrollPositionX;
  PRInt32                  mScrollPositionY;
  nsISHEntry*              mParent;
  nsCOMArray<nsISHEntry>   mChildren;
  bool                     mURIWasModified;
  nsCOMPtr<nsIStructuredCloneContainer> mStateData;
};

#endif /* nsSHEntry_h */
