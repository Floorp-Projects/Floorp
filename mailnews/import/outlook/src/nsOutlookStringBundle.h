/*
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
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */ 

#ifndef _nsOutlookStringBundle_H__
#define _nsOutlookStringBundle_H__

#include "nsCRT.h"
#include "nsString.h"

class nsIStringBundle;

class nsOutlookStringBundle {
public:
	static PRUnichar     *		GetStringByID(PRInt32 stringID, nsIStringBundle *pBundle = nsnull);
	static void					GetStringByID(PRInt32 stringID, nsString& result, nsIStringBundle *pBundle = nsnull);
		// GetStringBundle creates a new one every time!
	static nsIStringBundle *	GetStringBundle( void);
	static void					FreeString( PRUnichar *pStr) { nsCRT::free( pStr);}
};



#define	OUTLOOKIMPORT_NAME				                    2000
#define	OUTLOOKIMPORT_DESCRIPTION							2001
#define OUTLOOKIMPORT_MAILBOX_SUCCESS						2002
#define OUTLOOKIMPORT_MAILBOX_BADPARAM						2003
#define OUTLOOKIMPORT_MAILBOX_CONVERTERROR					2004



#endif /* _nsOutlookStringBundle_H__ */
