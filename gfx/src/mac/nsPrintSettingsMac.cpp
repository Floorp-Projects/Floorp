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
 * Conrad Carlen <ccarlen@netscape.com>
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

#include "nsPrintSettingsMac.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIServiceManager.h"

#include "plbase64.h"
#include "prmem.h"
#include "nsGfxUtils.h"

// Constants
#define PRINTING_PREF_BRANCH            "print."
#define MAC_OS_PAGE_SETUP_PREFNAME      "macos.pagesetup"

NS_IMPL_ISUPPORTS_INHERITED1(nsPrintSettingsMac, 
                             nsPrintSettings, 
                             nsIPrintSettingsMac)

/** ---------------------------------------------------
 */
nsPrintSettingsMac::nsPrintSettingsMac() :
    mPrintRecord(nsnull)
{
}

/** ---------------------------------------------------
 */
nsPrintSettingsMac::nsPrintSettingsMac(const nsPrintSettingsMac& src) :
    mPrintRecord(nsnull)
{
  *this = src;
}

/** ---------------------------------------------------
 */
nsPrintSettingsMac::~nsPrintSettingsMac()
{
	if (mPrintRecord)
		::DisposeHandle((Handle)mPrintRecord);
}

/** ---------------------------------------------------
 */
nsPrintSettingsMac& nsPrintSettingsMac::operator=(const nsPrintSettingsMac& rhs)
{
  if (this == &rhs) {
    return *this;
  }

  nsPrintSettings::operator=(rhs);

  if (mPrintRecord) {
    ::DisposeHandle((Handle)mPrintRecord);
	mPrintRecord = nsnull;
  }
  Handle copyH = (Handle)rhs.mPrintRecord;
  if (::HandToHand(&copyH) == noErr)
    mPrintRecord = (THPrint)copyH;

  return *this;
}

/** ---------------------------------------------------
 */
NS_IMETHODIMP nsPrintSettingsMac::Init()
{
	// create the print style and print record
	mPrintRecord = (THPrint)::NewHandleClear(sizeof(TPrint));
	if (!mPrintRecord)
	  return NS_ERROR_OUT_OF_MEMORY;

	::PrOpen();
  if (::PrError() != noErr)
    return NS_ERROR_FAILURE;
  ::PrintDefault(mPrintRecord);
  ::PrClose();
	
	return NS_OK;
}

/** ---------------------------------------------------
 */
NS_IMETHODIMP nsPrintSettingsMac::GetTHPrint(THPrint *aThPrint)
{
  NS_ENSURE_ARG_POINTER(aThPrint);
  *aThPrint = nsnull;
  
  if (!mPrintRecord)
    return NS_ERROR_NOT_INITIALIZED;
    
  Handle temp = (Handle)mPrintRecord;
  if (::HandToHand(&temp) != noErr)
    return NS_ERROR_OUT_OF_MEMORY;
    
  *aThPrint = (THPrint)temp;
  return NS_OK;
}

/** ---------------------------------------------------
 */
NS_IMETHODIMP nsPrintSettingsMac::SetTHPrint(THPrint aThPrint)
{
    NS_ENSURE_ARG(aThPrint);
    
    if (mPrintRecord) {
      ::DisposeHandle((Handle)mPrintRecord);
      mPrintRecord = nsnull;
    }
    mPrintRecord = aThPrint;
    if (::HandToHand((Handle *)&mPrintRecord) != noErr)
      return NS_ERROR_OUT_OF_MEMORY;
      
    return NS_OK;
}

/** ---------------------------------------------------
 */
NS_IMETHODIMP nsPrintSettingsMac::ReadPageSetupFromPrefs()
{
  nsresult rv;
  nsCOMPtr<nsIPrefService> prefService(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  nsCOMPtr<nsIPrefBranch> prefBranch;
  rv = prefService->GetBranch(PRINTING_PREF_BRANCH, getter_AddRefs(prefBranch));
  if (NS_FAILED(rv))
    return rv;
    
  nsXPIDLCString  encodedData;
  rv = prefBranch->GetCharPref(MAC_OS_PAGE_SETUP_PREFNAME, getter_Copies(encodedData));
  if (NS_FAILED(rv))
    return rv;

  // decode the base64
  PRInt32   encodedDataLen = strlen(encodedData.get());
  char* decodedData = ::PL_Base64Decode(encodedData.get(), encodedDataLen, nsnull);
  if (!decodedData)
    return NS_ERROR_FAILURE;

  if (((encodedDataLen * 3) / 4) >= sizeof(TPrint))
    ::BlockMoveData(decodedData, *mPrintRecord, sizeof(TPrint));
  else
    rv = NS_ERROR_FAILURE;    // the data was too small
    
  PR_Free(decodedData);
  return rv;
}

/** ---------------------------------------------------
 */

NS_IMETHODIMP nsPrintSettingsMac::WritePageSetupToPrefs()
{
  if (!mPrintRecord)
    return NS_ERROR_NOT_INITIALIZED;
    
  nsresult rv;
  nsCOMPtr<nsIPrefService> prefService(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  nsCOMPtr<nsIPrefBranch> prefBranch;
  rv = prefService->GetBranch(PRINTING_PREF_BRANCH, getter_AddRefs(prefBranch));
  if (NS_FAILED(rv))
    return rv;

  StHandleLocker  locker((Handle)mPrintRecord);

  nsXPIDLCString  encodedData;
  encodedData.Adopt(::PL_Base64Encode((char *)*mPrintRecord, sizeof(TPrint), nsnull));
  if (!encodedData.get())
    return NS_ERROR_OUT_OF_MEMORY;

  return prefBranch->SetCharPref(MAC_OS_PAGE_SETUP_PREFNAME, encodedData);
}

//-------------------------------------------
nsresult nsPrintSettingsMac::_Clone(nsIPrintSettings **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  
  nsPrintSettingsMac *newSettings = new nsPrintSettingsMac(*this);
  if (!newSettings)
    return NS_ERROR_FAILURE;
  *_retval = newSettings;
  NS_ADDREF(*_retval);
  return NS_OK;
}

//-------------------------------------------
nsresult nsPrintSettingsMac::_Assign(nsIPrintSettings *aPS)
{
  nsPrintSettingsMac *printSettingsMac = dynamic_cast<nsPrintSettingsMac*>(aPS);
  if (!printSettingsMac)
    return NS_ERROR_UNEXPECTED;
  *this = *printSettingsMac;
  return NS_OK;
}

