/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Contributor(s): Radha Kulkarni (radha@netscape.com)
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

#include <stdio.h>

//locals
#include "nsQABrowserView.h"
#include "nsIQABrowserView.h"
#include "nsIInterfaceRequestor.h"

//essentials
#include "nsIComponentManager.h"


//Interfaces Needed
#include "nsCWebBrowser.h"
#include "nsIDocShellTreeItem.h"
#include "nsIWebProgressListener.h"
#include "nsIWebBrowserChrome.h"
#include "nsWeakPtr.h"
#include "nsWeakReference.h"

nsQABrowserView::nsQABrowserView():mWebBrowser(nsnull)
{
  mWebBrowser = nsnull;
  mWebNav = nsnull;
  mBaseWindow = nsnull;
}

nsQABrowserView::~nsQABrowserView()
{
}

//*****************************************************************************
// nsQABrowserView::nsISupports
//*****************************************************************************   

NS_IMPL_ISUPPORTS2(nsQABrowserView, nsIQABrowserView, nsIInterfaceRequestor)

//*****************************************************************************
// nsQABrowserView::nsIInterfacerequestor
//*****************************************************************************

NS_IMETHODIMP
nsQABrowserView::GetInterface(const nsIID& aIID, void ** aSink)
{
  NS_ENSURE_ARG_POINTER(aSink);  
  printf("In nsQABrowserView::GetInterface\n");
  if (NS_SUCCEEDED(QueryInterface(aIID, aSink)))
    return NS_OK;

  if (mWebBrowser) {
    printf("nsQABrowserView::GetInterface, Got mWebBrowser\n");
    nsCOMPtr<nsIInterfaceRequestor>  ifcReq(do_QueryInterface(mWebBrowser));
    if (ifcReq)
      return ifcReq->GetInterface(aIID, aSink);
  }

  return NS_NOINTERFACE;

}


NS_IMETHODIMP
nsQABrowserView::CreateBrowser(nativeWindow aNativeWnd, nsIWebBrowserChrome * aChrome)
{

  nsresult rv;
  mWebBrowser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID, &rv);
  if (NS_FAILED(rv) || !mWebBrowser)
    return NS_ERROR_FAILURE; 

  mWebNav = do_QueryInterface(mWebBrowser, &rv);
	if(NS_FAILED(rv))
		return rv;
  
  // Set the chrome object the container window. 
  mWebBrowser->SetContainerWindow(aChrome);
  if(NS_FAILED(rv))
		return rv;

  rv = NS_OK;
  nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(mWebBrowser, &rv);
	if(NS_FAILED(rv))
		return rv;

  // If the browser window hosting chrome or content?
  dsti->SetItemType(nsIDocShellTreeItem::typeContentWrapper);

  // Create the real webbrowser window
	mBaseWindow = do_QueryInterface(mWebBrowser, &rv);
	if(NS_FAILED(rv))
		return rv;

  // Create the actual window
	rv = mBaseWindow->InitWindow(aNativeWnd, nsnull, 0, 0, -1, -1);
	rv = mBaseWindow->Create();

  // set a handle to nsIWebBrowser in nsQAWebBrowserChrome
  aChrome->SetWebBrowser(mWebBrowser);

  // Register the QABrowserChrome object to receive progress messages
	// These callbacks will be used to update the status/progress bars
  nsWeakPtr weakling(do_GetWeakReference(aChrome));
  (void)mWebBrowser->AddWebBrowserListener(weakling, NS_GET_IID(nsIWebProgressListener));

	// Finally, show the web browser window
	mBaseWindow->SetVisibility(PR_TRUE);

	return NS_OK;
}

// XXX Not needed
NS_IMETHODIMP
nsQABrowserView::DestroyBrowser()
{
  // destroy the actual window.
  mBaseWindow->Destroy();
  mWebBrowser = nsnull;
  mWebNav = nsnull;
  return NS_OK;
}



// Get a handle to the nsIWebBrowser object. 
NS_IMETHODIMP
nsQABrowserView::GetWebBrowser(nsIWebBrowser ** aWebBrowser)
{
  if (!mWebBrowser || !(aWebBrowser))
    return NS_ERROR_FAILURE;
  
  *aWebBrowser = mWebBrowser;
  NS_ADDREF(*aWebBrowser);
  return NS_OK;
}
 
