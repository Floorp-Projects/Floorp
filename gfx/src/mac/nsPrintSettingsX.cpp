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

#include "nsPrintSettingsX.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIServiceManagerUtils.h"

#include "plbase64.h"
#include "prmem.h"
#include "nsGfxUtils.h"

// Constants
#define PRINTING_PREF_BRANCH            "print."
#define MAC_OS_X_PAGE_SETUP_PREFNAME    "macosx.pagesetup"


NS_IMPL_ISUPPORTS_INHERITED1(nsPrintSettingsX, 
                             nsPrintSettings, 
                             nsIPrintSettingsX)

/** ---------------------------------------------------
 */
nsPrintSettingsX::nsPrintSettingsX() :
  mPageFormat(kPMNoPageFormat),
  mPrintSettings(kPMNoPrintSettings)
{
    NS_INIT_REFCNT();
}

/** ---------------------------------------------------
 */
nsPrintSettingsX::~nsPrintSettingsX()
{
  if (mPageFormat != kPMNoPageFormat) {
    ::PMDisposePageFormat(mPageFormat);
    mPageFormat = kPMNoPageFormat;
  }
}

/** ---------------------------------------------------
 */
nsresult nsPrintSettingsX::Init()
{
  OSStatus status;
  
  status = ::PMNewPageFormat(&mPageFormat);
  if (status != noErr)
    return NS_ERROR_FAILURE;
  status = ::PMNewPrintSettings(&mPrintSettings);
  if (status != noErr)
    return NS_ERROR_FAILURE;
  
  status = ::PMBegin();
  NS_ASSERTION(status == noErr, "Error from PMBegin()");
  if (status == noErr) {
    status = ::PMDefaultPageFormat(mPageFormat);
    NS_ASSERTION(status == noErr, "Error from PMDefaultPageFormat()");
    status = ::PMDefaultPrintSettings(mPrintSettings);
    NS_ASSERTION(status == noErr, "Error from PMDefaultPrintSettings()");
    
    ::PMEnd();
  }
  return NS_OK;
}

/** ---------------------------------------------------
 */
NS_IMETHODIMP nsPrintSettingsX::GetPMPageFormat(PMPageFormat *aPMPageFormat)
{
  NS_ENSURE_ARG_POINTER(aPMPageFormat);
  *aPMPageFormat = kPMNoPageFormat;
  NS_ENSURE_STATE(mPageFormat != kPMNoPageFormat);
  
  PMPageFormat    pageFormat;
  OSStatus status = ::PMNewPageFormat(&pageFormat);
  if (status != noErr) return NS_ERROR_FAILURE;

  status = ::PMCopyPageFormat(mPageFormat, pageFormat);
  if (status != noErr) {
    ::PMDisposePageFormat(pageFormat);
    return NS_ERROR_FAILURE;
  }
  *aPMPageFormat = pageFormat;
  
  return NS_OK;
}

/** ---------------------------------------------------
 */
NS_IMETHODIMP nsPrintSettingsX::SetPMPageFormat(PMPageFormat aPMPageFormat)
{
  NS_ENSURE_ARG(aPMPageFormat); 
  NS_ENSURE_STATE(mPageFormat != kPMNoPageFormat);
  
  OSErr status = ::PMCopyPageFormat(aPMPageFormat, mPageFormat);
  if (status != noErr)
    return NS_ERROR_FAILURE;
      
  return NS_OK;
}

/** ---------------------------------------------------
 */
NS_IMETHODIMP nsPrintSettingsX::GetPMPrintSettings(PMPrintSettings *aPMPrintSettings)
{
  NS_ENSURE_ARG_POINTER(aPMPrintSettings);
  *aPMPrintSettings = kPMNoPrintSettings;
  NS_ENSURE_STATE(mPrintSettings != kPMNoPrintSettings);
  
  PMPrintSettings    printSettings;
  OSStatus status = ::PMNewPrintSettings(&printSettings);
  if (status != noErr) return NS_ERROR_FAILURE;

  status = ::PMCopyPrintSettings(mPrintSettings, printSettings);
  if (status != noErr) {
    ::PMDisposePrintSettings(printSettings);
    return NS_ERROR_FAILURE;
  }
  *aPMPrintSettings = printSettings;
  
  return NS_OK;
}

/** ---------------------------------------------------
 */
NS_IMETHODIMP nsPrintSettingsX::SetPMPrintSettings(PMPrintSettings aPMPrintSettings)
{
  NS_ENSURE_ARG(aPMPrintSettings); 
  NS_ENSURE_STATE(mPrintSettings != kPMNoPrintSettings);
  
  OSErr status = ::PMCopyPrintSettings(aPMPrintSettings, mPrintSettings);
  if (status != noErr)
    return NS_ERROR_FAILURE;
      
  return NS_OK;
}

/** ---------------------------------------------------
 */
NS_IMETHODIMP nsPrintSettingsX::ReadPageFormatFromPrefs()
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
  rv = prefBranch->GetCharPref(MAC_OS_X_PAGE_SETUP_PREFNAME, getter_Copies(encodedData));
  if (NS_FAILED(rv))
    return rv;

  // decode the base64
  PRInt32   encodedDataLen = nsCRT::strlen(encodedData.get());
  char* decodedData = ::PL_Base64Decode(encodedData.get(), encodedDataLen, nsnull);
  if (!decodedData)
    return NS_ERROR_FAILURE;

  Handle    decodedDataHandle = nsnull;
  OSErr err = ::PtrToHand(decodedData, &decodedDataHandle, (encodedDataLen * 3) / 4);
  PR_Free(decodedData);
  if (err != noErr)
    return NS_ERROR_OUT_OF_MEMORY;

  StHandleOwner   handleOwner(decodedDataHandle);  

  OSStatus      status;
  PMPageFormat  newPageFormat = kPMNoPageFormat;
  
  status = ::PMNewPageFormat(&newPageFormat);
  if (status != noErr) 
    return NS_ERROR_FAILURE;  
  status = ::PMUnflattenPageFormat(decodedDataHandle, &newPageFormat);
  if (status != noErr) 
    return NS_ERROR_FAILURE;

  status = ::PMCopyPageFormat(newPageFormat, mPageFormat);
  ::PMDisposePageFormat(newPageFormat);

  return (status == noErr) ? NS_OK : NS_ERROR_FAILURE;
}

/** ---------------------------------------------------
 */
NS_IMETHODIMP nsPrintSettingsX::WritePageFormatToPrefs()
{
  if (mPageFormat == kPMNoPageFormat)
    return NS_ERROR_NOT_INITIALIZED;
    
  nsresult rv;
  nsCOMPtr<nsIPrefService> prefService(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  nsCOMPtr<nsIPrefBranch> prefBranch;
  rv = prefService->GetBranch(PRINTING_PREF_BRANCH, getter_AddRefs(prefBranch));
  if (NS_FAILED(rv))
    return rv;

  Handle    pageFormatHandle = nsnull;
  OSStatus  err = ::PMFlattenPageFormat(mPageFormat, &pageFormatHandle);
  if (err != noErr)
    return NS_ERROR_FAILURE;
    
  StHandleOwner   handleOwner(pageFormatHandle);
  StHandleLocker  handleLocker(pageFormatHandle);
  
  nsXPIDLCString  encodedData;
  encodedData.Adopt(::PL_Base64Encode(*pageFormatHandle, ::GetHandleSize(pageFormatHandle), nsnull));
  if (!encodedData.get())
    return NS_ERROR_OUT_OF_MEMORY;

  return prefBranch->SetCharPref(MAC_OS_X_PAGE_SETUP_PREFNAME, encodedData);
}
