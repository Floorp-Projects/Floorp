/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ex: set tabstop=8 softtabstop=2 shiftwidth=2 expandtab: */
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
 *   Ken Herron <kherron@fastmail.us>
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
 * ***** END LICENSE BLOCK *****
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 * 10/09/2000       IPLabs Linux Team      True Unicode glyps support added.
 */

#include "gfx-config.h"
 
#define FORCE_PR_LOG /* Allow logging in the release build */
#define PR_LOGGING 1
#include "prlog.h"

#include "nscore.h"
#include "nsIAtom.h"
#include "nsPostScriptObj.h"
#include "isotab.c"
#include "nsFont.h"
#include "nsIImage.h"
#include "nsAFMObject.h"

#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
#include "nsReadableUtils.h"

#include "nsICharsetAlias.h"
#include "nsNetUtil.h"
#include "nsIPersistentProperties2.h"
#include "nsCRT.h"
#include "nsFontMetricsPS.h"
#include "nsPaperPS.h"

#ifndef NS_BUILD_ID
#include "nsBuildID.h"
#endif /* !NS_BUILD_ID */

#include "prenv.h"
#include "prprf.h"
#include "prerror.h"

#include <errno.h>
#include <sys/wait.h>

#ifdef PR_LOGGING
static PRLogModuleInfo *nsPostScriptObjLM = PR_NewLogModule("nsPostScriptObj");
#endif /* PR_LOGGING */

/* A private class to format floating-point values into strings. This
 * is used to write floating-point values into the postscript document.
 * printf()-based functions can't be used because they may generate
 * locale-ized output, e.g. using a comma for the decimal point, which
 * isn't a valid postscript number.
 */
class fpCString : public nsCAutoString {
  public:
    inline fpCString(float aValue) { AppendFloat(aValue); }
};


#define NS_PS_RED(x) (((float)(NS_GET_R(x))) / 255.0) 
#define NS_PS_GREEN(x) (((float)(NS_GET_G(x))) / 255.0) 
#define NS_PS_BLUE(x) (((float)(NS_GET_B(x))) / 255.0) 
#define NS_PS_GRAY(x) (((float)(x)) / 255.0) 
#define NS_RGB_TO_GRAY(r,g,b) ((int(r) * 77 + int(g) * 150 + int(b) * 29) / 256)
#define NS_IS_BOLD(x) (((x) >= 401) ? 1 : 0) 

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

/* 
 * global
 */
static nsIUnicodeEncoder *gEncoder = nsnull;
static nsHashtable *gU2Ntable = nsnull;
static nsIPref *gPrefs = nsnull;
static nsHashtable *gLangGroups = nsnull;

static PRBool
FreeU2Ntable(nsHashKey * aKey, void *aData, void *aClosure)
{
  delete(int *) aData;
  return PR_TRUE;
}

static PRBool
ResetU2Ntable(nsHashKey * aKey, void *aData, void *aClosure)
{
  PS_LangGroupInfo *linfo = (PS_LangGroupInfo *)aData;
  if (linfo && linfo->mU2Ntable) {
    linfo->mU2Ntable->Reset(FreeU2Ntable, nsnull);
  }
  return PR_TRUE;
}

static PRBool
FreeLangGroups(nsHashKey * aKey, void *aData, void *aClosure)
{
  PS_LangGroupInfo *linfo = (PS_LangGroupInfo *) aData;

  NS_IF_RELEASE(linfo->mEncoder);

  if (linfo->mU2Ntable) {
    linfo->mU2Ntable->Reset(FreeU2Ntable, nsnull);
    delete linfo->mU2Ntable;
    linfo->mU2Ntable = nsnull;
  }
  delete linfo;
  linfo = nsnull;
  return PR_TRUE;
}

static void
PrintAsDSCTextline(FILE *f, const char *text, int maxlen)
{
  NS_ASSERTION(maxlen > 1, "bad max length");

  if (*text != '(') {
    // Format as DSC textline type
    fprintf(f, "%.*s", maxlen, text);
    return;
  }

  // Fallback: Format as DSC text type
  fprintf(f, "(");

  int len = maxlen - 2;
  while (*text && len > 0) {
    if (!isprint(*text)) {
      if (len < 4) break;
      fprintf(f, "\\%03o", *text);
      len -= 4;
    }
    else if (*text == '(' || *text == ')' || *text == '\\') {
      if (len < 2) break;
      fprintf(f, "\\%c", *text);
      len -= 2;
    }
    else {
      fprintf(f, "%c", *text);
      len--;
    }
    text++;
  }
  fprintf(f, ")");
}

/** ---------------------------------------------------
 *  Default Constructor
 *	@update 2/1/99 dwc
 */
nsPostScriptObj::nsPostScriptObj() :
  mPrintSetup(nsnull),
  mPrintContext(nsnull),
  mTitle(nsnull),
  mScriptFP(nsnull)
{
  PR_LOG(nsPostScriptObjLM, PR_LOG_DEBUG, ("nsPostScriptObj::nsPostScriptObj()\n"));

  nsServiceManager::GetService(kPrefCID, NS_GET_IID(nsIPref), (nsISupports**) &gPrefs);

  gLangGroups = new nsHashtable();
}

/** ---------------------------------------------------
 *  Destructor
 *	@update 2/1/99 dwc
 */
nsPostScriptObj::~nsPostScriptObj()
{
  PR_LOG(nsPostScriptObjLM, PR_LOG_DEBUG, ("nsPostScriptObj::~nsPostScriptObj()\n"));

  if (mScriptFP)
    fclose(mScriptFP);
  if (mDocScript)
    mDocScript->Remove(PR_FALSE);
  finalize_translation();

  // Cleanup things allocated along the way
  if (nsnull != mTitle){
    nsMemory::Free(mTitle);
  }

  if (nsnull != mPrintContext){
    if (nsnull != mPrintContext->prInfo){
      delete mPrintContext->prInfo;
    }
    if (nsnull != mPrintContext->prSetup){
      delete mPrintContext->prSetup;
    }
    delete mPrintContext;
    mPrintContext = nsnull;
  }

  if (nsnull != mPrintSetup) {
	  delete mPrintSetup;
	  mPrintSetup = nsnull;
  }

  NS_IF_RELEASE(gPrefs);

  if (gLangGroups) {
    gLangGroups->Reset(FreeLangGroups, nsnull);
    delete gLangGroups;
    gLangGroups = nsnull;
  }

  PR_LOG(nsPostScriptObjLM, PR_LOG_DEBUG, ("nsPostScriptObj::~nsPostScriptObj(): printing done."));
}

void 
nsPostScriptObj::settitle(PRUnichar * aTitle)
{
  if (aTitle) {
    mTitle = ToNewCString(nsDependentString(aTitle));
  }
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/20/2004 kherron
 */
nsresult 
nsPostScriptObj::Init( nsIDeviceContextSpecPS *aSpec )
{
  PRBool      isGray,
              isFirstPageFirst;
  int         landscape;

  PrintInfo* pi = new PrintInfo(); 
  mPrintSetup = new PrintSetup();

  if( (nsnull!=pi) && (nsnull!=mPrintSetup) ){
    memset(mPrintSetup, 0, sizeof(struct PrintSetup_));
    
    mPrintSetup->color = PR_TRUE;              // Image output 
    mPrintSetup->deep_color = PR_TRUE;         // 24 bit color output 
    mPrintSetup->reverse = 0;                  // Output order, 0 is acsending 
    if ( aSpec != nsnull ) {
      aSpec->GetCopies(mPrintSetup->num_copies);
      aSpec->GetGrayscale( isGray );
      if ( isGray == PR_TRUE ) {
        mPrintSetup->color = PR_FALSE; 
        mPrintSetup->deep_color = PR_FALSE; 
      }

      aSpec->GetFirstPageFirst( isFirstPageFirst );
      if ( isFirstPageFirst == PR_FALSE )
        mPrintSetup->reverse = 1;
    } else 
        return NS_ERROR_FAILURE;

    /* Open a temporary file for the document body */
    nsresult rv = mTempfileFactory.CreateTempFile(
	getter_AddRefs(mDocScript), &mScriptFP, "a+");
    NS_ENSURE_SUCCESS(rv, NS_ERROR_GFX_PRINTER_FILE_IO_ERROR);

    mPrintContext = new PSContext();
    memset(mPrintContext, 0, sizeof(struct PSContext_));
    memset(pi, 0, sizeof(struct PrintInfo_));

    /* Find PS paper size record by name */
    aSpec->GetPaperName(&(mPrintSetup->paper_name));
    nsPaperSizePS paper;
    if (!paper.Find(mPrintSetup->paper_name))
      return NS_ERROR_GFX_PRINTER_PAPER_SIZE_NOT_SUPPORTED;

    aSpec->GetLandscape( landscape );
    mPrintSetup->width = NS_MILLIMETERS_TO_TWIPS(paper.Width_mm());
    mPrintSetup->height = NS_MILLIMETERS_TO_TWIPS(paper.Height_mm());

    if (landscape) {
      nscoord temp = mPrintSetup->width;
      mPrintSetup->width = mPrintSetup->height;
      mPrintSetup->height = temp;
    }

#ifdef DEBUG
    printf("\nPaper Width = %d twips (%gmm) Height = %d twips (%gmm)\n",
        mPrintSetup->width, paper.Width_mm(),
        mPrintSetup->height, paper.Height_mm());
#endif
    mPrintSetup->header = "header";
    mPrintSetup->footer = "footer";
    mPrintSetup->sizes = nsnull;

    mPrintSetup->landscape = (landscape) ? PR_TRUE : PR_FALSE; // Rotated output 
    //mPrintSetup->landscape = PR_FALSE;

    mPrintSetup->underline = PR_TRUE;             // underline links 
    mPrintSetup->scale_images = PR_TRUE;          // Scale unsized images which are too big 
    mPrintSetup->scale_pre = PR_FALSE;		        // do the pre-scaling thing 


    mPrintSetup->rules = 1.0f;			            // Scale factor for rulers 
    mPrintSetup->n_up = 0;                     // cool page combining 
    mPrintSetup->bigger = 1;                   // Used to init sizes if sizesin NULL 
    mPrintSetup->prefix = "";                  // For text xlate, prepended to each line 
    mPrintSetup->eol = "";			    // For text translation, line terminator 
    mPrintSetup->bullet = "+";                 // What char to use for bullets 

#ifdef NOTYET
    URL_Struct_* url = new URL_Struct_;
    memset(url, 0, sizeof(URL_Struct_));
    mPrintSetup->url = url;                    // url of doc being translated 
#else
    mPrintSetup->url = nsnull;
#endif
    mPrintSetup->completion = nsnull;          // Called when translation finished 
    mPrintSetup->carg = nsnull;                // Data saved for completion routine 
    mPrintSetup->status = 0;                   // Status of URL on completion 

    mTitle = nsnull;

    pi->doc_title = mTitle;

    mPrintContext->prInfo = pi;

    // begin the document
    initialize_translation(mPrintSetup);

    mPageNumber = 1;                  // we are on the first page

    // read the printer properties file
    NS_LoadPersistentPropertiesFromURISpec(getter_AddRefs(mPrinterProps),
      NS_LITERAL_CSTRING("resource:/res/unixpsfonts.properties"));

    return NS_OK;
    } else {
    return NS_ERROR_FAILURE;
    }
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::finalize_translation()
{
  if (mPrintContext) {
    free(mPrintContext->prSetup);
    mPrintContext->prSetup = nsnull;
  }
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::initialize_translation(PrintSetup* pi)
{
  PrintSetup *dup = (PrintSetup *)malloc(sizeof(PrintSetup));
  *dup = *pi;
  mPrintContext->prSetup = dup;
}

/** ---------------------------------------------------
 *  Write the document prolog.
 *  @param aHandle File handle which receives the prolog
 */
void 
nsPostScriptObj::write_prolog(FILE *aHandle)
{
  int i;
  FILE *f = aHandle;

  nscoord paper_width = mPrintContext->prSetup->width;
  nscoord paper_height = mPrintContext->prSetup->height;
  const char *orientation;

  if (paper_height < paper_width) {
    // prSetup->width and height have been swapped, indicating landscape.
    // The bounding box etc. must be in terms of the default PS coordinate
    // system.
    nscoord temp = paper_width;
    paper_width = paper_height;
    paper_height = temp;
    orientation = "Landscape";
  }
  else
    orientation = "Portrait";

  fprintf(f, "%%!PS-Adobe-3.0\n");
  fprintf(f, "%%%%BoundingBox: 0 0 %s %s\n",
    fpCString(NSTwipsToFloatPoints(paper_width)).get(),
    fpCString(NSTwipsToFloatPoints(paper_height)).get());

  fprintf(f, "%%%%Creator: Mozilla PostScript module (%s/%lu)\n",
             "rv:" MOZILLA_VERSION, (unsigned long)NS_BUILD_ID);
  fprintf(f, "%%%%DocumentData: Clean8Bit\n");
  fprintf(f, "%%%%DocumentPaperSizes: %s\n", mPrintSetup->paper_name);
  fprintf(f, "%%%%Orientation: %s\n", orientation);
  fprintf(f, "%%%%Pages: %d\n", (int) mPageNumber - 1);

  fprintf(f, "%%%%PageOrder: %s\n",
      mPrintContext->prSetup->reverse ? "Descend" : "Ascend");

  if (nsnull != mPrintContext->prInfo->doc_title) {
    // DSC spec: max line length is 255 characters
    fprintf(f, "%%%%Title: ");
    PrintAsDSCTextline(f, mPrintContext->prInfo->doc_title, 230);
    fprintf(f, "\n");
  }

  fprintf(f, "%%%%EndComments\n");

  // general comments: Mozilla-specific 
  fputs("% MozillaCharsetName: iso-8859-1\n\n", f);
    
    // now begin prolog 
  fprintf(f, "%%%%BeginProlog\n");

  // Tell the printer what size paper it should use
  fprintf(f,
    "/setpagedevice where\n"			// Test for the feature
    "{ pop 1 dict\n"				// Set up a dictionary
    "  dup /PageSize [ %s %s ] put\n"		// Paper dimensions
    "  setpagedevice\n"				// Install settings
    "} if\n", 
    fpCString(NSTwipsToFloatPoints(paper_width)).get(),
    fpCString(NSTwipsToFloatPoints(paper_height)).get());

  fprintf(f, "[");
  for (i = 0; i < 256; i++){
	  if (*isotab[i] == '\0'){
      fprintf(f, " /.notdef");
    }else{
	    fprintf(f, " /%s", isotab[i]);
    }

    if (( i % 6) == 5){
      fprintf(f, "\n");
    }
  }

  fprintf(f, "] /isolatin1encoding exch def\n");

  // Procedure to reencode a font
  fprintf(f, "%s",
      "/Mfr {\n"
      "  findfont dup length dict\n"
      "  begin\n"
      "    {1 index /FID ne {def} {pop pop} ifelse} forall\n"
      "    /Encoding isolatin1encoding def\n"
      "    currentdict\n"
      "  end\n"
      "  definefont pop\n"
      "} bind def\n");

  // Procedure to select and scale a font, using selectfont if available. See
  // Adobe Technical note 5048. Note msf args are backwards from selectfont.
  fprintf(f, "%s",
    "/Msf /selectfont where\n"
    "  { pop { exch selectfont } }\n"
    "  { { findfont exch scalefont setfont } }\n"
    "  ifelse\n"
    "  bind def\n");

  for(i=0;i<NUM_AFM_FONTS;i++){
    fprintf(f, 
      "/F%d /%s Mfr\n"
      "/f%d { dup /csize exch def /F%d Msf } bind def\n",
      i, gSubstituteFonts[i].mPSName, i, i);
  }

  fprintf(f, "%s",
    // Unicode glyph dictionary
    "/UniDict\n"
    "1051 dict dup begin\n"
    "16#0020	/space def\n"
    "16#0021	/exclam def\n"
    "16#0022	/quotedbl def\n"
    "16#0023	/numbersign def\n"
    "16#0024	/dollar def\n"
    "16#0025	/percent def\n"
    "16#0026	/ampersand def\n"
    "16#0027	/quotesingle def\n"
    "16#0028	/parenleft def\n"
    "16#0029	/parenright def\n"
    "16#002A	/asterisk def\n"
    "16#002B	/plus def\n"
    "16#002C	/comma def\n"
    "16#002D	/hyphen def\n"
    "16#002E	/period def\n"
    "16#002F	/slash def\n"
    "16#0030	/zero def\n"
    "16#0031	/one def\n"
    "16#0032	/two def\n"
    "16#0033	/three def\n"
    "16#0034	/four def\n"
    "16#0035	/five def\n"
    "16#0036	/six def\n"
    "16#0037	/seven def\n"
    "16#0038	/eight def\n"
    "16#0039	/nine def\n"
    "16#003A	/colon def\n"
    "16#003B	/semicolon def\n"
    "16#003C	/less def\n"
    "16#003D	/equal def\n"
    "16#003E	/greater def\n"
    "16#003F	/question def\n"
    "16#0040	/at def\n"
    "16#0041	/A def\n"
    "16#0042	/B def\n"
    "16#0043	/C def\n"
    "16#0044	/D def\n"
    "16#0045	/E def\n"
    "16#0046	/F def\n"
    "16#0047	/G def\n"
    "16#0048	/H def\n"
    "16#0049	/I def\n"
    "16#004A	/J def\n"
    "16#004B	/K def\n"
    "16#004C	/L def\n"
    "16#004D	/M def\n"
    "16#004E	/N def\n"
    "16#004F	/O def\n"
    "16#0050	/P def\n"
    "16#0051	/Q def\n"
    "16#0052	/R def\n"
    "16#0053	/S def\n"
    "16#0054	/T def\n"
    "16#0055	/U def\n"
    "16#0056	/V def\n"
    "16#0057	/W def\n"
    "16#0058	/X def\n"
    "16#0059	/Y def\n"
    "16#005A	/Z def\n"
    "16#005B	/bracketleft def\n"
    "16#005C	/backslash def\n"
    "16#005D	/bracketright def\n"
    "16#005E	/asciicircum def\n"
    "16#005F	/underscore def\n"
    "16#0060	/grave def\n"
    "16#0061	/a def\n"
    "16#0062	/b def\n"
    "16#0063	/c def\n"
    "16#0064	/d def\n"
    "16#0065	/e def\n"
    "16#0066	/f def\n"
    "16#0067	/g def\n"
    "16#0068	/h def\n"
    "16#0069	/i def\n"
    "16#006A	/j def\n"
    "16#006B	/k def\n"
    "16#006C	/l def\n"
    "16#006D	/m def\n"
    "16#006E	/n def\n"
    "16#006F	/o def\n"
    "16#0070	/p def\n"
    "16#0071	/q def\n"
    "16#0072	/r def\n"
    "16#0073	/s def\n"
    "16#0074	/t def\n"
    "16#0075	/u def\n"
    "16#0076	/v def\n"
    "16#0077	/w def\n"
    "16#0078	/x def\n"
    "16#0079	/y def\n"
    "16#007A	/z def\n"
    "16#007B	/braceleft def\n"
    "16#007C	/bar def\n"
    "16#007D	/braceright def\n"
    "16#007E	/asciitilde def\n"
    "16#00A0	/space def\n"
    "16#00A1	/exclamdown def\n"
    "16#00A2	/cent def\n"
    "16#00A3	/sterling def\n"
    "16#00A4	/currency def\n"
    "16#00A5	/yen def\n"
    "16#00A6	/brokenbar def\n"
    "16#00A7	/section def\n"
    "16#00A8	/dieresis def\n"
    "16#00A9	/copyright def\n"
    "16#00AA	/ordfeminine def\n"
    "16#00AB	/guillemotleft def\n"
    "16#00AC	/logicalnot def\n"
    "16#00AD	/hyphen def\n"
    "16#00AE	/registered def\n"
    "16#00AF	/macron def\n"
    "16#00B0	/degree def\n"
    "16#00B1	/plusminus def\n"
    "16#00B2	/twosuperior def\n"
    "16#00B3	/threesuperior def\n"
    "16#00B4	/acute def\n"
    "16#00B5	/mu def\n"
    "16#00B6	/paragraph def\n"
    "16#00B7	/periodcentered def\n"
    "16#00B8	/cedilla def\n"
    "16#00B9	/onesuperior def\n"
    "16#00BA	/ordmasculine def\n"
    "16#00BB	/guillemotright def\n"
    "16#00BC	/onequarter def\n"
    "16#00BD	/onehalf def\n"
    "16#00BE	/threequarters def\n"
    "16#00BF	/questiondown def\n"
    "16#00C0	/Agrave def\n"
    "16#00C1	/Aacute def\n"
    "16#00C2	/Acircumflex def\n"
    "16#00C3	/Atilde def\n"
    "16#00C4	/Adieresis def\n"
    "16#00C5	/Aring def\n"
    "16#00C6	/AE def\n"
    "16#00C7	/Ccedilla def\n"
    "16#00C8	/Egrave def\n"
    "16#00C9	/Eacute def\n"
    "16#00CA	/Ecircumflex def\n"
    "16#00CB	/Edieresis def\n"
    "16#00CC	/Igrave def\n"
    "16#00CD	/Iacute def\n"
    "16#00CE	/Icircumflex def\n"
    "16#00CF	/Idieresis def\n"
    "16#00D0	/Eth def\n"
    "16#00D1	/Ntilde def\n"
    "16#00D2	/Ograve def\n"
    "16#00D3	/Oacute def\n"
    "16#00D4	/Ocircumflex def\n"
    "16#00D5	/Otilde def\n"
    "16#00D6	/Odieresis def\n"
    "16#00D7	/multiply def\n"
    "16#00D8	/Oslash def\n"
    "16#00D9	/Ugrave def\n"
    "16#00DA	/Uacute def\n"
    "16#00DB	/Ucircumflex def\n"
    "16#00DC	/Udieresis def\n"
    "16#00DD	/Yacute def\n"
    "16#00DE	/Thorn def\n"
    "16#00DF	/germandbls def\n"
    "16#00E0	/agrave def\n"
    "16#00E1	/aacute def\n"
    "16#00E2	/acircumflex def\n"
    "16#00E3	/atilde def\n"
    "16#00E4	/adieresis def\n"
    "16#00E5	/aring def\n"
    "16#00E6	/ae def\n"
    "16#00E7	/ccedilla def\n"
    "16#00E8	/egrave def\n"
    "16#00E9	/eacute def\n"
    "16#00EA	/ecircumflex def\n"
    "16#00EB	/edieresis def\n"
    "16#00EC	/igrave def\n"
    "16#00ED	/iacute def\n"
    "16#00EE	/icircumflex def\n"
    "16#00EF	/idieresis def\n"
    "16#00F0	/eth def\n"
    "16#00F1	/ntilde def\n"
    "16#00F2	/ograve def\n"
    "16#00F3	/oacute def\n"
    "16#00F4	/ocircumflex def\n"
    "16#00F5	/otilde def\n"
    "16#00F6	/odieresis def\n"
    "16#00F7	/divide def\n"
    "16#00F8	/oslash def\n"
    "16#00F9	/ugrave def\n"
    "16#00FA	/uacute def\n"
    "16#00FB	/ucircumflex def\n"
    "16#00FC	/udieresis def\n"
    "16#00FD	/yacute def\n"
    "16#00FE	/thorn def\n"
    "16#00FF	/ydieresis def\n"
    "16#0100	/Amacron def\n"
    "16#0101	/amacron def\n"
    "16#0102	/Abreve def\n"
    "16#0103	/abreve def\n"
    "16#0104	/Aogonek def\n"
    "16#0105	/aogonek def\n"
    "16#0106	/Cacute def\n"
    "16#0107	/cacute def\n"
    "16#0108	/Ccircumflex def\n"
    "16#0109	/ccircumflex def\n"
    "16#010A	/Cdotaccent def\n"
    "16#010B	/cdotaccent def\n"
    "16#010C	/Ccaron def\n"
    "16#010D	/ccaron def\n"
    "16#010E	/Dcaron def\n"
    "16#010F	/dcaron def\n"
    "16#0110	/Dcroat def\n"
    "16#0111	/dcroat def\n"
    "16#0112	/Emacron def\n"
    "16#0113	/emacron def\n"
    "16#0114	/Ebreve def\n"
    "16#0115	/ebreve def\n"
    "16#0116	/Edotaccent def\n"
    "16#0117	/edotaccent def\n"
    "16#0118	/Eogonek def\n"
    "16#0119	/eogonek def\n"
    "16#011A	/Ecaron def\n"
    "16#011B	/ecaron def\n"
    "16#011C	/Gcircumflex def\n"
    "16#011D	/gcircumflex def\n"
    "16#011E	/Gbreve def\n"
    "16#011F	/gbreve def\n"
    "16#0120	/Gdotaccent def\n"
    "16#0121	/gdotaccent def\n"
    "16#0122	/Gcommaaccent def\n"
    "16#0123	/gcommaaccent def\n"
    "16#0124	/Hcircumflex def\n"
    "16#0125	/hcircumflex def\n"
    "16#0126	/Hbar def\n"
    "16#0127	/hbar def\n"
    "16#0128	/Itilde def\n"
    "16#0129	/itilde def\n"
    "16#012A	/Imacron def\n"
    "16#012B	/imacron def\n"
    "16#012C	/Ibreve def\n"
    "16#012D	/ibreve def\n"
    "16#012E	/Iogonek def\n"
    "16#012F	/iogonek def\n"
    "16#0130	/Idotaccent def\n"
    "16#0131	/dotlessi def\n"
    "16#0132	/IJ def\n"
    "16#0133	/ij def\n"
    "16#0134	/Jcircumflex def\n"
    "16#0135	/jcircumflex def\n"
    "16#0136	/Kcommaaccent def\n"
    "16#0137	/kcommaaccent def\n"
    "16#0138	/kgreenlandic def\n"
    "16#0139	/Lacute def\n"
    "16#013A	/lacute def\n"
    "16#013B	/Lcommaaccent def\n"
    "16#013C	/lcommaaccent def\n"
    "16#013D	/Lcaron def\n"
    "16#013E	/lcaron def\n"
    "16#013F	/Ldot def\n"
    "16#0140	/ldot def\n"
    "16#0141	/Lslash def\n"
    "16#0142	/lslash def\n"
    "16#0143	/Nacute def\n"
    "16#0144	/nacute def\n"
    "16#0145	/Ncommaaccent def\n"
    "16#0146	/ncommaaccent def\n"
    "16#0147	/Ncaron def\n"
    "16#0148	/ncaron def\n"
    "16#0149	/napostrophe def\n"
    "16#014A	/Eng def\n"
    "16#014B	/eng def\n"
    "16#014C	/Omacron def\n"
    "16#014D	/omacron def\n"
    "16#014E	/Obreve def\n"
    "16#014F	/obreve def\n"
    "16#0150	/Ohungarumlaut def\n"
    "16#0151	/ohungarumlaut def\n"
    "16#0152	/OE def\n"
    "16#0153	/oe def\n"
    "16#0154	/Racute def\n"
    "16#0155	/racute def\n"
    "16#0156	/Rcommaaccent def\n"
    "16#0157	/rcommaaccent def\n"
    "16#0158	/Rcaron def\n"
    "16#0159	/rcaron def\n"
    "16#015A	/Sacute def\n"
    "16#015B	/sacute def\n"
    "16#015C	/Scircumflex def\n"
    "16#015D	/scircumflex def\n"
    "16#015E	/Scedilla def\n"
    "16#015F	/scedilla def\n"
    "16#0160	/Scaron def\n"
    "16#0161	/scaron def\n"
    "16#0162	/Tcommaaccent def\n"
    "16#0163	/tcommaaccent def\n"
    "16#0164	/Tcaron def\n"
    "16#0165	/tcaron def\n"
    "16#0166	/Tbar def\n"
    "16#0167	/tbar def\n"
    "16#0168	/Utilde def\n"
    "16#0169	/utilde def\n"
    "16#016A	/Umacron def\n"
    "16#016B	/umacron def\n"
    "16#016C	/Ubreve def\n"
    "16#016D	/ubreve def\n"
    "16#016E	/Uring def\n"
    "16#016F	/uring def\n"
    "16#0170	/Uhungarumlaut def\n"
    "16#0171	/uhungarumlaut def\n"
    "16#0172	/Uogonek def\n"
    "16#0173	/uogonek def\n"
    "16#0174	/Wcircumflex def\n"
    "16#0175	/wcircumflex def\n"
    "16#0176	/Ycircumflex def\n"
    "16#0177	/ycircumflex def\n"
    "16#0178	/Ydieresis def\n"
    "16#0179	/Zacute def\n"
    "16#017A	/zacute def\n"
    "16#017B	/Zdotaccent def\n"
    "16#017C	/zdotaccent def\n"
    "16#017D	/Zcaron def\n"
    "16#017E	/zcaron def\n"
    "16#017F	/longs def\n"
    "16#0192	/florin def\n"
    "16#01A0	/Ohorn def\n"
    "16#01A1	/ohorn def\n"
    "16#01AF	/Uhorn def\n"
    "16#01B0	/uhorn def\n"
    "16#01E6	/Gcaron def\n"
    "16#01E7	/gcaron def\n"
    "16#01FA	/Aringacute def\n"
    "16#01FB	/aringacute def\n"
    "16#01FC	/AEacute def\n"
    "16#01FD	/aeacute def\n"
    "16#01FE	/Oslashacute def\n"
    "16#01FF	/oslashacute def\n"
    "16#0218	/Scommaaccent def\n"
    "16#0219	/scommaaccent def\n"
    "16#021A	/Tcommaaccent def\n"
    "16#021B	/tcommaaccent def\n"
    "16#02BC	/afii57929 def\n"
    "16#02BD	/afii64937 def\n"
    "16#02C6	/circumflex def\n"
    "16#02C7	/caron def\n"
    "16#02C9	/macron def\n"
    "16#02D8	/breve def\n"
    "16#02D9	/dotaccent def\n"
    "16#02DA	/ring def\n"
    "16#02DB	/ogonek def\n"
    "16#02DC	/tilde def\n"
    "16#02DD	/hungarumlaut def\n"
    "16#0300	/gravecomb def\n"
    "16#0301	/acutecomb def\n"
    "16#0303	/tildecomb def\n"
    "16#0309	/hookabovecomb def\n"
    "16#0323	/dotbelowcomb def\n"
    "16#0384	/tonos def\n"
    "16#0385	/dieresistonos def\n"
    "16#0386	/Alphatonos def\n"
    "16#0387	/anoteleia def\n"
    "16#0388	/Epsilontonos def\n"
    "16#0389	/Etatonos def\n"
    "16#038A	/Iotatonos def\n"
    "16#038C	/Omicrontonos def\n"
    "16#038E	/Upsilontonos def\n"
    "16#038F	/Omegatonos def\n"
    "16#0390	/iotadieresistonos def\n"
    "16#0391	/Alpha def\n"
    "16#0392	/Beta def\n"
    "16#0393	/Gamma def\n"
    "16#0394	/Delta def\n"
    "16#0395	/Epsilon def\n"
    "16#0396	/Zeta def\n"
    "16#0397	/Eta def\n"
    "16#0398	/Theta def\n"
    "16#0399	/Iota def\n"
    "16#039A	/Kappa def\n"
    "16#039B	/Lambda def\n"
    "16#039C	/Mu def\n"
    "16#039D	/Nu def\n"
    "16#039E	/Xi def\n"
    "16#039F	/Omicron def\n"
    "16#03A0	/Pi def\n"
    "16#03A1	/Rho def\n"
    "16#03A3	/Sigma def\n"
    "16#03A4	/Tau def\n"
    "16#03A5	/Upsilon def\n"
    "16#03A6	/Phi def\n"
    "16#03A7	/Chi def\n"
    "16#03A8	/Psi def\n"
    "16#03A9	/Omega def\n"
    "16#03AA	/Iotadieresis def\n"
    "16#03AB	/Upsilondieresis def\n"
    "16#03AC	/alphatonos def\n"
    "16#03AD	/epsilontonos def\n"
    "16#03AE	/etatonos def\n"
    "16#03AF	/iotatonos def\n"
    "16#03B0	/upsilondieresistonos def\n"
    "16#03B1	/alpha def\n"
    "16#03B2	/beta def\n"
    "16#03B3	/gamma def\n"
    "16#03B4	/delta def\n"
    "16#03B5	/epsilon def\n"
    "16#03B6	/zeta def\n"
    "16#03B7	/eta def\n"
    "16#03B8	/theta def\n"
    "16#03B9	/iota def\n"
    "16#03BA	/kappa def\n"
    "16#03BB	/lambda def\n"
    "16#03BC	/mu def\n"
    "16#03BD	/nu def\n"
    "16#03BE	/xi def\n"
    "16#03BF	/omicron def\n"
    "16#03C0	/pi def\n"
    "16#03C1	/rho def\n"
    "16#03C2	/sigma1 def\n"
    "16#03C3	/sigma def\n"
    "16#03C4	/tau def\n"
    "16#03C5	/upsilon def\n"
    "16#03C6	/phi def\n"
    "16#03C7	/chi def\n"
    "16#03C8	/psi def\n"
    "16#03C9	/omega def\n"
    "16#03CA	/iotadieresis def\n"
    "16#03CB	/upsilondieresis def\n"
    "16#03CC	/omicrontonos def\n"
    );
  fprintf(f, "%s",
    "16#03CD	/upsilontonos def\n"
    "16#03CE	/omegatonos def\n"
    "16#03D1	/theta1 def\n"
    "16#03D2	/Upsilon1 def\n"
    "16#03D5	/phi1 def\n"
    "16#03D6	/omega1 def\n"
    "16#0401	/afii10023 def\n"
    "16#0402	/afii10051 def\n"
    "16#0403	/afii10052 def\n"
    "16#0404	/afii10053 def\n"
    "16#0405	/afii10054 def\n"
    "16#0406	/afii10055 def\n"
    "16#0407	/afii10056 def\n"
    "16#0408	/afii10057 def\n"
    "16#0409	/afii10058 def\n"
    "16#040A	/afii10059 def\n"
    "16#040B	/afii10060 def\n"
    "16#040C	/afii10061 def\n"
    "16#040E	/afii10062 def\n"
    "16#040F	/afii10145 def\n"
    "16#0410	/afii10017 def\n"
    "16#0411	/afii10018 def\n"
    "16#0412	/afii10019 def\n"
    "16#0413	/afii10020 def\n"
    "16#0414	/afii10021 def\n"
    "16#0415	/afii10022 def\n"
    "16#0416	/afii10024 def\n"
    "16#0417	/afii10025 def\n"
    "16#0418	/afii10026 def\n"
    "16#0419	/afii10027 def\n"
    "16#041A	/afii10028 def\n"
    "16#041B	/afii10029 def\n"
    "16#041C	/afii10030 def\n"
    "16#041D	/afii10031 def\n"
    "16#041E	/afii10032 def\n"
    "16#041F	/afii10033 def\n"
    "16#0420	/afii10034 def\n"
    "16#0421	/afii10035 def\n"
    "16#0422	/afii10036 def\n"
    "16#0423	/afii10037 def\n"
    "16#0424	/afii10038 def\n"
    "16#0425	/afii10039 def\n"
    "16#0426	/afii10040 def\n"
    "16#0427	/afii10041 def\n"
    "16#0428	/afii10042 def\n"
    "16#0429	/afii10043 def\n"
    "16#042A	/afii10044 def\n"
    "16#042B	/afii10045 def\n"
    "16#042C	/afii10046 def\n"
    "16#042D	/afii10047 def\n"
    "16#042E	/afii10048 def\n"
    "16#042F	/afii10049 def\n"
    "16#0430	/afii10065 def\n"
    "16#0431	/afii10066 def\n"
    "16#0432	/afii10067 def\n"
    "16#0433	/afii10068 def\n"
    "16#0434	/afii10069 def\n"
    "16#0435	/afii10070 def\n"
    "16#0436	/afii10072 def\n"
    "16#0437	/afii10073 def\n"
    "16#0438	/afii10074 def\n"
    "16#0439	/afii10075 def\n"
    "16#043A	/afii10076 def\n"
    "16#043B	/afii10077 def\n"
    "16#043C	/afii10078 def\n"
    "16#043D	/afii10079 def\n"
    "16#043E	/afii10080 def\n"
    "16#043F	/afii10081 def\n"
    "16#0440	/afii10082 def\n"
    "16#0441	/afii10083 def\n"
    "16#0442	/afii10084 def\n"
    "16#0443	/afii10085 def\n"
    "16#0444	/afii10086 def\n"
    "16#0445	/afii10087 def\n"
    "16#0446	/afii10088 def\n"
    "16#0447	/afii10089 def\n"
    "16#0448	/afii10090 def\n"
    "16#0449	/afii10091 def\n"
    "16#044A	/afii10092 def\n"
    "16#044B	/afii10093 def\n"
    "16#044C	/afii10094 def\n"
    "16#044D	/afii10095 def\n"
    "16#044E	/afii10096 def\n"
    "16#044F	/afii10097 def\n"
    "16#0451	/afii10071 def\n"
    "16#0452	/afii10099 def\n"
    "16#0453	/afii10100 def\n"
    "16#0454	/afii10101 def\n"
    "16#0455	/afii10102 def\n"
    "16#0456	/afii10103 def\n"
    "16#0457	/afii10104 def\n"
    "16#0458	/afii10105 def\n"
    "16#0459	/afii10106 def\n"
    "16#045A	/afii10107 def\n"
    "16#045B	/afii10108 def\n"
    "16#045C	/afii10109 def\n"
    "16#045E	/afii10110 def\n"
    "16#045F	/afii10193 def\n"
    "16#0462	/afii10146 def\n"
    "16#0463	/afii10194 def\n"
    "16#0472	/afii10147 def\n"
    "16#0473	/afii10195 def\n"
    "16#0474	/afii10148 def\n"
    "16#0475	/afii10196 def\n"
    "16#0490	/afii10050 def\n"
    "16#0491	/afii10098 def\n"
    "16#04D9	/afii10846 def\n"
    "16#05B0	/afii57799 def\n"
    "16#05B1	/afii57801 def\n"
    "16#05B2	/afii57800 def\n"
    "16#05B3	/afii57802 def\n"
    "16#05B4	/afii57793 def\n"
    "16#05B5	/afii57794 def\n"
    "16#05B6	/afii57795 def\n"
    "16#05B7	/afii57798 def\n"
    "16#05B8	/afii57797 def\n"
    "16#05B9	/afii57806 def\n"
    "16#05BB	/afii57796 def\n"
    "16#05BC	/afii57807 def\n"
    "16#05BD	/afii57839 def\n"
    "16#05BE	/afii57645 def\n"
    "16#05BF	/afii57841 def\n"
    "16#05C0	/afii57842 def\n"
    "16#05C1	/afii57804 def\n"
    "16#05C2	/afii57803 def\n"
    "16#05C3	/afii57658 def\n"
    "16#05D0	/afii57664 def\n"
    "16#05D1	/afii57665 def\n"
    "16#05D2	/afii57666 def\n"
    "16#05D3	/afii57667 def\n"
    "16#05D4	/afii57668 def\n"
    "16#05D5	/afii57669 def\n"
    "16#05D6	/afii57670 def\n"
    "16#05D7	/afii57671 def\n"
    "16#05D8	/afii57672 def\n"
    "16#05D9	/afii57673 def\n"
    "16#05DA	/afii57674 def\n"
    "16#05DB	/afii57675 def\n"
    "16#05DC	/afii57676 def\n"
    "16#05DD	/afii57677 def\n"
    "16#05DE	/afii57678 def\n"
    "16#05DF	/afii57679 def\n"
    "16#05E0	/afii57680 def\n"
    "16#05E1	/afii57681 def\n"
    "16#05E2	/afii57682 def\n"
    "16#05E3	/afii57683 def\n"
    "16#05E4	/afii57684 def\n"
    "16#05E5	/afii57685 def\n"
    "16#05E6	/afii57686 def\n"
    "16#05E7	/afii57687 def\n"
    "16#05E8	/afii57688 def\n"
    "16#05E9	/afii57689 def\n"
    "16#05EA	/afii57690 def\n"
    "16#05F0	/afii57716 def\n"
    "16#05F1	/afii57717 def\n"
    "16#05F2	/afii57718 def\n"
    "16#060C	/afii57388 def\n"
    "16#061B	/afii57403 def\n"
    "16#061F	/afii57407 def\n"
    "16#0621	/afii57409 def\n"
    "16#0622	/afii57410 def\n"
    "16#0623	/afii57411 def\n"
    "16#0624	/afii57412 def\n"
    "16#0625	/afii57413 def\n"
    "16#0626	/afii57414 def\n"
    "16#0627	/afii57415 def\n"
    "16#0628	/afii57416 def\n"
    "16#0629	/afii57417 def\n"
    "16#062A	/afii57418 def\n"
    "16#062B	/afii57419 def\n"
    "16#062C	/afii57420 def\n"
    "16#062D	/afii57421 def\n"
    "16#062E	/afii57422 def\n"
    "16#062F	/afii57423 def\n"
    "16#0630	/afii57424 def\n"
    "16#0631	/afii57425 def\n"
    "16#0632	/afii57426 def\n"
    "16#0633	/afii57427 def\n"
    "16#0634	/afii57428 def\n"
    "16#0635	/afii57429 def\n"
    "16#0636	/afii57430 def\n"
    "16#0637	/afii57431 def\n"
    "16#0638	/afii57432 def\n"
    "16#0639	/afii57433 def\n"
    "16#063A	/afii57434 def\n"
    "16#0640	/afii57440 def\n"
    "16#0641	/afii57441 def\n"
    "16#0642	/afii57442 def\n"
    "16#0643	/afii57443 def\n"
    "16#0644	/afii57444 def\n"
    "16#0645	/afii57445 def\n"
    "16#0646	/afii57446 def\n"
    "16#0647	/afii57470 def\n"
    "16#0648	/afii57448 def\n"
    "16#0649	/afii57449 def\n"
    "16#064A	/afii57450 def\n"
    "16#064B	/afii57451 def\n"
    "16#064C	/afii57452 def\n"
    "16#064D	/afii57453 def\n"
    "16#064E	/afii57454 def\n"
    "16#064F	/afii57455 def\n"
    "16#0650	/afii57456 def\n"
    "16#0651	/afii57457 def\n"
    "16#0652	/afii57458 def\n"
    "16#0660	/afii57392 def\n"
    "16#0661	/afii57393 def\n"
    "16#0662	/afii57394 def\n"
    "16#0663	/afii57395 def\n"
    "16#0664	/afii57396 def\n"
    "16#0665	/afii57397 def\n"
    "16#0666	/afii57398 def\n"
    "16#0667	/afii57399 def\n"
    "16#0668	/afii57400 def\n"
    "16#0669	/afii57401 def\n"
    "16#066A	/afii57381 def\n"
    "16#066D	/afii63167 def\n"
    "16#0679	/afii57511 def\n"
    "16#067E	/afii57506 def\n"
    "16#0686	/afii57507 def\n"
    "16#0688	/afii57512 def\n"
    "16#0691	/afii57513 def\n"
    "16#0698	/afii57508 def\n"
    "16#06A4	/afii57505 def\n"
    "16#06AF	/afii57509 def\n"
    "16#06BA	/afii57514 def\n"
    "16#06D2	/afii57519 def\n"
    "16#06D5	/afii57534 def\n"
    "16#1E80	/Wgrave def\n"
    "16#1E81	/wgrave def\n"
    "16#1E82	/Wacute def\n"
    "16#1E83	/wacute def\n"
    "16#1E84	/Wdieresis def\n"
    "16#1E85	/wdieresis def\n"
    "16#1EF2	/Ygrave def\n"
    "16#1EF3	/ygrave def\n"
    "16#200C	/afii61664 def\n"
    "16#200D	/afii301 def\n"
    "16#200E	/afii299 def\n"
    "16#200F	/afii300 def\n"
    "16#2012	/figuredash def\n"
    "16#2013	/endash def\n"
    "16#2014	/emdash def\n"
    "16#2015	/afii00208 def\n"
    "16#2017	/underscoredbl def\n"
    "16#2018	/quoteleft def\n"
    "16#2019	/quoteright def\n"
    "16#201A	/quotesinglbase def\n"
    "16#201B	/quotereversed def\n"
    "16#201C	/quotedblleft def\n"
    "16#201D	/quotedblright def\n"
    "16#201E	/quotedblbase def\n"
    "16#2020	/dagger def\n"
    "16#2021	/daggerdbl def\n"
    "16#2022	/bullet def\n"
    "16#2024	/onedotenleader def\n"
    "16#2025	/twodotenleader def\n"
    "16#2026	/ellipsis def\n"
    "16#202C	/afii61573 def\n"
    "16#202D	/afii61574 def\n"
    "16#202E	/afii61575 def\n"
    "16#2030	/perthousand def\n"
    "16#2032	/minute def\n"
    "16#2033	/second def\n"
    "16#2039	/guilsinglleft def\n"
    "16#203A	/guilsinglright def\n"
    "16#203C	/exclamdbl def\n"
    "16#2044	/fraction def\n"
    "16#2070	/zerosuperior def\n"
    "16#2074	/foursuperior def\n"
    "16#2075	/fivesuperior def\n"
    "16#2076	/sixsuperior def\n"
    "16#2077	/sevensuperior def\n"
    "16#2078	/eightsuperior def\n"
    "16#2079	/ninesuperior def\n"
    "16#207D	/parenleftsuperior def\n"
    "16#207E	/parenrightsuperior def\n"
    "16#207F	/nsuperior def\n"
    "16#2080	/zeroinferior def\n"
    "16#2081	/oneinferior def\n"
    "16#2082	/twoinferior def\n"
    "16#2083	/threeinferior def\n"
    "16#2084	/fourinferior def\n"
    "16#2085	/fiveinferior def\n"
    "16#2086	/sixinferior def\n"
    "16#2087	/seveninferior def\n"
    "16#2088	/eightinferior def\n"
    "16#2089	/nineinferior def\n"
    "16#208D	/parenleftinferior def\n"
    "16#208E	/parenrightinferior def\n"
    "16#20A1	/colonmonetary def\n"
    "16#20A3	/franc def\n"
    "16#20A4	/lira def\n"
    "16#20A7	/peseta def\n"
    "16#20AA	/afii57636 def\n"
    "16#20AB	/dong def\n"
    "16#20AC	/Euro def\n"
    "16#2105	/afii61248 def\n"
    "16#2111	/Ifraktur def\n"
    "16#2113	/afii61289 def\n"
    "16#2116	/afii61352 def\n"
    "16#2118	/weierstrass def\n"
    "16#211C	/Rfraktur def\n"
    "16#211E	/prescription def\n"
    "16#2122	/trademark def\n"
    "16#2126	/Omega def\n"
    "16#212E	/estimated def\n"
    "16#2135	/aleph def\n"
    "16#2153	/onethird def\n"
    "16#2154	/twothirds def\n"
    "16#215B	/oneeighth def\n"
    "16#215C	/threeeighths def\n"
    "16#215D	/fiveeighths def\n"
    "16#215E	/seveneighths def\n"
    "16#2190	/arrowleft def\n"
    "16#2191	/arrowup def\n"
    "16#2192	/arrowright def\n"
    "16#2193	/arrowdown def\n"
    "16#2194	/arrowboth def\n"
    "16#2195	/arrowupdn def\n"
    "16#21A8	/arrowupdnbse def\n"
    "16#21B5	/carriagereturn def\n"
    "16#21D0	/arrowdblleft def\n"
    "16#21D1	/arrowdblup def\n"
    "16#21D2	/arrowdblright def\n"
    "16#21D3	/arrowdbldown def\n"
    "16#21D4	/arrowdblboth def\n"
    "16#2200	/universal def\n"
    "16#2202	/partialdiff def\n"
    "16#2203	/existential def\n"
    "16#2205	/emptyset def\n"
    "16#2206	/Delta def\n"
    "16#2207	/gradient def\n"
    "16#2208	/element def\n"
    "16#2209	/notelement def\n"
    "16#220B	/suchthat def\n"
    "16#220F	/product def\n"
    "16#2211	/summation def\n"
    "16#2212	/minus def\n"
    "16#2215	/fraction def\n"
    "16#2217	/asteriskmath def\n"
    "16#2219	/periodcentered def\n"
    "16#221A	/radical def\n"
    "16#221D	/proportional def\n"
    "16#221E	/infinity def\n"
    "16#221F	/orthogonal def\n"
    "16#2220	/angle def\n"
    "16#2227	/logicaland def\n"
    "16#2228	/logicalor def\n"
    "16#2229	/intersection def\n"
    "16#222A	/union def\n"
    "16#222B	/integral def\n"
    "16#2234	/therefore def\n"
    "16#223C	/similar def\n"
    "16#2245	/congruent def\n"
    "16#2248	/approxequal def\n"
    "16#2260	/notequal def\n"
    "16#2261	/equivalence def\n"
    "16#2264	/lessequal def\n"
    "16#2265	/greaterequal def\n"
    "16#2282	/propersubset def\n"
    "16#2283	/propersuperset def\n"
    "16#2284	/notsubset def\n"
    "16#2286	/reflexsubset def\n"
    "16#2287	/reflexsuperset def\n"
    "16#2295	/circleplus def\n"
    "16#2297	/circlemultiply def\n"
    "16#22A5	/perpendicular def\n"
    "16#22C5	/dotmath def\n"
    "16#2302	/house def\n"
    "16#2310	/revlogicalnot def\n"
    "16#2320	/integraltp def\n"
    "16#2321	/integralbt def\n"
    "16#2329	/angleleft def\n"
    "16#232A	/angleright def\n"
    "16#2500	/SF100000 def\n"
    "16#2502	/SF110000 def\n"
    "16#250C	/SF010000 def\n"
    "16#2510	/SF030000 def\n"
    "16#2514	/SF020000 def\n"
    "16#2518	/SF040000 def\n"
    "16#251C	/SF080000 def\n"
    "16#2524	/SF090000 def\n"
    "16#252C	/SF060000 def\n"
    "16#2534	/SF070000 def\n"
    "16#253C	/SF050000 def\n"
    "16#2550	/SF430000 def\n"
    "16#2551	/SF240000 def\n"
    "16#2552	/SF510000 def\n"
    "16#2553	/SF520000 def\n"
    "16#2554	/SF390000 def\n"
    "16#2555	/SF220000 def\n"
    "16#2556	/SF210000 def\n"
    "16#2557	/SF250000 def\n"
    "16#2558	/SF500000 def\n"
    "16#2559	/SF490000 def\n"
    "16#255A	/SF380000 def\n"
    "16#255B	/SF280000 def\n"
    "16#255C	/SF270000 def\n"
    "16#255D	/SF260000 def\n"
    "16#255E	/SF360000 def\n"
    "16#255F	/SF370000 def\n"
    "16#2560	/SF420000 def\n"
    "16#2561	/SF190000 def\n"
    "16#2562	/SF200000 def\n"
    "16#2563	/SF230000 def\n"
    "16#2564	/SF470000 def\n"
    "16#2565	/SF480000 def\n"
    "16#2566	/SF410000 def\n"
    "16#2567	/SF450000 def\n"
    "16#2568	/SF460000 def\n"
    "16#2569	/SF400000 def\n"
    "16#256A	/SF540000 def\n"
    "16#256B	/SF530000 def\n"
    "16#256C	/SF440000 def\n"
    "16#2580	/upblock def\n"
    "16#2584	/dnblock def\n"
    "16#2588	/block def\n"
    "16#258C	/lfblock def\n"
    "16#2590	/rtblock def\n"
    "16#2591	/ltshade def\n"
    "16#2592	/shade def\n"
    "16#2593	/dkshade def\n"
    "16#25A0	/filledbox def\n"
    );
  fprintf(f, "%s",
    "16#25A1	/H22073 def\n"
    "16#25AA	/H18543 def\n"
    "16#25AB	/H18551 def\n"
    "16#25AC	/filledrect def\n"
    "16#25B2	/triagup def\n"
    "16#25BA	/triagrt def\n"
    "16#25BC	/triagdn def\n"
    "16#25C4	/triaglf def\n"
    "16#25CA	/lozenge def\n"
    "16#25CB	/circle def\n"
    "16#25CF	/H18533 def\n"
    "16#25D8	/invbullet def\n"
    "16#25D9	/invcircle def\n"
    "16#25E6	/openbullet def\n"
    "16#263A	/smileface def\n"
    "16#263B	/invsmileface def\n"
    "16#263C	/sun def\n"
    "16#2640	/female def\n"
    "16#2642	/male def\n"
    "16#2660	/spade def\n"
    "16#2663	/club def\n"
    "16#2665	/heart def\n"
    "16#2666	/diamond def\n"
    "16#266A	/musicalnote def\n"
    "16#266B	/musicalnotedbl def\n"
    "16#F6BE	/dotlessj def\n"
    "16#F6BF	/LL def\n"
    "16#F6C0	/ll def\n"
    "16#F6C1	/Scedilla def\n"
    "16#F6C2	/scedilla def\n"
    "16#F6C3	/commaaccent def\n"
    "16#F6C4	/afii10063 def\n"
    "16#F6C5	/afii10064 def\n"
    "16#F6C6	/afii10192 def\n"
    "16#F6C7	/afii10831 def\n"
    "16#F6C8	/afii10832 def\n"
    "16#F6C9	/Acute def\n"
    "16#F6CA	/Caron def\n"
    "16#F6CB	/Dieresis def\n"
    "16#F6CC	/DieresisAcute def\n"
    "16#F6CD	/DieresisGrave def\n"
    "16#F6CE	/Grave def\n"
    "16#F6CF	/Hungarumlaut def\n"
    "16#F6D0	/Macron def\n"
    "16#F6D1	/cyrBreve def\n"
    "16#F6D2	/cyrFlex def\n"
    "16#F6D3	/dblGrave def\n"
    "16#F6D4	/cyrbreve def\n"
    "16#F6D5	/cyrflex def\n"
    "16#F6D6	/dblgrave def\n"
    "16#F6D7	/dieresisacute def\n"
    "16#F6D8	/dieresisgrave def\n"
    "16#F6D9	/copyrightserif def\n"
    "16#F6DA	/registerserif def\n"
    "16#F6DB	/trademarkserif def\n"
    "16#F6DC	/onefitted def\n"
    "16#F6DD	/rupiah def\n"
    "16#F6DE	/threequartersemdash def\n"
    "16#F6DF	/centinferior def\n"
    "16#F6E0	/centsuperior def\n"
    "16#F6E1	/commainferior def\n"
    "16#F6E2	/commasuperior def\n"
    "16#F6E3	/dollarinferior def\n"
    "16#F6E4	/dollarsuperior def\n"
    "16#F6E5	/hypheninferior def\n"
    "16#F6E6	/hyphensuperior def\n"
    "16#F6E7	/periodinferior def\n"
    "16#F6E8	/periodsuperior def\n"
    "16#F6E9	/asuperior def\n"
    "16#F6EA	/bsuperior def\n"
    "16#F6EB	/dsuperior def\n"
    "16#F6EC	/esuperior def\n"
    "16#F6ED	/isuperior def\n"
    "16#F6EE	/lsuperior def\n"
    "16#F6EF	/msuperior def\n"
    "16#F6F0	/osuperior def\n"
    "16#F6F1	/rsuperior def\n"
    "16#F6F2	/ssuperior def\n"
    "16#F6F3	/tsuperior def\n"
    "16#F6F4	/Brevesmall def\n"
    "16#F6F5	/Caronsmall def\n"
    "16#F6F6	/Circumflexsmall def\n"
    "16#F6F7	/Dotaccentsmall def\n"
    "16#F6F8	/Hungarumlautsmall def\n"
    "16#F6F9	/Lslashsmall def\n"
    "16#F6FA	/OEsmall def\n"
    "16#F6FB	/Ogoneksmall def\n"
    "16#F6FC	/Ringsmall def\n"
    "16#F6FD	/Scaronsmall def\n"
    "16#F6FE	/Tildesmall def\n"
    "16#F6FF	/Zcaronsmall def\n"
    "16#F721	/exclamsmall def\n"
    "16#F724	/dollaroldstyle def\n"
    "16#F726	/ampersandsmall def\n"
    "16#F730	/zerooldstyle def\n"
    "16#F731	/oneoldstyle def\n"
    "16#F732	/twooldstyle def\n"
    "16#F733	/threeoldstyle def\n"
    "16#F734	/fouroldstyle def\n"
    "16#F735	/fiveoldstyle def\n"
    "16#F736	/sixoldstyle def\n"
    "16#F737	/sevenoldstyle def\n"
    "16#F738	/eightoldstyle def\n"
    "16#F739	/nineoldstyle def\n"
    "16#F73F	/questionsmall def\n"
    "16#F760	/Gravesmall def\n"
    "16#F761	/Asmall def\n"
    "16#F762	/Bsmall def\n"
    "16#F763	/Csmall def\n"
    "16#F764	/Dsmall def\n"
    "16#F765	/Esmall def\n"
    "16#F766	/Fsmall def\n"
    "16#F767	/Gsmall def\n"
    "16#F768	/Hsmall def\n"
    "16#F769	/Ismall def\n"
    "16#F76A	/Jsmall def\n"
    "16#F76B	/Ksmall def\n"
    "16#F76C	/Lsmall def\n"
    "16#F76D	/Msmall def\n"
    "16#F76E	/Nsmall def\n"
    "16#F76F	/Osmall def\n"
    "16#F770	/Psmall def\n"
    "16#F771	/Qsmall def\n"
    "16#F772	/Rsmall def\n"
    "16#F773	/Ssmall def\n"
    "16#F774	/Tsmall def\n"
    "16#F775	/Usmall def\n"
    "16#F776	/Vsmall def\n"
    "16#F777	/Wsmall def\n"
    "16#F778	/Xsmall def\n"
    "16#F779	/Ysmall def\n"
    "16#F77A	/Zsmall def\n"
    "16#F7A1	/exclamdownsmall def\n"
    "16#F7A2	/centoldstyle def\n"
    "16#F7A8	/Dieresissmall def\n"
    "16#F7AF	/Macronsmall def\n"
    "16#F7B4	/Acutesmall def\n"
    "16#F7B8	/Cedillasmall def\n"
    "16#F7BF	/questiondownsmall def\n"
    "16#F7E0	/Agravesmall def\n"
    "16#F7E1	/Aacutesmall def\n"
    "16#F7E2	/Acircumflexsmall def\n"
    "16#F7E3	/Atildesmall def\n"
    "16#F7E4	/Adieresissmall def\n"
    "16#F7E5	/Aringsmall def\n"
    "16#F7E6	/AEsmall def\n"
    "16#F7E7	/Ccedillasmall def\n"
    "16#F7E8	/Egravesmall def\n"
    "16#F7E9	/Eacutesmall def\n"
    "16#F7EA	/Ecircumflexsmall def\n"
    "16#F7EB	/Edieresissmall def\n"
    "16#F7EC	/Igravesmall def\n"
    "16#F7ED	/Iacutesmall def\n"
    "16#F7EE	/Icircumflexsmall def\n"
    "16#F7EF	/Idieresissmall def\n"
    "16#F7F0	/Ethsmall def\n"
    "16#F7F1	/Ntildesmall def\n"
    "16#F7F2	/Ogravesmall def\n"
    "16#F7F3	/Oacutesmall def\n"
    "16#F7F4	/Ocircumflexsmall def\n"
    "16#F7F5	/Otildesmall def\n"
    "16#F7F6	/Odieresissmall def\n"
    "16#F7F8	/Oslashsmall def\n"
    "16#F7F9	/Ugravesmall def\n"
    "16#F7FA	/Uacutesmall def\n"
    "16#F7FB	/Ucircumflexsmall def\n"
    "16#F7FC	/Udieresissmall def\n"
    "16#F7FD	/Yacutesmall def\n"
    "16#F7FE	/Thornsmall def\n"
    "16#F7FF	/Ydieresissmall def\n"
    "16#F8E5	/radicalex def\n"
    "16#F8E6	/arrowvertex def\n"
    "16#F8E7	/arrowhorizex def\n"
    "16#F8E8	/registersans def\n"
    "16#F8E9	/copyrightsans def\n"
    "16#F8EA	/trademarksans def\n"
    "16#F8EB	/parenlefttp def\n"
    "16#F8EC	/parenleftex def\n"
    "16#F8ED	/parenleftbt def\n"
    "16#F8EE	/bracketlefttp def\n"
    "16#F8EF	/bracketleftex def\n"
    "16#F8F0	/bracketleftbt def\n"
    "16#F8F1	/bracelefttp def\n"
    "16#F8F2	/braceleftmid def\n"
    "16#F8F3	/braceleftbt def\n"
    "16#F8F4	/braceex def\n"
    "16#F8F5	/integralex def\n"
    "16#F8F6	/parenrighttp def\n"
    "16#F8F7	/parenrightex def\n"
    "16#F8F8	/parenrightbt def\n"
    "16#F8F9	/bracketrighttp def\n"
    "16#F8FA	/bracketrightex def\n"
    "16#F8FB	/bracketrightbt def\n"
    "16#F8FC	/bracerighttp def\n"
    "16#F8FD	/bracerightmid def\n"
    "16#F8FE	/bracerightbt def\n"
    "16#FB00	/ff def\n"
    "16#FB01	/fi def\n"
    "16#FB02	/fl def\n"
    "16#FB03	/ffi def\n"
    "16#FB04	/ffl def\n"
    "16#FB1F	/afii57705 def\n"
    "16#FB2A	/afii57694 def\n"
    "16#FB2B	/afii57695 def\n"
    "16#FB35	/afii57723 def\n"
    "16#FB4B	/afii57700 def\n"
    "end\n"
    "def\n"
    );

  fprintf(f, "%s",
    "10 dict dup begin\n"
    "  /FontType 3 def\n"
    "  /FontMatrix [.001 0 0 .001 0 0 ] def\n"
    "  /FontBBox [0 0 100 100] def\n"
    "  /Encoding 256 array def\n"
    "  0 1 255 {Encoding exch /.notdef put} for\n"
    "  Encoding 97 /openbox put\n"
    "  /CharProcs 2 dict def\n"
    "  CharProcs begin\n"
    "    /.notdef { } def\n"
    "    /openbox\n"
    "      { newpath\n"
    "          90 30 moveto  90 670 lineto\n"
    "          730 670 lineto  730 30 lineto\n"
    "        closepath\n"
    "        60 setlinewidth\n"
    "        stroke } def\n"
    "  end\n"
    "  /BuildChar\n"
    "    { 1000 0 0\n"
    "	0 750 750\n"
    "        setcachedevice\n"
    "	exch begin\n"
    "        Encoding exch get\n"
    "        CharProcs exch get\n"
    "	end\n"
    "	exec\n"
    "    } def\n"
    "end\n"
    "/NoglyphFont exch definefont pop\n"
    "\n"

    "/mbshow {                       % num\n"
    "    8 array                     % num array\n"
    "    -1                          % num array counter\n"
    "    {\n"
    "        dup 7 ge { exit } if\n"
    "        1 add                   % num array counter\n"
    "        2 index 16#100 mod      % num array counter mod\n"
    "        3 copy put pop          % num array counter\n"
    "        2 index 16#100 idiv     % num array counter num\n"
    "        dup 0 le\n"
    "        {\n"
    "            pop exit\n"
    "        } if\n"
    "        4 -1 roll pop\n"
    "        3 1 roll\n"
    "    } loop                      % num array counter\n"
    "    3 -1 roll pop               % array counter\n"
    "    dup 1 add string            % array counter string\n"
    "    0 1 3 index\n"
    "    {                           % array counter string index\n"
    "        2 index 1 index sub     % array counter string index sid\n"
    "        4 index 3 2 roll get    % array counter string sid byte\n"
    "        2 index 3 1 roll put    % array counter string\n"
    "    } for\n"
    "    show pop pop\n"
    "} def\n"
    );

  fprintf(f, "%s",
    "/draw_undefined_char\n"
    "{\n"
    "  csize /NoglyphFont Msf (a) show\n"
    "} bind def\n"
    "\n"
    "/real_unicodeshow\n"
    "{\n"
    "  /ccode exch def\n"
    "  /Unicodedict where {\n"
    "    pop\n"
    "    Unicodedict ccode known {\n"
    "      /cwidth {currentfont /ScaleMatrix get 0 get} def \n"
    "      /cheight cwidth def \n"
    "      gsave\n"
    "      currentpoint translate\n"
    "      cwidth 1056 div cheight 1056 div scale\n"
    "      2 -2 translate\n"
    "      ccode Unicodedict exch get\n"
    "      cvx exec\n"
    "      grestore\n"
    "      currentpoint exch cwidth add exch moveto\n"
    "      true\n"
    "    } {\n"
    "      false\n"
    "    } ifelse\n"
    "  } {\n"
    "    false\n"
    "  } ifelse\n"
    "} bind def\n"
    "\n"
    "/real_unicodeshow_native\n"
    "{\n"
    "  /ccode exch def\n"
    "  /NativeFont where {\n"
    "    pop\n"
    "    NativeFont findfont /FontName get /Courier eq {\n"
    "      false\n"
    "    } {\n"
    "      csize NativeFont Msf\n"
    "      /Unicode2NativeDict where {\n"
    "        pop\n"
    "        Unicode2NativeDict ccode known {\n"
    "          Unicode2NativeDict ccode get show\n"
    "          true\n"
    "        } {\n"
    "          false\n"
    "        } ifelse\n"
    "      } {\n"
    "	  false\n"
    "      } ifelse\n"
    "    } ifelse\n"
    "  } {\n"
    "    false\n"
    "  } ifelse\n"
    "} bind def\n"
    "\n"
    "/real_glyph_unicodeshow\n"
    "{\n"
    "  /ccode exch def\n"
    "      /UniDict where {\n"
    "        pop\n"
    "        UniDict ccode known {\n"
    "          UniDict ccode get glyphshow\n"
    "          true\n"
    "        } {\n"
    "          false\n"
    "        } ifelse\n"
    "      } {\n"
    "	  false\n"
    "      } ifelse\n"
    "} bind def\n"
    "/real_unicodeshow_cid\n"
    "{\n"
    "  /ccode exch def\n"
    "  /UCS2Font where {\n"
    "    pop\n"
    "    UCS2Font findfont /FontName get /Courier eq {\n"
    "      false\n"
    "    } {\n"
    "      csize UCS2Font Msf\n"
    "      ccode mbshow\n"
    "      true\n"
    "    } ifelse\n"
    "  } {\n"
    "    false\n"
    "  } ifelse\n"
    "} bind def\n"
    "\n"
    "/unicodeshow \n"
    "{\n"
    "  /cfont currentfont def\n"
    "  /str exch def\n"
    "  /i 0 def\n"
    "  str length /ls exch def\n"
    "  {\n"
    "    i 1 add ls ge {exit} if\n"
    "    str i get /c1 exch def\n"
    "    str i 1 add get /c2 exch def\n"
    "    /c c2 256 mul c1 add def\n"
    "    c2 1 ge \n"
    "    {\n"
    "      c unicodeshow1\n"
    "      {\n"
    "        % do nothing\n"
    "      } {\n"
    "        c real_unicodeshow_cid	% try CID \n"
    "        {\n"
    "          % do nothing\n"
    "        } {\n"
    "          c unicodeshow2\n"
    "          {\n"
    "            % do nothing\n"
    "          } {\n"
    "            draw_undefined_char\n"
    "          } ifelse\n"
    "        } ifelse\n"
    "      } ifelse\n"
    "    } {\n"
    "      % ascii\n"
    "      cfont setfont\n"
    "      str i 1 getinterval show\n"
    "    } ifelse\n"
    "    /i i 2 add def\n"
    "  } loop\n"
    "}  bind def\n"
    "\n"

    "/u2nadd {Unicode2NativeDict 3 1 roll put} bind def\n"
    "\n"

    "/Unicode2NativeDictdef 0 dict def\n"
    "/default_ls {\n"
    "  /Unicode2NativeDict Unicode2NativeDictdef def\n"
    "  /UCS2Font   /Courier def\n"
    "  /NativeFont /Courier def\n"
    "  /unicodeshow1 { real_glyph_unicodeshow } bind def\n"
    "  /unicodeshow2 { real_unicodeshow_native } bind def\n"
    "} bind def\n"
    );

    // Procedure to stroke a rectangle. Coordinates are rounded
    // to device pixel boundaries. See Adobe Technical notes 5111 and
    // 5126 and the "Scan Conversion" section of the PS Language
    // Reference for background.
  fprintf(f, "%s",
    "/Mrect { % x y w h Mrect -\n"
    "  2 index add\n"		// x y w h+y
    "  4 1 roll\n"		// h+y x y w
    "  2 index add\n"		// h+y x y w+x
    "  4 1 roll\n"		// w+x h+y x y
    "  transform round .1 add exch round .1 add exch itransform\n"
    "  4 -2 roll\n"		// x' y' w+x h+x
    "  transform round .1 sub exch round .1 sub exch itransform\n"
    "  2 index sub\n"		// x' y' w'+x h'
    "  4 1 roll\n"		// h' x' y' w'+x
    "  2 index sub\n"		// h' x' y' w'
    "  4 1 roll\n"		// w' h' x' y'
    "  moveto\n"		// w' h'
    "  dup 0 exch rlineto\n"	// w' h'
    "  exch 0 rlineto\n"	// h'
    "  neg 0 exch rlineto\n"	// -
    "  closepath\n"
    "} bind def\n"

    // Setstrokeadjust, or null if not supported
    "/Msetstrokeadjust /setstrokeadjust where\n"
    "  { pop /setstrokeadjust } { /pop } ifelse\n"
    "  load def\n"

    "\n"
    );

  // setup prolog for each langgroup
  initlanggroup(f);

  fprintf(f, "%%%%EndProlog\n");
}

/** ---------------------------------------------------
 *  Copy the body of the print job to a stream.
 *  @param aDestHandle Stream which will receive the print job.
 *  @return NS_OK on success.
 *          NS_ERROR_GFX_PRINTER_FILE_IO_ERROR on any I/O error.
 */
nsresult
nsPostScriptObj::write_script(FILE *aDestHandle)
{
  NS_PRECONDITION(aDestHandle, "Handle must not be NULL");
  char buf[BUFSIZ];
  size_t readAmt;

  rewind(mScriptFP);
  while ((readAmt = fread(buf, 1, sizeof buf, mScriptFP))) {
    size_t writeAmt = fwrite(buf, 1, readAmt, aDestHandle);
    if (readAmt != writeAmt)
      break;
  }
  return ferror(mScriptFP) || ferror(aDestHandle) ?
    NS_ERROR_GFX_PRINTER_FILE_IO_ERROR : NS_OK;
}


/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::begin_page()
{
  fprintf(mScriptFP, "%%%%Page: %d %d\n", mPageNumber, mPageNumber);
  fprintf(mScriptFP, "%%%%BeginPageSetup\n");
  if(mPrintSetup->num_copies != 1) {
    fprintf(mScriptFP, "1 dict dup /NumCopies %d put setpagedevice\n",
      mPrintSetup->num_copies);
  }
  fprintf(mScriptFP,"/pagelevel save def\n");
  // Rescale the coordinate system from points to twips.
  scale(1.0 / TWIPS_PER_POINT_FLOAT, 1.0 / TWIPS_PER_POINT_FLOAT);
  // Rotate and shift the coordinate system for landscape
  if (mPrintContext->prSetup->landscape){
    fprintf(mScriptFP, "90 rotate 0 -%d translate\n",
	mPrintContext->prSetup->height);
  }

  // Try to turn on automatic stroke adjust
  fputs("true Msetstrokeadjust\n", mScriptFP);
  fprintf(mScriptFP, "%%%%EndPageSetup\n");

  // need to reset all U2Ntable
  gLangGroups->Enumerate(ResetU2Ntable, nsnull);
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::end_page()
{
  fputs("pagelevel restore showpage\n", mScriptFP);
  mPageNumber++;
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
nsresult 
nsPostScriptObj::end_document()
{
  PR_LOG(nsPostScriptObjLM, PR_LOG_DEBUG, ("nsPostScriptObj::end_document()\n"));
  NS_PRECONDITION(mScriptFP, "No script file handle");

  // Finish up the document.
  fprintf(mScriptFP, "%%%%Trailer\n");
  fprintf(mScriptFP, "%%%%EOF\n");

  PR_LOG(nsPostScriptObjLM, PR_LOG_DEBUG, ("postscript generation completed.\n"));
  
  return ferror(mScriptFP) ?  NS_ERROR_GFX_PRINTER_FILE_IO_ERROR : NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc. Updated 3/22/2000 to deal with only non-Unicode chars. yueheng.xu@intel.com
 */
void 
nsPostScriptObj::show(const char* txt, int len, const char *align)
{
  fputc('(', mScriptFP);

  while (len-- > 0) {
    switch (*txt) {
	    case '(':
	    case ')':
	    case '\\':
	      fputc('\\', mScriptFP);
	      // Fall through
	    default:
	      fputc(*txt, mScriptFP);     
	      break;
	  }
	  txt++;
  }
  fprintf(mScriptFP, ") %sshow\n", align);
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 6/1/2000 katakai
 */
void 
nsPostScriptObj::preshow(const PRUnichar* txt, int len)
{
  unsigned char highbyte;
  PRUnichar uch;

  char outbuffer[6];
  PRUnichar inbuffer[2];
  nsresult res = NS_OK;

  if (gEncoder && gU2Ntable) {
    while (len-- > 0) {
      uch = *txt;
      highbyte = (uch >> 8 ) & 0xff;
      if (highbyte > 0) {
	inbuffer[0] = uch;
	inbuffer[1] = 0;

	PRInt32 *ncode = nsnull;
	nsStringKey key(inbuffer, 1);

	ncode = (PRInt32 *) gU2Ntable->Get(&key);

	if (ncode && *ncode) {
	} else {
	  PRInt32 insize, outsize;
	  outsize = 6;
	  insize = 1;
          // example, 13327 (USC2 0x340f) -> 14913679 (gb18030 0xe3908f)
	  res = gEncoder->Convert(inbuffer, &insize, outbuffer, &outsize);
	  if (NS_SUCCEEDED(res) && outsize > 1) {
            int i;
            PRInt32 code = 0;
            for(i=1;i<=outsize;i++) {
              code += (outbuffer[i - 1] & 0xff) << (8 * (outsize - i));
            }
            if (code) {
	      ncode = new PRInt32;
	      *ncode = code;
	      gU2Ntable->Put(&key, ncode);
	      fprintf(mScriptFP, "%d <%x> u2nadd\n", uch, code);
            }
	  }
	}
      }
      txt++;
    }
  }
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 3/22/2000 to deal with only unicode chars. yueheng.xu@intel.com
 */
void 
nsPostScriptObj::show(const PRUnichar* txt, int len,
                      const char *align, int aType)
{
  unsigned char highbyte, lowbyte;
  PRUnichar uch;
 
  if (aType == 1) {
    int i;
    fputc('<', mScriptFP);
    for (i=0; i < len; i++) {
      if (i == 0)
        fprintf(mScriptFP, "%04x", txt[i]);
      else
        fprintf(mScriptFP, " %04x", txt[i]);
    }
    fputs("> show\n", mScriptFP);
    return;
  }
 
  fputc('(', mScriptFP);

  while (len-- > 0) {
    switch (*txt) {
        case 0x0028:     // '('
            fputs("\\050\\000", mScriptFP);
		    break;
        case 0x0029:     // ')' 
            fputs("\\051\\000", mScriptFP);
		    break;
        case 0x005c:     // '\\'
            fputs("\\134\\000", mScriptFP);
		    break;
	    default:
          uch = *txt;
          highbyte = (uch >> 8 ) & 0xff;
          lowbyte = ( uch & 0xff );

          // we output all unicode chars in the 2x3 digits oct format for easier post-processing
          // Our 'show' command will always treat the second 3 digit oct as high 8-bits of unicode, independent of Endians
          if ( lowbyte < 8 )
	    fprintf(mScriptFP, "\\00%o", lowbyte  & 0xff);
          else if ( lowbyte < 64  && lowbyte >= 8)
	    fprintf(mScriptFP, "\\0%o", lowbyte & 0xff);
          else
	    fprintf(mScriptFP, "\\%o", lowbyte & 0xff);      

          if ( highbyte < 8  )
	    fprintf(mScriptFP, "\\00%o", highbyte & 0xff);
          else if ( highbyte < 64  && highbyte >= 8)
            fprintf(mScriptFP, "\\0%o", highbyte & 0xff);
          else
	    fprintf(mScriptFP, "\\%o", highbyte & 0xff);      
         
	  break;
	  }
	  txt++;
  }
  fprintf(mScriptFP, ") %sunicodeshow\n", align);
}




/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::moveto(nscoord x, nscoord y)
{
  fprintf(mScriptFP, "%d %d moveto\n", x, y);
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::lineto(nscoord aX, nscoord aY)
{
  fprintf(mScriptFP, "%d %d lineto\n", aX, aY);
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *    @param aWidth  Width of the ellipse implied by the arc
 *           aHeight Height of the ellipse
 *           aStartAngle  Angle for the start of the arc
 *           aEndAngle    Angle for the end of the arc
 */
void 
nsPostScriptObj::arc(nscoord aWidth, nscoord aHeight,
  float aStartAngle,float aEndAngle)
{
  // Arc definition
  fprintf(mScriptFP,
      "%s %s matrix currentmatrix currentpoint translate\n"
      " 3 1 roll scale newpath 0 0 1 %s %s arc setmatrix\n",
      fpCString(aWidth * 0.5).get(), fpCString(aHeight * 0.5).get(),
      fpCString(aStartAngle).get(), fpCString(aEndAngle).get());
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::box(nscoord aX, nscoord aY, nscoord aW, nscoord aH)
{
  fprintf(mScriptFP, "%d %d %d %d Mrect ", aX, aY, aW, aH);
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::box_subtract(nscoord aX, nscoord aY, nscoord aW, nscoord aH)
{
  fprintf(mScriptFP,
    "%d %d moveto 0 %d rlineto %d 0 rlineto 0 %d rlineto closepath ",
    aX, aY, aH, aW, -aH);
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::clip()
{
  fputs(" clip\n", mScriptFP);
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::eoclip()
{
  fputs(" eoclip\n", mScriptFP);
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::clippath()
{
  fputs(" clippath\n", mScriptFP);
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::newpath()
{
  fputs(" newpath\n", mScriptFP);
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::closepath()
{
  fputs(" closepath\n", mScriptFP);
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::initclip()
{
  fputs(" initclip\n", mScriptFP);
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::line(nscoord aX1, nscoord aY1,
  nscoord aX2, nscoord aY2, nscoord aThick)
{
  fprintf(mScriptFP, "gsave %d setlinewidth\n ", aThick);
  fprintf(mScriptFP, " %d %d moveto %d %d lineto\n", aX1, aY1, aX2, aY2);
  stroke();
  fprintf(mScriptFP, "grestore\n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::stroke()
{
  fputs(" stroke\n", mScriptFP);
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::fill()
{
  fputs(" fill\n", mScriptFP);
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::graphics_save()
{
  fputs(" gsave\n", mScriptFP);
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::graphics_restore()
{
  fputs(" grestore\n", mScriptFP);
}


/** ---------------------------------------------------
 *  Output postscript to scale the current coordinate system
 *    @param aX   X scale factor
 *           aY   Y scale factor
 */
void
nsPostScriptObj::scale(float aX, float aY)
{
  fprintf(mScriptFP, "%s %s scale\n",
      fpCString(aX).get(), fpCString(aY).get());
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::translate(nscoord x, nscoord y)
{
    fprintf(mScriptFP, "%d %d translate\n", x, y);
}


  /** ---------------------------------------------------
   *  Draw an image. This involves two coordinate systems, one for the
   *  page and one for the image. dRect is in the page coordinate system;
   *  it describes the rectangle which the image should occupy on the page.
   *  sRect describes the same rectangle, only in image space; it's the
   *  portion of the image which should appear on the page. iRect describes
   *  the position and dimensions of the actual pixel data within image
   *  space.
   *
   *  In the simple case, sRect and iRect will be the same, and their (x,y)
   *  values will be (0,0). But images may have a virtual size larger than
   *  the pixel rectangle, so iRect's position may not be (0,0) and its
   *  dimensions may be smaller than sRect's. Similarly, it's possible that
   *  only part of the image is being printed, in which case sRect's
   *  position may not be (0,0) and its dimensions may not span the entire
   *  iRect rectangle. So in the general case, iRect and sRect may be
   *  completely arbitrary relative to each other and to the image-space
   *  origin.
   *
   *    @update 3/14/2004 kherron
   *    @param anImage  Image to draw
   *    @param dRect    Rectangle describing where on the page the image
   *                    should appear. Units are twips.
   *    @param sRect    Rectangle describing the portion of the image that
   *                    appears on the page, i.e. the part of the image's
   *                    coordinate space that maps to dRect.
   *    @param iRect    Rectangle describing the portion of the image's
   *                    coordinate space covered by the image pixel data.
   */

void
nsPostScriptObj::draw_image(nsIImage *anImage,
    const nsRect& sRect, const nsRect& iRect, const nsRect& dRect)
{
  // If a final image dimension is 0 pixels, just return (see bug 191684)
  if ((0 == dRect.width) || (0 == dRect.height)) {
    return;
  }

  anImage->LockImagePixels(PR_FALSE);
  PRUint8 *theBits = anImage->GetBits();

  // Image data is unavailable, or it has no height or width.
  // There's nothing to print, so just return.
  if (!theBits || (0 == iRect.width) || (0 == iRect.height)) {
    anImage->UnlockImagePixels(PR_FALSE);
    return;
  }

  // Save the current graphic state and define a PS variable that
  // can hold one line of pixel data.
  fprintf(mScriptFP, "gsave\n/rowdata %d string def\n",
     mPrintSetup->color ? iRect.width * 3 : iRect.width);
 
  // Translate the coordinate origin to the corner of the rectangle where
  // the image should appear, set up a clipping region, and scale the
  // coordinate system to the image's final size.
  translate(dRect.x, dRect.y);
  box(0, 0, dRect.width, dRect.height);
  clip();
  fprintf(mScriptFP, "%d %d scale\n", dRect.width, dRect.height);

  // Describe how the pixel data is to be interpreted: pixels per row,
  // rows, and bits per pixel (per component in color).
  fprintf(mScriptFP, "%d %d 8 ", iRect.width, iRect.height);

  // Output the transformation matrix for the image. This is a bit tricky
  // to understand. PS image-drawing operations involve two transformation
  // matrices: (1) A Transformation matrix associated with the image
  // describes how to map the pixel data (width x height) onto a unit square,
  // and (2) the document's TM maps the unit square to the desired size and
  // position. The image TM is technically an inverse TM, i.e. it describes
  // how to map the unit square onto the pixel array.
  //
  // sRect and dRect define the same rectangle, only in different coordinate
  // systems. Following the translate & scale operations above, the origin
  // is at [sRect.x, sRect.y]. The "real" origin of image space is thus at
  // [-sRect.x, -sRect.y] and the pixel data should start at
  // [-sRect.x + iRect.x, -sRect.y + iRect.y]. These are negated because the
  // TM is supposed to be an inverse TM.
  nscoord tmTX = sRect.x - iRect.x;
  nscoord tmTY = sRect.y - iRect.y;

  // In document space, the target rectangle is [dRect.width,
  // dRect.height]; in image space it's [sRect.width, sRect.height]. So
  // the proper scale factor is [1/sRect.width, 1/sRect.height], but
  // again, the output should be an inverse TM so these are inverted.
  nscoord tmSX = sRect.width;
  nscoord tmSY = sRect.height;

  // If the image is being stretched and clipped, it's possible that only
  // a fraction of a pixel is supposed to be visible, resulting in a 
  // dimension of zero after rounding. 0 isn't a valid scaling factor.
  // But something should appear on the page, unlike the case where dRect
  // or iRect is zero-sized, so force the dimension to 1. See bug 236801.
  if (0 == tmSX)
    tmSX = 1;
  if (0 == tmSY)
    tmSY = 1;

  // If the image data is in the wrong order, invert the TM, causing
  // the image to be drawn inverted.
  if (!anImage->GetIsRowOrderTopToBottom()) {
    tmTY += tmSY;
    tmSY = -tmSY;
  }
  fprintf(mScriptFP, "[ %d 0 0 %d %d %d ]\n", tmSX, tmSY, tmTX, tmTY);

  // Output the data-reading procedure and the appropriate image command.
  fputs(" { currentfile rowdata readhexstring pop }", mScriptFP);
  if (mPrintSetup->color)
    fputs(" false 3 colorimage\n", mScriptFP);
  else
    fputs(" image\n", mScriptFP);

  // Output the image data. The entire image is written, even
  // if it's partially clipped in the document.
  int outputCount = 0;
  PRInt32 bytesPerRow = anImage->GetLineStride();

  for (nscoord y = 0; y < iRect.height; y++) {
    // calculate the starting point for this row of pixels
    PRUint8 *row = theBits                // Pixel buffer start
      + y * bytesPerRow;                  // Rows already output

    for (nscoord x = 0; x < iRect.width; x++) {
      PRUint8 *pixel = row + (x * 3);
      if (mPrintSetup->color)
        outputCount +=
          fprintf(mScriptFP, "%02x%02x%02x", pixel[0], pixel[1], pixel[2]);
      else
        outputCount +=
          fprintf(mScriptFP, "%02x",
	      NS_RGB_TO_GRAY(pixel[0], pixel[1], pixel[2]));
      if (outputCount >= 72) {
        fputc('\n', mScriptFP);
        outputCount = 0;
      }
    }
  }
  anImage->UnlockImagePixels(PR_FALSE);

  // Free the PS data buffer and restore the previous graphics state.
  fputs("\n/rowdata where { /rowdata undef } if\n", mScriptFP);
  fputs("grestore\n", mScriptFP);
}


/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::setcolor(nscolor aColor)
{
float greyBrightness;

  /* For greyscale postscript, find the average brightness of red, green, and
   * blue.  Using this average value as the brightness for red, green, and
   * blue is a simple way to make the postscript greyscale instead of color.
   */

  if(mPrintSetup->color == PR_FALSE ) {
    greyBrightness=NS_PS_GRAY(NS_RGB_TO_GRAY(NS_GET_R(aColor),
                                             NS_GET_G(aColor),
                                             NS_GET_B(aColor)));
    fprintf(mScriptFP, "%s setgray\n", fpCString(greyBrightness).get());
  } else {
    fprintf(mScriptFP, "%s %s %s setrgbcolor\n",
      fpCString(NS_PS_RED(aColor)).get(),
      fpCString(NS_PS_GREEN(aColor)).get(),
      fpCString(NS_PS_BLUE(aColor)).get());
  }

}


void nsPostScriptObj::setfont(const nsCString aFontName, PRUint32 aHeight)
{
  fprintf(mScriptFP, "%d /%s Msf\n", aHeight, aFontName.get());
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/98 dwc
 */
void 
nsPostScriptObj::setscriptfont(PRInt16 aFontIndex,const nsString &aFamily,nscoord aHeight, PRUint8 aStyle, 
											 PRUint8 aVariant, PRUint16 aWeight, PRUint8 decorations)
{
int postscriptFont = 0;

  fprintf(mScriptFP, "%d", aHeight);
 
  if( aFontIndex >= 0) {
    postscriptFont = aFontIndex;
  } else {
  

  //#ifdef NOTNOW
  //XXX:PS Add bold, italic and other settings here
	switch(aStyle){
	  case NS_FONT_STYLE_NORMAL :
		  if (NS_IS_BOLD(aWeight)) {
		    postscriptFont = 1;   // TIMES NORMAL BOLD
      }else{
        postscriptFont = 0; // Times ROMAN Normal
		  }
	  break;

	  case NS_FONT_STYLE_ITALIC:
		  if (NS_IS_BOLD(aWeight)) {		  
		    postscriptFont = 2; // TIMES BOLD ITALIC
      }else{			  
		    postscriptFont = 3; // TIMES ITALIC
		  }
	  break;

	  case NS_FONT_STYLE_OBLIQUE:
		  if (NS_IS_BOLD(aWeight)) {	
        postscriptFont = 6;   // HELVETICA OBLIQUE
      }else{	
        postscriptFont = 7;   // HELVETICA OBLIQUE
		  }
	    break;
	}
    //#endif
   }
   fprintf(mScriptFP, " f%d\n", postscriptFont);


#if 0
     // The style of font (normal, italic, oblique)
  PRUint8 style;

  // The variant of the font (normal, small-caps)
  PRUint8 variant;

  // The weight of the font (0-999)
  PRUint16 weight;

  // The decorations on the font (underline, overline,
  // line-through). The decorations can be binary or'd together.
  PRUint8 decorations;

  // The size of the font, in nscoord units
  nscoord size; 
#endif

}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/98 dwc
 */
void 
nsPostScriptObj::comment(const char *aTheComment)
{
  fprintf(mScriptFP, "%%%s\n", aTheComment);
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 5/30/00 katakai
 */
void
nsPostScriptObj::setlanggroup(nsIAtom * aLangGroup)
{
  gEncoder = nsnull;
  gU2Ntable = nsnull;

  if (aLangGroup == nsnull) {
    fputs("default_ls\n", mScriptFP);
    return;
  }
  nsAutoString langstr;
  aLangGroup->ToString(langstr);

  /* already exist */
  nsStringKey key(langstr);
  PS_LangGroupInfo *linfo = (PS_LangGroupInfo *) gLangGroups->Get(&key);

  if (linfo) {
    nsCAutoString str; str.AssignWithConversion(langstr);
    fprintf(mScriptFP, "%s_ls\n", str.get());
    gEncoder = linfo->mEncoder;
    gU2Ntable = linfo->mU2Ntable;
    return;
  } else {
    fputs("default_ls\n", mScriptFP);
  }
}

PRBool
nsPostScriptObj::GetUnixPrinterSetting(const nsCAutoString& aKey, char** aVal)
{

  if (!mPrinterProps) {
    return nsnull;
  }

  nsAutoString oValue;
  nsresult res = mPrinterProps->GetStringProperty(aKey, oValue);
  if (NS_FAILED(res)) {
    return PR_FALSE;
  }
  *aVal = ToNewCString(oValue);
  return PR_TRUE;
}


typedef struct _unixPrinterFallbacks_t {
    const char *key;
    const char *val;
} unixPrinterFallbacks_t;

static const unixPrinterFallbacks_t unixPrinterFallbacks[] = {
  {"print.postscript.nativefont.ja", "Ryumin-Light-EUC-H"},
  {"print.postscript.nativecode.ja", "euc-jp"},
  {nsnull, nsnull}
};

PRBool
GetUnixPrinterFallbackSetting(const nsCAutoString& aKey, char** aVal)
{
  const unixPrinterFallbacks_t *p;
  const char* key = aKey.get();
  for (p=unixPrinterFallbacks; p->key; p++) {
    if (strcmp(key, p->key) == 0) {
      *aVal = nsCRT::strdup(p->val);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}


const char* kNativeFontPrefix  = "print.postscript.nativefont.";
const char* kUnicodeFontPrefix = "print.postscript.unicodefont.";

struct PrefEnumClosure {
  FILE *handle;			// Output file handle
  nsPostScriptObj *psObj;	// Renderer
};

/* make <langgroup>_ls define for each LangGroup here */
static void PrefEnumCallback(const char *aName, void *aClosure)
{
  nsPostScriptObj *psObj = ((PrefEnumClosure *)aClosure)->psObj;
  FILE *f = ((PrefEnumClosure *)aClosure)->handle;
  nsAutoString lang; lang.AssignWithConversion(aName);


  if (strstr(aName, kNativeFontPrefix)) {
    NS_ASSERTION(strlen(kNativeFontPrefix) == 28, "Wrong hard-coded size.");
    lang.Cut(0, 28);
  } else if (strstr(aName, kUnicodeFontPrefix)) {
    NS_ASSERTION(strlen(kNativeFontPrefix) == 29, "Wrong hard-coded size.");
    lang.Cut(0, 29);
  }
  nsStringKey key(lang);

  PS_LangGroupInfo *linfo = (PS_LangGroupInfo *) gLangGroups->Get(&key);

  if (linfo) {
    /* already exist */
    return;
  }

  /* define new language group */
  nsXPIDLCString psnativefont;
  nsXPIDLCString psnativecode;
  nsXPIDLCString psunicodefont;
  int psfontorder = 0;
  PRBool use_prefsfile = PR_FALSE;
  PRBool use_vendorfile = PR_FALSE;

  //
  // Try to get the info from the user's prefs file
  //
  nsCAutoString namepsnativefont(kNativeFontPrefix);
  namepsnativefont.AppendWithConversion(lang);
  gPrefs->CopyCharPref(namepsnativefont.get(), getter_Copies(psnativefont));

  nsCAutoString namepsnativecode("print.postscript.nativecode.");
  namepsnativecode.AppendWithConversion(lang);
  gPrefs->CopyCharPref(namepsnativecode.get(), getter_Copies(psnativecode));
  if (((psnativefont)&&(*psnativefont)) && ((psnativecode)&&(*psnativecode))) {
    use_prefsfile = PR_TRUE;
  } else {
    psnativefont.Adopt(0);
    psnativecode.Adopt(0);
  }

  //
  // if not found look for info from the printer vendor
  //
  if (use_prefsfile != PR_TRUE) {
    psObj->GetUnixPrinterSetting(namepsnativefont, getter_Copies(psnativefont));
    psObj->GetUnixPrinterSetting(namepsnativecode, getter_Copies(psnativecode));
    if ((psnativefont) && (psnativecode)) {
      use_vendorfile = PR_TRUE;
    } else {
      psnativefont.Adopt(0);
      psnativecode.Adopt(0);
    }
  }
  if (!use_prefsfile && !use_vendorfile) {
    GetUnixPrinterFallbackSetting(namepsnativefont, getter_Copies(psnativefont));
    GetUnixPrinterFallbackSetting(namepsnativecode, getter_Copies(psnativecode));
  }

  /* psnativefont and psnativecode both should be set */
  if (!psnativefont || !psnativecode) {
    psnativefont.Adopt(0);
    psnativecode.Adopt(0);
  } else {
    nsCAutoString namepsfontorder("print.postscript.fontorder.");
    namepsfontorder.AppendWithConversion(lang);
    if (use_prefsfile) {
      gPrefs->GetIntPref(namepsfontorder.get(), &psfontorder);
    }
    else if (use_vendorfile) {
      nsXPIDLCString psfontorder_str;
      psObj->GetUnixPrinterSetting(namepsfontorder, getter_Copies(psfontorder_str));
      if (psfontorder_str) {
        psfontorder = atoi(psfontorder_str);
      }
    }
  }

  /* check UCS fonts */
  nsCAutoString namepsunicodefont(kUnicodeFontPrefix);
  namepsunicodefont.AppendWithConversion(lang);
  if (use_prefsfile) {
    gPrefs->CopyCharPref(namepsunicodefont.get(), getter_Copies(psunicodefont));
  } else if (use_vendorfile) {
    psObj->GetUnixPrinterSetting(namepsunicodefont, getter_Copies(psunicodefont));
  }

  nsresult res = NS_OK;

  if (psnativefont || psunicodefont) {
    linfo = new PS_LangGroupInfo;
    linfo->mEncoder = nsnull;
    linfo->mU2Ntable = nsnull;

    if (psnativecode) {
      nsAutoString str;
      nsCOMPtr<nsICharsetConverterManager> ccMain =
        do_GetService(kCharsetConverterManagerCID, &res);
      
      if (NS_SUCCEEDED(res)) {
        res = ccMain->GetUnicodeEncoder(psnativecode.get(), &linfo->mEncoder);
      }
    }

    gLangGroups->Put(&key, (void *) linfo);

    nsCAutoString langstrC; langstrC.AssignWithConversion(lang);
    if (psnativefont && linfo->mEncoder) {
      fprintf(f, "/Unicode2NativeDict%s 0 dict def\n", langstrC.get());
    }

    fprintf(f, "/%s_ls {\n", langstrC.get());
    fprintf(f, "  /NativeFont /%s def\n",
      (psnativefont && linfo->mEncoder) ? psnativefont.get() : "Courier");
    fprintf(f, "  /UCS2Font /%s def\n",
		  psunicodefont ? psunicodefont.get() : "Courier");
    if (psnativefont && linfo->mEncoder) {
      fprintf(f, "  /Unicode2NativeDict Unicode2NativeDict%s def\n",
		    langstrC.get());
    }

    if (psfontorder) {
      fprintf(f, "  /unicodeshow1 { real_unicodeshow_native } bind def\n");
      fprintf(f, "  /unicodeshow2 { real_unicodeshow } bind def\n");
    } else {
      fprintf(f, "  /unicodeshow1 { real_unicodeshow } bind def\n");
      fprintf(f, "  /unicodeshow2 { real_unicodeshow_native } bind def\n");
    }

    fprintf(f, "} bind def\n");

    if (linfo->mEncoder) {
      linfo->mEncoder->SetOutputErrorBehavior(
		    linfo->mEncoder->kOnError_Replace, nsnull, '?');
      linfo->mU2Ntable = new nsHashtable();
    }
  }
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 5/30/00 katakai
 */
void
nsPostScriptObj::initlanggroup(FILE *aHandle)
{
  PrefEnumClosure closure;
  closure.handle = aHandle;
  closure.psObj = this;

  /* check langgroup of preference */
  gPrefs->EnumerateChildren(kNativeFontPrefix,
      PrefEnumCallback, (void *) &closure);

  gPrefs->EnumerateChildren(kUnicodeFontPrefix,
      PrefEnumCallback, (void *) &closure);
}


 /** ---------------------------------------------------
  *  See documentation in nsPostScriptObj.h
  *  @update 3/6/2004 kherron
  *  @update 3/25/2004 dantifer
  */
nsresult
nsPostScriptObj::render_eps(const nsRect& aRect, nsEPSObjectPS &anEPS)
{
  FILE     *bfile = mScriptFP;
  nsresult  rv;

  NS_PRECONDITION(nsnull != bfile, "No document body file handle");

  /* Set up EPSF state. See Adobe spec #5002 section 3.2 */
  fputs(
    "/b4_Inc_state save def\n"
    "/dict_count countdictstack def\n"
    "/op_count count 1 sub def\n"
    "userdict begin\n"
    "/showpage { } def\n"
    "0 setgray 0 setlinecap 1 setlinewidth 0 setlinejoin\n"
    "10 setmiterlimit [ ] 0 setdash newpath\n"
    "/languagelevel where\n"
    "{pop languagelevel\n"
    "  1 ne\n"
    "  {false setstrokeadjust false setoverprint\n"
    "  } if\n"
    "} if\n",
    bfile);

  /* Set up a clipping region around the EPS rectangle */
  box(aRect.x, aRect.y, aRect.width, aRect.height);
  clip();

  /* translate to the lower left corner of the rectangle */
  translate(aRect.x, aRect.y + aRect.height);

  /* Rescale */
  scale(
    aRect.width / (anEPS.GetBoundingBoxURX() - anEPS.GetBoundingBoxLLX()),
    -(aRect.height / (anEPS.GetBoundingBoxURY() - anEPS.GetBoundingBoxLLY()))
  );

  /* Translate to the EPSF origin. Can't use translate() here because
  * it takes integers.
  */
  fprintf(bfile, "%s %s translate\n",
    fpCString(-anEPS.GetBoundingBoxLLX()).get(),
    fpCString(-anEPS.GetBoundingBoxLLY()).get()
  );

  /* embeding EPS file content */
  comment("%BeginDocument: Mozilla-Internal");
  rv = anEPS.WriteTo(bfile);
  comment("%EndDocument");

  /* Restore previous state */
  fputs(
    "count op_count sub { pop } repeat\n"
    "countdictstack dict_count sub { end } repeat\n"
    "b4_Inc_state restore\n",
    bfile);

  return rv;
}
