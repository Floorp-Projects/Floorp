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
 * Portions created by the Initial Developer are Copyright (C) 1999
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

#ifndef _nsMsgThread_H
#define _nsMsgThread_H

#include "nsIMsgThread.h"
#include "nsString.h"
#include "MailNewsTypes.h"
#include "mdb.h"

class nsIMdbTable;
class nsIMsgDBHdr;
class nsMsgDatabase;

class nsMsgThread : public nsIMsgThread {
public:
  nsMsgThread();
  nsMsgThread(nsMsgDatabase *db, nsIMdbTable *table);
  virtual ~nsMsgThread();
  
  friend class nsMsgThreadEnumerator;
  
  NS_DECL_ISUPPORTS
    NS_DECL_NSIMSGTHREAD
    
    // non-interface methods
    nsIMdbTable		*GetMDBTable() {return m_mdbTable;}
  nsIMdbRow		*GetMetaRow() {return m_metaRow;}
  nsMsgDatabase	*m_mdbDB ;

protected:

  void                  Init();
  virtual nsresult      InitCachedValues();
  nsresult              ChangeChildCount(PRInt32 delta);
  nsresult              ChangeUnreadChildCount(PRInt32 delta);
  nsresult              RemoveChild(nsMsgKey msgKey);
  nsresult              SetThreadRootKey(nsMsgKey threadRootKey);
  nsresult              GetChildHdrForKey(nsMsgKey desiredKey, 
    nsIMsgDBHdr **result, PRInt32 *resultIndex); 
  nsresult              RerootThread(nsIMsgDBHdr *newParentOfOldRoot, nsIMsgDBHdr *oldRoot, nsIDBChangeAnnouncer *announcer);
  nsresult              ReparentChildrenOf(nsMsgKey oldParent, nsMsgKey newParent, nsIDBChangeAnnouncer *announcer);
  
  nsresult              ReparentNonReferenceChildrenOf(nsIMsgDBHdr *topLevelHdr, nsMsgKey newParentKey,
    nsIDBChangeAnnouncer *announcer);
  nsresult              ReparentMsgsWithInvalidParent(PRUint32 numChildren, nsMsgKey threadParentKey);
  
  nsMsgKey              m_threadKey; 
  PRUint32              m_numChildren;		
  PRUint32              m_numUnreadChildren;	
  PRUint32              m_flags;
  nsIMdbTable           *m_mdbTable;
  nsIMdbRow             *m_metaRow;
  PRBool                m_cachedValuesInitialized;
  nsMsgKey              m_threadRootKey;
};

#endif

