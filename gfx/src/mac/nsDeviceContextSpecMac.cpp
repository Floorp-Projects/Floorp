/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsDeviceContextSpecMac.h"
#include "prmem.h"
#include "plstr.h"
#include "nsWatchTask.h"


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

#if !TARGET_CARBON
	::GetPort(&oldport);
	
	// open the printing manager
	::PrOpen();
	if(::PrError() == noErr){
		mPrintManagerOpen = PR_TRUE;
		prtRec = (THPrint)::NewHandle(sizeof(TPrint));
		if(prtRec!=nsnull){
			::PrintDefault(prtRec);
		
			// standard print dialog, if true print
      nsWatchTask::GetTask().Suspend();
			if(::PrJobDialog(prtRec)){
				// have the print record
				theResult = NS_OK;
				mPrtRec = prtRec;
			}else{
				// don't print,
				::DisposeHandle((Handle)prtRec);
				::SetPort(oldport); 
			}
			nsWatchTask::GetTask().Resume();
		}
	}
#endif
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
#if !TARGET_CARBON
		::PrClose();
#endif
	}
	
	return NS_OK;
}  
