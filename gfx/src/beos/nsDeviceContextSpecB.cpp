/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDeviceContextSpecB.h"
 
#include "nsIPref.h" 
#include "prenv.h" /* for PR_GetEnv */ 
#include "nsIServiceManager.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"

//----------------------------------------------------------------------------------
// The printer data is shared between the PrinterEnumerator and the nsDeviceContextSpecG
// The PrinterEnumerator creates the printer info
// but the nsDeviceContextSpecG cleans it up
// If it gets created (via the Page Setup Dialog) but the user never prints anything
// then it will never be delete, so this class takes care of that.
class GlobalPrinters {
public:
  static GlobalPrinters* GetInstance()   { return &mGlobalPrinters; }
  ~GlobalPrinters()                      { FreeGlobalPrinters(); }

  void      FreeGlobalPrinters();
  nsresult  InitializeGlobalPrinters();

  PRBool    PrintersAreAllocated()       { return mGlobalPrinterList != nsnull; }
  PRInt32   GetNumPrinters()             { return mGlobalNumPrinters; }
  nsString* GetStringAt(PRInt32 aInx)    { return mGlobalPrinterList->StringAt(aInx); }

protected:
  GlobalPrinters() {}

  static GlobalPrinters mGlobalPrinters;
  static nsStringArray* mGlobalPrinterList;
  static int            mGlobalNumPrinters;

};
//---------------
// static members
GlobalPrinters GlobalPrinters::mGlobalPrinters;
nsStringArray* GlobalPrinters::mGlobalPrinterList = nsnull;
int            GlobalPrinters::mGlobalNumPrinters = 0;

/** -------------------------------------------------------
 *  Construct the nsDeviceContextSpecBeOS
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecBeOS :: nsDeviceContextSpecBeOS()
{
}

/** -------------------------------------------------------
 *  Destroy the nsDeviceContextSpecBeOS
 *  @update   dc 2/15/98
 */
nsDeviceContextSpecBeOS :: ~nsDeviceContextSpecBeOS()
{
} 
 
static NS_DEFINE_IID(kIDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID); 
#ifdef USE_POSTSCRIPT
static NS_DEFINE_IID(kIDeviceContextSpecPSIID, NS_IDEVICE_CONTEXT_SPEC_PS_IID); 
#endif /* USE_POSTSCRIPT */
 
#if 0 
NS_IMPL_ISUPPORTS1(nsDeviceContextSpecBeOS, nsIDeviceContextSpec)
#endif 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: QueryInterface(REFNSIID aIID, void** aInstancePtr) 
{ 
  if (nsnull == aInstancePtr) 
    return NS_ERROR_NULL_POINTER; 

  if (aIID.Equals(kIDeviceContextSpecIID)) 
  { 
    nsIDeviceContextSpec* tmp = this; 
    *aInstancePtr = (void*) tmp; 
    NS_ADDREF_THIS(); 
    return NS_OK; 
  } 
 
#ifdef USE_POSTSCRIPT
  if (aIID.Equals(kIDeviceContextSpecPSIID)) 
  { 
    nsIDeviceContextSpecPS* tmp = this; 
    *aInstancePtr = (void*) tmp; 
    NS_ADDREF_THIS(); 
    return NS_OK; 
  }
#endif /* USE_POSTSCRIPT */

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

  if (aIID.Equals(kISupportsIID)) 
  { 
    nsIDeviceContextSpec* tmp = this; 
    nsISupports* tmp2 = tmp; 
    *aInstancePtr = (void*) tmp2; 
    NS_ADDREF_THIS(); 
    return NS_OK; 
  } 
 
  return NS_NOINTERFACE; 
} 
 
NS_IMPL_ADDREF(nsDeviceContextSpecBeOS)
NS_IMPL_RELEASE(nsDeviceContextSpecBeOS)

/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecBeOS
 *  @update   dc 2/15/98
 *  @update   syd 3/2/99
 */
NS_IMETHODIMP nsDeviceContextSpecBeOS::Init(nsIPrintSettings* aPS)
{
  nsresult rv = NS_ERROR_FAILURE;
  NS_ASSERTION(nsnull != aPS, "No print settings.");

  mPrintSettings = aPS;
  
  // if there is a current selection then enable the "Selection" radio button
  if (aPS != nsnull) {
    PRBool isOn;
    aPS->GetPrintOptions(nsIPrintSettings::kEnableSelectionRB, &isOn);
    nsCOMPtr<nsIPref> pPrefs = do_GetService(NS_PREF_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      (void) pPrefs->SetBoolPref("print.selection_radio_enabled", isOn);
    }
  }

  char      *path;
  PRBool     reversed       = PR_FALSE;
  PRBool     color          = PR_FALSE;
  PRBool     tofile         = PR_FALSE;
  PRInt16    printRange     = nsIPrintSettings::kRangeAllPages;
  PRInt32    paper_size     = NS_LETTER_SIZE;
  PRInt32    orientation    = NS_PORTRAIT;
  PRInt32    fromPage       = 1;
  PRInt32    toPage         = 1;
  PRUnichar *command        = nsnull;
  PRInt32    copies         = 1;
  PRUnichar *printer        = nsnull;
  PRUnichar *printfile      = nsnull;
  double     dleft          = 0.5;
  double     dright         = 0.5;
  double     dtop           = 0.5;
  double     dbottom        = 0.5; 

  rv = GlobalPrinters::GetInstance()->InitializeGlobalPrinters();
  if (NS_FAILED(rv)) {
    return rv;
  }

  
  GlobalPrinters::GetInstance()->FreeGlobalPrinters();

  if (aPS != nsnull) {
    aPS->GetPrinterName(&printer);
    aPS->GetPrintReversed(&reversed);
    aPS->GetPrintInColor(&color);
    aPS->GetPaperSize(&paper_size);
    aPS->GetOrientation(&orientation);
    aPS->GetPrintCommand(&command);
    aPS->GetPrintRange(&printRange);
    aPS->GetToFileName(&printfile);
    aPS->GetPrintToFile(&tofile);
    aPS->GetStartPageRange(&fromPage);
    aPS->GetEndPageRange(&toPage);
    aPS->GetNumCopies(&copies);
    aPS->GetMarginTop(&dtop);
    aPS->GetMarginLeft(&dleft);
    aPS->GetMarginBottom(&dbottom);
    aPS->GetMarginRight(&dright);

    if (command != nsnull && printfile != nsnull) {
      // ToDo: Use LocalEncoding instead of UTF-8 (see bug 73446)
      strcpy(mPrData.command, NS_ConvertUCS2toUTF8(command).get());  
      strcpy(mPrData.path,    NS_ConvertUCS2toUTF8(printfile).get());
    }
    if (printer != nsnull) 
      strcpy(mPrData.printer, NS_ConvertUCS2toUTF8(printer).get());        
#ifdef DEBUG_rods
    printf("margins:       %5.2f,%5.2f,%5.2f,%5.2f\n", dtop, dleft, dbottom, dright);
    printf("printRange     %d\n", printRange);
    printf("fromPage       %d\n", fromPage);
    printf("toPage         %d\n", toPage);
#endif /* DEBUG_rods */
  } else {
#ifdef VMS
    // Note to whoever puts the "lpr" into the prefs file. Please contact me
    // as I need to make the default be "print" instead of "lpr" for OpenVMS.
    strcpy(mPrData.command, "print");
#else
    strcpy(mPrData.command, "lpr ${MOZ_PRINTER_NAME:+'-P'}${MOZ_PRINTER_NAME}");
#endif /* VMS */
  }

  mPrData.top       = dtop;
  mPrData.bottom    = dbottom;
  mPrData.left      = dleft;
  mPrData.right     = dright;
  mPrData.fpf       = !reversed;
  mPrData.grayscale = !color;
  mPrData.size      = paper_size;
  mPrData.orientation = orientation;
  mPrData.toPrinter = !tofile;
  mPrData.copies = copies;

  // PWD, HOME, or fail 
  
  if (!printfile) {
    if ( ( path = PR_GetEnv( "PWD" ) ) == (char *) nsnull ) 
      if ( ( path = PR_GetEnv( "HOME" ) ) == (char *) nsnull )
        strcpy(mPrData.path, "mozilla.ps");
        
    if ( path != (char *) nsnull )
      sprintf(mPrData.path, "%s/mozilla.ps", path);
    else
      return NS_ERROR_FAILURE;
  }
  
#ifdef NOT_IMPLEMENTED_YET
  if (mGlobalNumPrinters) {
     for(int i = 0; (i < mGlobalNumPrinters) && !mQueue; i++) {
        if (!(mGlobalPrinterList->StringAt(i)->EqualsIgnoreCase(mPrData.printer)))
           mQueue = PrnDlg.SetPrinterQueue(i);
     }
  }
#endif /* NOT_IMPLEMENTED_YET */
  
  if (command != nsnull) {
    nsMemory::Free(command);
  }
  if (printfile != nsnull) {
    nsMemory::Free(printfile);
  }

  return rv;
}
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetToPrinter( PRBool &aToPrinter )     
{ 
  aToPrinter = mPrData.toPrinter;
        return NS_OK;
} 

NS_IMETHODIMP nsDeviceContextSpecBeOS::GetPrinterName ( const char **aPrinter )
{
   *aPrinter = &mPrData.printer[0];
   return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecBeOS::GetCopies ( int &aCopies )
{
   aCopies = mPrData.copies;
   return NS_OK;
}
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetFirstPageFirst ( PRBool &aFpf )      
{ 
  aFpf = mPrData.fpf; 
  return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetGrayscale ( PRBool &aGrayscale )      
{ 
  aGrayscale = mPrData.grayscale; 
  return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetSize ( int &aSize )      
{ 
  aSize = mPrData.size; 
  return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetPageDimensions ( float &aWidth, float &aHeight )      
{ 
    if ( mPrData.size == NS_LETTER_SIZE ) { 
        aWidth = 8.5; 
        aHeight = 11.0; 
    } else if ( mPrData.size == NS_LEGAL_SIZE ) { 
        aWidth = 8.5; 
        aHeight = 14.0; 
    } else if ( mPrData.size == NS_EXECUTIVE_SIZE ) { 
        aWidth = 7.5; 
        aHeight = 10.0; 
    } else if ( mPrData.size == NS_A4_SIZE ) { 
        // 210mm X 297mm == 8.27in X 11.69in 
        aWidth = 8.27; 
        aHeight = 11.69; 
    } else if ( mPrData.size == NS_A3_SIZE ) {
        // 297mm X 420mm == 11.69in X 16.53in
        aWidth = 11.69;
        aHeight = 16.53;
    } 

    if (mPrData.orientation == NS_LANDSCAPE) {
      float temp;
      temp = aWidth;
      aWidth = aHeight;
      aHeight = temp;
    }

    return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetLandscape ( PRBool &landscape )      
{ 
  landscape = (mPrData.orientation == NS_LANDSCAPE);
  return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetTopMargin ( float &value )      
{ 
  value = mPrData.top; 
  return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetBottomMargin ( float &value )      
{ 
  value = mPrData.bottom; 
  return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetRightMargin ( float &value )      
{ 
  value = mPrData.right; 
  return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetLeftMargin ( float &value )      
{ 
  value = mPrData.left; 
  return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS::GetCommand ( const char **aCommand )      
{ 
  *aCommand = &mPrData.command[0]; 
  return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS::GetPath ( const char **aPath )      
{ 
  *aPath = &mPrData.path[0]; 
  return NS_OK; 
} 
 
NS_IMETHODIMP nsDeviceContextSpecBeOS :: GetUserCancelled( PRBool &aCancel )     
{ 
  aCancel = mPrData.cancel; 
  return NS_OK;
}

/** -------------------------------------------------------
 * Closes the printmanager if it is open.
 *  @update   dc 2/15/98
 */
NS_IMETHODIMP nsDeviceContextSpecBeOS :: ClosePrintManager()
{
  return NS_OK;
}  

NS_IMETHODIMP nsDeviceContextSpecBeOS::GetPageSizeInTwips(PRInt32 *aWidth, PRInt32 *aHeight)
{
  return mPrintSettings->GetPageSizeInTwips(aWidth, aHeight);
}

//  Printer Enumerator
nsPrinterEnumeratorBeOS::nsPrinterEnumeratorBeOS()
{
}

NS_IMPL_ISUPPORTS1(nsPrinterEnumeratorBeOS, nsIPrinterEnumerator)

NS_IMETHODIMP nsPrinterEnumeratorBeOS::EnumeratePrinters(PRUint32* aCount, PRUnichar*** aResult)
{
  NS_ENSURE_ARG(aCount);
  NS_ENSURE_ARG_POINTER(aResult);

  if (aCount) 
    *aCount = 0;
  else 
    return NS_ERROR_NULL_POINTER;
  
  if (aResult) 
    *aResult = nsnull;
  else 
    return NS_ERROR_NULL_POINTER;
  
  nsresult rv = GlobalPrinters::GetInstance()->InitializeGlobalPrinters();
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRInt32 numPrinters = GlobalPrinters::GetInstance()->GetNumPrinters();

  PRUnichar** array = (PRUnichar**) nsMemory::Alloc(numPrinters * sizeof(PRUnichar*));
  if (!array && numPrinters > 0) {
    GlobalPrinters::GetInstance()->FreeGlobalPrinters();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  int count = 0;
  while( count < numPrinters )
  {

    PRUnichar *str = ToNewUnicode(*GlobalPrinters::GetInstance()->GetStringAt(count));

    if (!str) {
      for (int i = count - 1; i >= 0; i--) 
        nsMemory::Free(array[i]);
      
      nsMemory::Free(array);

      GlobalPrinters::GetInstance()->FreeGlobalPrinters();
      return NS_ERROR_OUT_OF_MEMORY;
    }
    array[count++] = str;
    
  }
  *aCount = count;
  *aResult = array;
  GlobalPrinters::GetInstance()->FreeGlobalPrinters();

  return NS_OK;
}

/* readonly attribute wstring defaultPrinterName; */
NS_IMETHODIMP nsPrinterEnumeratorBeOS::GetDefaultPrinterName(PRUnichar * *aDefaultPrinterName)
{
  NS_ENSURE_ARG_POINTER(aDefaultPrinterName);
  *aDefaultPrinterName = nsnull;
  return NS_OK;
}

/* void initPrintSettingsFromPrinter (in wstring aPrinterName, in nsIPrintSettings aPrintSettings); */
NS_IMETHODIMP nsPrinterEnumeratorBeOS::InitPrintSettingsFromPrinter(const PRUnichar *aPrinterName, nsIPrintSettings *aPrintSettings)
{
    return NS_OK;
}

NS_IMETHODIMP nsPrinterEnumeratorBeOS::DisplayPropertiesDlg(const PRUnichar *aPrinter, nsIPrintSettings *aPrintSettings)
{
  return NS_OK;
}

//----------------------------------------------------------------------
nsresult GlobalPrinters::InitializeGlobalPrinters ()
{
  if (PrintersAreAllocated())
    return NS_OK;

  mGlobalNumPrinters = 0;

#ifdef USE_POSTSCRIPT
  mGlobalPrinterList = new nsStringArray();
  if (!mGlobalPrinterList) 
    return NS_ERROR_OUT_OF_MEMORY;

  /* add an entry for the default printer (see nsPostScriptObj.cpp) */
  mGlobalPrinterList->AppendString(
    nsString(NS_ConvertASCIItoUCS2(NS_POSTSCRIPT_DRIVER_NAME "default")));
  mGlobalNumPrinters++;

  /* get the list of printers */
  char *printerList = nsnull;
  
  /* the env var MOZILLA_PRINTER_LIST can "override" the prefs */
  printerList = PR_GetEnv("MOZILLA_PRINTER_LIST");
  
  if (!printerList) {
    nsresult rv;
    nsCOMPtr<nsIPref> pPrefs = do_GetService(NS_PREF_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      (void) pPrefs->CopyCharPref("print.printer_list", &printerList);
    }
  }  

  if (printerList) {
    char *tok_lasts;
    char *name;
    
    /* PL_strtok_r() will modify the string - copy it! */
    printerList = strdup(printerList);
    if (!printerList)
      return NS_ERROR_OUT_OF_MEMORY;    

    for( name = PL_strtok_r(printerList, " ", &tok_lasts) ; 
         name != nsnull ; 
         name = PL_strtok_r(nsnull, " ", &tok_lasts) )
    {
      mGlobalPrinterList->AppendString(
        nsString(NS_ConvertASCIItoUCS2(NS_POSTSCRIPT_DRIVER_NAME)) + 
        nsString(NS_ConvertASCIItoUCS2(name)));
      mGlobalNumPrinters++;      
    }

    free(printerList);
  }
#endif /* USE_POSTSCRIPT */
      
  if (mGlobalNumPrinters == 0)
    return NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAILABLE; 

  return NS_OK;
}

//----------------------------------------------------------------------
void GlobalPrinters::FreeGlobalPrinters()
{
  delete mGlobalPrinterList;
  mGlobalPrinterList = nsnull;
  mGlobalNumPrinters = 0;
}




