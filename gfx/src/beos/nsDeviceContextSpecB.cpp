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

#include "nsDeviceContextSpecB.h"
 
#include "nsCOMPtr.h" 
#include "nsIServiceManager.h" 
 
#include "nsIPref.h" 
#include "prenv.h" /* for PR_GetEnv */ 

#include "nsIDOMWindow.h" 
#include "nsIServiceManager.h" 
#include "nsIDialogParamBlock.h" 
#include "nsISupportsPrimitives.h"
#include "nsIWindowWatcher.h"
#include "nsIDOMWindowInternal.h"
 
//#include "prmem.h"
//#include "plstr.h"

/** -------------------------------------------------------
 *  Construct the nsDeviceContextSpecBeOS
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecBeOS :: nsDeviceContextSpecBeOS()
{
  NS_INIT_REFCNT();
	
}

/** -------------------------------------------------------
 *  Destroy the nsDeviceContextSpecBeOS
 *  @update   dc 2/15/98
 */
nsDeviceContextSpecBeOS :: ~nsDeviceContextSpecBeOS()
{
} 
 
static NS_DEFINE_IID(kIDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID); 
static NS_DEFINE_IID(kIDeviceContextSpecPSIID, NS_IDEVICE_CONTEXT_SPEC_PS_IID); 
 
#if 0 
NS_IMPL_ISUPPORTS1(nsDeviceContextSpecBeOS, nsIDeviceContextSpec)
#endif 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: QueryInterface(REFNSIID aIID, void** aInstancePtr) 
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
 
NS_IMPL_ADDREF(nsDeviceContextSpecBeOS)
NS_IMPL_RELEASE(nsDeviceContextSpecBeOS)

/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecBeOS
 *  @update   dc 2/15/98
 *  @update   syd 3/2/99
 */
NS_IMETHODIMP nsDeviceContextSpecBeOS :: Init(PRBool	aQuiet)
{
  char *path;

  PRBool reversed = PR_FALSE, color = PR_FALSE, landscape = PR_FALSE; 
  PRBool tofile = PR_FALSE; 
  PRInt32 paper_size = NS_LETTER_SIZE; 
  PRInt32 orientation = NS_PORTRAIT;
  int ileft = 500, iright = 0, itop = 500, ibottom = 0; 
  char *command; 
  char *printfile = nsnull; 
 
  nsresult rv = NS_ERROR_FAILURE;
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
      nsCOMPtr<nsIPref> pPrefs = do_GetService(NS_PREF_CONTRACTID, &rv); 
      if (NS_SUCCEEDED(rv) && pPrefs) { 
	(void) pPrefs->GetBoolPref("print.print_reversed", &reversed); 
	(void) pPrefs->GetBoolPref("print.print_color", &color); 
	(void) pPrefs->GetBoolPref("print.print_landscape", &landscape); 
	(void) pPrefs->GetIntPref("print.print_paper_size", &paper_size); 
	(void) pPrefs->GetIntPref("print.print_orientation", &orientation); 
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
      mPrData.orientation = orientation;
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

  return NS_ERROR_FAILURE;
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetToPrinter( PRBool &aToPrinter )     
{ 
  aToPrinter = mPrData.toPrinter;
        return NS_OK;
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetFirstPageFirst ( PRBool &aFpf )      
{ 
  aFpf = mPrData.fpf; 
  return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetGrayscale ( PRBool &aGrayscale )      
{ 
  aGrayscale = mPrData.grayscale; 
  return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetSize ( int &aSize )      
{ 
  aSize = mPrData.size; 
  return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetPageDimensions ( float &aWidth, float &aHeight )      
{ 
    if ( mPrData.size == NS_LETTER_SIZE ) { 
        aWidth = 8.5; 
        aHeight = 11.0; 
    } else if ( mPrData.size == NS_LEGAL_SIZE ) { 
        aWidth = 8.5; 
        aHeight = 14.0; 
    } else if ( mPrData.size == NS_EXECUTIVE_SIZE ) { 
        aWidth = 7.5; 
        aHeight = 10.0; 
    } else if ( mPrData.size == NS_A4_SIZE ) { 
        // 210mm X 297mm == 8.27in X 11.69in 
        aWidth = 8.27; 
        aHeight = 11.69; 
    } else if ( mPrData.size == NS_A3_SIZE ) {
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
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetLandscape ( PRBool &landscape )      
{ 
  landscape = (mPrData.orientation == NS_LANDSCAPE);
  return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetTopMargin ( float &value )      
{ 
  value = mPrData.top; 
  return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetBottomMargin ( float &value )      
{ 
  value = mPrData.bottom; 
  return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetRightMargin ( float &value )      
{ 
  value = mPrData.right; 
  return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetLeftMargin ( float &value )      
{ 
  value = mPrData.left; 
  return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetCommand ( char **aCommand )      
{ 
  *aCommand = &mPrData.command[0]; 
  return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetPath ( char **aPath )      
{ 
  *aPath = &mPrData.path[0]; 
  return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetUserCancelled( PRBool &aCancel )     
{ 
  aCancel = mPrData.cancel; 
  return NS_OK;
}

/** -------------------------------------------------------
 * Closes the printmanager if it is open.
 *  @update   dc 2/15/98
 */
NS_IMETHODIMP nsDeviceContextSpecBeOS :: ClosePrintManager()
{
	return NS_OK;
}  
