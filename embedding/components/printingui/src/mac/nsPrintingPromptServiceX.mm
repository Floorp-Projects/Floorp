/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Rod Spears <rods@netscape.com>
 *   Conrad Carlen <ccarlen@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsPrintingPromptService.h"

#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsObjCExceptions.h"

#include "nsIPrintingPromptService.h"
#include "nsIFactory.h"
#include "nsIPrintDialogService.h"

//*****************************************************************************
// nsPrintingPromptService
//*****************************************************************************   

NS_IMPL_ISUPPORTS2(nsPrintingPromptService, nsIPrintingPromptService, nsIWebProgressListener)

nsPrintingPromptService::nsPrintingPromptService()
{
}

nsPrintingPromptService::~nsPrintingPromptService()
{
}

nsresult nsPrintingPromptService::Init()
{
    return NS_OK;
}

//*****************************************************************************
// nsPrintingPromptService::nsIPrintingPromptService
//*****************************************************************************   

NS_IMETHODIMP 
nsPrintingPromptService::ShowPrintDialog(nsIDOMWindow *parent, nsIWebBrowserPrint *webBrowserPrint, nsIPrintSettings *printSettings)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsCOMPtr<nsIPrintDialogService> dlgPrint(do_GetService(
                                           NS_PRINTDIALOGSERVICE_CONTRACTID));
  if (dlgPrint)
    return dlgPrint->Show(parent, printSettings, webBrowserPrint);

  return NS_ERROR_FAILURE;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP 
nsPrintingPromptService::ShowProgress(nsIDOMWindow*            parent, 
                                      nsIWebBrowserPrint*      webBrowserPrint,    // ok to be null
                                      nsIPrintSettings*        printSettings,      // ok to be null
                                      nsIObserver*             openDialogObserver, // ok to be null
                                      bool                     isForPrinting,
                                      nsIWebProgressListener** webProgressListener,
                                      nsIPrintProgressParams** printProgressParams,
                                      bool*                  notifyOnOpen)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsPrintingPromptService::ShowPageSetup(nsIDOMWindow *parent, nsIPrintSettings *printSettings, nsIObserver *aObs)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;
  nsCOMPtr<nsIPrintDialogService> dlgPrint(do_GetService(
                                           NS_PRINTDIALOGSERVICE_CONTRACTID));
  if (dlgPrint)
    return dlgPrint->ShowPageSetup(parent, printSettings);

  return NS_ERROR_FAILURE;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP 
nsPrintingPromptService::ShowPrinterProperties(nsIDOMWindow *parent, const PRUnichar *printerName, nsIPrintSettings *printSettings)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


//*****************************************************************************
// nsPrintingPromptService::nsIWebProgressListener
//*****************************************************************************   

NS_IMETHODIMP 
nsPrintingPromptService::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus)
{
    return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP 
nsPrintingPromptService::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
    return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP 
nsPrintingPromptService::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP 
nsPrintingPromptService::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
    return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP 
nsPrintingPromptService::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
    return NS_OK;
}
