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

#ifndef _nsImportStringBundle_H__
#define _nsImportStringBundle_H__

#include "nsCRT.h"
#include "nsString.h"

class nsIStringBundle;

class nsImportStringBundle {
public:
	static PRUnichar     *		GetStringByID(PRInt32 stringID, nsIStringBundle *pBundle = nsnull);
	static void					GetStringByID(PRInt32 stringID, nsString& result, nsIStringBundle *pBundle = nsnull);
		// GetStringBundle creates a new one every time!
	static nsIStringBundle *	GetStringBundle( void);
	static void					FreeString( PRUnichar *pStr) { nsCRT::free( pStr);}
};



#define	IMPORT_NO_ADDRBOOKS				                    2000
#define	IMPORT_ERROR_AB_NOTINITIALIZED						2001
#define IMPORT_ERROR_AB_NOTHREAD							2002
#define IMPORT_ERROR_GETABOOK								2003
#define	IMPORT_NO_MAILBOXES				                    2004
#define	IMPORT_ERROR_MB_NOTINITIALIZED						2005
#define IMPORT_ERROR_MB_NOTHREAD							2006
#define IMPORT_ERROR_MB_NOPROXY								2007
#define IMPORT_ERROR_MB_FINDCHILD							2008
#define IMPORT_ERROR_MB_CREATE								2009
#define IMPORT_ERROR_MB_NODESTFOLDER						2010


#endif /* _nsImportStringBundle_H__ */
