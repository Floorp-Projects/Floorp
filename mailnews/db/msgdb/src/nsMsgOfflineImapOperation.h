/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef _nsMsgOfflineImapOperation_H_

#include "nsIMsgOfflineImapOperation.h"
#include "mdb.h"
#include "nsMsgDatabase.h"
#include "nsXPIDLString.h"

class nsMsgOfflineImapOperation : public nsIMsgOfflineImapOperation
{
public:
						/** Instance Methods **/
						nsMsgOfflineImapOperation(nsMsgDatabase *db, nsIMdbRow *row);
	virtual   ~nsMsgOfflineImapOperation();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGOFFLINEIMAPOPERATION

	
  nsIMdbRow		*GetMDBRow() {return m_mdbRow;}
  nsresult    GetCopiesFromDB();
  nsresult    SetCopiesToDB();

protected:
  nsOfflineImapOperationType m_operation;
	nsMsgKey          m_messageKey; 	
	nsMsgKey          m_sourceMessageKey; 	
	PRUint32          m_operationFlags;	// what to do on sync
  imapMessageFlagsType  m_initialFlags;
	imapMessageFlagsType	m_newFlags;			// used for kFlagsChanged
	
  // these are URI's, and are escaped. Thus, we can use a delimter like ' '
  // because the real spaces should be escaped.
  nsXPIDLCString         m_sourceFolder;
	nsXPIDLCString         m_moveDestination;	
	nsCStringArray    m_copyDestinations;

  // nsMsgOfflineImapOperation will have to know what db and row they belong to, since they are really
  // just a wrapper around the offline operation row in the mdb.
  // though I hope not.
  nsMsgDatabase    *m_mdb;
  nsIMdbRow         *m_mdbRow;
};
	


#endif /* _nsMsgOfflineImapOperation_H_ */

