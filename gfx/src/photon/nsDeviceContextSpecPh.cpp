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

#include "nsDeviceContextSpecPh.h"
#include "nsPrintOptionsPh.h"
#include "prmem.h"
#include "plstr.h"
#include "nsPhGfxLog.h"

nsDeviceContextSpecPh :: nsDeviceContextSpecPh()
{
	NS_INIT_REFCNT();
	mPC = nsnull;
}

nsDeviceContextSpecPh :: ~nsDeviceContextSpecPh()
{
	if (mPC)
		PpPrintReleasePC(mPC);
}

static NS_DEFINE_IID(kDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);

NS_IMPL_QUERY_INTERFACE(nsDeviceContextSpecPh, kDeviceContextSpecIID)
NS_IMPL_ADDREF(nsDeviceContextSpecPh)
NS_IMPL_RELEASE(nsDeviceContextSpecPh)

NS_IMETHODIMP nsDeviceContextSpecPh :: Init(PRBool aQuiet)
{
	int action;
	nsresult rv = NS_OK;

	if (!mPC)
		mPC = PpCreatePC();

	if (aQuiet)
	{
		// no dialogs
		PpLoadDefaultPrinter(mPC);
	}
	else
	{
#if 1
		action = PtPrintSelection(NULL, NULL, NULL, mPC, (Pt_PRINTSEL_DFLT_LOOK));
		switch (action)
		{
			case Pt_PRINTSEL_PRINT:
			case Pt_PRINTSEL_PREVIEW:
				rv = NS_OK;
				break;
			case Pt_PRINTSEL_CANCEL:
				rv = NS_ERROR_FAILURE;
				break;
		}
#else
		// do native print options
		nsIPrintOptions *printOptions = new nsPrintOptionsPh((void *)mPC);
		if ((nsPrintOptionsPh *)printOptions->ShowNativeDialog() != NS_OK)
		{
			// cancel
			rv =  NS_ERROR_FAILURE;
		}
		delete printOptions;
#endif
	}

  	return rv;
}

//NS_IMETHODIMP nsDeviceContextSpecPh :: GetPrintContext(PpPrintContext_t *&aPrintContext) const
PpPrintContext_t *nsDeviceContextSpecPh :: GetPrintContext()
{
  return (mPC);
}
