/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsImportService_h__
#define nsImportService_h__

#include "nsICharsetConverterManager.h"

#include "nsCRT.h"
#include "nsString.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsMemory.h"
#include "nsIEnumerator.h"
#include "nsIImportModule.h"
#include "nsIImportService.h"
#include "nsICategoryManager.h"

class nsImportModuleList;

class nsImportService : public nsIImportService
{
public:

	nsImportService();
	virtual ~nsImportService();
	
	NS_DECL_ISUPPORTS

	NS_DECL_NSIIMPORTSERVICE

private:
	nsresult	LoadModuleInfo( const char *pClsId, const char *pSupports);
	nsresult	DoDiscover( void);

private:
	nsImportModuleList *	m_pModules;
	PRBool					m_didDiscovery;	
	nsCString				m_sysCharset;
	nsIUnicodeDecoder *		m_pDecoder;
	nsIUnicodeEncoder *		m_pEncoder;
};

class ImportModuleDesc {
public:
	ImportModuleDesc() { m_pModule = nsnull;}
	~ImportModuleDesc() { ReleaseModule();	}
	
	void	SetCID( const nsCID& cid) { m_cid = cid;}
	void	SetName( const PRUnichar *pName) { m_name = pName;}
	void	SetDescription( const PRUnichar *pDesc) { m_description = pDesc;}
	void	SetSupports( const char *pSupports) { m_supports = pSupports;}
	
	nsCID			GetCID( void) { return( m_cid);}
	const PRUnichar *GetName( void) { return( m_name.get());}
	const PRUnichar *GetDescription( void) { return( m_description.get());}
	const char *	GetSupports( void) { return( m_supports.get());}
	
	nsIImportModule *	GetModule( PRBool keepLoaded = PR_FALSE); // Adds ref
	void				ReleaseModule( void);
	
	PRBool			SupportsThings( const char *pThings);

private:
	nsCID				m_cid;
	nsString			m_name;
	nsString			m_description;
	nsCString			m_supports;
	nsIImportModule *	m_pModule;
};

class nsImportModuleList {
public:
	nsImportModuleList() { m_pList = nsnull; m_alloc = 0; m_count = 0;}
	~nsImportModuleList() { ClearList(); }

	void	AddModule( const nsCID& cid, const char *pSupports, const PRUnichar *pName, const PRUnichar *pDesc);

	void	ClearList( void);
	
	PRInt32	GetCount( void) { return( m_count);}
	
	ImportModuleDesc *	GetModuleDesc( PRInt32 idx) 
		{ if ((idx < 0) || (idx >= m_count)) return( nsnull); else return( m_pList[idx]);}
	
private:
	
private:
	ImportModuleDesc **	m_pList;
	PRInt32				m_alloc;
	PRInt32				m_count;
};

#endif // nsImportService_h__
