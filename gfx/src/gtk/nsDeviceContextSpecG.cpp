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
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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
 
/* Store per-printer features in temp. prefs vars that the
 * print dialog can pick them up... */
#define SET_PRINTER_FEATURES_VIA_PREFS 1 
#define PRINTERFEATURES_PREF "print.tmp.printerfeatures"

#define FORCE_PR_LOG /* Allow logging in the release build */
#define PR_LOGGING 1
#include "prlog.h"

#include "nsDeviceContextSpecG.h"

#include "nsIPref.h"
#include "prenv.h" /* for PR_GetEnv */

#include "nsIDOMWindowInternal.h"
#include "nsIServiceManager.h"
#include "nsIDialogParamBlock.h"
#include "nsISupportsPrimitives.h"
#include "nsIWindowWatcher.h"

#include "nsReadableUtils.h"
#include "nsISupportsArray.h"

#include "nsPrintfCString.h"

#ifdef USE_XPRINT
#include "xprintutil.h"
#endif /* USE_XPRINT */

#ifdef USE_POSTSCRIPT
/* Fetch |postscript_module_paper_sizes| */
#include "nsPostScriptObj.h"
#endif /* USE_POSTSCRIPT */

#ifdef PR_LOGGING 
static PRLogModuleInfo *DeviceContextSpecGTKLM = PR_NewLogModule("DeviceContextSpecGTK");
#endif /* PR_LOGGING */
/* Macro to make lines shorter */
#define DO_PR_DEBUG_LOG(x) PR_LOG(DeviceContextSpecGTKLM, PR_LOG_DEBUG, x)

//----------------------------------------------------------------------------------
// The printer data is shared between the PrinterEnumerator and the nsDeviceContextSpecGTK
// The PrinterEnumerator creates the printer info
// but the nsDeviceContextSpecGTK cleans it up
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
  void      GetDefaultPrinterName(PRUnichar **aDefaultPrinterName);

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
//---------------

nsDeviceContextSpecGTK::nsDeviceContextSpecGTK()
{
  DO_PR_DEBUG_LOG(("nsDeviceContextSpecGTK::nsDeviceContextSpecGTK()\n"));
  NS_INIT_REFCNT();
}

nsDeviceContextSpecGTK::~nsDeviceContextSpecGTK()
{
  DO_PR_DEBUG_LOG(("nsDeviceContextSpecGTK::~nsDeviceContextSpecGTK()\n"));
}

/* Use both PostScript and Xprint module */
#if defined(USE_XPRINT) && defined(USE_POSTSCRIPT)
NS_IMPL_ISUPPORTS3(nsDeviceContextSpecGTK,
                   nsIDeviceContextSpec,
                   nsIDeviceContextSpecPS,
                   nsIDeviceContextSpecXp)
/* Use only PostScript module */
#elif !defined(USE_XPRINT) && defined(USE_POSTSCRIPT)
NS_IMPL_ISUPPORTS2(nsDeviceContextSpecGTK,
                   nsIDeviceContextSpec,
                   nsIDeviceContextSpecPS)
/* Use only Xprint module module */
#elif defined(USE_XPRINT) && !defined(USE_POSTSCRIPT)
NS_IMPL_ISUPPORTS2(nsDeviceContextSpecGTK,
                   nsIDeviceContextSpec,
                   nsIDeviceContextSpecXp)
/* Both Xprint and PostScript module are missing */
#elif !defined(USE_XPRINT) && !defined(USE_POSTSCRIPT)
NS_IMPL_ISUPPORTS1(nsDeviceContextSpecGTK,
                   nsIDeviceContextSpec)
#else
#error "This should not happen"
#endif

/** -------------------------------------------------------
 */
static nsresult DisplayXPDialog(nsIPrintSettings* aPS,
                                const char* aChromeURL, 
                                PRBool& aClickedOK)
{
  DO_PR_DEBUG_LOG(("nsDeviceContextSpecGTK::DisplayXPDialog()\n"));
  NS_ASSERTION(aPS, "Must have a print settings!");

  aClickedOK = PR_FALSE;
  nsresult rv = NS_ERROR_FAILURE;

  // create a nsISupportsArray of the parameters 
  // being passed to the window
  nsCOMPtr<nsISupportsArray> array;
  NS_NewISupportsArray(getter_AddRefs(array));
  if (!array) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPrintSettings> ps = aPS;
  nsCOMPtr<nsISupports> psSupports(do_QueryInterface(ps));
  NS_ASSERTION(psSupports, "PrintSettings must be a supports");
  array->AppendElement(psSupports);

  nsCOMPtr<nsIDialogParamBlock> ioParamBlock(do_CreateInstance("@mozilla.org/embedcomp/dialogparam;1"));
  if (ioParamBlock) {
    ioParamBlock->SetInt(0, 0);
    nsCOMPtr<nsISupports> blkSupps(do_QueryInterface(ioParamBlock));
    NS_ASSERTION(blkSupps, "IOBlk must be a supports");

    array->AppendElement(blkSupps);
    nsCOMPtr<nsISupports> arguments(do_QueryInterface(array));
    NS_ASSERTION(array, "array must be a supports");

    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
    if (wwatch) {
      nsCOMPtr<nsIDOMWindow> active;
      wwatch->GetActiveWindow(getter_AddRefs(active));    
      nsCOMPtr<nsIDOMWindowInternal> parent = do_QueryInterface(active);

      nsCOMPtr<nsIDOMWindow> newWindow;
      rv = wwatch->OpenWindow(parent, aChromeURL,
            "_blank", "chrome,modal,centerscreen", array,
            getter_AddRefs(newWindow));
    }
  }

  if (NS_SUCCEEDED(rv)) {
    PRInt32 buttonPressed = 0;
    ioParamBlock->GetInt(0, &buttonPressed);
    if (buttonPressed == 1) {
      aClickedOK = PR_TRUE;
    } else {
      rv = NS_ERROR_ABORT;
    }
  } else {
    rv = NS_ERROR_ABORT;
  }
  return rv;
}

/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecGTK
 *  @update   dc 2/15/98
 *  @update   syd 3/2/99
 *
 * gisburn: Please note that this function exists as 1:1 copy in other
 * toolkits including:
 * - GTK+-toolkit:
 *   file:     mozilla/gfx/src/gtk/nsDeviceContextSpecG.cpp
 *   function: NS_IMETHODIMP nsDeviceContextSpecGTK::Init(PRBool aQuiet)
 * - Xlib-toolkit: 
 *   file:     mozilla/gfx/src/xlib/nsDeviceContextSpecXlib.cpp 
 *   function: NS_IMETHODIMP nsDeviceContextSpecXlib::Init(PRBool aQuiet)
 * - Qt-toolkit:
 *   file:     mozilla/gfx/src/qt/nsDeviceContextSpecQT.cpp
 *   function: NS_IMETHODIMP nsDeviceContextSpecQT::Init(PRBool aQuiet)
 * 
 * ** Please update the other toolkits when changing this function.
 */
NS_IMETHODIMP nsDeviceContextSpecGTK::Init(nsIPrintSettings *aPS, PRBool aQuiet)
{
  DO_PR_DEBUG_LOG(("nsDeviceContextSpecGTK::Init(aPS=%p. qQuiet=%d)\n", aPS, (int)aQuiet));
  nsresult rv = NS_ERROR_FAILURE;

  mPrintSettings = aPS;

  // if there is a current selection then enable the "Selection" radio button
  if (mPrintSettings) {
    PRBool isOn;
    mPrintSettings->GetPrintOptions(nsIPrintSettings::kEnableSelectionRB, &isOn);
    nsCOMPtr<nsIPref> pPrefs = do_GetService(NS_PREF_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      (void) pPrefs->SetBoolPref("print.selection_radio_enabled", isOn);
    }
  }

  PRBool canPrint = PR_FALSE;

  rv = GlobalPrinters::GetInstance()->InitializeGlobalPrinters();
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!aQuiet) {
    rv = DisplayXPDialog(mPrintSettings, 
                         "chrome://global/content/printdialog.xul", canPrint);
  } else {
    rv = NS_OK;
    canPrint = PR_TRUE;
  }
  
  GlobalPrinters::GetInstance()->FreeGlobalPrinters();

  if (NS_SUCCEEDED(rv) && canPrint) {
    if (aPS) {
      PRBool     reversed       = PR_FALSE;
      PRBool     color          = PR_FALSE;
      PRBool     tofile         = PR_FALSE;
      PRInt16    printRange     = nsIPrintSettings::kRangeAllPages;
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

      aPS->GetPrinterName(&printer);
      aPS->GetPrintReversed(&reversed);
      aPS->GetPrintInColor(&color);
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

      if (printfile)
        strcpy(mPath,    NS_ConvertUCS2toUTF8(printfile).get());
      if (command)
        strcpy(mCommand, NS_ConvertUCS2toUTF8(command).get());  
      if (printer) 
        strcpy(mPrinter, NS_ConvertUCS2toUTF8(printer).get());        

      DO_PR_DEBUG_LOG(("margins:   %5.2f,%5.2f,%5.2f,%5.2f\n", dtop, dleft, dbottom, dright));
      DO_PR_DEBUG_LOG(("printRange %d\n",   printRange));
      DO_PR_DEBUG_LOG(("fromPage   %d\n",   fromPage));
      DO_PR_DEBUG_LOG(("toPage     %d\n",   toPage));
      DO_PR_DEBUG_LOG(("tofile     %d\n",   tofile));
      DO_PR_DEBUG_LOG(("printfile  '%s'\n", printfile? NS_ConvertUCS2toUTF8(printfile).get():"<NULL>"));
      DO_PR_DEBUG_LOG(("command    '%s'\n", command? NS_ConvertUCS2toUTF8(command).get():"<NULL>"));
      DO_PR_DEBUG_LOG(("printer    '%s'\n", printer? NS_ConvertUCS2toUTF8(printer).get():"<NULL>"));

      mTop         = dtop;
      mBottom      = dbottom;
      mLeft        = dleft;
      mRight       = dright;
      mFpf         = !reversed;
      mGrayscale   = !color;
      mOrientation = orientation;
      mToPrinter   = !tofile;
      mCopies      = copies;
    }
  }

  return rv;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::GetToPrinter(PRBool &aToPrinter)
{
  aToPrinter = mToPrinter;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::GetPrinterName ( const char **aPrinter )
{
   *aPrinter = mPrinter;
   return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::GetCopies ( int &aCopies )
{
   aCopies = mCopies;
   return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::GetFirstPageFirst(PRBool &aFpf)      
{
  aFpf = mFpf;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::GetGrayscale(PRBool &aGrayscale)      
{
  aGrayscale = mGrayscale;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::GetLandscape(PRBool &aLandscape)
{
  aLandscape = (mOrientation == NS_LANDSCAPE);
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::GetTopMargin(float &aValue)      
{
  aValue = mTop;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::GetBottomMargin(float &aValue)      
{
  aValue = mBottom;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::GetRightMargin(float &aValue)      
{
  aValue = mRight;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::GetLeftMargin(float &aValue)      
{
  aValue = mLeft;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::GetCommand(const char **aCommand)      
{
  *aCommand = mCommand;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::GetPath(const char **aPath)      
{
  *aPath = mPath;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::GetUserCancelled(PRBool &aCancel)     
{
  aCancel = mCancel;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::GetPageSizeInTwips(PRInt32 *aWidth, PRInt32 *aHeight)
{
  return mPrintSettings->GetPageSizeInTwips(aWidth, aHeight);
}

NS_IMETHODIMP nsDeviceContextSpecGTK::GetPrintMethod(PrintMethod &aMethod)
{
  return GetPrintMethod(mPrinter, aMethod);
}

/* static !! */
nsresult nsDeviceContextSpecGTK::GetPrintMethod(const char *aPrinter, PrintMethod &aMethod)
{
#if defined(USE_POSTSCRIPT) && defined(USE_XPRINT)
  /* printer names for the PostScript module alwas start with 
   * the NS_POSTSCRIPT_DRIVER_NAME string */
  if (strncmp(aPrinter, NS_POSTSCRIPT_DRIVER_NAME, 
              NS_POSTSCRIPT_DRIVER_NAME_LEN) != 0)
    aMethod = pmXprint;
  else
    aMethod = pmPostScript;
  return NS_OK;
#elif defined(USE_XPRINT)
  aMethod = pmXprint;
  return NS_OK;
#elif defined(USE_POSTSCRIPT)
  aMethod = pmPostScript;
  return NS_OK;
#else
  return NS_ERROR_UNEXPECTED;
#endif
}

NS_IMETHODIMP nsDeviceContextSpecGTK::ClosePrintManager()
{
  return NS_OK;
}

/* Get prefs for printer
 * Search order:
 * - Get prefs per printer name and module name
 * - Get prefs per printer name
 * - Get prefs per module name
 * - Get prefs
 */
static
nsresult CopyPrinterCharPref(nsIPref *pref, const char *modulename, const char *printername, const char *prefname, char **return_buf)
{
  DO_PR_DEBUG_LOG(("CopyPrinterCharPref('%s', '%s', '%s')\n", modulename, printername, prefname));

  NS_ENSURE_ARG_POINTER(return_buf);

  nsXPIDLCString name;
  nsresult rv = NS_ERROR_FAILURE;
 
  if (printername && modulename) {
    /* Get prefs per printer name and module name */
    name = nsPrintfCString(512, "print.%s.printer_%s.%s", modulename, printername, prefname);
    DO_PR_DEBUG_LOG(("trying to get '%s'\n", name.get()));
    rv = pref->CopyCharPref(name, return_buf);
  }
  
  if (NS_FAILED(rv)) { 
    if (printername) {
      /* Get prefs per printer name */
      name = nsPrintfCString(512, "print.printer_%s.%s", printername, prefname);
      DO_PR_DEBUG_LOG(("trying to get '%s'\n", name.get()));
      rv = pref->CopyCharPref(name, return_buf);
    }

    if (NS_FAILED(rv)) {
      if (modulename) {
        /* Get prefs per module name */
        name = nsPrintfCString(512, "print.%s.%s", modulename, prefname);
        DO_PR_DEBUG_LOG(("trying to get '%s'\n", name.get()));
        rv = pref->CopyCharPref(name, return_buf);
      }
      
      if (NS_FAILED(rv)) {
        /* Get prefs */
        name = nsPrintfCString(512, "print.%s", prefname);
        DO_PR_DEBUG_LOG(("trying to get '%s'\n", name.get()));
        rv = pref->CopyCharPref(name, return_buf);
      }
    }
  }

#ifdef PR_LOG  
  if (NS_SUCCEEDED(rv)) {
    DO_PR_DEBUG_LOG(("CopyPrinterCharPref returning '%s'.\n", *return_buf));
  }
  else
  {
    DO_PR_DEBUG_LOG(("CopyPrinterCharPref failure.\n"));
  }
#endif /* PR_LOG */

  return rv;
}

//  Printer Enumerator
nsPrinterEnumeratorGTK::nsPrinterEnumeratorGTK()
{
  NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS1(nsPrinterEnumeratorGTK, nsIPrinterEnumerator)

NS_IMETHODIMP nsPrinterEnumeratorGTK::EnumeratePrinters(PRUint32* aCount, PRUnichar*** aResult)
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
NS_IMETHODIMP nsPrinterEnumeratorGTK::GetDefaultPrinterName(PRUnichar **aDefaultPrinterName)
{
  DO_PR_DEBUG_LOG(("nsPrinterEnumeratorGTK::GetDefaultPrinterName()\n"));
  NS_ENSURE_ARG_POINTER(aDefaultPrinterName);

  GlobalPrinters::GetInstance()->GetDefaultPrinterName(aDefaultPrinterName);

  DO_PR_DEBUG_LOG(("GetDefaultPrinterName(): default printer='%s'.\n", NS_ConvertUCS2toUTF8(*aDefaultPrinterName).get()));
  return NS_OK;
}

/* void initPrintSettingsFromPrinter (in wstring aPrinterName, in nsIPrintSettings aPrintSettings); */
NS_IMETHODIMP nsPrinterEnumeratorGTK::InitPrintSettingsFromPrinter(const PRUnichar *aPrinterName, nsIPrintSettings *aPrintSettings)
{
  DO_PR_DEBUG_LOG(("nsPrinterEnumeratorGTK::InitPrintSettingsFromPrinter()"));
  nsresult rv;

  NS_ENSURE_ARG_POINTER(aPrinterName);
  NS_ENSURE_ARG_POINTER(aPrintSettings);

  if (!*aPrinterName) {
    return NS_ERROR_FAILURE;
  }
  
  if (aPrintSettings == nsnull) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPref> pPrefs = do_GetService(NS_PREF_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsXPIDLCString printerName;
  printerName.Assign(NS_ConvertUCS2toUTF8(aPrinterName));
  DO_PR_DEBUG_LOG(("printerName='%s'\n", printerName.get()));
  
  PrintMethod type = pmInvalid;
  rv = nsDeviceContextSpecGTK::GetPrintMethod(printerName, type);
  if (NS_FAILED(rv))
    return rv;

#ifdef USE_POSTSCRIPT
  /* "Demangle" postscript printer name */
  if (type == pmPostScript) {
    /* Strip the leading NS_POSTSCRIPT_DRIVER_NAME from |printerName|,
     * e.g. turn "PostScript/foobar" to "foobar" */
    printerName.Cut(0, NS_POSTSCRIPT_DRIVER_NAME_LEN);
  }
#endif /* USE_POSTSCRIPT */

#ifdef SET_PRINTER_FEATURES_VIA_PREFS
  /* Defaults to FALSE */
  pPrefs->SetBoolPref(nsPrintfCString(256, PRINTERFEATURES_PREF ".%s.has_special_printerfeatures", printerName.get()).get(), PR_FALSE);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

  /* These switches may be dynamic in the future... */
  PRBool doingFilename    = PR_TRUE;
  PRBool doingNumCopies   = PR_TRUE;
  PRBool doingOrientation = PR_TRUE;
  PRBool doingPaperSize   = PR_TRUE;
  PRBool doingCommand     = PR_TRUE;

  if (doingFilename) {
    nsXPIDLCString filename;
    if (NS_FAILED(CopyPrinterCharPref(pPrefs, nsnull, printerName, "filename", getter_Copies(filename)))) {
      const char *path;
    
      if (!(path = PR_GetEnv("PWD")))
        path = PR_GetEnv("HOME");
    
      if (path)
        filename = nsPrintfCString(PATH_MAX, "%s/mozilla.ps", path);
      else
        filename.Assign("mozilla.ps");  
    }
    
    DO_PR_DEBUG_LOG(("Setting default filename to '%s'\n", filename.get()));
    aPrintSettings->SetToFileName(NS_ConvertUTF8toUCS2(filename).get());
  }

#ifdef USE_XPRINT
  if (type == pmXprint) {
    DO_PR_DEBUG_LOG(("InitPrintSettingsFromPrinter() for Xprint printer\n"));

    Display   *pdpy;
    XPContext  pcontext;
    if (XpuGetPrinter(printerName, &pdpy, &pcontext) != 1)
      return NS_ERROR_GFX_PRINTER_NAME_NOT_FOUND;

    if (doingOrientation) {
      XpuOrientationList  olist;
      int                 ocount;
      XpuOrientationRec  *default_orientation;
      
      /* Get list of supported orientations */
      olist = XpuGetOrientationList(pdpy, pcontext, &ocount);
      if (olist) {
        default_orientation = &olist[0]; /* First entry is the default one */
      
        if (!strcasecmp(default_orientation->orientation, "portrait")) {
          DO_PR_DEBUG_LOG(("setting default orientation to 'portrait'\n"));
          aPrintSettings->SetOrientation(nsIPrintSettings::kPortraitOrientation);
        }
        else if (!strcasecmp(default_orientation->orientation, "landscape")) {
          DO_PR_DEBUG_LOG(("setting default orientation to 'landscape'\n"));
          aPrintSettings->SetOrientation(nsIPrintSettings::kLandscapeOrientation);
        }  
        else {
          DO_PR_DEBUG_LOG(("Unknown default orientation '%s'\n", default_orientation->orientation));
        }
   
        XpuFreeOrientationList(olist);
      }  
    }

    /* Setup Number of Copies */
    if (doingNumCopies) {
#ifdef NOT_IMPLEMENTED_YET
      aPrintSettings->SetNumCopies(PRInt32(aDevMode->dmCopies));
#endif /* NOT_IMPLEMENTED_YET */
    }
    
    if (doingPaperSize) {
      XpuMediumSourceSizeList mlist;
      int                     mcount;
      XpuMediumSourceSizeRec *default_medium;
      
      mlist = XpuGetMediumSourceSizeList(pdpy, pcontext, &mcount);
      if (mlist) {
        default_medium = &mlist[0]; /* First entry is the default one */
        double total_width  = default_medium->ma1 + default_medium->ma2,
               total_height = default_medium->ma3 + default_medium->ma4;
 
        DO_PR_DEBUG_LOG(("setting default paper size to %g/%g mm\n", total_width, total_height));
        aPrintSettings->SetPaperSizeType(nsIPrintSettings::kPaperSizeDefined);
        aPrintSettings->SetPaperSizeUnit(nsIPrintSettings::kPaperSizeMillimeters);
        aPrintSettings->SetPaperWidth(total_width);
        aPrintSettings->SetPaperHeight(total_height);

#ifdef SET_PRINTER_FEATURES_VIA_PREFS
        pPrefs->SetBoolPref(nsPrintfCString(256, PRINTERFEATURES_PREF ".%s.has_special_printerfeatures", printerName.get()).get(), PR_TRUE);

        int i;
        for( i = 0 ; i < mcount ; i++ )
        {
          XpuMediumSourceSizeRec *curr = &mlist[i];
          double total_width  = curr->ma1 + curr->ma2,
                 total_height = curr->ma3 + curr->ma4;
          pPrefs->SetCharPref(nsPrintfCString(256, PRINTERFEATURES_PREF ".%s.paper.%d.name",      printerName.get(), i).get(), curr->medium_name);
          pPrefs->SetIntPref( nsPrintfCString(256, PRINTERFEATURES_PREF ".%s.paper.%d.width_mm",  printerName.get(), i).get(), PRInt32(total_width));
          pPrefs->SetIntPref( nsPrintfCString(256, PRINTERFEATURES_PREF ".%s.paper.%d.height_mm", printerName.get(), i).get(), PRInt32(total_height));
          pPrefs->SetBoolPref(nsPrintfCString(256, PRINTERFEATURES_PREF ".%s.paper.%d.is_inch",   printerName.get(), i).get(), PR_FALSE);
        }
        pPrefs->SetIntPref(nsPrintfCString(256, PRINTERFEATURES_PREF ".%s.paper.count", printerName.get(), i).get(), (PRInt32)mcount);          
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

        XpuFreeMediumSourceSizeList(mlist);
      }  
    }
 
    XpuClosePrinterDisplay(pdpy, pcontext);
    
    return NS_OK;    
  }
  else
#endif /* USE_XPRINT */

#ifdef USE_POSTSCRIPT
  if (type == pmPostScript) {
    DO_PR_DEBUG_LOG(("InitPrintSettingsFromPrinter() for PostScript printer\n"));
      
    if (doingNumCopies) {
      /* Not implemented yet */
    }
    
    if (doingOrientation) {
      nsXPIDLCString orientation;
      if (NS_SUCCEEDED(CopyPrinterCharPref(pPrefs, "postscript", printerName, "orientation", getter_Copies(orientation)))) {
        if (!strcasecmp(orientation, "portrait")) {
          DO_PR_DEBUG_LOG(("setting default orientation to 'portrait'\n"));
          aPrintSettings->SetOrientation(nsIPrintSettings::kPortraitOrientation);
        }
        else if (!strcasecmp(orientation, "landscape")) {
          DO_PR_DEBUG_LOG(("setting default orientation to 'landscape'\n"));
          aPrintSettings->SetOrientation(nsIPrintSettings::kLandscapeOrientation);  
        }
        else {
          DO_PR_DEBUG_LOG(("Unknown default orientation '%s'\n", orientation.get()));
        }
      }
    }
    
    if (doingPaperSize) {
      /* Not implemented yet */
      nsXPIDLCString papername;
      if (NS_SUCCEEDED(CopyPrinterCharPref(pPrefs, "postscript", printerName, "paper_size", getter_Copies(papername)))) {
        int    i;
        double width  = 0.,
               height = 0.;
        
        for( i = 0 ; postscript_module_paper_sizes[i].name != nsnull ; i++ )
        {
          const PSPaperSizeRec *curr = &postscript_module_paper_sizes[i];

          if (!strcasecmp(papername, curr->name)) {          
            width  = curr->width;  
            height = curr->height;
            break;
          }
        }  

        if (width!=0.0 && height!=0.0) {
          DO_PR_DEBUG_LOG(("setting default paper size to %g/%g inch\n", width, height));
          aPrintSettings->SetPaperSizeType(nsIPrintSettings::kPaperSizeDefined);
          aPrintSettings->SetPaperSizeUnit(nsIPrintSettings::kPaperSizeInches);
          aPrintSettings->SetPaperWidth(width);
          aPrintSettings->SetPaperHeight(height);
        }
        else {
          DO_PR_DEBUG_LOG(("Unknown paper size '%s' given.\n", papername));
        }         
      }
    }

    if (doingCommand) {
      nsXPIDLCString command;
      if (NS_SUCCEEDED(CopyPrinterCharPref(pPrefs, "postscript", printerName, "print_command", getter_Copies(command)))) {
        DO_PR_DEBUG_LOG(("setting default print command to '%s'\n", command.get()));
        aPrintSettings->SetPrintCommand(NS_ConvertUTF8toUCS2(command).get());
      }
    }
  
    return NS_OK;    
  }
#endif /* USE_POSTSCRIPT */

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsPrinterEnumeratorGTK::DisplayPropertiesDlg(const PRUnichar *aPrinter, nsIPrintSettings *aPrintSettings)
{
  /* fixme: We simply ignore the |aPrinter| argument here
   * We should get the supported printer attributes from the printer and 
   * populate the print job options dialog with these data instead of using 
   * the "default set" here.
   * However, this requires changes on all platforms and is another big chunk
   * of patches ... ;-(
   */

  PRBool pressedOK;
  return DisplayXPDialog(aPrintSettings,
                         "chrome://global/content/printjoboptions.xul", 
                         pressedOK);

}

//----------------------------------------------------------------------
nsresult GlobalPrinters::InitializeGlobalPrinters ()
{
  if (PrintersAreAllocated()) {
    return NS_OK;
  }

  mGlobalNumPrinters = 0;
  mGlobalPrinterList = new nsStringArray();
  if (!mGlobalPrinterList) 
    return NS_ERROR_OUT_OF_MEMORY;
      
#ifdef USE_XPRINT   
  XPPrinterList plist = XpuGetPrinterList(nsnull, &mGlobalNumPrinters);
  
  if (plist && (mGlobalNumPrinters > 0))
  {  
    int i;
    for(  i = 0 ; i < mGlobalNumPrinters ; i++ )
    {
      mGlobalPrinterList->AppendString(nsString(NS_ConvertASCIItoUCS2(plist[i].name)));
    }
    
    XpuFreePrinterList(plist);
  }  
#endif /* USE_XPRINT */

#ifdef USE_POSTSCRIPT
  /* Get the list of PostScript-module printers */
  char   *printerList           = nsnull;
  PRBool  added_default_printer = PR_FALSE; /* Did we already add the default printer ? */
  
  /* The env var MOZILLA_POSTSCRIPT_PRINTER_LIST can "override" the prefs */
  printerList = PR_GetEnv("MOZILLA_POSTSCRIPT_PRINTER_LIST");
  
  if (!printerList) {
    nsresult rv;
    nsCOMPtr<nsIPref> pPrefs = do_GetService(NS_PREF_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      (void) pPrefs->CopyCharPref("print.printer_list", &printerList);
    }
  }  

  if (printerList) {
    char       *tok_lasts;
    const char *name;
    
    /* PL_strtok_r() will modify the string - copy it! */
    printerList = strdup(printerList);
    if (!printerList)
      return NS_ERROR_OUT_OF_MEMORY;    
    
    for( name = PL_strtok_r(printerList, " ", &tok_lasts) ; 
         name != nsnull ; 
         name = PL_strtok_r(nsnull, " ", &tok_lasts) )
    {
      /* Is this the "default" printer ? */
      if (!strcmp(name, "default"))
        added_default_printer = PR_TRUE;

      mGlobalPrinterList->AppendString(
        nsString(NS_ConvertASCIItoUCS2(NS_POSTSCRIPT_DRIVER_NAME)) + 
        nsString(NS_ConvertASCIItoUCS2(name)));
      mGlobalNumPrinters++;      
    }
    
    free(printerList);
  }

  /* Add an entry for the default printer (see nsPostScriptObj.cpp) if we
   * did not add it already... */
  if (!added_default_printer)
  {
    mGlobalPrinterList->AppendString(
      nsString(NS_ConvertASCIItoUCS2(NS_POSTSCRIPT_DRIVER_NAME "default")));
    mGlobalNumPrinters++;
  }  
#endif /* USE_POSTSCRIPT */  
      
  if (mGlobalNumPrinters == 0)
    return NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAILABLE; 

  return NS_OK;
}

//----------------------------------------------------------------------
void GlobalPrinters::FreeGlobalPrinters()
{
  if (mGlobalPrinterList) {
    delete mGlobalPrinterList;
    mGlobalPrinterList = nsnull;
    mGlobalNumPrinters = 0;
  }  
}

void 
GlobalPrinters::GetDefaultPrinterName(PRUnichar **aDefaultPrinterName)
{
  *aDefaultPrinterName = nsnull;
  
  PRBool allocate = (GlobalPrinters::GetInstance()->PrintersAreAllocated() == PR_FALSE);
  
  if (allocate) {
    nsresult rv = GlobalPrinters::GetInstance()->InitializeGlobalPrinters();
    if (NS_FAILED(rv)) {
      return;
    }
  }
  NS_ASSERTION(GlobalPrinters::GetInstance()->PrintersAreAllocated(), "no GlobalPrinters");

  if (GlobalPrinters::GetInstance()->GetNumPrinters() == 0)
    return;
  
  *aDefaultPrinterName = ToNewUnicode(*GlobalPrinters::GetInstance()->GetStringAt(0));

  if (allocate) {  
    GlobalPrinters::GetInstance()->FreeGlobalPrinters();
  }  
}

