/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef _nsMailDatabase_H_
#define _nsMailDatabase_H_

#include "nsMsgDatabase.h"
#include "nsMsgMessageFlags.h"

// This is the subclass of nsMsgDatabase that handles local mail messages.
class nsMsgKeyArray;
class nsIOFileStream;
class nsFileSpec;
class nsOfflineImapOperation;

class nsMailDatabase : public nsMsgDatabase
{
public:
	nsMailDatabase();
	virtual ~nsMailDatabase();
	NS_IMETHOD				Open(nsIFileSpec *aFolderName, PRBool create, PRBool upgrading, nsIMsgDatabase** pMessageDB);

	static  nsresult		CloneInvalidDBInfoIntoNewDB(nsFileSpec &pathName, nsMailDatabase** pMailDB);

	NS_IMETHOD OnNewPath(nsFileSpec &newPath);

	NS_IMETHOD DeleteMessages(nsMsgKeyArray* nsMsgKeys, nsIDBChangeListener *instigator);

  NS_IMETHOD StartBatch();
  NS_IMETHOD EndBatch();

	static  nsresult		SetFolderInfoValid(nsFileSpec *folderSpec, int num, int numunread);
	nsresult				GetFolderName(nsString &folderName);
	virtual nsMailDatabase	*GetMailDB() {return this;}

	virtual PRUint32		GetCurVersion() {return kMsgDBVersion;}
	
	NS_IMETHOD			GetOfflineOpForKey(nsMsgKey opKey, PRBool create, nsIMsgOfflineImapOperation **op);
  NS_IMETHOD      RemoveOfflineOp(nsIMsgOfflineImapOperation *op);

	nsresult				SetSourceMailbox(nsOfflineImapOperation *op, const char *mailbox, nsMsgKey key);
	
    NS_IMETHOD				SetSummaryValid(PRBool valid);
	
  NS_IMETHOD    EnumerateOfflineOps(nsISimpleEnumerator **enumerator);
	nsresult 				GetIdsWithNoBodies (nsMsgKeyArray &bodylessIds);
  NS_IMETHOD    ListAllOfflineOpIds(nsMsgKeyArray *offlineOpIds);
  NS_IMETHOD    ListAllOfflineDeletes(nsMsgKeyArray *offlineDeletes);

    friend class nsMsgOfflineOpEnumerator;
protected:

  nsresult      GetAllOfflineOpsTable(); // get this on demand

  nsIMdbTable       *m_mdbAllOfflineOpsTable;
	mdb_token			m_offlineOpsRowScopeToken;
  mdb_token     m_offlineOpsTableKindToken;

  virtual PRBool			SetHdrFlag(nsIMsgDBHdr *, PRBool bSet, MsgFlags flag);
	virtual void			UpdateFolderFlag(nsIMsgDBHdr *msgHdr, PRBool bSet, 
									 MsgFlags flag, nsIOFileStream **ppFileStream);
	virtual void			SetReparse(PRBool reparse);

protected:
	virtual PRBool	ThreadBySubjectWithoutRe() ;

	PRBool					m_reparse;
	nsFileSpec				*m_folderSpec;
	nsIOFileStream			*m_folderStream; 	/* this is a cache for loops which want file left open */
};

#endif
