/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsDeviceContextSpecMac.h"
#include "prmem.h"
#include "plstr.h"

/** -------------------------------------------------------
 *  Construct the nsDeviceContextSpecMac
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecMac :: nsDeviceContextSpecMac()
{
  NS_INIT_REFCNT();
	mPrtRec = nsnull;
	mPrintManagerOpen = PR_FALSE;
	
}

/** -------------------------------------------------------
 *  Destroy the nsDeviceContextSpecMac
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecMac :: ~nsDeviceContextSpecMac()
{

	if(mPrtRec != nsnull){
		::DisposeHandle((Handle)mPrtRec);
		mPrtRec = nsnull;
		ClosePrintManager();
	}

}

static NS_DEFINE_IID(kDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);

NS_IMPL_QUERY_INTERFACE(nsDeviceContextSpecMac, kDeviceContextSpecIID)
NS_IMPL_ADDREF(nsDeviceContextSpecMac)
NS_IMPL_RELEASE(nsDeviceContextSpecMac)

/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecMac
 *  @update   dc 12/02/98
 */
NS_IMETHODIMP nsDeviceContextSpecMac :: Init(PRBool	aQuiet)
{
nsresult  theResult = NS_ERROR_FAILURE;
THPrint		prtRec;
GrafPtr		oldport;

	::GetPort(&oldport);
	
	// open the printing manager
	::PrOpen();
	if(::PrError() == noErr){
		mPrintManagerOpen = PR_TRUE;
		prtRec = (THPrint)::NewHandle(sizeof(TPrint));
		if(prtRec!=nsnull){
			::PrintDefault(prtRec);
		
			// standard print dialog, if true print
			if(::PrJobDialog(prtRec)){
				// have the print record
				theResult = NS_OK;
				mPrtRec = prtRec;
			}else{
				// don't print,
				::DisposeHandle((Handle)prtRec);
				::SetPort(oldport); 
			}
		}
	}
  return theResult;
}

/** -------------------------------------------------------
 * Closes the printmanager if it is open.
 * @update   dc 12/03/98
 */
NS_IMETHODIMP nsDeviceContextSpecMac :: ClosePrintManager()
{
PRBool	isPMOpen;

	this->PrintManagerOpen(&isPMOpen);
	if(isPMOpen){
		::PrClose();
	}
	
	return NS_OK;
}  
