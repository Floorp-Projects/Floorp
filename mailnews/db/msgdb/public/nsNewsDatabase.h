/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef _nsNewsDatabase_H_
#define _nsNewsDatabase_H_

#include "nsMsgDatabase.h"
#include "nsINewsDatabase.h"

class nsIDBChangeListener;
class nsMsgKeyArray;
class MSG_RetrieveArtInfo;
class MSG_PurgeInfo;
// news group database

class nsNewsDatabase : public nsMsgDatabase , public nsINewsDatabase
{
public:
  nsNewsDatabase();
  virtual ~nsNewsDatabase();

  NS_DECL_ISUPPORTS_INHERITED 
  NS_DECL_NSINEWSDATABASE

  NS_IMETHOD				Open(nsIFileSpec *newsgroupName, PRBool create, PRBool upgrading, nsIMsgDatabase** pMessageDB);
  NS_IMETHOD				Close(PRBool forceCommit);
  NS_IMETHOD				ForceClosed();
  NS_IMETHOD				Commit(nsMsgDBCommit commitType);
  virtual PRUint32          GetCurVersion();

  // methods to get and set docsets for ids.
  NS_IMETHOD				IsRead(nsMsgKey key, PRBool *pRead);
  virtual nsresult  IsHeaderRead(nsIMsgDBHdr *msgHdr, PRBool *pRead);

  NS_IMETHOD                GetHighWaterArticleNum(nsMsgKey *key);
  NS_IMETHOD                GetLowWaterArticleNum(nsMsgKey *key);
  NS_IMETHOD         MarkAllRead(nsMsgKeyArray *thoseMarked);

  virtual nsresult		ExpireUpTo(nsMsgKey expireKey);
  virtual nsresult		ExpireRange(nsMsgKey startRange, nsMsgKey endRange);
 
  virtual PRBool        SetHdrReadFlag(nsIMsgDBHdr *msgHdr, PRBool bRead);
 
  virtual nsNewsDatabase 	*GetNewsDB() ;

  virtual nsresult AdjustExpungedBytesOnDelete(nsIMsgDBHdr *msgHdr);
  
  // used to handle filters editing on open news groups.
  //static void			NotifyOpenDBsOfFilterChange(MSG_FolderInfo *folder);
  void					ClearFilterList();	// filter was changed by user.
  void					OpenFilterList();
  // void               OnFolderFilterListChanged(MSG_FolderInfo *folder);

  NS_IMETHOD GetDefaultViewFlags(nsMsgViewFlagsTypeValue *aDefaultViewFlags);
  NS_IMETHOD GetDefaultSortType(nsMsgViewSortTypeValue *aDefaultSortType);

protected:
  virtual PRBool	    ThreadBySubjectWithoutRe() ;

  //	MSG_FilterList*		m_filterList;
  
  // at a specified entry.

  nsMsgKeySet           *m_readSet;
};

#endif
