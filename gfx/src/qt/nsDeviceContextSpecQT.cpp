/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsDeviceContextSpecQT.h"
#include "nsRenderingContextQT.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
 
#include "nsIPref.h"
 
#include "prenv.h" /* for PR_GetEnv */

#include "nsIAppShellComponentImpl.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDialogParamBlock.h"
#include "nsINetSupportDialogService.h"

static NS_DEFINE_IID( kAppShellServiceCID, NS_APPSHELL_SERVICE_CID );
static NS_DEFINE_CID(kDialogParamBlockCID, NS_DialogParamBlock_CID);

#include "stdlib.h"  // getenv() on Solaris/CC
#include <qapplication.h>

/** -------------------------------------------------------
 *  Construct the nsDeviceContextSpecQT
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecQT::nsDeviceContextSpecQT()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextSpecQT::nsDeviceContextSpecQT\n"));
    NS_INIT_REFCNT();
}

/** -------------------------------------------------------
 *  Destroy the nsDeviceContextSpecQT
 *  @update   dc 2/15/98
 */
nsDeviceContextSpecQT::~nsDeviceContextSpecQT()
{
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextSpecQT::~nsDeviceContextSpecQT\n"));
}

static NS_DEFINE_IID(kIDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);
static NS_DEFINE_IID(kIDeviceContextSpecPSIID, NS_IDEVICE_CONTEXT_SPEC_PS_IID); 

NS_IMPL_ADDREF(nsDeviceContextSpecQT)
NS_IMPL_RELEASE(nsDeviceContextSpecQT)

NS_IMETHODIMP nsDeviceContextSpecQT::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr)
    return NS_ERROR_NULL_POINTER;
 
  if (aIID.Equals(kIDeviceContextSpecIID))
  {
    nsIDeviceContextSpec* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
 
  if (aIID.Equals(kIDeviceContextSpecPSIID))
  {
    nsIDeviceContextSpecPS* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
 
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
 
  if (aIID.Equals(kISupportsIID))
  {
    nsIDeviceContextSpec* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
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
    PR_LOG(QtGfxLM, PR_LOG_DEBUG, ("nsDeviceContextSpecQT::Init\n"));
  char *path;

  PRBool reversed = PR_FALSE, color = PR_FALSE, landscape = PR_FALSE;
  PRBool tofile = PR_FALSE;
  PRInt32 paper_size = NS_LETTER_SIZE;
  int ileft = 500, iright = 0, itop = 500, ibottom = 0;
  char *command;
  char *printfile = nsnull;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIDialogParamBlock> ioParamBlock;

  rv = nsComponentManager::CreateInstance(kDialogParamBlockCID,
                                          nsnull,
                                          NS_GET_IID(nsIDialogParamBlock),
                                          getter_AddRefs(ioParamBlock));

  if (NS_SUCCEEDED(rv)) {
    NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIDOMWindowInternal> hiddenWindow;
      nsCOMPtr<nsIDOMWindowInternal> mWindow;

      JSContext *jsContext;
      rv = appShell->GetHiddenWindowAndJSContext(getter_AddRefs(hiddenWindow), &jsContext);
      if (NS_SUCCEEDED(rv)) {
        void *stackPtr;
        jsval *argv = JS_PushArguments(jsContext,
                                       &stackPtr,
                                       "sss%ip",
                                       "chrome://global/content/printdialog.xul",
                                       "_blank",
                                       "chrome,modal",
                                       (const nsIID *) (&NS_GET_IID(nsIDialogParamBlock)),
                                       (nsISupports *) ioParamBlock);
        if (argv) {
          nsCOMPtr<nsIDOMWindowInternal> newWindow;

          rv = hiddenWindow->OpenDialog(jsContext,
                                        argv,
                                        4,
                                        getter_AddRefs(newWindow));

          if (NS_SUCCEEDED(rv)) {
            JS_PopArguments(jsContext, stackPtr);
            PRInt32 buttonPressed = 0;
            ioParamBlock->GetInt(0, &buttonPressed);
            if (buttonPressed == 0) {
              nsCOMPtr<nsIPref> pPrefs = do_GetService(NS_PREF_CONTRACTID, &rv);
              if (NS_SUCCEEDED(rv) && pPrefs) {
                (void) pPrefs->GetBoolPref("print.print_reversed", &reversed);
                (void) pPrefs->GetBoolPref("print.print_color", &color);
                (void) pPrefs->GetBoolPref("print.print_landscape", &landscape);
                (void) pPrefs->GetIntPref("print.print_paper_size", &paper_size);
                (void) pPrefs->CopyCharPref("print.print_command", (char **) &command);
                (void) pPrefs->GetIntPref("print.print_margin_top", &itop);
                (void) pPrefs->GetIntPref("print.print_margin_left", &ileft);
                (void) pPrefs->GetIntPref("print.print_margin_bottom", &ibottom);
                (void) pPrefs->GetIntPref("print.print_margin_right", &iright);
                (void) pPrefs->CopyCharPref("print.print_file", (char **) &printfile);
                (void) pPrefs->GetBoolPref("print.print_tofile", &tofile);
                sprintf( mPrData.command, command );
                sprintf( mPrData.path, printfile );
              } else {
#ifndef VMS
                sprintf( mPrData.command, "lpr" );
#else
                // Note to whoever puts the "lpr" into the prefs file. Please contact me
                // as I need to make the default be "print" instead of "lpr" for OpenVMS.
                sprintf( mPrData.command, "print" );
#endif
              }

              mPrData.top = itop / 1000.0;
              mPrData.bottom = ibottom / 1000.0;
              mPrData.left = ileft / 1000.0;
              mPrData.right = iright / 1000.0;
              mPrData.fpf = !reversed;
              mPrData.grayscale = !color;
              mPrData.size = paper_size;
              mPrData.toPrinter = !tofile;

              // PWD, HOME, or fail

              if (!printfile) {
                if ( ( path = PR_GetEnv( "PWD" ) ) == (char *) NULL )
                    if ( ( path = PR_GetEnv( "HOME" ) ) == (char *) NULL )
                            strcpy( mPrData.path, "mozilla.ps" );
                if ( path != (char *) NULL )
                    sprintf( mPrData.path, "%s/mozilla.ps", path );
                else
                    return NS_ERROR_FAILURE;
              }
              return NS_OK;
            }
          }
        }
      }
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
 
NS_IMETHODIMP nsDeviceContextSpecQT::GetPageDimensions(float &aWidth,float &aHeight)
{
    if (mPrData.size == NS_LETTER_SIZE)
    {
        aWidth = 8.5;
        aHeight = 11.0;
    }
    else if (mPrData.size == NS_LEGAL_SIZE)
    {
        aWidth = 8.5;
        aHeight = 14.0;
    }
    else if (mPrData.size == NS_EXECUTIVE_SIZE)
    {
        aWidth = 7.5;
        aHeight = 10.0;
    }
    else if (mPrData.size == NS_A4_SIZE)
    {
        // 210mm X 297mm == 8.27in X 11.69in
        aWidth = 8.27;
        aHeight = 11.69;
    }
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
    PR_LOG(QtGfxLM,PR_LOG_DEBUG,("nsDeviceContextSpecQT::ClosePrintManager\n"));
    return NS_OK;
}
