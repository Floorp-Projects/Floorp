/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */ 

#ifndef nsEudoraStringBundle_H__
#define nsEudoraStringBundle_H__

#include "nsCRT.h"
#include "nsString.h"

class nsIStringBundle;

class nsEudoraStringBundle {
public:
	static PRUnichar     *		GetStringByID(PRInt32 stringID, nsIStringBundle *pBundle = nsnull);
	static void					GetStringByID(PRInt32 stringID, nsString& result, nsIStringBundle *pBundle = nsnull);
		// GetStringBundle creates a new one every time!
	static nsIStringBundle *	GetStringBundle( void);
	static void					FreeString( PRUnichar *pStr) { nsCRT::free( pStr);}
};



#define	EUDORAIMPORT_NAME				                    2000
#define	EUDORAIMPORT_DESCRIPTION							2001
#define EUDORAIMPORT_MAILBOX_SUCCESS						2002
#define EUDORAIMPORT_MAILBOX_BADPARAM						2003
#define EUDORAIMPORT_MAILBOX_BADSOURCEFILE					2004
#define EUDORAIMPORT_MAILBOX_CONVERTERROR					2005



#endif /* nsEudoraStringBundle_H__ */
