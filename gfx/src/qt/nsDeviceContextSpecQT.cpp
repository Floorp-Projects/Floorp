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
 *		John C. Griggs <johng@corel.com>
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

#include "nsDeviceContextSpecQT.h"
#include "nsRenderingContextQT.h"

#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsIServiceManager.h"
#include "nsIPrintOptions.h"
#include "nsGfxCIID.h"

#include "nsIPref.h"
#include "prenv.h" /* for PR_GetEnv */

#include "nsIAppShellComponentImpl.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDialogParamBlock.h"
#include "nsISupportsPrimitives.h"
#include "nsIWindowWatcher.h"

static NS_DEFINE_CID(kPrintOptionsCID, NS_PRINTOPTIONS_CID);

#include <qapplication.h>

//JCG #define DBG_JCG 1

#ifdef DBG_JCG
PRUint32 gDCSpecCount = 0;
PRUint32 gDCSpecID = 0;
#endif

/** -------------------------------------------------------
 *  Construct the nsDeviceContextSpecQT
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecQT::nsDeviceContextSpecQT()
{
#ifdef DBG_JCG
  gDCSpecCount++;
  mID = gDCSpecID++;
  printf("JCG: nsDeviceContextSpecQT CTOR (%p) ID: %d, Count: %d\n",this,mID,gDCSpecCount);
#endif
  NS_INIT_REFCNT();
}

/** -------------------------------------------------------
 *  Destroy the nsDeviceContextSpecQT
 *  @update   dc 2/15/98
 */
nsDeviceContextSpecQT::~nsDeviceContextSpecQT()
{
#ifdef DBG_JCG
  gDCSpecCount--;
  printf("JCG: nsDeviceContextSpecQT DTOR (%p) ID: %d, Count: %d\n",this,mID,gDCSpecCount);
#endif
}

static NS_DEFINE_IID(kIDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);
static NS_DEFINE_IID(kIDeviceContextSpecPSIID, NS_IDEVICE_CONTEXT_SPEC_PS_IID); 

NS_IMPL_ADDREF(nsDeviceContextSpecQT)
NS_IMPL_RELEASE(nsDeviceContextSpecQT)

NS_IMETHODIMP nsDeviceContextSpecQT::QueryInterface(REFNSIID aIID,
                                                    void** aInstancePtr)
{
  if (nsnull == aInstancePtr)
    return NS_ERROR_NULL_POINTER;
 
  if (aIID.Equals(kIDeviceContextSpecIID)) {
    nsIDeviceContextSpec* tmp = this;
    *aInstancePtr = (void*)tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
 
  if (aIID.Equals(kIDeviceContextSpecPSIID)) {
    nsIDeviceContextSpecPS* tmp = this;
    *aInstancePtr = (void*)tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
 
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
 
  if (aIID.Equals(kISupportsIID)) {
    nsIDeviceContextSpec* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*)tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
} 

/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecQT
 *  @update   dc 2/15/98
 *  @update   syd 3/2/99
 */
NS_IMETHODIMP nsDeviceContextSpecQT::Init(PRBool aQuiet)
{
  nsresult  rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIPrintOptions> printService(do_GetService(kPrintOptionsCID, &rv));
 
  // if there is a current selection then enable the "Selection" radio button
  if (NS_SUCCEEDED(rv) && printService) {
    PRBool isOn;
    printService->GetPrintOptions(nsIPrintOptions::kPrintOptionsEnableSelectionRB,
                                  &isOn);
    nsCOMPtr<nsIPref> pPrefs = do_GetService(NS_PREF_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv) && pPrefs) {
      (void)pPrefs->SetBoolPref("print.selection_radio_enabled",isOn);
    }
  }
  char *path;
  PRBool reversed = PR_FALSE;
  PRBool color = PR_FALSE;
  PRBool tofile = PR_FALSE;
  PRInt16 printRange = nsIPrintOptions::kRangeAllPages;
  PRInt32 paper_size = NS_LETTER_SIZE;
  PRInt32 orientation    = NS_PORTRAIT;
  PRInt32 fromPage = 1;
  PRInt32 toPage = 1;
  PRUnichar *command = nsnull;
  PRUnichar *printfile = nsnull;
  double dleft = 0.5;
  double dright = 0.5;
  double dtop = 0.5;
  double dbottom = 0.5;

  rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIDialogParamBlock> ioParamBlock(do_CreateInstance("@mozilla.org/embedcomp/dialogparam;1"));

  nsCOMPtr<nsISupportsInterfacePointer> paramBlockWrapper;
  if (ioParamBlock)
    paramBlockWrapper = do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID);

  if (paramBlockWrapper) {
    paramBlockWrapper->SetData(ioParamBlock);
    paramBlockWrapper->SetDataIID(&NS_GET_IID(nsIDialogParamBlock));

    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
    if (wwatch) {

      nsCOMPtr<nsIDOMWindowInternal> parent;
      nsCOMPtr<nsIDOMWindow> active;
      wwatch->GetActiveWindow(getter_AddRefs(active));
      if (active) {
        active->QueryInterface(NS_GET_IID(nsIDOMWindowInternal), getter_AddRefs(parent));
      }

      nsCOMPtr<nsIDOMWindow> newWindow;
      rv = wwatch->OpenWindow(parent, "chrome://global/content/printdialog.xul",
		    "_blank", "chrome,modal", paramBlockWrapper,
		    getter_AddRefs(newWindow));
    }
  }

  if (NS_SUCCEEDED(rv)) {
    PRInt32 buttonPressed = 0;
    ioParamBlock->GetInt(0, &buttonPressed);
    if (buttonPressed == 0) {
      if (printService) {
	printService->GetPrintReversed(&reversed);
	printService->GetPrintInColor(&color);
	printService->GetPaperSize(&paper_size);
        printService->GetOrientation(&orientation);
	printService->GetPrintCommand(&command);
	printService->GetPrintRange(&printRange);
	printService->GetToFileName(&printfile);
	printService->GetPrintToFile(&tofile);
	printService->GetStartPageRange(&fromPage);
	printService->GetEndPageRange(&toPage);
	printService->GetMarginTop(&dtop);
	printService->GetMarginLeft(&dleft);
	printService->GetMarginBottom(&dbottom);
	printService->GetMarginRight(&dright);

	if (command != nsnull && printfile != nsnull) {
	  // convert Unicode strings to cstrings
	  nsAutoString cmdStr;
	  nsAutoString printFileStr;

	  cmdStr = command;
	  printFileStr = printfile;
	  char *pCmdStr = ToNewCString(cmdStr);
	  char *pPrintFileStr = ToNewCString(printFileStr);
	  sprintf(mPrData.command,pCmdStr);
	  sprintf(mPrData.path,pPrintFileStr);
	  nsMemory::Free(pCmdStr);
	  nsMemory::Free(pPrintFileStr);
	}
      }
      else {
#ifndef VMS
	sprintf( mPrData.command, "lpr" );
#else
	// Note to whoever puts the "lpr" into the prefs file. Please contact me
	// as I need to make the default be "print" instead of "lpr" for OpenVMS.
	sprintf( mPrData.command, "print" );
#endif
      }
      mPrData.top = dtop;
      mPrData.bottom = dbottom;
      mPrData.left = dleft;
      mPrData.right = dright;
      mPrData.fpf = !reversed;
      mPrData.grayscale = !color;
      mPrData.size = paper_size;
      mPrData.orientation = orientation;
      mPrData.toPrinter = !tofile;

      // PWD, HOME, or fail
      if (!printfile) {
	if ((path = PR_GetEnv("PWD")) == (char*)NULL)
	  if ((path = PR_GetEnv("HOME")) == (char*)NULL)
	    strcpy(mPrData.path,"mozilla.ps");
	if (path != (char*)NULL)
	  sprintf(mPrData.path,"%s/mozilla.ps",path);
	else
	  return NS_ERROR_FAILURE;
      }
      if (command != nsnull) {
	nsMemory::Free(command);
      }
      if (printfile != nsnull) {
	nsMemory::Free(printfile);
      }
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDeviceContextSpecQT::GetToPrinter(PRBool &aToPrinter)
{
  aToPrinter = mPrData.toPrinter;
  return NS_OK;
}
 
NS_IMETHODIMP nsDeviceContextSpecQT::GetFirstPageFirst(PRBool &aFpf)
{
  aFpf = mPrData.fpf;
  return NS_OK;
}
 
NS_IMETHODIMP nsDeviceContextSpecQT::GetGrayscale(PRBool &aGrayscale)
{
  aGrayscale = mPrData.grayscale;
  return NS_OK;
}
 
NS_IMETHODIMP nsDeviceContextSpecQT::GetSize(int &aSize)
{
  aSize = mPrData.size;
  return NS_OK;
}
 
NS_IMETHODIMP nsDeviceContextSpecQT::GetPageDimensions(float &aWidth,
                                                       float &aHeight)
{
    if (mPrData.size == NS_LETTER_SIZE) {
        aWidth = 8.5;
        aHeight = 11.0;
    }
    else if (mPrData.size == NS_LEGAL_SIZE) {
        aWidth = 8.5;
        aHeight = 14.0;
    }
    else if (mPrData.size == NS_EXECUTIVE_SIZE) {
        aWidth = 7.5;
        aHeight = 10.0;
    }
    else if (mPrData.size == NS_A4_SIZE) {
        // 210mm X 297mm == 8.27in X 11.69in
        aWidth = 8.27;
        aHeight = 11.69;
    }
    else if (mPrData.size == NS_A3_SIZE) {
        // 297mm X 420mm == 11.69in X 16.53in
        aWidth = 11.69;
        aHeight = 16.53;
    }
        
    if (mPrData.orientation == NS_LANDSCAPE) {
      float temp;
      temp = aWidth;
      aWidth = aHeight;
      aHeight = temp;
    }

  return NS_OK;
}
 
NS_IMETHODIMP nsDeviceContextSpecQT::GetLandscape(PRBool &landscape)
{
  landscape = (mPrData.orientation == NS_LANDSCAPE);
  return NS_OK;
}
 
NS_IMETHODIMP nsDeviceContextSpecQT::GetTopMargin(float &value)
{
  value = mPrData.top;
  return NS_OK;
}
 
NS_IMETHODIMP nsDeviceContextSpecQT::GetBottomMargin(float &value)
{
  value = mPrData.bottom;
  return NS_OK;
}
 
NS_IMETHODIMP nsDeviceContextSpecQT::GetRightMargin(float &value)
{
  value = mPrData.right;
  return NS_OK;
}
 
NS_IMETHODIMP nsDeviceContextSpecQT::GetLeftMargin(float &value)
{
  value = mPrData.left;
  return NS_OK;
}
 
NS_IMETHODIMP nsDeviceContextSpecQT::GetCommand(char **aCommand)
{
  *aCommand = &mPrData.command[0];
  return NS_OK;
}
 
NS_IMETHODIMP nsDeviceContextSpecQT::GetPath(char **aPath)
{
  *aPath = &mPrData.path[0];
  return NS_OK;
}
 
NS_IMETHODIMP nsDeviceContextSpecQT::GetUserCancelled(PRBool &aCancel)
{
  aCancel = mPrData.cancel;
  return NS_OK;
}

/** -------------------------------------------------------
 * Closes the printmanager if it is open.
 *  @update   dc 2/15/98
 */
NS_IMETHODIMP nsDeviceContextSpecQT::ClosePrintManager()
{
    return NS_OK;
}
