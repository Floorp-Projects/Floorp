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
#include "nsPrintOptionsPh.h"

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsPrintOptionsPh, nsIPrintOptions)

nsPrintOptionsPh::nsPrintOptionsPh(void *data)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  mPC = (PpPrintContext_t *) data;
}

nsPrintOptionsPh::~nsPrintOptionsPh()
{
  /* destructor code */
}

/* void SetMargins (in PRInt32 aTop, in PRInt32 aLeft, in PRInt32 aRight, in PRInt32 aBottom); */
NS_IMETHODIMP nsPrintOptionsPh::SetMargins(PRInt32 aTop, PRInt32 aLeft, PRInt32 aRight, PRInt32 aBottom)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void GetMargins (out PRInt32 aTop, out PRInt32 aLeft, out PRInt32 aRight, out PRInt32 aBottom); */
NS_IMETHODIMP nsPrintOptionsPh::GetMargins(PRInt32 *aTop, PRInt32 *aLeft, PRInt32 *aRight, PRInt32 *aBottom)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void ShowNativeDialog (); */
NS_IMETHODIMP nsPrintOptionsPh::ShowNativeDialog()
{
	nsresult rv = NS_ERROR_FAILURE;
	int action;

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

	return rv;
}
