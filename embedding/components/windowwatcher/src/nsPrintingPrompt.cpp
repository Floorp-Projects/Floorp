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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsIServiceManager.h"
#include "nsPrintingPrompt.h"
#include "nsReadableUtils.h"

#include "nsIWebProgressListener.h"

nsresult
NS_NewPrintingPrompter(nsIPrintingPrompt **result, nsIDOMWindow *aParent)
{
  nsresult rv;
  *result = 0;

  nsPrintingPrompt *prompter = new nsPrintingPrompt(aParent);
  if (!prompter)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(prompter);
  rv = prompter->Init();
  if (NS_FAILED(rv)) {
    NS_RELEASE(prompter);
    return rv;
  }

  *result = prompter;
  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsPrintingPrompt, nsIPrintingPrompt)

nsPrintingPrompt::nsPrintingPrompt(nsIDOMWindow *aParent)
  : mParent(aParent)
{
  NS_INIT_ISUPPORTS();
}

nsresult
nsPrintingPrompt::Init()
{
  mPromptService = do_GetService("@mozilla.org/embedcomp/printingprompt-service;1");
  return mPromptService ? NS_OK : NS_ERROR_FAILURE;
}

//*****************************************************************************
// nsPrintingPrompt::nsIPrintingPrompt
//*****************************************************************************   

/* void showPrintDialog (in nsIWebBrowserPrint webBrowserPrint, in nsIPrintSettings printSettings); */
NS_IMETHODIMP 
nsPrintingPrompt::ShowPrintDialog(nsIWebBrowserPrint *webBrowserPrint, nsIPrintSettings *printSettings)
{
   return mPromptService->ShowPrintDialog(mParent, webBrowserPrint, printSettings);
}

/* void showProgress (in nsIWebBrowserPrint webBrowserPrint, in nsIPrintSettings printSettings, out nsIWebProgressListener webProgressListener, out nsIPrintProgressParams printProgressParams); */
NS_IMETHODIMP 
nsPrintingPrompt::ShowProgress(nsIWebBrowserPrint *webBrowserPrint, nsIPrintSettings *printSettings, nsIObserver *openDialogObserver, PRBool isForPrinting, nsIWebProgressListener **webProgressListener, nsIPrintProgressParams** printProgressParams, PRBool* notifyOnOpen)
{
   return mPromptService->ShowProgress(mParent, webBrowserPrint, printSettings, openDialogObserver, isForPrinting, webProgressListener, printProgressParams, notifyOnOpen);
}

/* void showPageSetup (in nsIPrintSettings printSettings); */
NS_IMETHODIMP 
nsPrintingPrompt::ShowPageSetup(nsIPrintSettings *printSettings, nsIObserver *aObs)
{
   return mPromptService->ShowPageSetup(mParent, printSettings, aObs);
}

/* void showPrinterProperties (in wstring printerName, in nsIPrintSettings printSettings); */
NS_IMETHODIMP 
nsPrintingPrompt::ShowPrinterProperties(const PRUnichar *printerName, nsIPrintSettings *printSettings)
{
   return mPromptService->ShowPrinterProperties(mParent, printerName, printSettings);
}

