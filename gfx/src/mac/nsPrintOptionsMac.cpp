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
#include "nsPrintOptionsMac.h"



/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsWin.h
 *	@update 6/21/00 dwc
 */
nsPrintOptionsMac::nsPrintOptionsMac()
{
	// create the print style and print record
	mPrintRecord = (THPrint)::NewHandle(sizeof(TPrint));
	if(nsnull != mPrintRecord){
		::PrintDefault(mPrintRecord);
	}


}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 */
nsPrintOptionsMac::~nsPrintOptionsMac()
{

	// get rid of the print record
	if(nsnull != mPrintRecord){
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
	
	if(nsnull != mPrintRecord){
		::PrStlDialog(mPrintRecord);		// open up and process the style record
	}
	return (NS_OK);
} 
