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

#define IMPORT_FIELD_DESC_START								2100
#define IMPORT_FIELD_DESC_END								2135


#endif /* _nsImportStringBundle_H__ */
