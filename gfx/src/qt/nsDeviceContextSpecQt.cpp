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
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
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

#include "nsDeviceContextSpecQt.h"

#include "nsIPref.h"
#include "prenv.h" /* for PR_GetEnv */

#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsIServiceManager.h"
#include "nsCRT.h"

#ifdef USE_XPRINT
#include "xprintutil.h"
#endif /* USE_XPRINT */

#ifdef USE_POSTSCRIPT
/* Fetch |postscript_module_paper_sizes| */
#undef USE_POSTSCRIPT
#warning "fixme: postscript disabled"
//#include "nsPaperPS.h"
#endif /* USE_POSTSCRIPT */

/* Ensure that the result is always equal to either PR_TRUE or PR_FALSE */
#define MAKE_PR_BOOL(val) ((val)?(PR_TRUE):(PR_FALSE))

#ifdef PR_LOGGING
static PRLogModuleInfo *DeviceContextSpecQtLM = PR_NewLogModule("DeviceContextSpecQt");
#endif /* PR_LOGGING */
/* Macro to make lines shorter */
#define DO_PR_DEBUG_LOG(x) PR_LOG(DeviceContextSpecQtLM, PR_LOG_DEBUG, x)

//----------------------------------------------------------------------------------
// The printer data is shared between the PrinterEnumerator and the nsDeviceContextSpecQt
// The PrinterEnumerator creates the printer info
// but the nsDeviceContextSpecQt cleans it up
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

#ifdef SET_PRINTER_FEATURES_VIA_PREFS
/* "Prototype" for the new nsPrinterFeatures service */
class nsPrinterFeatures {
public:
  nsPrinterFeatures( const char *printername );
  ~nsPrinterFeatures() {};

  /* Does this device allow to set/change the paper size ? */
  void SetCanChangePaperSize( PRBool aCanSetPaperSize );
  /* Set number of paper size records and the records itself */
  void SetNumPaperSizeRecords( PRInt32 aCount );
  void SetPaperRecord( PRInt32 aIndex, const char *aName, PRInt32 aWidthMM, PRInt32 aHeightMM, PRBool aIsInch );

  /* Does this device allow to set/change the content orientation ? */
  void SetCanChangeOrientation( PRBool aCanSetOrientation );
  /* Set number of orientation records and the records itself */
  void SetNumOrientationRecords( PRInt32 aCount );
  void SetOrientationRecord( PRInt32 aIndex, const char *aName );

  /* Does this device allow to set/change the spooler command ? */
  void SetCanChangeSpoolerCommand( PRBool aCanSetSpoolerCommand );

  /* Does this device allow to set/change number of copies for an document ? */
  void SetCanChangeNumCopies( PRBool aCanSetNumCopies );

  /* Does this device allow multiple devicecontext instances to be used in
   * parallel (e.g. print while the device is already in use by print-preview
   * or printing while another print job is in progress) ? */
  void SetMultipleConcurrentDeviceContextsSupported( PRBool aCanUseMultipleInstances );

private:
  /* private helper methods */
  void SetBoolValue( const char *tagname, PRBool value );
  void SetIntValue(  const char *tagname, PRInt32 value );
  void SetCharValue(  const char *tagname, const char *value );

  nsXPIDLCString    mPrinterName;
  nsCOMPtr<nsIPref> mPrefs;
};

void nsPrinterFeatures::SetBoolValue( const char *tagname, PRBool value )
{
  mPrefs->SetBoolPref(nsPrintfCString(256, PRINTERFEATURES_PREF ".%s.%s", mPrinterName.get(), tagname).get(), value);
}

void nsPrinterFeatures::SetIntValue(  const char *tagname, PRInt32 value )
{
  mPrefs->SetIntPref(nsPrintfCString(256, PRINTERFEATURES_PREF ".%s.%s", mPrinterName.get(), tagname).get(), value);
}

void nsPrinterFeatures::SetCharValue(  const char *tagname, const char *value )
{
  mPrefs->SetCharPref(nsPrintfCString(256, PRINTERFEATURES_PREF ".%s.%s", mPrinterName.get(), tagname).get(), value);
}

nsPrinterFeatures::nsPrinterFeatures( const char *printername )
{
  DO_PR_DEBUG_LOG(("nsPrinterFeatures::nsPrinterFeatures('%s')\n", printername));
  mPrinterName.Assign(printername);
  mPrefs = do_GetService(NS_PREF_CONTRACTID);

  SetBoolValue("has_special_printerfeatures", PR_TRUE);
}

void nsPrinterFeatures::SetCanChangePaperSize( PRBool aCanSetPaperSize )
{
  SetBoolValue("can_change_paper_size", aCanSetPaperSize);
}

/* Set number of paper size records and the records itself */
void nsPrinterFeatures::SetNumPaperSizeRecords( PRInt32 aCount )
{
  SetIntValue("paper.count", aCount);
}

void nsPrinterFeatures::SetPaperRecord(PRInt32 aIndex, const char *aPaperName, PRInt32 aWidthMM, PRInt32 aHeightMM, PRBool aIsInch)
{
  SetCharValue(nsPrintfCString(256, "paper.%d.name",      aIndex).get(), aPaperName);
  SetIntValue( nsPrintfCString(256, "paper.%d.width_mm",  aIndex).get(), aWidthMM);
  SetIntValue( nsPrintfCString(256, "paper.%d.height_mm", aIndex).get(), aHeightMM);
  SetBoolValue(nsPrintfCString(256, "paper.%d.is_inch",   aIndex).get(), aIsInch);
}

void nsPrinterFeatures::SetCanChangeOrientation( PRBool aCanSetOrientation )
{
  SetBoolValue("can_change_orientation", aCanSetOrientation);
}

void nsPrinterFeatures::SetNumOrientationRecords( PRInt32 aCount )
{
  SetIntValue("orientation.count", aCount);
}

void nsPrinterFeatures::SetOrientationRecord( PRInt32 aIndex, const char *aOrientationName )
{
  SetCharValue(nsPrintfCString(256, "orientation.%d.name", aIndex).get(), aOrientationName);
}

void nsPrinterFeatures::SetCanChangeSpoolerCommand( PRBool aCanSetSpoolerCommand )
{
  SetBoolValue("can_change_spoolercommand", aCanSetSpoolerCommand);
}

void nsPrinterFeatures::SetCanChangeNumCopies( PRBool aCanSetNumCopies )
{
  SetBoolValue("can_change_num_copies", aCanSetNumCopies);
}

void nsPrinterFeatures::SetMultipleConcurrentDeviceContextsSupported( PRBool aCanUseMultipleInstances )
{
  SetBoolValue("can_use_multiple_devicecontexts_concurrently", aCanUseMultipleInstances);
}

#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

//---------------
// static members
GlobalPrinters GlobalPrinters::mGlobalPrinters;
nsStringArray* GlobalPrinters::mGlobalPrinterList = nsnull;
int            GlobalPrinters::mGlobalNumPrinters = 0;
//---------------

nsDeviceContextSpecQt::nsDeviceContextSpecQt()
{
  DO_PR_DEBUG_LOG(("nsDeviceContextSpecQt::nsDeviceContextSpecQt()\n"));
}

nsDeviceContextSpecQt::~nsDeviceContextSpecQt()
{
  DO_PR_DEBUG_LOG(("nsDeviceContextSpecQt::~nsDeviceContextSpecQt()\n"));
}

/* Use both PostScript and Xprint module */
#if defined(USE_XPRINT) && defined(USE_POSTSCRIPT)
NS_IMPL_ISUPPORTS3(nsDeviceContextSpecQt,
                   nsIDeviceContextSpec,
                   nsIDeviceContextSpecPS,
                   nsIDeviceContextSpecXp)
/* Use only PostScript module */
#elif !defined(USE_XPRINT) && defined(USE_POSTSCRIPT)
NS_IMPL_ISUPPORTS2(nsDeviceContextSpecQt,
                   nsIDeviceContextSpec,
                   nsIDeviceContextSpecPS)
/* Use only Xprint module module */
#elif defined(USE_XPRINT) && !defined(USE_POSTSCRIPT)
NS_IMPL_ISUPPORTS2(nsDeviceContextSpecQt,
                   nsIDeviceContextSpec,
                   nsIDeviceContextSpecXp)
/* Both Xprint and PostScript module are missing */
#elif !defined(USE_XPRINT) && !defined(USE_POSTSCRIPT)
NS_IMPL_ISUPPORTS1(nsDeviceContextSpecQt,
                   nsIDeviceContextSpec)
#else
#error "This should not happen"
#endif

/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecQt
 *  @update   dc 2/15/98
 *  @update   syd 3/2/99
 *
 * gisburn: Please note that this function exists as 1:1 copy in other
 * toolkits including:
 * - GTK+-toolkit:
 *   file:     mozilla/gfx/src/gtk/nsDeviceContextSpecG.cpp
 *   function: NS_IMETHODIMP nsDeviceContextSpecGTK::Init()
 * - Xlib-toolkit:
 *   file:     mozilla/gfx/src/xlib/nsDeviceContextSpecQt.cpp
 *   function: NS_IMETHODIMP nsDeviceContextSpecQt::Init()
 * - Qt-toolkit:
 *   file:     mozilla/gfx/src/qt/nsDeviceContextSpecQt.cpp
 *   function: NS_IMETHODIMP nsDeviceContextSpecQt::Init()
 *
 * ** Please update the other toolkits when changing this function.
 */
NS_IMETHODIMP nsDeviceContextSpecQt::Init(nsIPrintSettings *aPS)
{
  DO_PR_DEBUG_LOG(("nsDeviceContextSpecQt::Init(aPS=%p\n", aPS));
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

  rv = GlobalPrinters::GetInstance()->InitializeGlobalPrinters();
  if (NS_FAILED(rv)) {
    return rv;
  }

  GlobalPrinters::GetInstance()->FreeGlobalPrinters();

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
    PRUnichar *papername      = nsnull;
    PRUnichar *printfile      = nsnull;
    double     dleft          = 0.5;
    double     dright         = 0.5;
    double     dtop           = 0.5;
    double     dbottom        = 0.5;

    aPS->GetPrinterName(&printer);
    aPS->GetPrintReversed(&reversed);
    aPS->GetPrintInColor(&color);
    aPS->GetPaperName(&papername);
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
    if (papername)
      strcpy(mPaperName, NS_ConvertUCS2toUTF8(papername).get());

    DO_PR_DEBUG_LOG(("margins:   %5.2f,%5.2f,%5.2f,%5.2f\n", dtop, dleft, dbottom, dright));
    DO_PR_DEBUG_LOG(("printRange %d\n",   printRange));
    DO_PR_DEBUG_LOG(("fromPage   %d\n",   fromPage));
    DO_PR_DEBUG_LOG(("toPage     %d\n",   toPage));
    DO_PR_DEBUG_LOG(("tofile     %d\n",   tofile));
    DO_PR_DEBUG_LOG(("printfile  '%s'\n", printfile? NS_ConvertUCS2toUTF8(printfile).get():"<NULL>"));
    DO_PR_DEBUG_LOG(("command    '%s'\n", command? NS_ConvertUCS2toUTF8(command).get():"<NULL>"));
    DO_PR_DEBUG_LOG(("printer    '%s'\n", printer? NS_ConvertUCS2toUTF8(printer).get():"<NULL>"));
    DO_PR_DEBUG_LOG(("papername  '%s'\n", papername? NS_ConvertUCS2toUTF8(papername).get():"<NULL>"));

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

  return rv;
}

NS_IMETHODIMP nsDeviceContextSpecQt::GetToPrinter(PRBool &aToPrinter)
{
  aToPrinter = mToPrinter;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecQt::GetPrinterName ( const char **aPrinter )
{
   *aPrinter = mPrinter;
   return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecQt::GetCopies ( int &aCopies )
{
   aCopies = mCopies;
   return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecQt::GetFirstPageFirst(PRBool &aFpf)
{
  aFpf = mFpf;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecQt::GetGrayscale(PRBool &aGrayscale)
{
  aGrayscale = mGrayscale;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecQt::GetLandscape(PRBool &aLandscape)
{
  aLandscape = (mOrientation == NS_LANDSCAPE);
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecQt::GetTopMargin(float &aValue)
{
  aValue = mTop;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecQt::GetBottomMargin(float &aValue)
{
  aValue = mBottom;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecQt::GetRightMargin(float &aValue)
{
  aValue = mRight;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecQt::GetLeftMargin(float &aValue)
{
  aValue = mLeft;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecQt::GetCommand(const char **aCommand)
{
  *aCommand = mCommand;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecQt::GetPath(const char **aPath)
{
  *aPath = mPath;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecQt::GetUserCancelled(PRBool &aCancel)
{
  aCancel = mCancel;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecQt::GetPaperName( const char **aPaperName )
{
  *aPaperName = mPaperName;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecQt::GetPageSizeInTwips(PRInt32 *aWidth, PRInt32 *aHeight)
{
  return mPrintSettings->GetPageSizeInTwips(aWidth, aHeight);
}

NS_IMETHODIMP nsDeviceContextSpecQt::GetPrintMethod(PrintMethod &aMethod)
{
  return GetPrintMethod(mPrinter, aMethod);
}

/* static !! */
nsresult nsDeviceContextSpecQt::GetPrintMethod(const char *aPrinter, PrintMethod &aMethod)
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

NS_IMETHODIMP nsDeviceContextSpecQt::ClosePrintManager()
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
nsPrinterEnumeratorQt::nsPrinterEnumeratorQt()
{
}

NS_IMPL_ISUPPORTS1(nsPrinterEnumeratorQt, nsIPrinterEnumerator)

NS_IMETHODIMP nsPrinterEnumeratorQt::EnumeratePrinters(PRUint32* aCount, PRUnichar*** aResult)
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
NS_IMETHODIMP nsPrinterEnumeratorQt::GetDefaultPrinterName(PRUnichar **aDefaultPrinterName)
{
  DO_PR_DEBUG_LOG(("nsPrinterEnumeratorQt::GetDefaultPrinterName()\n"));
  NS_ENSURE_ARG_POINTER(aDefaultPrinterName);

  GlobalPrinters::GetInstance()->GetDefaultPrinterName(aDefaultPrinterName);

  DO_PR_DEBUG_LOG(("GetDefaultPrinterName(): default printer='%s'.\n", NS_ConvertUCS2toUTF8(*aDefaultPrinterName).get()));
  return NS_OK;
}

/* void initPrintSettingsFromPrinter (in wstring aPrinterName, in nsIPrintSettings aPrintSettings); */
NS_IMETHODIMP nsPrinterEnumeratorQt::InitPrintSettingsFromPrinter(const PRUnichar *aPrinterName, nsIPrintSettings *aPrintSettings)
{
  DO_PR_DEBUG_LOG(("nsPrinterEnumeratorQt::InitPrintSettingsFromPrinter()"));
  nsresult rv;

  NS_ENSURE_ARG_POINTER(aPrinterName);
  NS_ENSURE_ARG_POINTER(aPrintSettings);

  NS_ENSURE_TRUE(*aPrinterName, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(aPrintSettings, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPref> pPrefs = do_GetService(NS_PREF_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsXPIDLCString fullPrinterName, /* Full name of printer incl. driver-specific prefix */
                 printerName;     /* "Stripped" name of printer */
  fullPrinterName.Assign(NS_ConvertUCS2toUTF8(aPrinterName));
  printerName.Assign(NS_ConvertUCS2toUTF8(aPrinterName));
  DO_PR_DEBUG_LOG(("printerName='%s'\n", printerName.get()));

  PrintMethod type = pmInvalid;
  rv = nsDeviceContextSpecQt::GetPrintMethod(printerName, type);
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
  pPrefs->SetBoolPref(nsPrintfCString(256, PRINTERFEATURES_PREF ".%s.has_special_printerfeatures", fullPrinterName.get()).get(), PR_FALSE);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */


  /* Set filename */
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

  aPrintSettings->SetIsInitializedFromPrinter(PR_TRUE);
#ifdef USE_XPRINT
  if (type == pmXprint) {
    DO_PR_DEBUG_LOG(("InitPrintSettingsFromPrinter() for Xprint printer\n"));

    Display   *pdpy;
    XPContext  pcontext;
    if (XpuGetPrinter(printerName, &pdpy, &pcontext) != 1)
      return NS_ERROR_GFX_PRINTER_NAME_NOT_FOUND;

    XpuSupportedFlags supported_doc_attrs = XpuGetSupportedDocAttributes(pdpy, pcontext);

#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    nsPrinterFeatures printerFeatures(fullPrinterName);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

    /* Setup orientation stuff */
    XpuOrientationList  olist;
    int                 ocount;
    XpuOrientationRec  *default_orientation;

#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    PRBool canSetOrientation = MAKE_PR_BOOL(supported_doc_attrs & XPUATTRIBUTESUPPORTED_CONTENT_ORIENTATION);
    printerFeatures.SetCanChangeOrientation(canSetOrientation);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

    /* Get list of supported orientations */
    olist = XpuGetOrientationList(pdpy, pcontext, &ocount);
    if (olist) {
      default_orientation = &olist[0]; /* First entry is the default one */

      if (!PL_strcasecmp(default_orientation->orientation, "portrait")) {
        DO_PR_DEBUG_LOG(("setting default orientation to 'portrait'\n"));
        aPrintSettings->SetOrientation(nsIPrintSettings::kPortraitOrientation);
      }
      else if (!PL_strcasecmp(default_orientation->orientation, "landscape")) {
        DO_PR_DEBUG_LOG(("setting default orientation to 'landscape'\n"));
        aPrintSettings->SetOrientation(nsIPrintSettings::kLandscapeOrientation);
      }
      else {
        DO_PR_DEBUG_LOG(("Unknown default orientation '%s'\n", default_orientation->orientation));
      }

#ifdef SET_PRINTER_FEATURES_VIA_PREFS
      int i;
      for( i = 0 ; i < ocount ; i++ )
      {
        XpuOrientationRec *curr = &olist[i];
        printerFeatures.SetOrientationRecord(i, curr->orientation);
      }
      printerFeatures.SetNumOrientationRecords(ocount);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

      XpuFreeOrientationList(olist);
    }

    /* Setup Number of Copies */
#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    PRBool canSetNumCopies = MAKE_PR_BOOL(supported_doc_attrs & XPUATTRIBUTESUPPORTED_COPY_COUNT);
    printerFeatures.SetCanChangeNumCopies(canSetNumCopies);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */
    long numCopies;
    if( XpuGetOneLongAttribute(pdpy, pcontext, XPDocAttr, "copy-count", &numCopies) != 1 )
    {
      /* Fallback on failure */
      numCopies = 1;
    }
    aPrintSettings->SetNumCopies(numCopies);

    /* Setup paper size stuff */
    XpuMediumSourceSizeList mlist;
    int                     mcount;
    XpuMediumSourceSizeRec *default_medium;

#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    PRBool canSetPaperSize = MAKE_PR_BOOL(supported_doc_attrs & XPUATTRIBUTESUPPORTED_DEFAULT_MEDIUM);
    printerFeatures.SetCanChangePaperSize(canSetPaperSize);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

    mlist = XpuGetMediumSourceSizeList(pdpy, pcontext, &mcount);
    if (mlist) {
      nsXPIDLCString papername;

      default_medium = &mlist[0]; /* First entry is the default one */
      double total_width  = default_medium->ma1 + default_medium->ma2,
             total_height = default_medium->ma3 + default_medium->ma4;

      /* Either "paper" or "tray/paper" */
      if (default_medium->tray_name) {
        papername = nsPrintfCString(256, "%s/%s", default_medium->tray_name, default_medium->medium_name);
      }
      else {
        papername.Assign(default_medium->medium_name);
      }

      DO_PR_DEBUG_LOG(("setting default paper size to '%s' (%g/%g mm)\n", papername.get(), total_width, total_height));
      aPrintSettings->SetPaperSizeType(nsIPrintSettings::kPaperSizeDefined);
      aPrintSettings->SetPaperSizeUnit(nsIPrintSettings::kPaperSizeMillimeters);
      aPrintSettings->SetPaperWidth(total_width);
      aPrintSettings->SetPaperHeight(total_height);
      aPrintSettings->SetPaperName(NS_ConvertUTF8toUCS2(papername).get());

#ifdef SET_PRINTER_FEATURES_VIA_PREFS
      int i;
      for( i = 0 ; i < mcount ; i++ )
      {
        XpuMediumSourceSizeRec *curr = &mlist[i];
        double total_width  = curr->ma1 + curr->ma2,
               total_height = curr->ma3 + curr->ma4;
        if (curr->tray_name) {
          papername = nsPrintfCString(256, "%s/%s", curr->tray_name, curr->medium_name);
        }
        else {
          papername.Assign(curr->medium_name);
        }

        printerFeatures.SetPaperRecord(i, papername, PRInt32(total_width), PRInt32(total_height), PR_FALSE);
      }
      printerFeatures.SetNumPaperSizeRecords(mcount);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

      XpuFreeMediumSourceSizeList(mlist);
    }

#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    /* Xprint does not allow the client to set a spooler command.
     * Job spooling is the job of the server side (=Xprt) */
    printerFeatures.SetCanChangeSpoolerCommand(PR_FALSE);

    /* Mozilla's Xprint support allows multiple nsIDeviceContext instances
     * be used in parallel */
    printerFeatures.SetMultipleConcurrentDeviceContextsSupported(PR_TRUE);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

    XpuClosePrinterDisplay(pdpy, pcontext);

    return NS_OK;
  }
  else
#endif /* USE_XPRINT */

#ifdef USE_POSTSCRIPT
  if (type == pmPostScript) {
    DO_PR_DEBUG_LOG(("InitPrintSettingsFromPrinter() for PostScript printer\n"));

#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    nsPrinterFeatures printerFeatures(fullPrinterName);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    printerFeatures.SetCanChangeOrientation(PR_TRUE);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

    nsXPIDLCString orientation;
    if (NS_SUCCEEDED(CopyPrinterCharPref(pPrefs, "postscript", printerName, "orientation", getter_Copies(orientation)))) {
      if (!PL_strcasecmp(orientation, "portrait")) {
        DO_PR_DEBUG_LOG(("setting default orientation to 'portrait'\n"));
        aPrintSettings->SetOrientation(nsIPrintSettings::kPortraitOrientation);
      }
      else if (!PL_strcasecmp(orientation, "landscape")) {
        DO_PR_DEBUG_LOG(("setting default orientation to 'landscape'\n"));
        aPrintSettings->SetOrientation(nsIPrintSettings::kLandscapeOrientation);
      }
      else {
        DO_PR_DEBUG_LOG(("Unknown default orientation '%s'\n", orientation.get()));
      }
    }

#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    int i;
    for( i = 0 ; postscript_module_orientations[i].orientation != nsnull ; i++ )
    {
      const PSOrientationRec *curr = &postscript_module_orientations[i];
      printerFeatures.SetOrientationRecord(i, curr->orientation);
    }
    printerFeatures.SetNumOrientationRecords(i);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    printerFeatures.SetCanChangePaperSize(PR_TRUE);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */
    nsXPIDLCString papername;
    if (NS_SUCCEEDED(CopyPrinterCharPref(pPrefs, "postscript", printerName, "paper_size", getter_Copies(papername)))) {
      int                   i;
      const PSPaperSizeRec *default_paper = nsnull;

      for( i = 0 ; postscript_module_paper_sizes[i].name != nsnull ; i++ )
      {
        const PSPaperSizeRec *curr = &postscript_module_paper_sizes[i];

        if (!PL_strcasecmp(papername, curr->name)) {
          default_paper = curr;
          break;
        }
      }

      if (default_paper) {
        DO_PR_DEBUG_LOG(("setting default paper size to '%s' (%g inch/%g inch)\n",
                        default_paper->name,
                        PSPaperSizeRec_FullPaperWidth(default_paper),
                        PSPaperSizeRec_FullPaperHeight(default_paper)));
        aPrintSettings->SetPaperSizeType(nsIPrintSettings::kPaperSizeDefined);
        aPrintSettings->SetPaperSizeUnit(nsIPrintSettings::kPaperSizeInches);
        aPrintSettings->SetPaperWidth(PSPaperSizeRec_FullPaperWidth(default_paper));
        aPrintSettings->SetPaperHeight(PSPaperSizeRec_FullPaperHeight(default_paper));
        aPrintSettings->SetPaperName(NS_ConvertUTF8toUCS2(default_paper->name).get());
      }
      else {
        DO_PR_DEBUG_LOG(("Unknown paper size '%s' given.\n", papername.get()));
      }
#ifdef SET_PRINTER_FEATURES_VIA_PREFS
      for( i = 0 ; postscript_module_paper_sizes[i].name != nsnull ; i++ )
      {
        const PSPaperSizeRec *curr = &postscript_module_paper_sizes[i];
#define CONVERT_INCH_TO_MILLIMETERS(inch) ((inch) * 25.4)
        double total_width  = CONVERT_INCH_TO_MILLIMETERS(PSPaperSizeRec_FullPaperWidth(curr)),
               total_height = CONVERT_INCH_TO_MILLIMETERS(PSPaperSizeRec_FullPaperHeight(curr));

        printerFeatures.SetPaperRecord(i, curr->name, PRInt32(total_width), PRInt32(total_height), PR_TRUE);
      }
      printerFeatures.SetNumPaperSizeRecords(i);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */
    }

#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    printerFeatures.SetCanChangeSpoolerCommand(PR_TRUE);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

    nsXPIDLCString command;
    if (NS_SUCCEEDED(CopyPrinterCharPref(pPrefs, "postscript", printerName, "print_command", getter_Copies(command)))) {
      DO_PR_DEBUG_LOG(("setting default print command to '%s'\n", command.get()));
      aPrintSettings->SetPrintCommand(NS_ConvertUTF8toUCS2(command).get());
    }

#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    printerFeatures.SetCanChangeNumCopies(PR_TRUE);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

    return NS_OK;
  }
#endif /* USE_POSTSCRIPT */

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsPrinterEnumeratorQt::DisplayPropertiesDlg(const PRUnichar *aPrinter, nsIPrintSettings *aPrintSettings)
{
  return NS_OK;
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

