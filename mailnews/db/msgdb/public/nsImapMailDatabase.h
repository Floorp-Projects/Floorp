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
#ifndef _nsImapMailDatabase_H_
#define _nsImapMailDatabase_H_

#include "nsMailDatabase.h"

class nsImapMailDatabase : public nsMailDatabase
{
public:
	// OK, it's dumb that this should require a fileSpec, since there is no file
	// for the folder. This is mainly because we're deriving from nsMailDatabase;
	// Perhaps we shouldn't...
	nsImapMailDatabase();
	virtual ~nsImapMailDatabase();
	
	NS_IMETHOD		Open(nsIFileSpec *folderName, PRBool create, PRBool upgrading, nsIMsgDatabase** pMessageDB);

  NS_IMETHOD    StartBatch();
  NS_IMETHOD    EndBatch();
	NS_IMETHOD    SetSummaryValid(PRBool valid = PR_TRUE);
  NS_IMETHOD    DeleteMessages(nsMsgKeyArray* nsMsgKeys, nsIDBChangeListener *instigator);
  virtual nsresult AdjustExpungedBytesOnDelete(nsIMsgDBHdr *msgHdr);

protected:
	// IMAP does not set local file flags, override does nothing
	virtual void	UpdateFolderFlag(nsIMsgDBHdr *msgHdr, PRBool bSet, 
									 MsgFlags flag, nsIOFileStream **ppFileStream);

};


#endif
