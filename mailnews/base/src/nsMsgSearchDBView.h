/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsMsgSearchDBViewsH_
#define _nsMsgSearchDBView_H_

#include "nsMsgDBView.h"
#include "nsIMsgSearchNotify.h"
#include "nsIMsgCopyServiceListener.h"

class nsMsgSearchDBView : public nsMsgDBView, public nsIMsgSearchNotify
{
public:
  nsMsgSearchDBView();
  virtual ~nsMsgSearchDBView();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIMSGSEARCHNOTIFY

  virtual const char * GetViewName(void) {return "SearchView"; }
  NS_IMETHOD Open(nsIMsgFolder *folder, nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder, 
        nsMsgViewFlagsTypeValue viewFlags, PRInt32 *pCount);
  NS_IMETHOD Close();
  NS_IMETHOD Sort(nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder);
  NS_IMETHOD DoCommand(nsMsgViewCommandTypeValue command);
  NS_IMETHOD DoCommandWithFolder(nsMsgViewCommandTypeValue command, nsIMsgFolder *destFolder);
  NS_IMETHOD GetHdrForFirstSelectedMessage(nsIMsgDBHdr **hdr);
  // override to get location
  NS_IMETHOD GetCellText(PRInt32 aRow, const PRUnichar * aColID, PRUnichar ** aValue);
  virtual nsresult GetMsgHdrForViewIndex(nsMsgViewIndex index, nsIMsgDBHdr **msgHdr);
  virtual nsresult OnNewHeader(nsMsgKey newKey, nsMsgKey parentKey, PRBool ensureListed);
  NS_IMETHOD GetFolderForViewIndex(nsMsgViewIndex index, nsIMsgFolder **folder);

  // override to chain move/copies from next folder in search results
  NS_IMETHOD OnStopCopy(nsresult aStatus);

  virtual nsresult GetFolders(nsISupportsArray **aFolders);
protected:
  nsresult FetchLocation(PRInt32 aRow, PRUnichar ** aLocationString);
  virtual nsresult GetDBForViewIndex(nsMsgViewIndex index, nsIMsgDatabase **db);
  virtual nsresult RemoveByIndex(nsMsgViewIndex index);
  virtual nsresult CopyMessages(nsIMsgWindow *window, nsMsgViewIndex *indices, PRInt32 numIndices, PRBool isMove, nsIMsgFolder *destFolder);
  virtual nsresult DeleteMessages(nsIMsgWindow *window, nsMsgViewIndex *indices, PRInt32 numIndices, PRBool deleteStorage);
  nsresult InitializeGlobalsForDeleteAndFile(nsMsgViewIndex *indices, PRInt32 numIndices);
  nsresult GroupSearchResultsByFolder();
  
  nsCOMPtr <nsISupportsArray> m_folders; // maybe we should store ranges, or the actual headers instead.
  nsCOMPtr <nsISupportsArray> m_hdrsForEachFolder;
  nsCOMPtr <nsISupportsArray> m_copyListenerList;
  nsCOMPtr <nsISupportsArray> m_uniqueFolders;
  PRInt32 mCurIndex;

  nsMsgViewIndex* mIndicesForChainedDeleteAndFile;
  nsUInt32Array* mTestIndices;
  PRInt32 mTotalIndices;
  nsCOMPtr <nsISupportsArray> m_dbToUseList;
  nsMsgViewCommandTypeValue mCommand;
  nsCOMPtr <nsIMsgFolder> mDestFolder;
  nsresult ProcessRequestsInOneFolder(nsIMsgWindow *window);
  nsresult ProcessRequestsInAllFolders(nsIMsgWindow *window);
};

#endif
