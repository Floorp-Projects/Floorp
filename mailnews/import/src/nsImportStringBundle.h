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
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#ifndef _nsImportStringBundle_H__
#define _nsImportStringBundle_H__

#include "nsCRT.h"
#include "nsString.h"

class nsIStringBundle;

class nsImportStringBundle {
public:
	static PRUnichar     *		GetStringByID(PRInt32 stringID, nsIStringBundle *pBundle = nsnull);
	static void					GetStringByID(PRInt32 stringID, nsString& result, nsIStringBundle *pBundle = nsnull);
	static nsIStringBundle *	GetStringBundle( void); // dont release
	static void					FreeString( PRUnichar *pStr) { nsCRT::free( pStr);}
	static void					Cleanup( void);
	static nsIStringBundle *	GetStringBundleProxy( void); // release

private:
	static nsIStringBundle *	m_pBundle;
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
