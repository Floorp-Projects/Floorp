/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Ashish Bhatt <ashishbhatt@netscape.com> 
 *
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

// File Overview....
//
// Test cases for the nsiDirectoryService Interface

#include "stdafx.h"
#include "TestEmbed.h"
#include "BrowserImpl.h"
#include "BrowserFrm.h"
#include "UrlDialog.h"
#include "ProfileMgr.h"
#include "ProfilesDlg.h"
#include "QaUtils.h"
#include "nsiDirServ.h"
#include "nsIDirectoryService.h"
#include "winEmbedFileLocProvider.h"
#include <stdio.h>


/////////////////////////////////////////////////////////////////////////////
// CNsIDirectoryService

CNsIDirectoryService::CNsIDirectoryService()
{

}

/*CNsIDirectoryService::CNsIDirectoryService(nsIWebBrowser* mWebBrowser,
			   nsIBaseWindow* mBaseWindow,
			   nsIWebNavigation* mWebNav,
			   CBrowserImpl *mpBrowserImpl):CTests(mWebBrowser,mBaseWindow,mWebNav,mpBrowserImpl)
{

}
*/

CNsIDirectoryService::~CNsIDirectoryService()
{
}

/*
BEGIN_MESSAGE_MAP(CNsIDirectoryService, CTests)
	//{{AFX_MSG_MAP(CNsIDirectoryService)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
*/

/////////////////////////////////////////////////////////////////////////////
// CNsIDirectoryService message handlers
// ***********************************************************************
// ***********************************************************************
//  NsIDirectoryService iface


void CNsIDirectoryService::StartTests(UINT nMenuID)
{

    QAOutput("------------------------------------------------",1);
    QAOutput("Start DirectoryService Test",1);
   	
	// Calls  all or indivdual test cases on the basis of the 
	// option selected from menu.

	switch(nMenuID)
	{
	case ID_INTERFACES_NSIDIRECTORYSERVICE_RUNALLTESTS :
			  Init(); 
			  RegisterProvider(); 	
			  UnRegisterProvider(); 	
			  break ;
	case ID_INTERFACES_NSIDIRECTORYSERVICE_INIT :
			  Init(); 
			  break ;
	case ID_INTERFACES_NSIDIRECTORYSERVICE_REGISTERPROVIDER :
			  RegisterProvider(); 	
			  break ;
	case ID_INTERFACES_NSIDIRECTORYSERVICE_UNREGISTERPROVIDER :
			  UnRegisterProvider(); 	
			  break ;
	default :  
			AfxMessageBox("Menu handler not added for this menu item");
			break ;
	}	
}

void CNsIDirectoryService::Init()
{
   nsCOMPtr<nsIDirectoryService> theDirService(do_CreateInstance(NS_DIRECTORY_SERVICE_CONTRACTID));

    if (!theDirService)
 	{
		QAOutput("Directory Service object doesn't exist.", 0);
		return;
	}

	rv = theDirService->Init();
	RvTestResult(rv, "rv Init() test", 1);

}
void CNsIDirectoryService::RegisterProvider()
{
   nsCOMPtr<nsIDirectoryService> theDirService(do_CreateInstance(NS_DIRECTORY_SERVICE_CONTRACTID));
   winEmbedFileLocProvider *provider = new winEmbedFileLocProvider("TestEmbed");
    if (!theDirService)
 	{
		QAOutput("Directory Service object doesn't exist.", 0);
		return;
	}
    if (!provider)
 	{
		QAOutput("Directory Service Provider object doesn't exist.", 0);
		return;
	}
	rv= theDirService->RegisterProvider(provider);
	RvTestResult(rv, "rv RegisterProvider() test", 1);

}
void CNsIDirectoryService::UnRegisterProvider()
{
   nsCOMPtr<nsIDirectoryService> theDirService(do_CreateInstance(NS_DIRECTORY_SERVICE_CONTRACTID));
   winEmbedFileLocProvider *provider = new winEmbedFileLocProvider("TestEmbed");
    if (!theDirService)
 	{
		QAOutput("Directory Service object doesn't exist.", 0);
		return;
	}
    if (!provider)
 	{
		QAOutput("Directory Service Provider object doesn't exist.", 0);
		return;
	}
	rv= theDirService->RegisterProvider(provider);
	rv= theDirService->UnregisterProvider(provider);
	RvTestResult(rv, "rv UnRegisterProvider() test", 1);
}


