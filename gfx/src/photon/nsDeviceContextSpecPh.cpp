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

#include "nsDeviceContextSpecPh.h"
#include "nsPrintOptionsPh.h"
#include "prmem.h"
#include "plstr.h"
#include "nsPhGfxLog.h"

#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsReadableUtils.h"
#include "nsIPref.h"

nsDeviceContextSpecPh :: nsDeviceContextSpecPh()
{
	mPC = nsnull;
}

nsDeviceContextSpecPh :: ~nsDeviceContextSpecPh()
{
	if (mPC)
		PpPrintReleasePC(mPC);
}

NS_IMPL_ISUPPORTS1(nsDeviceContextSpecPh, nsIDeviceContextSpec)


NS_IMETHODIMP nsDeviceContextSpecPh :: Init(nsIWidget* aWidget,
                                             nsIPrintSettings* aPrintSettings,
                                             PRBool aQuiet)
{
	nsresult rv = NS_OK;
	int 		action;
	PtWidget_t 	*parent;

  	mPrintSettings = aPrintSettings;
  	parent = (PtWidget_t *) aWidget->GetNativeData(NS_NATIVE_WINDOW);

	if( !mPC ) 
		mPC = PpCreatePC();

	if (aQuiet) 
	{
		PpLoadDefaultPrinter(mPC);
	}
	else
	{
		PtSetParentWidget(parent);
		action = PtPrintSelection(parent, NULL, NULL, mPC, Pt_PRINTSEL_DFLT_LOOK);
		switch( action ) 
		{
			case Pt_PRINTSEL_PRINT:
			case Pt_PRINTSEL_PREVIEW:
				rv = NS_OK;
				break;
			case Pt_PRINTSEL_CANCEL:
				rv = NS_ERROR_ABORT;
				break;
		}
  	}

  	return rv;
}

//NS_IMETHODIMP nsDeviceContextSpecPh :: GetPrintContext(PpPrintContext_t *&aPrintContext) const
PpPrintContext_t *nsDeviceContextSpecPh :: GetPrintContext()
{
	return (mPC);
}

void nsDeviceContextSpecPh :: SetPrintContext(PpPrintContext_t* pc)
{
	if (mPC)
		PpPrintReleasePC(mPC);

	mPC = pc; 
}

//***********************************************************
//  Printer Enumerator
//***********************************************************
nsPrinterEnumeratorPh::nsPrinterEnumeratorPh()
{
}

nsPrinterEnumeratorPh::~nsPrinterEnumeratorPh()
{
}

NS_IMPL_ISUPPORTS1(nsPrinterEnumeratorPh, nsIPrinterEnumerator)

//----------------------------------------------------------------------------------
// Enumerate all the Printers from the global array and pass their
// names back (usually to script)
NS_IMETHODIMP
nsPrinterEnumeratorPh::EnumeratePrinters(PRUint32* aCount, PRUnichar*** aResult)
{
  	return DoEnumeratePrinters(PR_FALSE, aCount, aResult);
}

/* readonly attribute wstring defaultPrinterName; */
NS_IMETHODIMP nsPrinterEnumeratorPh::GetDefaultPrinterName(PRUnichar * *aDefaultPrinterName)
{
  NS_ENSURE_ARG_POINTER(aDefaultPrinterName);
  *aDefaultPrinterName = nsnull;
  return NS_OK;
}

/* void initPrintSettingsFromPrinter (in wstring aPrinterName, in nsIPrintSettings aPrintSettings); */
NS_IMETHODIMP nsPrinterEnumeratorPh::InitPrintSettingsFromPrinter(const PRUnichar *aPrinterName, nsIPrintSettings *aPrintSettings)
{
    return NS_OK;
}

//----------------------------------------------------------------------------------
// Display the AdvancedDocumentProperties for the selected Printer
NS_IMETHODIMP nsPrinterEnumeratorPh::DisplayPropertiesDlg(const PRUnichar *aPrinterName, nsIPrintSettings* aPrintSettings)
{
	nsresult rv = NS_ERROR_FAILURE;
	return rv;
}

static void CleanupArray(PRUnichar**& aArray, PRInt32& aCount)
{
	for (PRInt32 i = aCount - 1; i >= 0; i--) 
	{
		nsMemory::Free(aArray[i]);
	}
	nsMemory::Free(aArray);
	aArray = NULL;
	aCount = 0;
}


nsresult
nsPrinterEnumeratorPh::DoEnumeratePrinters(PRBool aDoExtended, PRUint32* aCount,
                                           PRUnichar*** aResult)
{
  	NS_ENSURE_ARG(aCount);
  	NS_ENSURE_ARG_POINTER(aResult);

	char 	**plist = NULL;
	int		pcount = 0, count = 0;

	if (!(plist = PpLoadPrinterList()))
		return NS_ERROR_FAILURE;

	for (pcount = 0; plist[pcount] != NULL; pcount++);

	PRUnichar** array = (PRUnichar**) nsMemory::Alloc(pcount * sizeof(PRUnichar*));
	if (!array)
	{
		PpFreePrinterList(plist);
		return NS_ERROR_OUT_OF_MEMORY;
	}

	while (count < pcount) 
	{
		nsString newName;
		newName.AssignWithConversion(plist[count]);
		PRUnichar *str = ToNewUnicode(newName);
		if (!str) 
		{
			CleanupArray(array, count);
			PpFreePrinterList(plist);
			return NS_ERROR_OUT_OF_MEMORY;
		}
	  	array[count++] = str;
	}

	*aCount  = count;
	*aResult = array;

	return NS_OK;
}

