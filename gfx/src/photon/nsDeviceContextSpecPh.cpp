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

#include "nsGfxCIID.h"
#include "nsIPrintOptions.h"
#include "nsIDOMWindow.h"
#include "nsIDialogParamBlock.h"
#include "nsISupportsPrimitives.h"
#include "nsIWindowWatcher.h"
#include "nsIDOMWindowInternal.h"
#include "nsVoidArray.h"
#include "nsSupportsArray.h"

#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsReadableUtils.h"
#include "nsIPref.h"

nsDeviceContextSpecPh :: nsDeviceContextSpecPh()
{
	NS_INIT_ISUPPORTS();
	mPC = PpCreatePC();
}

nsDeviceContextSpecPh :: ~nsDeviceContextSpecPh()
{
	PpPrintReleasePC(mPC);
}

NS_IMPL_ISUPPORTS1(nsDeviceContextSpecPh, nsIDeviceContextSpec)


#define Pp_COLOR_CMYK			4
#define Pp_COLOR_BW				1

NS_IMETHODIMP nsDeviceContextSpecPh :: Init(nsIWidget* aWidget,
                                             nsIPrintSettings* aPS,
                                             PRBool aQuiet)
{
	nsresult rv = NS_OK;
	PRUnichar *printer        = nsnull;
	PRBool silent;

	aPS->GetPrinterName(&printer);
	aPS->GetPrintSilent( &silent );

	if( printer ) {
		int res = 111;
		const char *pname = NS_ConvertUCS2toUTF8(printer).get();
		if( !strcmp( pname, "<Preview>" ) ) {
			char preview = 1;
			PpSetPC( mPC, Pp_PC_DO_PREVIEW, &preview, 0 );
			}
		else res = PpLoadPrinter( mPC, pname );
		}
	else PpLoadDefaultPrinter( mPC );

	if( !silent ) 
	{
		PRBool tofile = PR_FALSE;
		PRUnichar *printfile = nsnull;
		PRInt32 copies = 1;
		PRBool color = PR_FALSE;
		PRInt32 orientation = nsIPrintSettings::kPortraitOrientation;
		PRBool reversed = PR_FALSE;

		aPS->GetPrintToFile(&tofile);
		if( tofile == PR_TRUE ) {
			aPS->GetToFileName(&printfile);
			if( printfile ) PpSetPC( mPC, Pp_PC_FILENAME, NS_ConvertUCS2toUTF8(printfile).get(), 0 );
			}

		aPS->GetNumCopies(&copies);
		char pcopies = ( char ) copies;
		PpSetPC( mPC, Pp_PC_COPIES, (void *) &pcopies, 0 );

		aPS->GetPrintInColor(&color);
		char ink = color == PR_TRUE ? Pp_COLOR_CMYK : Pp_COLOR_BW;
		PpSetPC( mPC, Pp_PC_INKTYPE, &ink, 0 );

		aPS->GetOrientation(&orientation);
		char paper_orientation = orientation == nsIPrintSettings::kPortraitOrientation ? 0 : 1;
		PpSetPC( mPC, Pp_PC_ORIENTATION, &paper_orientation, 0 );

		aPS->GetPrintReversed(&reversed);
		char rev = reversed == PR_TRUE ? 1 : 0;
		PpSetPC( mPC, Pp_PC_REVERSED, &rev, 0 );


		PRInt16 unit;
		double width, height;
		aPS->GetPaperSizeUnit(&unit);
		aPS->GetPaperWidth(&width);
		aPS->GetPaperHeight(&height);

		PhDim_t *pdim, dim;
		PpGetPC( mPC, Pp_PC_PAPER_SIZE, &pdim );
		dim = *pdim;
		if( unit == nsIPrintSettings::kPaperSizeInches ) {
		  dim.w  = width * 1000;
		  dim.h = height * 1000;
			}
		else if( unit == nsIPrintSettings::kPaperSizeMillimeters ) {
			dim.w = short(NS_TWIPS_TO_INCHES(NS_MILLIMETERS_TO_TWIPS(float(width*1000))));
			dim.h = short(NS_TWIPS_TO_INCHES(NS_MILLIMETERS_TO_TWIPS(float(height*1000))));
			}

		PpSetPC( mPC, Pp_PC_PAPER_SIZE, &dim, 0 );
  }
	else { /* silent is set - used when the call is comming from the embedded version */
		PRInt32 p;
		aPS->GetEndPageRange( &p );
		PpPrintReleasePC(mPC);
		mPC = ( PpPrintContext_t *) p;
		}

 	return rv;
}

//NS_IMETHODIMP nsDeviceContextSpecPh :: GetPrintContext(PpPrintContext_t *&aPrintContext) const
PpPrintContext_t *nsDeviceContextSpecPh :: GetPrintContext()
{
	return (mPC);
}

//***********************************************************
//  Printer Enumerator
//***********************************************************
nsPrinterEnumeratorPh::nsPrinterEnumeratorPh()
{
		NS_INIT_ISUPPORTS();
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
	char *printer;

  NS_ENSURE_ARG_POINTER(aDefaultPrinterName);

	PpPrintContext_t *pc = PpCreatePC();
	if( pc ) {
		PpLoadDefaultPrinter( pc );
		PpGetPC( pc, Pp_PC_NAME, &printer );

  	*aDefaultPrinterName = ToNewUnicode( NS_LITERAL_STRING( printer ) );
		PpReleasePC( pc );
		}
	else *aDefaultPrinterName = nsnull;
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

	/* allow a fake <Preview> printer to do the photon native preview */
	pcount++;

	PRUnichar** array = (PRUnichar**) nsMemory::Alloc(pcount * sizeof(PRUnichar*));
	if (!array)
	{
		PpFreePrinterList(plist);
		return NS_ERROR_OUT_OF_MEMORY;
	}

	while (count < pcount) 
	{
		nsString newName;

		if( count < pcount-1 )
			newName.AssignWithConversion(plist[count]);
		else newName.AssignWithConversion( "<Preview>" );

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
