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

#include "nsCOMPtr.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"

#include "nsWatchTask.h"
#include "nsPrintOptionsMac.h"
#include "nsGfxUtils.h"

#include "plbase64.h"
#include "prmem.h"

#define MAC_OS_PAGE_SETUP_PREFNAME  "print.macos.pagesetup"

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsWin.h
 *	@update 6/21/00 dwc
 */
nsPrintOptionsMac::nsPrintOptionsMac()
{
	// create the print style and print record
	mPrintRecord = (THPrint)::NewHandleClear(sizeof(TPrint));
	if (mPrintRecord)
	{
	  nsresult rv = ReadPageSetupFromPrefs();
	  ::PrOpen();
    if (::PrError() == noErr)
    {
  	  if (NS_FAILED(rv))
    		::PrintDefault(mPrintRecord);
      else
        ::PrValidate(mPrintRecord);

  		::PrClose();
		}
	}

}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 */
nsPrintOptionsMac::~nsPrintOptionsMac()
{
	// get rid of the print record
	if (mPrintRecord) {
		::DisposeHandle((Handle)mPrintRecord);
	}
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 */
NS_IMETHODIMP
nsPrintOptionsMac::ShowNativeDialog(void) 
{
  if (!mPrintRecord) return NS_ERROR_NOT_INITIALIZED;

  // open the printing manager
  ::PrOpen();
  if(::PrError() != noErr)
    return NS_ERROR_FAILURE;
  
  ::PrValidate(mPrintRecord);
  NS_ASSERTION(::PrError() == noErr, "Printing error");

  nsWatchTask::GetTask().Suspend();
  ::InitCursor();
  Boolean   dialogOK = ::PrStlDialog(mPrintRecord);		// open up and process the style record
  nsWatchTask::GetTask().Resume();
  
  OSErr err = ::PrError();
  
  ::PrClose();
  
  if (err != noErr)
    return NS_ERROR_FAILURE;
  return NS_OK;
} 

NS_IMETHODIMP 
nsPrintOptionsMac::ReadPrefs()
{
  // it doesn't really matter if this fails
  nsresult  rv = ReadPageSetupFromPrefs();
  NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to write page setup to prefs");
  
  return nsPrintOptions::ReadPrefs();
}

NS_IMETHODIMP 
nsPrintOptionsMac::WritePrefs()
{
  // it doesn't really matter if this fails
  nsresult  rv = WritePageSetupToPrefs();
  NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to save page setup to prefs");
  
  return nsPrintOptions::WritePrefs();
}

/* [noscript] voidPtr GetNativeData (in short aDataType); */
NS_IMETHODIMP
nsPrintOptionsMac::GetNativeData(PRInt16 aDataType, void * *_retval)
{
  nsresult rv = NS_OK;
  
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  
  switch (aDataType)
  {
    case kNativeDataPrintRecord:
      if (mPrintRecord)
      {
        void*   printRecord = nsMemory::Alloc(sizeof(TPrint));
        if (!printRecord) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }
        
        ::BlockMoveData(*mPrintRecord, printRecord, sizeof(TPrint));
        *_retval = printRecord;
      }
      break;
      
    default:
      rv = NS_ERROR_FAILURE;
      break;
  }

  return rv;
}

#pragma mark -

nsresult
nsPrintOptionsMac::ReadPageSetupFromPrefs()
{
  nsresult rv;
  nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;
    
  nsXPIDLCString  encodedData;
  rv = prefs->GetCharPref(MAC_OS_PAGE_SETUP_PREFNAME, getter_Copies(encodedData));
  if (NS_FAILED(rv))
    return rv;

  // decode the base64
  PRInt32   encodedDataLen = nsCRT::strlen(encodedData.get());
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

nsresult
nsPrintOptionsMac::WritePageSetupToPrefs()
{
  if (!mPrintRecord)
    return NS_ERROR_NOT_INITIALIZED;
    
  nsresult rv;
  nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  StHandleLocker  locker((Handle)mPrintRecord);

  nsXPIDLCString  encodedData;
  encodedData.Adopt(::PL_Base64Encode((char *)*mPrintRecord, sizeof(TPrint), nsnull));
  if (!encodedData.get())
    return NS_ERROR_OUT_OF_MEMORY;

  return prefs->SetCharPref(MAC_OS_PAGE_SETUP_PREFNAME, encodedData);
}

