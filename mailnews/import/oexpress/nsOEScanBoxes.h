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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsOEScanBoxes_h___
#define nsOEScanBoxes_h___

#include "prtypes.h"
#include "nsString.h"
#include "nsIImportModule.h"
#include "nsVoidArray.h"
#include "nsISupportsArray.h"
#include "nsIFileSpec.h"
#include "nsIImportService.h"
	
class nsOEScanBoxes {
public:
	nsOEScanBoxes();
	~nsOEScanBoxes();

	static PRBool	FindMail( nsIFileSpec *pWhere);
	
	PRBool	GetMailboxes( nsIFileSpec *pWhere, nsISupportsArray **pArray);
		

private:
	typedef struct {
		PRUint32	index;
		PRUint32	parent;
		PRInt32		child;
		PRInt32		sibling;
		PRInt32		type;
		nsString	mailName;
		nsCString	fileName;
	} MailboxEntry;

	static PRBool	Find50Mail( nsIFileSpec *pWhere);

	void	Reset( void);
	PRBool	FindMailBoxes( nsIFileSpec * descFile);
	PRBool	Find50MailBoxes( nsIFileSpec * descFile);

	// If find mailboxes fails you can use this routine to get the raw mailbox file names
	void	ScanMailboxDir( nsIFileSpec * srcDir);
	PRBool	Scan50MailboxDir( nsIFileSpec * srcDir);

	MailboxEntry *	GetIndexEntry( PRUint32 index);
	void			AddChildEntry( MailboxEntry *pEntry, PRUint32 rootIndex);


	PRBool			ReadLong( nsIFileSpec * stream, PRInt32& val, PRUint32 offset);
	PRBool			ReadLong( nsIFileSpec * stream, PRUint32& val, PRUint32 offset);
	PRBool			ReadString( nsIFileSpec * stream, nsString& str, PRUint32 offset);
	PRBool			ReadString( nsIFileSpec * stream, nsCString& str, PRUint32 offset);
	PRUint32 		CountMailboxes( MailboxEntry *pBox);

	void 			BuildMailboxList( MailboxEntry *pBox, nsIFileSpec * root, PRInt32 depth, nsISupportsArray *pArray);
	PRBool 			GetMailboxList( nsIFileSpec * root, nsISupportsArray **pArray);
	
	void			ConvertToUnicode(const char *pStr, nsString &dist);
	
private:
	MailboxEntry *				m_pFirst;
	nsVoidArray					m_entryArray;

	nsCOMPtr<nsIImportService>	mService;
};

#endif // nsOEScanBoxes_h__
