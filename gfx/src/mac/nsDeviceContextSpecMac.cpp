/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsDeviceContextSpecMac.h"
#include "nsIServiceManager.h"
#include "nsIPrintOptions.h"
#include "nsIPrintSettingsMac.h"


/** -------------------------------------------------------
 *  Construct the nsDeviceContextSpecMac
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecMac::nsDeviceContextSpecMac()
: mPrtRec(nsnull)
, mPrintManagerOpen(PR_FALSE)
{
}

/** -------------------------------------------------------
 *  Destroy the nsDeviceContextSpecMac
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecMac::~nsDeviceContextSpecMac()
{
  ClosePrintManager();

  if (mPrtRec) {
    ::DisposeHandle((Handle)mPrtRec);
    mPrtRec = nsnull;
  }
}

NS_IMPL_ISUPPORTS2(nsDeviceContextSpecMac, nsIDeviceContextSpec, nsIPrintingContext)

/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecMac
 *  @update   dc 05/04/2001
 */
NS_IMETHODIMP nsDeviceContextSpecMac::Init(nsIPrintSettings* aPS, PRBool aIsPrintPreview)
{
  nsCOMPtr<nsIPrintSettingsMac> printSettingsMac(do_QueryInterface(aPS));
  if (!printSettingsMac)
    return NS_ERROR_NO_INTERFACE;
    
  // open the printing manager if not print preview
  if (!aIsPrintPreview) {
    ::PrOpen();
    if (::PrError() != noErr)
      return NS_ERROR_FAILURE;
    mPrintManagerOpen = PR_TRUE;
  }

  nsresult rv = printSettingsMac->GetTHPrint(&mPrtRec);
    
  // make sure the print record is valid
  ::PrValidate(mPrtRec);
  if (::PrError() != noErr)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

/** -------------------------------------------------------
 * Closes the printmanager if it is open.
 * @update   dc 12/03/98
 */
NS_IMETHODIMP nsDeviceContextSpecMac::ClosePrintManager()
{
  if (mPrintManagerOpen)
    ::PrClose();
  
  return NS_OK;
}  

NS_IMETHODIMP nsDeviceContextSpecMac::BeginDocument(PRInt32     aStartPage, 
                                                    PRInt32     aEndPage)
{
    nsresult rv = NS_OK;
    return rv;
}

NS_IMETHODIMP nsDeviceContextSpecMac::EndDocument()
{
    nsresult rv = NS_OK;
    return rv;
}

NS_IMETHODIMP nsDeviceContextSpecMac::BeginPage()
{
    nsresult rv = NS_OK;
    return rv;
}

NS_IMETHODIMP nsDeviceContextSpecMac::EndPage()
{
    nsresult rv = NS_OK;
    return rv;
}

NS_IMETHODIMP nsDeviceContextSpecMac::GetPrinterResolution(double* aResolution)
{
    nsresult rv = NS_ERROR_NOT_IMPLEMENTED;
    return rv;
}

NS_IMETHODIMP nsDeviceContextSpecMac::GetPageRect(double* aTop, double* aLeft, double* aBottom, double* aRight)
{
    nsresult rv = NS_ERROR_NOT_IMPLEMENTED;
    return rv;
}
