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
	static nsIStringBundle *	GetStringBundle( void); // don't release
	static nsIStringBundle *	GetStringBundleProxy( void); // release
	static void					FreeString( PRUnichar *pStr) { nsCRT::free( pStr);}
	static void					Cleanup( void);

private:
	static nsIStringBundle *	m_pBundle;
};



#define	EUDORAIMPORT_NAME				                    2000
#define	EUDORAIMPORT_DESCRIPTION							2001
#define EUDORAIMPORT_MAILBOX_SUCCESS						2002
#define EUDORAIMPORT_MAILBOX_BADPARAM						2003
#define EUDORAIMPORT_MAILBOX_BADSOURCEFILE					2004
#define EUDORAIMPORT_MAILBOX_CONVERTERROR					2005
#define EUDORAIMPORT_ACCOUNTNAME							2006

#define	EUDORAIMPORT_NICKNAMES_NAME							2007
#define	EUDORAIMPORT_ADDRESS_SUCCESS						2008
#define	EUDORAIMPORT_ADDRESS_BADPARAM						2009
#define	EUDORAIMPORT_ADDRESS_BADSOURCEFILE					2010
#define	EUDORAIMPORT_ADDRESS_CONVERTERROR					2011



#endif /* nsEudoraStringBundle_H__ */
