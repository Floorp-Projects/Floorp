/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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

#ifndef NS_BUILD_ID
#include "nsBuildID.h"
#endif /* !NS_BUILD_ID */

#include "prenv.h"
#include "prprf.h"
#include "prerror.h"

#include <locale.h>
#include <errno.h>

#ifdef VMS
#include <stdlib.h>
#endif /* VMS */

#ifdef MOZ_ENABLE_FREETYPE2
#include "nsType8.h"
#endif

#ifdef PR_LOGGING
static PRLogModuleInfo *nsPostScriptObjLM = PR_NewLogModule("nsPostScriptObj");
#endif /* PR_LOGGING */

// These set the location to standard C and back
// which will keep the "." from converting to a "," 
// in certain languages for floating point output to postscript
#define XL_SET_NUMERIC_LOCALE() char* cur_locale = setlocale(LC_NUMERIC, "C")
#define XL_RESTORE_NUMERIC_LOCALE() setlocale(LC_NUMERIC, cur_locale)

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
  mTitle(nsnull)
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

  // The mPrintContext can be null
  // if  opening the PostScript document
  // fails.  Giving an invalid path, relative path
  // or a directory which the user does not have
  // write permissions for will fail to open a document
  // see bug 85535 
  if (mPrintContext) {
    if (mPrintSetup->out) {
      fclose(mPrintSetup->out);
      mPrintSetup->out = nsnull;
    }
    if (mPrintSetup->tmpBody) {
      fclose(mPrintSetup->tmpBody);
      mPrintSetup->tmpBody = nsnull;
    }  
  
    finalize_translation();
  }

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

  if (mDocProlog)
    mDocProlog->Remove(PR_FALSE);
  if (mDocScript)
    mDocScript->Remove(PR_FALSE);

  PR_LOG(nsPostScriptObjLM, PR_LOG_DEBUG, ("nsPostScriptObj::~nsPostScriptObj(): printing done."));
}

void 
nsPostScriptObj::settitle(PRUnichar * aTitle)
{
  if (aTitle) {
    mTitle = ToNewCString(nsDependentString(aTitle));
  }
}

static
const PSPaperSizeRec *paper_name_to_PSPaperSizeRec(const char *paper_name)
{
  int i;
        
  for( i = 0 ; postscript_module_paper_sizes[i].name != nsnull ; i++ )
  {
    const PSPaperSizeRec *curr = &postscript_module_paper_sizes[i];

    if (!PL_strcasecmp(paper_name, curr->name))
      return curr;
  }

  return nsnull;
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
nsresult 
nsPostScriptObj::Init( nsIDeviceContextSpecPS *aSpec )
{
  PRBool      isGray,
              isAPrinter,
              isFirstPageFirst;
  int         landscape;
  float       fwidth, fheight;
  const char *printername;
  nsresult    rv;

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

      /* Find PS paper size record by name */
      const char *paper_name = nsnull;
      aSpec->GetPaperName(&paper_name);    
      mPrintSetup->paper_size = paper_name_to_PSPaperSizeRec(paper_name);

      if (!mPrintSetup->paper_size)
        return NS_ERROR_GFX_PRINTER_PAPER_SIZE_NOT_SUPPORTED;

      // Clean up tempfile remnants of any previous print job
      if (mDocProlog)
        mDocProlog->Remove(PR_FALSE);
      if (mDocScript)
        mDocScript->Remove(PR_FALSE);
               
      aSpec->GetToPrinter( isAPrinter );
      if (isAPrinter) {
        /* Define the destination printer (queue). 
         * We assume that the print command is set to 
         * "lpr ${MOZ_PRINTER_NAME:+'-P'}${MOZ_PRINTER_NAME}" 
         * - which means that if the ${MOZ_PRINTER_NAME} env var is not empty
         * the "-P" option of lpr will be set to the printer name.
         */

        /* get printer name */
        aSpec->GetPrinterName(&printername);
        
        /* do not set the ${MOZ_PRINTER_NAME} env var if we want the default 
         * printer */
        if (printername)
        {
          /* strip the leading NS_POSTSCRIPT_DRIVER_NAME string */
          printername = printername + NS_POSTSCRIPT_DRIVER_NAME_LEN;
          
          if (!strcmp(printername, "default"))
            printername = "";
        }
        else 
          printername = "";

        /* Construct an environment string MOZ_PRINTER_NAME=<printername>
         * and add it to the environment.
         * On a POSIX system the original buffer becomes part of the
         * environment, so it must remain valid until replaced. To preserve
         * the ability to unload shared libraries, we have to either remove
         * the string from the environment at unload time or else store the
         * string in the heap, where it'll be left behind after unloading
         * the library.
         */
        static char *moz_printer_string;
        char *old_printer_string = moz_printer_string;
        moz_printer_string = PR_smprintf("MOZ_PRINTER_NAME=%s", printername);
#ifdef DEBUG
        printf("setting '%s'\n", moz_printer_string);
#endif

        if (!moz_printer_string) {
          /* We're probably out of memory */
          moz_printer_string = old_printer_string;
          return (PR_OUT_OF_MEMORY_ERROR == PR_GetError()) ? 
            NS_ERROR_OUT_OF_MEMORY : NS_ERROR_UNEXPECTED;
        }
        else {
          PR_SetEnv(moz_printer_string);
          if (old_printer_string)
            PR_smprintf_free(old_printer_string);
        }

        aSpec->GetCommand(&mPrintSetup->print_cmd);
        // Create a temporary file for the document prolog
        rv = mTempfileFactory.CreateTempFile(getter_AddRefs(mDocProlog),
            &mPrintSetup->out, "w+");
        NS_ENSURE_SUCCESS(rv, NS_ERROR_GFX_PRINTER_FILE_IO_ERROR);
        NS_POSTCONDITION(nsnull != mPrintSetup->out,
          "CreateTempFile succeeded but no file handle");

      } else {
        const char *path;
        aSpec->GetPath(&path);
        rv = NS_NewNativeLocalFile(nsDependentCString(path),
          PR_FALSE, getter_AddRefs(mDocProlog));
        rv = mDocProlog->OpenANSIFileDesc("w", &mPrintSetup->out);
        if (NS_FAILED(rv))
          return NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE;
        NS_POSTCONDITION(nsnull != mPrintSetup->out,
          "OpenANSIFileDesc succeeded but no file handle");
        mPrintSetup->print_cmd = NULL;	// Indicate print-to-file
      }

      // Open the temporary file for the document script (printable content)
      rv = mTempfileFactory.CreateTempFile(getter_AddRefs(mDocScript),
          &mPrintSetup->tmpBody, "w+");
      if (NS_FAILED(rv)) {
        fclose(mPrintSetup->out);
        mPrintSetup->out = nsnull;
        mDocProlog->Remove(PR_FALSE);
        mDocProlog = nsnull;
        return NS_ERROR_GFX_PRINTER_FILE_IO_ERROR;
      }
      NS_POSTCONDITION(nsnull != mPrintSetup->tmpBody,
        "CreateTempFile succeeded but no file handle");
    } else 
        return NS_ERROR_FAILURE;

    /* make sure the open worked */

    if (!mPrintSetup->out)
      return NS_ERROR_GFX_PRINTER_CMD_FAILURE;
      
    mPrintContext = new PSContext();
    memset(mPrintContext, 0, sizeof(struct PSContext_));
    memset(pi, 0, sizeof(struct PrintInfo_));

    mPrintSetup->dpi = 72.0f;   // dpi for externally sized items 
    aSpec->GetLandscape( landscape );
    fwidth  = mPrintSetup->paper_size->width;
    fheight = mPrintSetup->paper_size->height;

    if (landscape) {
      float temp;
      temp   = fwidth;
      fwidth = fheight;
      fheight = temp;
    }

    mPrintSetup->left   = NSToCoordRound(mPrintSetup->paper_size->left
                            * mPrintSetup->dpi * TWIPS_PER_POINT_FLOAT);
    mPrintSetup->top    = NSToCoordRound(mPrintSetup->paper_size->top
                            * mPrintSetup->dpi * TWIPS_PER_POINT_FLOAT);
    mPrintSetup->bottom = NSToCoordRound(mPrintSetup->paper_size->bottom
                            * mPrintSetup->dpi * TWIPS_PER_POINT_FLOAT);
    mPrintSetup->right  = NSToCoordRound(mPrintSetup->paper_size->right
                            * mPrintSetup->dpi * TWIPS_PER_POINT_FLOAT);
    
    mPrintSetup->width  = NSToCoordRound(fwidth
                            * mPrintSetup->dpi * TWIPS_PER_POINT_FLOAT);
    mPrintSetup->height = NSToCoordRound(fheight
                            * mPrintSetup->dpi * TWIPS_PER_POINT_FLOAT);
#ifdef DEBUG
    printf("\nPreWidth = %f PreHeight = %f\n",fwidth,fheight);
    printf("\nWidth = %d Height = %d\n",mPrintSetup->width,mPrintSetup->height);
#endif
    mPrintSetup->header = "header";
    mPrintSetup->footer = "footer";
    mPrintSetup->sizes = nsnull;

    mPrintSetup->landscape = (landscape) ? PR_TRUE : PR_FALSE; // Rotated output 
    //mPrintSetup->landscape = PR_FALSE;

    mPrintSetup->underline = PR_TRUE;             // underline links 
    mPrintSetup->scale_images = PR_TRUE;          // Scale unsized images which are too big 
    mPrintSetup->scale_pre = PR_FALSE;		        // do the pre-scaling thing 
    // scale margins (specified in inches) to dots.

    PR_LOG(nsPostScriptObjLM, PR_LOG_DEBUG, ("dpi %g top %d bottom %d left %d right %d\n", 
           mPrintSetup->dpi, mPrintSetup->top, mPrintSetup->bottom, mPrintSetup->left, mPrintSetup->right));

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

    pi->page_height = mPrintSetup->height;	// Size of printable area on page 
    pi->page_width = mPrintSetup->width;	// Size of printable area on page 
    pi->page_break = 0;	              // Current page bottom 
    pi->page_topy = 0;	              // Current page top 
    pi->phase = 0;

 
    pi->pages = nsnull;		                // Contains extents of each page 

    pi->pt_size = 0;		              // Size of above table 
    pi->n_pages = 0;	        	      // # of valid entries in above table 

    mTitle = nsnull;

    pi->doc_title = mTitle;
    pi->doc_width = 0;	              // Total document width 
    pi->doc_height = 0;	              // Total document height 

    mPrintContext->prInfo = pi;

    // begin the document
    initialize_translation(mPrintSetup);

    begin_document();	
    mPageNumber = 1;                  // we are on the first page
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
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::begin_document()
{
int i;
FILE *f;

  nscoord paper_width = mPrintContext->prSetup->left
    + mPrintContext->prSetup->width + mPrintContext->prSetup->right;
  nscoord paper_height = mPrintContext->prSetup->bottom
    + mPrintContext->prSetup->height + mPrintContext->prSetup->top;
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

  XL_SET_NUMERIC_LOCALE();
  f = mPrintContext->prSetup->out;
  fprintf(f, "%%!PS-Adobe-3.0\n");
  fprintf(f, "%%%%BoundingBox: %g %g %g %g\n",
    NSTwipsToFloatPoints(mPrintContext->prSetup->left),
    NSTwipsToFloatPoints(mPrintContext->prSetup->bottom),
    NSTwipsToFloatPoints(paper_width - mPrintContext->prSetup->right),
    NSTwipsToFloatPoints(paper_height - mPrintContext->prSetup->top));

  fprintf(f, "%%%%Creator: Mozilla PostScript module (%s/%lu)\n",
             "rv:" MOZILLA_VERSION, (unsigned long)NS_BUILD_ID);
  fprintf(f, "%%%%DocumentData: Clean8Bit\n");
  fprintf(f, "%%%%DocumentPaperSizes: %s\n", mPrintSetup->paper_size->name);
  fprintf(f, "%%%%Orientation: %s\n", orientation);

  // hmm, n_pages is always zero so don't use it
#if 0
  fprintf(f, "%%%%Pages: %d\n", (int) mPrintContext->prInfo->n_pages);
#else
  fprintf(f, "%%%%Pages: (atend)\n");
#endif

  if (mPrintContext->prSetup->reverse)
	  fprintf(f, "%%%%PageOrder: Descend\n");
  else
	  fprintf(f, "%%%%PageOrder: Ascend\n");

  if (nsnull != mPrintContext->prInfo->doc_title) {
    // DSC spec: max line length is 255 characters
    fprintf(f, "%%%%Title: ");
    PrintAsDSCTextline(f, mPrintContext->prInfo->doc_title, 230);
    fprintf(f, "\n");
  }

#ifdef NOTYET
  fprintf(f, "%%%%For: %n", user_name_stuff);
#endif
  fprintf(f, "%%%%EndComments\n");

  // general comments: Mozilla-specific 
#ifdef NOTYET
  fprintf(f, "\n%% MozillaURL: %s\n", mPrintContext->prSetup->url->address);
#endif
  fprintf(f, "%% MozillaCharsetName: iso-8859-1\n\n");
    
    // now begin prolog 
  fprintf(f, "%%%%BeginProlog\n");

  // Tell the printer what size paper it should use
  fprintf(f,
    "/setpagedevice where\n"			// Test for the feature
    "{ pop 2 dict\n"				// Set up a dictionary
    "  dup /PageSize [ %g %g ] put\n"		// Paper dimensions
    "  dup /ImagingBBox [ %g %g %g %g ] put\n"	// Bounding box
    "  setpagedevice\n"				// Install settings
    "} if\n", 
    NSTwipsToFloatPoints(paper_width),
    NSTwipsToFloatPoints(paper_height),
    NSTwipsToFloatPoints(mPrintContext->prSetup->left),
    NSTwipsToFloatPoints(mPrintContext->prSetup->bottom),
    NSTwipsToFloatPoints(paper_width - mPrintContext->prSetup->right),
    NSTwipsToFloatPoints(paper_height - mPrintContext->prSetup->top));

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
    "/UniDict    <<\n"
    "16#0020    /space\n"
    "16#0021    /exclam\n"
    "16#0022    /quotedbl\n"
    "16#0023    /numbersign\n"
    "16#0024    /dollar\n"
    "16#0025    /percent\n"
    "16#0026    /ampersand\n"
    "16#0027    /quotesingle\n"
    "16#0028    /parenleft\n"
    "16#0029    /parenright\n"
    "16#002A    /asterisk\n"
    "16#002B    /plus\n"
    "16#002C    /comma\n"
    "16#002D    /hyphen\n"
    "16#002E    /period\n"
    "16#002F    /slash\n"
    "16#0030    /zero\n"
    "16#0031    /one\n"
    "16#0032    /two\n"
    "16#0033    /three\n"
    "16#0034    /four\n"
    "16#0035    /five\n"
    "16#0036    /six\n"
    "16#0037    /seven\n"
    "16#0038    /eight\n"
    "16#0039    /nine\n"
    "16#003A    /colon\n"
    "16#003B    /semicolon\n"
    "16#003C    /less\n"
    "16#003D    /equal\n"
    "16#003E    /greater\n"
    "16#003F    /question\n"
    "16#0040    /at\n"
    "16#0041    /A\n"
    "16#0042    /B\n"
    "16#0043    /C\n"
    "16#0044    /D\n"
    "16#0045    /E\n"
    "16#0046    /F\n"
    "16#0047    /G\n"
    "16#0048    /H\n"
    "16#0049    /I\n"
    "16#004A    /J\n"
    "16#004B    /K\n"
    "16#004C    /L\n"
    "16#004D    /M\n"
    "16#004E    /N\n"
    "16#004F    /O\n"
    "16#0050    /P\n"
    "16#0051    /Q\n"
    "16#0052    /R\n"
    "16#0053    /S\n"
    "16#0054    /T\n"
    "16#0055    /U\n"
    "16#0056    /V\n"
    "16#0057    /W\n"
    "16#0058    /X\n"
    "16#0059    /Y\n"
    "16#005A    /Z\n"
    "16#005B    /bracketleft\n"
    "16#005C    /backslash\n"
    "16#005D    /bracketright\n"
    "16#005E    /asciicircum\n"
    "16#005F    /underscore\n"
    "16#0060    /grave\n"
    "16#0061    /a\n"
    "16#0062    /b\n"
    "16#0063    /c\n"
    "16#0064    /d\n"
    "16#0065    /e\n"
    "16#0066    /f\n"
    "16#0067    /g\n"
    "16#0068    /h\n"
    "16#0069    /i\n"
    "16#006A    /j\n"
    "16#006B    /k\n"
    "16#006C    /l\n"
    "16#006D    /m\n"
    "16#006E    /n\n"
    "16#006F    /o\n"
    "16#0070    /p\n"
    "16#0071    /q\n"
    "16#0072    /r\n"
    "16#0073    /s\n"
    "16#0074    /t\n"
    "16#0075    /u\n"
    "16#0076    /v\n"
    "16#0077    /w\n"
    "16#0078    /x\n"
    "16#0079    /y\n"
    "16#007A    /z\n"
    "16#007B    /braceleft\n"
    "16#007C    /bar\n"
    "16#007D    /braceright\n"
    "16#007E    /asciitilde\n"
    "16#00A0    /space\n"
    "16#00A1    /exclamdown\n"
    "16#00A2    /cent\n"
    "16#00A3    /sterling\n"
    "16#00A4    /currency\n"
    "16#00A5    /yen\n"
    "16#00A6    /brokenbar\n"
    "16#00A7    /section\n"
    "16#00A8    /dieresis\n"
    "16#00A9    /copyright\n"
    "16#00AA    /ordfeminine\n"
    "16#00AB    /guillemotleft\n"
    "16#00AC    /logicalnot\n"
    "16#00AD    /hyphen\n"
    "16#00AE    /registered\n"
    "16#00AF    /macron\n"
    "16#00B0    /degree\n"
    "16#00B1    /plusminus\n"
    "16#00B2    /twosuperior\n"
    "16#00B3    /threesuperior\n"
    "16#00B4    /acute\n"
    "16#00B5    /mu\n"
    "16#00B6    /paragraph\n"
    "16#00B7    /periodcentered\n"
    "16#00B8    /cedilla\n"
    "16#00B9    /onesuperior\n"
    "16#00BA    /ordmasculine\n"
    "16#00BB    /guillemotright\n"
    "16#00BC    /onequarter\n"
    "16#00BD    /onehalf\n"
    "16#00BE    /threequarters\n"
    "16#00BF    /questiondown\n"
    "16#00C0    /Agrave\n"
    "16#00C1    /Aacute\n"
    "16#00C2    /Acircumflex\n"
    "16#00C3    /Atilde\n"
    "16#00C4    /Adieresis\n"
    "16#00C5    /Aring\n"
    "16#00C6    /AE\n"
    "16#00C7    /Ccedilla\n"
    "16#00C8    /Egrave\n"
    "16#00C9    /Eacute\n"
    "16#00CA    /Ecircumflex\n"
    "16#00CB    /Edieresis\n"
    "16#00CC    /Igrave\n"
    "16#00CD    /Iacute\n"
    "16#00CE    /Icircumflex\n"
    "16#00CF    /Idieresis\n"
    "16#00D0    /Eth\n"
    "16#00D1    /Ntilde\n"
    "16#00D2    /Ograve\n"
    "16#00D3    /Oacute\n"
    "16#00D4    /Ocircumflex\n"
    "16#00D5    /Otilde\n"
    "16#00D6    /Odieresis\n"
    "16#00D7    /multiply\n"
    "16#00D8    /Oslash\n"
    "16#00D9    /Ugrave\n"
    "16#00DA    /Uacute\n"
    "16#00DB    /Ucircumflex\n"
    "16#00DC    /Udieresis\n"
    "16#00DD    /Yacute\n"
    "16#00DE    /Thorn\n"
    "16#00DF    /germandbls\n"
    "16#00E0    /agrave\n"
    "16#00E1    /aacute\n"
    "16#00E2    /acircumflex\n"
    "16#00E3    /atilde\n"
    "16#00E4    /adieresis\n"
    "16#00E5    /aring\n"
    "16#00E6    /ae\n"
    "16#00E7    /ccedilla\n"
    "16#00E8    /egrave\n"
    "16#00E9    /eacute\n"
    "16#00EA    /ecircumflex\n"
    "16#00EB    /edieresis\n"
    "16#00EC    /igrave\n"
    "16#00ED    /iacute\n"
    "16#00EE    /icircumflex\n"
    "16#00EF    /idieresis\n"
    "16#00F0    /eth\n"
    "16#00F1    /ntilde\n"
    "16#00F2    /ograve\n"
    "16#00F3    /oacute\n"
    "16#00F4    /ocircumflex\n"
    "16#00F5    /otilde\n"
    "16#00F6    /odieresis\n"
    "16#00F7    /divide\n"
    "16#00F8    /oslash\n"
    "16#00F9    /ugrave\n"
    "16#00FA    /uacute\n"
    "16#00FB    /ucircumflex\n"
    "16#00FC    /udieresis\n"
    "16#00FD    /yacute\n"
    "16#00FE    /thorn\n"
    "16#00FF    /ydieresis\n"
    "16#0100    /Amacron\n"
    "16#0101    /amacron\n"
    "16#0102    /Abreve\n"
    "16#0103    /abreve\n"
    "16#0104    /Aogonek\n"
    "16#0105    /aogonek\n"
    "16#0106    /Cacute\n"
    "16#0107    /cacute\n"
    "16#0108    /Ccircumflex\n"
    "16#0109    /ccircumflex\n"
    "16#010A    /Cdotaccent\n"
    "16#010B    /cdotaccent\n"
    "16#010C    /Ccaron\n"
    "16#010D    /ccaron\n"
    "16#010E    /Dcaron\n"
    "16#010F    /dcaron\n"
    "16#0110    /Dcroat\n"
    "16#0111    /dcroat\n"
    "16#0112    /Emacron\n"
    "16#0113    /emacron\n"
    "16#0114    /Ebreve\n"
    "16#0115    /ebreve\n"
    "16#0116    /Edotaccent\n"
    "16#0117    /edotaccent\n"
    "16#0118    /Eogonek\n"
    "16#0119    /eogonek\n"
    "16#011A    /Ecaron\n"
    "16#011B    /ecaron\n"
    "16#011C    /Gcircumflex\n"
    "16#011D    /gcircumflex\n"
    "16#011E    /Gbreve\n"
    "16#011F    /gbreve\n"
    "16#0120    /Gdotaccent\n"
    "16#0121    /gdotaccent\n"
    "16#0122    /Gcommaaccent\n"
    "16#0123    /gcommaaccent\n"
    "16#0124    /Hcircumflex\n"
    "16#0125    /hcircumflex\n"
    "16#0126    /Hbar\n"
    "16#0127    /hbar\n"
    "16#0128    /Itilde\n"
    "16#0129    /itilde\n"
    "16#012A    /Imacron\n"
    "16#012B    /imacron\n"
    "16#012C    /Ibreve\n"
    "16#012D    /ibreve\n"
    "16#012E    /Iogonek\n"
    "16#012F    /iogonek\n"
    "16#0130    /Idotaccent\n"
    "16#0131    /dotlessi\n"
    "16#0132    /IJ\n"
    "16#0133    /ij\n"
    "16#0134    /Jcircumflex\n"
    "16#0135    /jcircumflex\n"
    "16#0136    /Kcommaaccent\n"
    "16#0137    /kcommaaccent\n"
    "16#0138    /kgreenlandic\n"
    "16#0139    /Lacute\n"
    "16#013A    /lacute\n"
    "16#013B    /Lcommaaccent\n"
    "16#013C    /lcommaaccent\n"
    "16#013D    /Lcaron\n"
    "16#013E    /lcaron\n"
    "16#013F    /Ldot\n"
    "16#0140    /ldot\n"
    "16#0141    /Lslash\n"
    "16#0142    /lslash\n"
    "16#0143    /Nacute\n"
    "16#0144    /nacute\n"
    "16#0145    /Ncommaaccent\n"
    "16#0146    /ncommaaccent\n"
    "16#0147    /Ncaron\n"
    "16#0148    /ncaron\n"
    "16#0149    /napostrophe\n"
    "16#014A    /Eng\n"
    "16#014B    /eng\n"
    "16#014C    /Omacron\n"
    "16#014D    /omacron\n"
    "16#014E    /Obreve\n"
    "16#014F    /obreve\n"
    "16#0150    /Ohungarumlaut\n"
    "16#0151    /ohungarumlaut\n"
    "16#0152    /OE\n"
    "16#0153    /oe\n"
    "16#0154    /Racute\n"
    "16#0155    /racute\n"
    "16#0156    /Rcommaaccent\n"
    "16#0157    /rcommaaccent\n"
    "16#0158    /Rcaron\n"
    "16#0159    /rcaron\n"
    "16#015A    /Sacute\n"
    "16#015B    /sacute\n"
    "16#015C    /Scircumflex\n"
    "16#015D    /scircumflex\n"
    "16#015E    /Scedilla\n"
    "16#015F    /scedilla\n"
    "16#0160    /Scaron\n"
    "16#0161    /scaron\n"
    "16#0162    /Tcommaaccent\n"
    "16#0163    /tcommaaccent\n"
    "16#0164    /Tcaron\n"
    "16#0165    /tcaron\n"
    "16#0166    /Tbar\n"
    "16#0167    /tbar\n"
    "16#0168    /Utilde\n"
    "16#0169    /utilde\n"
    "16#016A    /Umacron\n"
    "16#016B    /umacron\n"
    "16#016C    /Ubreve\n"
    "16#016D    /ubreve\n"
    "16#016E    /Uring\n"
    "16#016F    /uring\n"
    "16#0170    /Uhungarumlaut\n"
    "16#0171    /uhungarumlaut\n"
    "16#0172    /Uogonek\n"
    "16#0173    /uogonek\n"
    "16#0174    /Wcircumflex\n"
    "16#0175    /wcircumflex\n"
    "16#0176    /Ycircumflex\n"
    "16#0177    /ycircumflex\n"
    "16#0178    /Ydieresis\n"
    "16#0179    /Zacute\n"
    "16#017A    /zacute\n"
    "16#017B    /Zdotaccent\n"
    "16#017C    /zdotaccent\n"
    "16#017D    /Zcaron\n"
    "16#017E    /zcaron\n"
    "16#017F    /longs\n"
    "16#0192    /florin\n"
    "16#01A0    /Ohorn\n"
    "16#01A1    /ohorn\n"
    "16#01AF    /Uhorn\n"
    "16#01B0    /uhorn\n"
    "16#01E6    /Gcaron\n"
    "16#01E7    /gcaron\n"
    "16#01FA    /Aringacute\n"
    "16#01FB    /aringacute\n"
    "16#01FC    /AEacute\n"
    "16#01FD    /aeacute\n"
    "16#01FE    /Oslashacute\n"
    "16#01FF    /oslashacute\n"
    "16#0218    /Scommaaccent\n"
    "16#0219    /scommaaccent\n"
    "16#021A    /Tcommaaccent\n"
    "16#021B    /tcommaaccent\n"
    "16#02BC    /afii57929\n"
    "16#02BD    /afii64937\n"
    "16#02C6    /circumflex\n"
    "16#02C7    /caron\n"
    "16#02C9    /macron\n"
    "16#02D8    /breve\n"
    "16#02D9    /dotaccent\n"
    "16#02DA    /ring\n"
    "16#02DB    /ogonek\n"
    "16#02DC    /tilde\n"
    "16#02DD    /hungarumlaut\n"
    "16#0300    /gravecomb\n"
    "16#0301    /acutecomb\n"
    "16#0303    /tildecomb\n"
    "16#0309    /hookabovecomb\n"
    "16#0323    /dotbelowcomb\n"
    "16#0384    /tonos\n"
    "16#0385    /dieresistonos\n"
    "16#0386    /Alphatonos\n"
    "16#0387    /anoteleia\n"
    "16#0388    /Epsilontonos\n"
    "16#0389    /Etatonos\n"
    "16#038A    /Iotatonos\n"
    "16#038C    /Omicrontonos\n"
    "16#038E    /Upsilontonos\n"
    "16#038F    /Omegatonos\n"
    "16#0390    /iotadieresistonos\n"
    "16#0391    /Alpha\n"
    "16#0392    /Beta\n"
    "16#0393    /Gamma\n"
    "16#0394    /Delta\n"
    "16#0395    /Epsilon\n"
    "16#0396    /Zeta\n"
    "16#0397    /Eta\n"
    "16#0398    /Theta\n"
    "16#0399    /Iota\n"
    "16#039A    /Kappa\n"
    "16#039B    /Lambda\n"
    "16#039C    /Mu\n"
    "16#039D    /Nu\n"
    "16#039E    /Xi\n"
    "16#039F    /Omicron\n"
    "16#03A0    /Pi\n"
    "16#03A1    /Rho\n"
    "16#03A3    /Sigma\n"
    "16#03A4    /Tau\n"
    "16#03A5    /Upsilon\n"
    "16#03A6    /Phi\n"
    "16#03A7    /Chi\n"
    "16#03A8    /Psi\n"
    "16#03A9    /Omega\n"
    "16#03AA    /Iotadieresis\n"
    "16#03AB    /Upsilondieresis\n"
    "16#03AC    /alphatonos\n"
    "16#03AD    /epsilontonos\n"
    "16#03AE    /etatonos\n"
    "16#03AF    /iotatonos\n"
    "16#03B0    /upsilondieresistonos\n"
    "16#03B1    /alpha\n"
    "16#03B2    /beta\n"
    "16#03B3    /gamma\n"
    "16#03B4    /delta\n"
    "16#03B5    /epsilon\n"
    "16#03B6    /zeta\n"
    "16#03B7    /eta\n"
    "16#03B8    /theta\n"
    "16#03B9    /iota\n"
    "16#03BA    /kappa\n"
    "16#03BB    /lambda\n"
    "16#03BC    /mu\n"
    "16#03BD    /nu\n"
    "16#03BE    /xi\n"
    "16#03BF    /omicron\n"
    "16#03C0    /pi\n"
    "16#03C1    /rho\n"
    "16#03C2    /sigma1\n"
    "16#03C3    /sigma\n"
    "16#03C4    /tau\n"
    "16#03C5    /upsilon\n"
    "16#03C6    /phi\n"
    "16#03C7    /chi\n"
    "16#03C8    /psi\n"
    "16#03C9    /omega\n"
    "16#03CA    /iotadieresis\n"
    "16#03CB    /upsilondieresis\n"
    "16#03CC    /omicrontonos\n"
    "16#03CD    /upsilontonos\n"
    "16#03CE    /omegatonos\n"
    "16#03D1    /theta1\n"
    "16#03D2    /Upsilon1\n"
    "16#03D5    /phi1\n"
    "16#03D6    /omega1\n"
    "16#0401    /afii10023\n"
    "16#0402    /afii10051\n"
    "16#0403    /afii10052\n"
    "16#0404    /afii10053\n"
    "16#0405    /afii10054\n"
    "16#0406    /afii10055\n"
    "16#0407    /afii10056\n"
    "16#0408    /afii10057\n"
    "16#0409    /afii10058\n"
    "16#040A    /afii10059\n"
    "16#040B    /afii10060\n"
    "16#040C    /afii10061\n"
    "16#040E    /afii10062\n"
    "16#040F    /afii10145\n"
    "16#0410    /afii10017\n"
    "16#0411    /afii10018\n"
    "16#0412    /afii10019\n"
    "16#0413    /afii10020\n"
    "16#0414    /afii10021\n"
    "16#0415    /afii10022\n"
    "16#0416    /afii10024\n"
    "16#0417    /afii10025\n"
    "16#0418    /afii10026\n"
    "16#0419    /afii10027\n"
    "16#041A    /afii10028\n"
    "16#041B    /afii10029\n"
    "16#041C    /afii10030\n"
    "16#041D    /afii10031\n"
    "16#041E    /afii10032\n"
    "16#041F    /afii10033\n"
    "16#0420    /afii10034\n"
    "16#0421    /afii10035\n"
    "16#0422    /afii10036\n"
    "16#0423    /afii10037\n"
    "16#0424    /afii10038\n"
    "16#0425    /afii10039\n"
    "16#0426    /afii10040\n"
    "16#0427    /afii10041\n"
    "16#0428    /afii10042\n"
    "16#0429    /afii10043\n"
    "16#042A    /afii10044\n"
    "16#042B    /afii10045\n"
    "16#042C    /afii10046\n"
    "16#042D    /afii10047\n"
    "16#042E    /afii10048\n"
    "16#042F    /afii10049\n"
    "16#0430    /afii10065\n"
    "16#0431    /afii10066\n"
    "16#0432    /afii10067\n"
    "16#0433    /afii10068\n"
    "16#0434    /afii10069\n"
    "16#0435    /afii10070\n"
    "16#0436    /afii10072\n"
    "16#0437    /afii10073\n"
    "16#0438    /afii10074\n"
    "16#0439    /afii10075\n"
    "16#043A    /afii10076\n"
    "16#043B    /afii10077\n"
    "16#043C    /afii10078\n"
    "16#043D    /afii10079\n"
    "16#043E    /afii10080\n"
    "16#043F    /afii10081\n"
    "16#0440    /afii10082\n"
    "16#0441    /afii10083\n"
    "16#0442    /afii10084\n"
    "16#0443    /afii10085\n"
    "16#0444    /afii10086\n"
    "16#0445    /afii10087\n"
    "16#0446    /afii10088\n"
    "16#0447    /afii10089\n"
    "16#0448    /afii10090\n"
    "16#0449    /afii10091\n"
    "16#044A    /afii10092\n"
    "16#044B    /afii10093\n"
    "16#044C    /afii10094\n"
    "16#044D    /afii10095\n"
    "16#044E    /afii10096\n"
    "16#044F    /afii10097\n"
    "16#0451    /afii10071\n"
    "16#0452    /afii10099\n"
    "16#0453    /afii10100\n"
    "16#0454    /afii10101\n"
    "16#0455    /afii10102\n"
    "16#0456    /afii10103\n"
    "16#0457    /afii10104\n"
    "16#0458    /afii10105\n"
    "16#0459    /afii10106\n"
    "16#045A    /afii10107\n"
    "16#045B    /afii10108\n"
    "16#045C    /afii10109\n"
    "16#045E    /afii10110\n"
    "16#045F    /afii10193\n"
    "16#0462    /afii10146\n"
    "16#0463    /afii10194\n"
    "16#0472    /afii10147\n"
    "16#0473    /afii10195\n"
    "16#0474    /afii10148\n"
    "16#0475    /afii10196\n"
    "16#0490    /afii10050\n"
    "16#0491    /afii10098\n"
    "16#04D9    /afii10846\n"
    "16#05B0    /afii57799\n"
    "16#05B1    /afii57801\n"
    "16#05B2    /afii57800\n"
    "16#05B3    /afii57802\n"
    "16#05B4    /afii57793\n"
    "16#05B5    /afii57794\n"
    "16#05B6    /afii57795\n"
    "16#05B7    /afii57798\n"
    "16#05B8    /afii57797\n"
    "16#05B9    /afii57806\n"
    "16#05BB    /afii57796\n"
    "16#05BC    /afii57807\n"
    "16#05BD    /afii57839\n"
    "16#05BE    /afii57645\n"
    "16#05BF    /afii57841\n"
    "16#05C0    /afii57842\n"
    "16#05C1    /afii57804\n"
    "16#05C2    /afii57803\n"
    "16#05C3    /afii57658\n"
    "16#05D0    /afii57664\n"
    "16#05D1    /afii57665\n"
    "16#05D2    /afii57666\n"
    "16#05D3    /afii57667\n"
    "16#05D4    /afii57668\n"
    "16#05D5    /afii57669\n"
    "16#05D6    /afii57670\n"
    "16#05D7    /afii57671\n"
    "16#05D8    /afii57672\n"
    "16#05D9    /afii57673\n"
    "16#05DA    /afii57674\n"
    "16#05DB    /afii57675\n"
    "16#05DC    /afii57676\n"
    "16#05DD    /afii57677\n"
    "16#05DE    /afii57678\n"
    "16#05DF    /afii57679\n"
    "16#05E0    /afii57680\n"
    "16#05E1    /afii57681\n"
    "16#05E2    /afii57682\n"
    "16#05E3    /afii57683\n"
    "16#05E4    /afii57684\n"
    "16#05E5    /afii57685\n"
    "16#05E6    /afii57686\n"
    "16#05E7    /afii57687\n"
    "16#05E8    /afii57688\n"
    "16#05E9    /afii57689\n"
    "16#05EA    /afii57690\n"
    "16#05F0    /afii57716\n"
    "16#05F1    /afii57717\n"
    "16#05F2    /afii57718\n"
    "16#060C    /afii57388\n"
    "16#061B    /afii57403\n"
    "16#061F    /afii57407\n"
    "16#0621    /afii57409\n"
    "16#0622    /afii57410\n"
    "16#0623    /afii57411\n"
    "16#0624    /afii57412\n"
    "16#0625    /afii57413\n"
    "16#0626    /afii57414\n"
    "16#0627    /afii57415\n"
    "16#0628    /afii57416\n"
    "16#0629    /afii57417\n"
    "16#062A    /afii57418\n"
    "16#062B    /afii57419\n"
    "16#062C    /afii57420\n"
    "16#062D    /afii57421\n"
    "16#062E    /afii57422\n"
    "16#062F    /afii57423\n"
    "16#0630    /afii57424\n"
    "16#0631    /afii57425\n"
    "16#0632    /afii57426\n"
    "16#0633    /afii57427\n"
    "16#0634    /afii57428\n"
    "16#0635    /afii57429\n"
    "16#0636    /afii57430\n"
    "16#0637    /afii57431\n"
    "16#0638    /afii57432\n"
    "16#0639    /afii57433\n"
    "16#063A    /afii57434\n"
    "16#0640    /afii57440\n"
    "16#0641    /afii57441\n"
    "16#0642    /afii57442\n"
    "16#0643    /afii57443\n"
    "16#0644    /afii57444\n"
    "16#0645    /afii57445\n"
    "16#0646    /afii57446\n"
    "16#0647    /afii57470\n"
    "16#0648    /afii57448\n"
    "16#0649    /afii57449\n"
    "16#064A    /afii57450\n"
    "16#064B    /afii57451\n"
    "16#064C    /afii57452\n"
    "16#064D    /afii57453\n"
    "16#064E    /afii57454\n"
    "16#064F    /afii57455\n"
    "16#0650    /afii57456\n"
    "16#0651    /afii57457\n"
    "16#0652    /afii57458\n"
    "16#0660    /afii57392\n"
    "16#0661    /afii57393\n"
    "16#0662    /afii57394\n"
    "16#0663    /afii57395\n"
    "16#0664    /afii57396\n"
    "16#0665    /afii57397\n"
    "16#0666    /afii57398\n"
    "16#0667    /afii57399\n"
    "16#0668    /afii57400\n"
    "16#0669    /afii57401\n"
    "16#066A    /afii57381\n"
    "16#066D    /afii63167\n"
    "16#0679    /afii57511\n"
    "16#067E    /afii57506\n"
    "16#0686    /afii57507\n"
    "16#0688    /afii57512\n"
    "16#0691    /afii57513\n"
    "16#0698    /afii57508\n"
    "16#06A4    /afii57505\n"
    "16#06AF    /afii57509\n"
    "16#06BA    /afii57514\n"
    "16#06D2    /afii57519\n"
    "16#06D5    /afii57534\n"
    "16#1E80    /Wgrave\n"
    "16#1E81    /wgrave\n"
    "16#1E82    /Wacute\n"
    "16#1E83    /wacute\n"
    "16#1E84    /Wdieresis\n"
    "16#1E85    /wdieresis\n"
    "16#1EF2    /Ygrave\n"
    "16#1EF3    /ygrave\n"
    "16#200C    /afii61664\n"
    "16#200D    /afii301\n"
    "16#200E    /afii299\n"
    "16#200F    /afii300\n"
    "16#2012    /figuredash\n"
    "16#2013    /endash\n"
    "16#2014    /emdash\n"
    "16#2015    /afii00208\n"
    "16#2017    /underscoredbl\n"
    "16#2018    /quoteleft\n"
    "16#2019    /quoteright\n"
    "16#201A    /quotesinglbase\n"
    "16#201B    /quotereversed\n"
    "16#201C    /quotedblleft\n"
    "16#201D    /quotedblright\n"
    "16#201E    /quotedblbase\n"
    "16#2020    /dagger\n"
    "16#2021    /daggerdbl\n"
    "16#2022    /bullet\n"
    "16#2024    /onedotenleader\n"
    "16#2025    /twodotenleader\n"
    "16#2026    /ellipsis\n"
    "16#202C    /afii61573\n"
    "16#202D    /afii61574\n"
    "16#202E    /afii61575\n"
    "16#2030    /perthousand\n"
    "16#2032    /minute\n"
    "16#2033    /second\n"
    "16#2039    /guilsinglleft\n"
    "16#203A    /guilsinglright\n"
    "16#203C    /exclamdbl\n"
    "16#2044    /fraction\n"
    "16#2070    /zerosuperior\n"
    "16#2074    /foursuperior\n"
    "16#2075    /fivesuperior\n"
    "16#2076    /sixsuperior\n"
    "16#2077    /sevensuperior\n"
    "16#2078    /eightsuperior\n"
    "16#2079    /ninesuperior\n"
    "16#207D    /parenleftsuperior\n"
    "16#207E    /parenrightsuperior\n"
    "16#207F    /nsuperior\n"
    "16#2080    /zeroinferior\n"
    "16#2081    /oneinferior\n"
    "16#2082    /twoinferior\n"
    "16#2083    /threeinferior\n"
    "16#2084    /fourinferior\n"
    "16#2085    /fiveinferior\n"
    "16#2086    /sixinferior\n"
    "16#2087    /seveninferior\n"
    "16#2088    /eightinferior\n"
    "16#2089    /nineinferior\n"
    "16#208D    /parenleftinferior\n"
    "16#208E    /parenrightinferior\n"
    "16#20A1    /colonmonetary\n"
    "16#20A3    /franc\n"
    "16#20A4    /lira\n"
    "16#20A7    /peseta\n"
    "16#20AA    /afii57636\n"
    "16#20AB    /dong\n"
    "16#20AC    /Euro\n"
    "16#2105    /afii61248\n"
    "16#2111    /Ifraktur\n"
    "16#2113    /afii61289\n"
    "16#2116    /afii61352\n"
    "16#2118    /weierstrass\n"
    "16#211C    /Rfraktur\n"
    "16#211E    /prescription\n"
    "16#2122    /trademark\n"
    "16#2126    /Omega\n"
    "16#212E    /estimated\n"
    "16#2135    /aleph\n"
    "16#2153    /onethird\n"
    "16#2154    /twothirds\n"
    "16#215B    /oneeighth\n"
    "16#215C    /threeeighths\n"
    "16#215D    /fiveeighths\n"
    "16#215E    /seveneighths\n"
    "16#2190    /arrowleft\n"
    "16#2191    /arrowup\n"
    "16#2192    /arrowright\n"
    "16#2193    /arrowdown\n"
    "16#2194    /arrowboth\n"
    "16#2195    /arrowupdn\n"
    "16#21A8    /arrowupdnbse\n"
    "16#21B5    /carriagereturn\n"
    "16#21D0    /arrowdblleft\n"
    "16#21D1    /arrowdblup\n"
    "16#21D2    /arrowdblright\n"
    "16#21D3    /arrowdbldown\n"
    "16#21D4    /arrowdblboth\n"
    "16#2200    /universal\n"
    "16#2202    /partialdiff\n"
    "16#2203    /existential\n"
    "16#2205    /emptyset\n"
    "16#2206    /Delta\n"
    "16#2207    /gradient\n"
    "16#2208    /element\n"
    "16#2209    /notelement\n"
    "16#220B    /suchthat\n"
    "16#220F    /product\n"
    "16#2211    /summation\n"
    "16#2212    /minus\n"
    "16#2215    /fraction\n"
    "16#2217    /asteriskmath\n"
    "16#2219    /periodcentered\n"
    "16#221A    /radical\n"
    "16#221D    /proportional\n"
    "16#221E    /infinity\n"
    "16#221F    /orthogonal\n"
    "16#2220    /angle\n"
    "16#2227    /logicaland\n"
    "16#2228    /logicalor\n"
    "16#2229    /intersection\n"
    "16#222A    /union\n"
    "16#222B    /integral\n"
    "16#2234    /therefore\n"
    "16#223C    /similar\n"
    "16#2245    /congruent\n"
    "16#2248    /approxequal\n"
    "16#2260    /notequal\n"
    "16#2261    /equivalence\n"
    "16#2264    /lessequal\n"
    "16#2265    /greaterequal\n"
    "16#2282    /propersubset\n"
    "16#2283    /propersuperset\n"
    "16#2284    /notsubset\n"
    "16#2286    /reflexsubset\n"
    "16#2287    /reflexsuperset\n"
    "16#2295    /circleplus\n"
    "16#2297    /circlemultiply\n"
    "16#22A5    /perpendicular\n"
    "16#22C5    /dotmath\n"
    "16#2302    /house\n"
    "16#2310    /revlogicalnot\n"
    "16#2320    /integraltp\n"
    "16#2321    /integralbt\n"
    "16#2329    /angleleft\n"
    "16#232A    /angleright\n"
    "16#2500    /SF100000\n"
    "16#2502    /SF110000\n"
    "16#250C    /SF010000\n"
    "16#2510    /SF030000\n"
    "16#2514    /SF020000\n"
    "16#2518    /SF040000\n"
    "16#251C    /SF080000\n"
    "16#2524    /SF090000\n"
    "16#252C    /SF060000\n"
    "16#2534    /SF070000\n"
    "16#253C    /SF050000\n"
    "16#2550    /SF430000\n"
    "16#2551    /SF240000\n"
    "16#2552    /SF510000\n"
    "16#2553    /SF520000\n"
    "16#2554    /SF390000\n"
    "16#2555    /SF220000\n"
    "16#2556    /SF210000\n"
    "16#2557    /SF250000\n"
    "16#2558    /SF500000\n"
    "16#2559    /SF490000\n"
    "16#255A    /SF380000\n"
    "16#255B    /SF280000\n"
    "16#255C    /SF270000\n"
    "16#255D    /SF260000\n"
    "16#255E    /SF360000\n"
    "16#255F    /SF370000\n"
    "16#2560    /SF420000\n"
    "16#2561    /SF190000\n"
    "16#2562    /SF200000\n"
    "16#2563    /SF230000\n"
    "16#2564    /SF470000\n"
    "16#2565    /SF480000\n"
    "16#2566    /SF410000\n"
    "16#2567    /SF450000\n"
    "16#2568    /SF460000\n"
    "16#2569    /SF400000\n"
    "16#256A    /SF540000\n"
    "16#256B    /SF530000\n"
    "16#256C    /SF440000\n"
    "16#2580    /upblock\n"
    "16#2584    /dnblock\n"
    "16#2588    /block\n"
    "16#258C    /lfblock\n"
    "16#2590    /rtblock\n"
    "16#2591    /ltshade\n"
    "16#2592    /shade\n"
    "16#2593    /dkshade\n"
    "16#25A0    /filledbox\n"
    "16#25A1    /H22073\n"
    "16#25AA    /H18543\n"
    "16#25AB    /H18551\n"
    "16#25AC    /filledrect\n"
    "16#25B2    /triagup\n"
    "16#25BA    /triagrt\n"
    "16#25BC    /triagdn\n"
    "16#25C4    /triaglf\n"
    "16#25CA    /lozenge\n"
    "16#25CB    /circle\n"
    "16#25CF    /H18533\n"
    "16#25D8    /invbullet\n"
    "16#25D9    /invcircle\n"
    "16#25E6    /openbullet\n"
    "16#263A    /smileface\n"
    "16#263B    /invsmileface\n"
    "16#263C    /sun\n"
    "16#2640    /female\n"
    "16#2642    /male\n"
    "16#2660    /spade\n"
    "16#2663    /club\n"
    "16#2665    /heart\n"
    "16#2666    /diamond\n"
    "16#266A    /musicalnote\n"
    "16#266B    /musicalnotedbl\n"
    "16#F6BE    /dotlessj\n"
    "16#F6BF    /LL\n"
    "16#F6C0    /ll\n"
    "16#F6C1    /Scedilla\n"
    "16#F6C2    /scedilla\n"
    "16#F6C3    /commaaccent\n"
    "16#F6C4    /afii10063\n"
    "16#F6C5    /afii10064\n"
    "16#F6C6    /afii10192\n"
    "16#F6C7    /afii10831\n"
    "16#F6C8    /afii10832\n"
    "16#F6C9    /Acute\n"
    "16#F6CA    /Caron\n"
    "16#F6CB    /Dieresis\n"
    "16#F6CC    /DieresisAcute\n"
    "16#F6CD    /DieresisGrave\n"
    "16#F6CE    /Grave\n"
    "16#F6CF    /Hungarumlaut\n"
    "16#F6D0    /Macron\n"
    "16#F6D1    /cyrBreve\n"
    "16#F6D2    /cyrFlex\n"
    "16#F6D3    /dblGrave\n"
    "16#F6D4    /cyrbreve\n"
    "16#F6D5    /cyrflex\n"
    "16#F6D6    /dblgrave\n"
    "16#F6D7    /dieresisacute\n"
    "16#F6D8    /dieresisgrave\n"
    "16#F6D9    /copyrightserif\n"
    "16#F6DA    /registerserif\n"
    "16#F6DB    /trademarkserif\n"
    "16#F6DC    /onefitted\n"
    "16#F6DD    /rupiah\n"
    "16#F6DE    /threequartersemdash\n"
    "16#F6DF    /centinferior\n"
    "16#F6E0    /centsuperior\n"
    "16#F6E1    /commainferior\n"
    "16#F6E2    /commasuperior\n"
    "16#F6E3    /dollarinferior\n"
    "16#F6E4    /dollarsuperior\n"
    "16#F6E5    /hypheninferior\n"
    "16#F6E6    /hyphensuperior\n"
    "16#F6E7    /periodinferior\n"
    "16#F6E8    /periodsuperior\n"
    "16#F6E9    /asuperior\n"
    "16#F6EA    /bsuperior\n"
    "16#F6EB    /dsuperior\n"
    "16#F6EC    /esuperior\n"
    "16#F6ED    /isuperior\n"
    "16#F6EE    /lsuperior\n"
    "16#F6EF    /msuperior\n"
    "16#F6F0    /osuperior\n"
    "16#F6F1    /rsuperior\n"
    "16#F6F2    /ssuperior\n"
    "16#F6F3    /tsuperior\n"
    "16#F6F4    /Brevesmall\n"
    "16#F6F5    /Caronsmall\n"
    "16#F6F6    /Circumflexsmall\n"
    "16#F6F7    /Dotaccentsmall\n"
    "16#F6F8    /Hungarumlautsmall\n"
    "16#F6F9    /Lslashsmall\n"
    "16#F6FA    /OEsmall\n"
    "16#F6FB    /Ogoneksmall\n"
    "16#F6FC    /Ringsmall\n"
    "16#F6FD    /Scaronsmall\n"
    "16#F6FE    /Tildesmall\n"
    "16#F6FF    /Zcaronsmall\n"
    "16#F721    /exclamsmall\n"
    "16#F724    /dollaroldstyle\n"
    "16#F726    /ampersandsmall\n"
    "16#F730    /zerooldstyle\n"
    "16#F731    /oneoldstyle\n"
    "16#F732    /twooldstyle\n"
    "16#F733    /threeoldstyle\n"
    "16#F734    /fouroldstyle\n"
    "16#F735    /fiveoldstyle\n"
    "16#F736    /sixoldstyle\n"
    "16#F737    /sevenoldstyle\n"
    "16#F738    /eightoldstyle\n"
    "16#F739    /nineoldstyle\n"
    "16#F73F    /questionsmall\n"
    "16#F760    /Gravesmall\n"
    "16#F761    /Asmall\n"
    "16#F762    /Bsmall\n"
    "16#F763    /Csmall\n"
    "16#F764    /Dsmall\n"
    "16#F765    /Esmall\n"
    "16#F766    /Fsmall\n"
    "16#F767    /Gsmall\n"
    "16#F768    /Hsmall\n"
    "16#F769    /Ismall\n"
    "16#F76A    /Jsmall\n"
    "16#F76B    /Ksmall\n"
    "16#F76C    /Lsmall\n"
    "16#F76D    /Msmall\n"
    "16#F76E    /Nsmall\n"
    "16#F76F    /Osmall\n"
    "16#F770    /Psmall\n"
    "16#F771    /Qsmall\n"
    "16#F772    /Rsmall\n"
    "16#F773    /Ssmall\n"
    "16#F774    /Tsmall\n"
    "16#F775    /Usmall\n"
    "16#F776    /Vsmall\n"
    "16#F777    /Wsmall\n"
    "16#F778    /Xsmall\n"
    "16#F779    /Ysmall\n"
    "16#F77A    /Zsmall\n"
    "16#F7A1    /exclamdownsmall\n"
    "16#F7A2    /centoldstyle\n"
    "16#F7A8    /Dieresissmall\n"
    "16#F7AF    /Macronsmall\n"
    "16#F7B4    /Acutesmall\n"
    "16#F7B8    /Cedillasmall\n"
    "16#F7BF    /questiondownsmall\n"
    "16#F7E0    /Agravesmall\n"
    "16#F7E1    /Aacutesmall\n"
    "16#F7E2    /Acircumflexsmall\n"
    "16#F7E3    /Atildesmall\n"
    "16#F7E4    /Adieresissmall\n"
    "16#F7E5    /Aringsmall\n"
    "16#F7E6    /AEsmall\n"
    "16#F7E7    /Ccedillasmall\n"
    "16#F7E8    /Egravesmall\n"
    "16#F7E9    /Eacutesmall\n"
    "16#F7EA    /Ecircumflexsmall\n"
    "16#F7EB    /Edieresissmall\n"
    "16#F7EC    /Igravesmall\n"
    "16#F7ED    /Iacutesmall\n"
    "16#F7EE    /Icircumflexsmall\n"
    "16#F7EF    /Idieresissmall\n"
    "16#F7F0    /Ethsmall\n"
    "16#F7F1    /Ntildesmall\n"
    "16#F7F2    /Ogravesmall\n"
    "16#F7F3    /Oacutesmall\n"
    "16#F7F4    /Ocircumflexsmall\n"
    "16#F7F5    /Otildesmall\n"
    "16#F7F6    /Odieresissmall\n"
    "16#F7F8    /Oslashsmall\n"
    "16#F7F9    /Ugravesmall\n"
    "16#F7FA    /Uacutesmall\n"
    "16#F7FB    /Ucircumflexsmall\n"
    "16#F7FC    /Udieresissmall\n"
    "16#F7FD    /Yacutesmall\n"
    "16#F7FE    /Thornsmall\n"
    "16#F7FF    /Ydieresissmall\n"
    "16#F8E5    /radicalex\n"
    "16#F8E6    /arrowvertex\n"
    "16#F8E7    /arrowhorizex\n"
    "16#F8E8    /registersans\n"
    "16#F8E9    /copyrightsans\n"
    "16#F8EA    /trademarksans\n"
    "16#F8EB    /parenlefttp\n"
    "16#F8EC    /parenleftex\n"
    "16#F8ED    /parenleftbt\n"
    "16#F8EE    /bracketlefttp\n"
    "16#F8EF    /bracketleftex\n"
    "16#F8F0    /bracketleftbt\n"
    "16#F8F1    /bracelefttp\n"
    "16#F8F2    /braceleftmid\n"
    "16#F8F3    /braceleftbt\n"
    "16#F8F4    /braceex\n"
    "16#F8F5    /integralex\n"
    "16#F8F6    /parenrighttp\n"
    "16#F8F7    /parenrightex\n"
    "16#F8F8    /parenrightbt\n"
    "16#F8F9    /bracketrighttp\n"
    "16#F8FA    /bracketrightex\n"
    "16#F8FB    /bracketrightbt\n"
    "16#F8FC    /bracerighttp\n"
    "16#F8FD    /bracerightmid\n"
    "16#F8FE    /bracerightbt\n"
    "16#FB00    /ff\n"
    "16#FB01    /fi\n"
    "16#FB02    /fl\n"
    "16#FB03    /ffi\n"
    "16#FB04    /ffl\n"
    "16#FB1F    /afii57705\n"
    "16#FB2A    /afii57694\n"
    "16#FB2B    /afii57695\n"
    "16#FB35    /afii57723\n"
    "16#FB4B    /afii57700\n"
    ">>    def\n"

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

    // Procedure to stroke a rectangle. Coordinates are rounded
    // to device pixel boundaries. See Adobe Technical notes 5111 and
    // 5126 and the "Scan Conversion" section of the PS Language
    // Reference for background.
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

  // read the printer properties file
  NS_LoadPersistentPropertiesFromURISpec(getter_AddRefs(mPrinterProps),
    NS_LITERAL_CSTRING("resource:/res/unixpsfonts.properties"));

  // setup prolog for each langgroup
  initlanggroup();

  fprintf(f, "%%%%EndProlog\n");
  XL_RESTORE_NUMERIC_LOCALE();
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 20/01/03 louie
 */
void
nsPostScriptObj::add_cid_check()
{
#ifdef MOZ_ENABLE_FREETYPE2
  AddCIDCheckCode(mPrintContext->prSetup->out);
#endif
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::begin_page()
{
FILE *f;

  XL_SET_NUMERIC_LOCALE();
  f = mPrintContext->prSetup->tmpBody;
  fprintf(f, "%%%%Page: %d %d\n", mPageNumber, mPageNumber);
  fprintf(f, "%%%%BeginPageSetup\n");
  if(mPrintSetup->num_copies != 1) {
    fprintf(f, "1 dict dup /NumCopies %d put setpagedevice\n",
      mPrintSetup->num_copies);
  }
  fprintf(f,"/pagelevel save def\n");
  // Rescale the coordinate system from points to twips.
  fprintf(f, "%g %g scale\n",
    1.0 / TWIPS_PER_POINT_FLOAT, 1.0 / TWIPS_PER_POINT_FLOAT);
  // Move the origin to the bottom left of the printable region.
  if (mPrintContext->prSetup->landscape){
    fprintf(f, "90 rotate %d -%d translate\n",
      mPrintContext->prSetup->left,
      mPrintContext->prSetup->height + mPrintContext->prSetup->top);
  }
  else {
    fprintf(f, "%d %d translate\n",
      mPrintContext->prSetup->left,
      mPrintContext->prSetup->bottom);
  }
  // Try to turn on automatic stroke adjust
  fputs("true Msetstrokeadjust\n", f);
  fprintf(f, "%%%%EndPageSetup\n");
#if 0
  annotate_page( mPrintContext->prSetup->header, 0, -1, pn);
#endif
  // Set up a clipping rectangle around the printable area.
  fprintf(f, "0 0 %d %d Mrect closepath clip newpath\n",
    mPrintContext->prInfo->page_width, mPrintContext->prInfo->page_height);

  // need to reset all U2Ntable
  gLangGroups->Enumerate(ResetU2Ntable, nsnull);
  XL_RESTORE_NUMERIC_LOCALE();
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::end_page()
{
#if 0
  annotate_page( mPrintContext->prSetup->footer,
		   mPrintContext->prSetup->height-mPrintContext->prSetup->bottom-mPrintContext->prSetup->top,
		   1, pn);
  fprintf(mPrintContext->prSetup->out, "pagelevel restore\nshowpage\n");
#endif

  fprintf(mPrintContext->prSetup->tmpBody, "pagelevel restore\n");
  annotate_page(mPrintContext->prSetup->header, mPrintContext->prSetup->top/2, -1, mPageNumber);
  annotate_page( mPrintContext->prSetup->footer,
				   mPrintContext->prSetup->height - mPrintContext->prSetup->bottom/2,
				   1, mPageNumber);
  fprintf(mPrintContext->prSetup->tmpBody, "showpage\n");
  mPageNumber++;
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
nsresult 
nsPostScriptObj::end_document()
{
  nsresult rv;
  PR_LOG(nsPostScriptObjLM, PR_LOG_DEBUG, ("nsPostScriptObj::end_document()\n"));

  // insurance against breakage
  if (!mPrintContext || !mPrintContext->prSetup|| !mPrintContext->prSetup->out || !mPrintSetup) 
    return NS_ERROR_GFX_PRINTER_CMD_FAILURE;

  FILE *f = mPrintContext->prSetup->out;
  
  // output "tmpBody"
  char   buffer[256];
  size_t length;
        
  if (!mPrintContext->prSetup->tmpBody)
    return NS_ERROR_GFX_PRINTER_CMD_FAILURE;
    
  /* Reset file pointer to the beginning of the temp tmpBody file... */
  fseek(mPrintContext->prSetup->tmpBody, 0, SEEK_SET);

  /* Copy the document script (body) to the end of the prolog (header) */
  while ((length = fread(buffer, 1, sizeof(buffer),
          mPrintContext->prSetup->tmpBody)) > 0)  {
    fwrite(buffer, 1, length, f);
  }

  /* Close the script file handle and dispose of the temporary file */
  if (mPrintSetup->tmpBody) {
    fclose(mPrintSetup->tmpBody);
    mPrintSetup->tmpBody = nsnull;
  }
  mDocScript->Remove(PR_FALSE);
  mDocScript = nsnull;
  
  // Finish up the document.
  // n_pages is zero so use mPageNumber
  fprintf(f, "%%%%Trailer\n");
  fprintf(f, "%%%%Pages: %d\n", (int) mPageNumber - 1);
  fprintf(f, "%%%%EOF\n");

  if (!mPrintSetup->print_cmd) {
    PR_LOG(nsPostScriptObjLM, PR_LOG_DEBUG, ("print to file completed.\n"));
    fclose(mPrintSetup->out);
    rv = NS_OK;
  }  
  else {
    PR_LOG(nsPostScriptObjLM, PR_LOG_DEBUG, ("piping job to '%s'\n", mPrintSetup->print_cmd));
        
#ifdef VMS
    // VMS can't print to a pipe, so issue a print command for the
    // mDocProlog file.

    fclose(mPrintSetup->out);

    nsCAutoString prologFile;
    rv = mDocProlog->GetNativePath(prologFile);
    if (NS_SUCCEEDED(rv)) {
      char *VMSPrintCommand =
        PR_smprintf("%s %s.", mPrintSetup->print_cmd, prologFile.get());
      if (!VMSPrintCommand)
        rv = NS_ERROR_OUT_OF_MEMORY;
      else {
        PR_LOG(nsPostScriptObjLM, PR_LOG_DEBUG, ("VMS print command '%s'\n", 
          VMSPrintCommand));
        int VMSstatus = system(VMSPrintCommand);
        PR_smprintf_free(VMSPrintCommand);
        PR_LOG(nsPostScriptObjLM, PR_LOG_DEBUG, ("VMS print status = %d\n",
          VMSstatus));
        rv = WIFEXITED(VMSstatus) ? NS_OK : NS_ERROR_GFX_PRINTER_CMD_FAILURE;
      }
    }
#else
    // On *nix, popen() the print command and pipe the contents of
    // mDocProlog into it.

    FILE  *pipe;

    pipe = popen(mPrintSetup->print_cmd, "w");
    if (!pipe)
      rv = NS_ERROR_GFX_PRINTER_CMD_FAILURE;
    else {
      unsigned long job_size = 0;
    
      /* Reset file pointer to the beginning of the temp file... */
      fseek(mPrintSetup->out, 0, SEEK_SET);

      while ((length = fread(buffer, 1, sizeof(buffer), mPrintSetup->out)) > 0)
      {
        fwrite(buffer, 1, length, pipe);
        job_size += length;
      }
      fclose(mPrintSetup->out);
      PR_LOG(nsPostScriptObjLM, PR_LOG_DEBUG,
          ("piping done, copied %ld bytes.\n", job_size));
      int exitStatus = pclose(pipe);
      rv = WIFEXITED(exitStatus) ? NS_OK : NS_ERROR_GFX_PRINTER_CMD_FAILURE;
    }
#endif
    mDocProlog->Remove(PR_FALSE);
  }
  mPrintSetup->out = nsnull;
  mDocProlog = nsnull;
  
  return rv;
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::annotate_page(const char *aTemplate,
                               int y, int delta_dir, int pn)
{

}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc. Updated 3/22/2000 to deal with only non-Unicode chars. yueheng.xu@intel.com
 */
void 
nsPostScriptObj::show(const char* txt, int len, const char *align)
{
FILE *f;

  f = mPrintContext->prSetup->tmpBody;
  fprintf(f, "(");

  while (len-- > 0) {
    switch (*txt) {
	    case '(':
	    case ')':
	    case '\\':
        fprintf(f, "\\%c", *txt);
		    break;
	    default:
            fprintf(f, "%c", *txt);     
		    break;
	  }
	  txt++;
  }
  fprintf(f, ") %sshow\n", align);
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 6/1/2000 katakai
 */
void 
nsPostScriptObj::preshow(const PRUnichar* txt, int len)
{
  FILE *f = mPrintContext->prSetup->tmpBody;
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
	      fprintf(f, "%d <%x> u2nadd\n", uch, code);
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
  FILE *f = mPrintContext->prSetup->tmpBody;
  unsigned char highbyte, lowbyte;
  PRUnichar uch;
 
  if (aType == 1) {
    int i;
    fprintf(f, "<");
    for (i=0; i < len; i++) {
      if (i == 0)
        fprintf(f, "%04x", txt[i]);
      else
        fprintf(f, " %04x", txt[i]);
    }
    fprintf(f, "> show\n");
    return;
  }
 
  fprintf(f, "(");

  while (len-- > 0) {
    switch (*txt) {
        case 0x0028:     // '('
            fprintf(f, "\\050\\000");
		    break;
        case 0x0029:     // ')' 
            fprintf(f, "\\051\\000");
		    break;
        case 0x005c:     // '\\'
            fprintf(f, "\\134\\000");
		    break;
	    default:
          uch = *txt;
          highbyte = (uch >> 8 ) & 0xff;
          lowbyte = ( uch & 0xff );

          // we output all unicode chars in the 2x3 digits oct format for easier post-processing
          // Our 'show' command will always treat the second 3 digit oct as high 8-bits of unicode, independent of Endians
          if ( lowbyte < 8 )
		      fprintf(f, "\\00%o", lowbyte  & 0xff);
          else if ( lowbyte < 64  && lowbyte >= 8)
            fprintf(f, "\\0%o", lowbyte & 0xff);
          else
             fprintf(f, "\\%o", lowbyte & 0xff);      

          if ( highbyte < 8  )
		      fprintf(f, "\\00%o", highbyte & 0xff);
          else if ( highbyte < 64  && highbyte >= 8)
            fprintf(f, "\\0%o", highbyte & 0xff);
          else
             fprintf(f, "\\%o", highbyte & 0xff);      
         
		break;
	  }
	  txt++;
  }
  fprintf(f, ") %sunicodeshow\n", align);
}




/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::moveto(nscoord x, nscoord y)
{
  XL_SET_NUMERIC_LOCALE();
  fprintf(mPrintContext->prSetup->tmpBody, "%d %d moveto\n", x, y);
  XL_RESTORE_NUMERIC_LOCALE();
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::lineto(nscoord aX, nscoord aY)
{
  XL_SET_NUMERIC_LOCALE();
  fprintf(mPrintContext->prSetup->tmpBody, "%d %d lineto\n", aX, aY);
  XL_RESTORE_NUMERIC_LOCALE();
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::ellipse(nscoord aWidth, nscoord aHeight)
{
  XL_SET_NUMERIC_LOCALE();

  // Ellipse definition
  fprintf(mPrintContext->prSetup->tmpBody,"%g %g ",
                aWidth * 0.5, aHeight * 0.5);
  fprintf(mPrintContext->prSetup->tmpBody,
                " matrix currentmatrix currentpoint translate\n");
  fprintf(mPrintContext->prSetup->tmpBody,
          "     3 1 roll scale newpath 0 0 1 0 360 arc setmatrix  \n");
  XL_RESTORE_NUMERIC_LOCALE();
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::arc(nscoord aWidth, nscoord aHeight,
  float aStartAngle,float aEndAngle)
{

  XL_SET_NUMERIC_LOCALE();
  // Arc definition
  fprintf(mPrintContext->prSetup->tmpBody,"%g %g ",
                aWidth * 0.5, aHeight * 0.5);
  fprintf(mPrintContext->prSetup->tmpBody,
                " matrix currentmatrix currentpoint translate\n");
  fprintf(mPrintContext->prSetup->tmpBody,
          "     3 1 roll scale newpath 0 0 1 %g %g arc setmatrix  \n",aStartAngle,aEndAngle);

  XL_RESTORE_NUMERIC_LOCALE();


  
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::box(nscoord aX, nscoord aY, nscoord aW, nscoord aH)
{
  XL_SET_NUMERIC_LOCALE();
  fprintf(mPrintContext->prSetup->tmpBody,
    "%d %d %d %d Mrect ", aX, aY, aW, aH);
  XL_RESTORE_NUMERIC_LOCALE();
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::box_subtract(nscoord aX, nscoord aY, nscoord aW, nscoord aH)
{
  XL_SET_NUMERIC_LOCALE();
  fprintf(mPrintContext->prSetup->tmpBody,
    "%d %d moveto 0 %d rlineto %d 0 rlineto 0 %d rlineto closepath ",
    aX, aY, aH, aW, -aH);
  XL_RESTORE_NUMERIC_LOCALE();
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::clip()
{
  fprintf(mPrintContext->prSetup->tmpBody, " clip \n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::eoclip()
{
  fprintf(mPrintContext->prSetup->tmpBody, " eoclip \n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::clippath()
{
  fprintf(mPrintContext->prSetup->tmpBody, " clippath \n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::newpath()
{
  fprintf(mPrintContext->prSetup->tmpBody, " newpath \n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::closepath()
{
  fprintf(mPrintContext->prSetup->tmpBody, " closepath \n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::initclip()
{
  fprintf(mPrintContext->prSetup->tmpBody, " initclip \n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::line(nscoord aX1, nscoord aY1,
  nscoord aX2, nscoord aY2, nscoord aThick)
{
  XL_SET_NUMERIC_LOCALE();
  fprintf(mPrintContext->prSetup->tmpBody, "gsave %d setlinewidth\n ", aThick);
  fprintf(mPrintContext->prSetup->tmpBody, " %d %d moveto %d %d lineto\n",
    aX1, aY1, aX2, aY2);
  stroke();
  fprintf(mPrintContext->prSetup->tmpBody, "grestore\n");
  XL_RESTORE_NUMERIC_LOCALE();
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::stroke()
{
  fprintf(mPrintContext->prSetup->tmpBody, " stroke \n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::fill()
{
  fprintf(mPrintContext->prSetup->tmpBody, " fill \n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::graphics_save()
{
  fprintf(mPrintContext->prSetup->tmpBody, " gsave \n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::graphics_restore()
{
  fprintf(mPrintContext->prSetup->tmpBody, " grestore \n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::translate(nscoord x, nscoord y)
{
    XL_SET_NUMERIC_LOCALE();
    fprintf(mPrintContext->prSetup->tmpBody, "%d %d translate\n", x, y);
    XL_RESTORE_NUMERIC_LOCALE();
}


  /** ---------------------------------------------------
   *  Draw an image. dRect may be thought of as a hole in the document
   *  page, through which we can see another page containing the image.
   *  sRect is the portion of the image page which is visible through the
   *  hole in the document. iRect is the portion of the image page which
   *  contains the image represented by anImage. sRect and iRect may be
   *  at arbitrary positions relative to each other.
   *
   *    @update 11/25/2003 kherron
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
  FILE *f = mPrintContext->prSetup->tmpBody;
  
  // If a final image dimension is 0 pixels, just return (see bug 191684)
  if ((0 == dRect.width) || (0 == dRect.height)) {
    return;
  }

  anImage->LockImagePixels(PR_FALSE);
  PRUint8 *theBits = anImage->GetBits();

  /* image data might not be available (ex: spacer image) */
  if (!theBits)
  {
    anImage->UnlockImagePixels(PR_FALSE);
    return;
  }

  // Save the current graphic state and define a PS variable that
  // can hold one line of pixel data.
  fprintf(f, "gsave\n/rowdata %d string def\n",
     mPrintSetup->color ? iRect.width * 3 : iRect.width);
 
  // Translate the coordinate origin to the corner of the rectangle where
  // the image should appear, set up a clipping region, and scale the
  // coordinate system to the image's final size.
  translate(dRect.x, dRect.y);
  box(0, 0, dRect.width, dRect.height);
  clip();
  fprintf(f, "%d %d scale\n", dRect.width, dRect.height);

  // Describe how the pixel data is to be interpreted: pixels per row,
  // rows, and bits per pixel (per component in color).
  fprintf(f, "%d %d 8 ", iRect.width, iRect.height);

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

  // If the image data is in the wrong order, invert the TM, causing
  // the image to be drawn inverted.
  if (!anImage->GetIsRowOrderTopToBottom()) {
    tmTY += tmSY;
    tmSY = -tmSY;
  }
  fprintf(f, "[ %d 0 0 %d %d %d ]\n", tmSX, tmSY, tmTX, tmTY);

  // Output the data-reading procedure and the appropriate image command.
  fputs(" { currentfile rowdata readhexstring pop }", f);
  if (mPrintSetup->color)
    fputs(" false 3 colorimage\n", f);
  else
    fputs(" image\n", f);

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
          fprintf(f, "%02x%02x%02x", pixel[0], pixel[1], pixel[2]);
      else
        outputCount +=
          fprintf(f, "%02x", NS_RGB_TO_GRAY(pixel[0], pixel[1], pixel[2]));
      if (outputCount >= 72) {
        fputc('\n', f);
        outputCount = 0;
      }
    }
  }
  anImage->UnlockImagePixels(PR_FALSE);

  // Free the PS data buffer and restore the previous graphics state.
  fputs("\n/rowdata where { /rowdata undef } if\n", f);
  fputs("grestore\n", f);
}


/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::setcolor(nscolor aColor)
{
float greyBrightness;

  XL_SET_NUMERIC_LOCALE();

  /* For greyscale postscript, find the average brightness of red, green, and
   * blue.  Using this average value as the brightness for red, green, and
   * blue is a simple way to make the postscript greyscale instead of color.
   */

  if(mPrintSetup->color == PR_FALSE ) {
    greyBrightness=NS_PS_GRAY(NS_RGB_TO_GRAY(NS_GET_R(aColor),
                                             NS_GET_G(aColor),
                                             NS_GET_B(aColor)));
    fprintf(mPrintContext->prSetup->tmpBody,"%3.2f setgray\n", greyBrightness);
  } else {
    fprintf(mPrintContext->prSetup->tmpBody,"%3.2f %3.2f %3.2f setrgbcolor\n",
    NS_PS_RED(aColor), NS_PS_GREEN(aColor), NS_PS_BLUE(aColor));
  }

  XL_RESTORE_NUMERIC_LOCALE();
}


void nsPostScriptObj::setfont(const nsCString aFontName, PRUint32 aHeight)
{
  fprintf(mPrintContext->prSetup->tmpBody,
          "%d /%s Msf\n", aHeight, aFontName.get());
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


//    fprintf(mPrintContext->prSetup->out, "%% aFontIndex = %d, Family = %s, aStyle = %d, 
//        aWeight=%d, postscriptfont = %d\n", aFontIndex, &aFamily, aStyle, aWeight, postscriptFont);
  fprintf(mPrintContext->prSetup->tmpBody,"%d", aHeight);

 
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
   fprintf(mPrintContext->prSetup->tmpBody, " f%d\n", postscriptFont);


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
  fprintf(mPrintContext->prSetup->tmpBody,"%%%s\n", aTheComment);
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 5/30/00 katakai
 */
void
nsPostScriptObj::setlanggroup(nsIAtom * aLangGroup)
{
  FILE *f = mPrintContext->prSetup->tmpBody;

  gEncoder = nsnull;
  gU2Ntable = nsnull;

  if (aLangGroup == nsnull) {
    fprintf(f, "default_ls\n");
    return;
  }
  nsAutoString langstr;
  aLangGroup->ToString(langstr);

  /* already exist */
  nsStringKey key(langstr);
  PS_LangGroupInfo *linfo = (PS_LangGroupInfo *) gLangGroups->Get(&key);

  if (linfo) {
    nsCAutoString str; str.AssignWithConversion(langstr);
    fprintf(f, "%s_ls\n", str.get());
    gEncoder = linfo->mEncoder;
    gU2Ntable = linfo->mU2Ntable;
    return;
  } else {
    fprintf(f, "default_ls\n");
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

FILE * nsPostScriptObj::GetPrintFile()
{
  return(mPrintContext->prSetup->out);
}

const char* kNativeFontPrefix  = "print.postscript.nativefont.";
const char* kUnicodeFontPrefix = "print.postscript.unicodefont.";

/* make <langgroup>_ls define for each LangGroup here */
static void PrefEnumCallback(const char *aName, void *aClosure)
{
  nsPostScriptObj *psObj = (nsPostScriptObj*)aClosure;
  FILE *f = psObj->GetPrintFile();

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
nsPostScriptObj::initlanggroup()
{

  /* check langgroup of preference */
  gPrefs->EnumerateChildren(kNativeFontPrefix,
	    PrefEnumCallback, (void *) this);

  gPrefs->EnumerateChildren(kUnicodeFontPrefix,
	    PrefEnumCallback, (void *) this);
}

