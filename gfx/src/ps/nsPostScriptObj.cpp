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
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIPersistentProperties2.h"
#include "nsCRT.h"
#include "nsFontMetricsPS.h"

#ifndef NS_BUILD_ID
#include "nsBuildID.h"
#endif /* !NS_BUILD_ID */

#include "prenv.h"
#include "prprf.h"

#include <locale.h>
#include <limits.h>
#include <errno.h>

#ifdef VMS
#include <stdlib.h>
#endif

#ifdef MOZ_ENABLE_FREETYPE2
#include "nsType8.h"
#endif

#ifdef PR_LOGGING
static PRLogModuleInfo *nsPostScriptObjLM = PR_NewLogModule("nsPostScriptObj");
#endif /* PR_LOGGING */

extern "C" PS_FontInfo *PSFE_MaskToFI[N_FONTS];   // need fontmetrics.c

// These set the location to standard C and back
// which will keep the "." from converting to a "," 
// in certain languages for floating point output to postscript
#define XL_SET_NUMERIC_LOCALE() char* cur_locale = setlocale(LC_NUMERIC, "C")
#define XL_RESTORE_NUMERIC_LOCALE() setlocale(LC_NUMERIC, cur_locale)

#define NS_PS_RED(x) (((float)(NS_GET_R(x))) / 255.0) 
#define NS_PS_GREEN(x) (((float)(NS_GET_G(x))) / 255.0) 
#define NS_PS_BLUE(x) (((float)(NS_GET_B(x))) / 255.0) 
#define NS_TWIPS_TO_POINTS(x) (((x) / 20))
#define NS_IS_BOLD(x) (((x) >= 401) ? 1 : 0) 

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_IID(kICharsetConverterManagerIID, NS_ICHARSETCONVERTERMANAGER_IID);

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
#ifdef VMS
      // if the file was still open and we have a print command, then it was
      // a print preview operation and so we need to delete the temp file.
      if (mPrintSetup->print_cmd)
        remove(mPrintSetup->filename);
#endif
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
               
      aSpec->GetToPrinter( isAPrinter );
      if (isAPrinter) {
        /* Define the destination printer (queue). 
         * We assume that the print command is set to 
         * "lpr ${MOZ_PRINTER_NAME:+'-P'}${MOZ_PRINTER_NAME}" 
         * - which means that if the ${MOZ_PRINTER_NAME} env var is not empty
         * the "-P" option of lpr will be set to the printer name.
         */
#ifndef ARG_MAX
#define ARG_MAX 4096
#endif /* !ARG_MAX */
        /* |putenv()| will use the pointer to this buffer directly and will not
         * |strdup()| the content!!!! */
        static char envvar[ARG_MAX];

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

        /* We're using a |static| buffer (|envvar|) here to ensure that the
         * memory "remembered" in the env var pool does still exist when we
         * leave this context.
         * However we can't write to the buffer while it's memory is linked
         * to the env var pool - otherwise we may corrupt the pool.
         * Therefore we're feeding a "dummy" env name/value string to the pool
         * to "unlink" our static buffer (if it was set by an previous print
         * job), then we write to the buffer and finally we |putenv()| our
         * static buffer again.
         */
        PR_SetEnv("MOZ_PRINTER_NAME=dummy_value_to_make_putenv_happy");
        PRInt32 nchars = PR_snprintf(envvar, ARG_MAX,
                                  "MOZ_PRINTER_NAME=%s", printername);
        if (nchars < 0 || nchars >= ARG_MAX)
            sprintf(envvar, "MOZ_PRINTER_NAME=");

#ifdef DEBUG
        printf("setting printer name via '%s'\n", envvar);
#endif /* DEBUG */
        PR_SetEnv(envvar);
        
        aSpec->GetCommand(&mPrintSetup->print_cmd);
#ifndef VMS
        mPrintSetup->out = tmpfile();
        mPrintSetup->filename = nsnull;  
#else
        // We can not open a pipe and print the contents of it. Instead
        // we have to print to a file and then print that.
        
        mPrintSetup->filename = tempnam("SYS$SCRATCH:","MOZ_P");
        mPrintSetup->out = fopen(mPrintSetup->filename, "w");
#endif
      } else {
        const char *path;
        aSpec->GetPath(&path);
        mPrintSetup->filename = path;          
        mPrintSetup->out = fopen(mPrintSetup->filename, "w"); 
        if (!mPrintSetup->out)
          return NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE;
      }
#ifndef VMS
      mPrintSetup->tmpBody = tmpfile();
      NS_ENSURE_TRUE(mPrintSetup->tmpBody, NS_ERROR_FAILURE);
      mPrintSetup->tmpBody_filename = nsnull;
#else
      mPrintSetup->tmpBody_filename = tempnam("SYS$SCRATCH:", "MOZ_TT_P");
      mPrintSetup->tmpBody = fopen(mPrintSetup->tmpBody_filename, "w");
      NS_ENSURE_TRUE(mPrintSetup->tmpBody, NS_ERROR_FAILURE);
#endif
    } else 
        return NS_ERROR_FAILURE;

    /* make sure the open worked */

    if (!mPrintSetup->out)
      return NS_ERROR_GFX_PRINTER_CMD_FAILURE;
      
    mPrintContext = new PSContext();
    memset(mPrintContext, 0, sizeof(struct PSContext_));
    memset(pi, 0, sizeof(struct PrintInfo_));

    mPrintSetup->dpi = 72.0f;                  // dpi for externally sized items 
    aSpec->GetLandscape( landscape );
    fwidth  = mPrintSetup->paper_size->width;
    fheight = mPrintSetup->paper_size->height;

    if (landscape) {
      float temp;
      temp   = fwidth;
      fwidth = fheight;
      fheight = temp;
    }

    mPrintSetup->left   = (int)(mPrintSetup->paper_size->left   * mPrintSetup->dpi);
    mPrintSetup->top    = (int)(mPrintSetup->paper_size->top    * mPrintSetup->dpi);
    mPrintSetup->bottom = (int)(mPrintSetup->paper_size->bottom * mPrintSetup->dpi);
    mPrintSetup->right  = (int)(mPrintSetup->paper_size->right  * mPrintSetup->dpi);
    
    mPrintSetup->width  = (int)(fwidth  * mPrintSetup->dpi);
    mPrintSetup->height = (int)(fheight * mPrintSetup->dpi);
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
	                                    // "other" font is for encodings other than iso-8859-1 
    mPrintSetup->otherFontName[0] = nsnull;		   
  				                            // name of "other" PostScript font 
    mPrintSetup->otherFontInfo[0] = nsnull;	   
    // font info parsed from "other" afm file 
    mPrintSetup->otherFontCharSetID = 0;	      // charset ID of "other" font 
    //mPrintSetup->cx = nsnull;                 // original context, if available 
    pi->page_height = mPrintSetup->height * 10;	// Size of printable area on page 
    pi->page_width = mPrintSetup->width * 10;	// Size of printable area on page 
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
  dup->width = POINT_TO_PAGE(dup->width);
  dup->height = POINT_TO_PAGE(dup->height);
  dup->top = POINT_TO_PAGE(dup->top);
  dup->left = POINT_TO_PAGE(dup->left);
  dup->bottom = POINT_TO_PAGE(dup->bottom);
  dup->right = POINT_TO_PAGE(dup->right);
/*
  if (pi->landscape){
    dup->height = POINT_TO_PAGE(pi->width);
    dup->width = POINT_TO_PAGE(pi->height);
    //XXX Should I swap the margins too ??? 
    //XXX kaie: I don't think so... The user still sees the options
    //          named left margin etc.
  }	
*/
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

  f = mPrintContext->prSetup->out;
  fprintf(f, "%%!PS-Adobe-3.0\n");
  fprintf(f, "%%%%BoundingBox: %d %d %d %d\n",
              PAGE_TO_POINT_I(mPrintContext->prSetup->left),
	            PAGE_TO_POINT_I(mPrintContext->prSetup->top),
	            PAGE_TO_POINT_I(mPrintContext->prSetup->width-mPrintContext->prSetup->right),
	            PAGE_TO_POINT_I(mPrintContext->prSetup->height-(mPrintContext->prSetup->bottom + mPrintContext->prSetup->top)));

  nsXPIDLCString useragent;
  useragent.Assign("unknown"); /* Fallback */
  gPrefs->CopyCharPref("general.useragent.misc", getter_Copies(useragent));
  fprintf(f, "%%%%Creator: Mozilla PostScript module (%s/%lu)\n", useragent.get(), (unsigned long)NS_BUILD_ID);
  fprintf(f, "%%%%DocumentData: Clean8Bit\n");
  fprintf(f, "%%%%DocumentPaperSizes: %s\n", mPrintSetup->paper_size->name);
  fprintf(f, "%%%%Orientation: %s\n",
              (mPrintContext->prSetup->width < mPrintContext->prSetup->height) ? "Portrait" : "Landscape");

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

#ifdef OLDFONTS
  // output the fonts supported here    
  for (i = 0; i < N_FONTS; i++){
    fprintf(f, 
	          "/F%d\n"
	          "    /%s findfont\n"
	          "    dup length dict begin\n"
	          "	{1 index /FID ne {def} {pop pop} ifelse} forall\n"
	          "	/Encoding isolatin1encoding def\n"
	          "    currentdict end\n"
	          "definefont pop\n"
	          "/f%d { /csize exch def /F%d findfont csize scalefont setfont } bind def\n",
		        i, PSFE_MaskToFI[i]->name, i, i);
  }

  for (i = 0; i < N_FONTS; i++){
    if (mPrintContext->prSetup->otherFontName[i]) {
	    fprintf(f, 
	          "/of%d { /%s findfont exch scalefont setfont } bind def\n",
		        i, mPrintContext->prSetup->otherFontName[i]);
            //fprintf(f, "/of /of1;\n", mPrintContext->prSetup->otherFontName); 
    }
  }
#else
  for(i=0;i<NUM_AFM_FONTS;i++){
    fprintf(f, 
	          "/F%d\n"
	          "    /%s findfont\n"
	          "    dup length dict begin\n"
	          "	{1 index /FID ne {def} {pop pop} ifelse} forall\n"
	          "	/Encoding isolatin1encoding def\n"
	          "    currentdict end\n"
	          "definefont pop\n"
	          "/f%d { /csize exch def /F%d findfont csize scalefont setfont } bind def\n",
		        i, gSubstituteFonts[i].mPSName, i, i);

  }
#endif






  fprintf(f, "/rhc {\n");
  fprintf(f, "    {\n");
  fprintf(f, "        currentfile read {\n");
  fprintf(f, "	    dup 97 ge\n");
  fprintf(f, "		{ 87 sub true exit }\n");
  fprintf(f, "		{ dup 48 ge { 48 sub true exit } { pop } ifelse }\n");
  fprintf(f, "	    ifelse\n");
  fprintf(f, "	} {\n");
  fprintf(f, "	    false\n");
  fprintf(f, "	    exit\n");
  fprintf(f, "	} ifelse\n");
  fprintf(f, "    } loop\n");
  fprintf(f, "} bind def\n");
  fprintf(f, "\n");
  fprintf(f, "/cvgray { %% xtra_char npix cvgray - (string npix long)\n");
  fprintf(f, "    dup string\n");
  fprintf(f, "    0\n");
  fprintf(f, "    {\n");
  fprintf(f, "	rhc { cvr 4.784 mul } { exit } ifelse\n");
  fprintf(f, "	rhc { cvr 9.392 mul } { exit } ifelse\n");
  fprintf(f, "	rhc { cvr 1.824 mul } { exit } ifelse\n");
  fprintf(f, "	add add cvi 3 copy put pop\n");
  fprintf(f, "	1 add\n");
  fprintf(f, "	dup 3 index ge { exit } if\n");
  fprintf(f, "    } loop\n");
  fprintf(f, "    pop\n");
  fprintf(f, "    3 -1 roll 0 ne { rhc { pop } if } if\n");
  fprintf(f, "    exch pop\n");
  fprintf(f, "} bind def\n");
  fprintf(f, "\n");
  fprintf(f, "/smartimage12rgb { %% w h b [matrix] smartimage12rgb -\n");
  fprintf(f, "    /colorimage where {\n");
  fprintf(f, "	pop\n");
  fprintf(f, "	{ currentfile rowdata readhexstring pop }\n");
  fprintf(f, "	false 3\n");
  fprintf(f, "	colorimage\n");
  fprintf(f, "    } {\n");
  fprintf(f, "	exch pop 8 exch\n");
  fprintf(f, "	3 index 12 mul 8 mod 0 ne { 1 } { 0 } ifelse\n");
  fprintf(f, "	4 index\n");
  fprintf(f, "	6 2 roll\n");
  fprintf(f, "	{ 2 copy cvgray }\n");
  fprintf(f, "	image\n");
  fprintf(f, "	pop pop\n");
  fprintf(f, "    } ifelse\n");
  fprintf(f, "} def\n");
  fprintf(f,"/cshow { dup stringwidth pop 2 div neg 0 rmoveto show } bind def\n");  
  fprintf(f,"/rshow { dup stringwidth pop neg 0 rmoveto show } bind def\n");
  fprintf(f, "/BeginEPSF {\n");
  fprintf(f, "  /b4_Inc_state save def\n");
  fprintf(f, "  /dict_count countdictstack def\n");
  fprintf(f, "  /op_count count 1 sub def\n");
  fprintf(f, "  userdict begin\n");
  fprintf(f, "  /showpage {} def\n");
  fprintf(f, "  0 setgray 0 setlinecap 1 setlinewidth 0 setlinejoin\n");
  fprintf(f, "  10 setmiterlimit [] 0 setdash newpath\n");
  fprintf(f, "  /languagelevel where\n");
  fprintf(f, "  { pop languagelevel 1 ne\n");
  fprintf(f, "    { false setstrokeadjust false setoverprint } if\n");
  fprintf(f, "  } if\n");
  fprintf(f, "} bind def\n");
  fprintf(f, "/EndEPSF {\n");
  fprintf(f, "  count op_count sub {pop} repeat\n");
  fprintf(f, "  countdictstack dict_count sub {end} repeat\n");
  fprintf(f, "  b4_Inc_state restore\n");
  fprintf(f, "} bind def\n");

  fprintf(f, "\n");

  fprintf(f, "/UniDict    <<\n");
  fprintf(f, "16#0020    /space\n");
  fprintf(f, "16#0021    /exclam\n");
  fprintf(f, "16#0022    /quotedbl\n");
  fprintf(f, "16#0023    /numbersign\n");
  fprintf(f, "16#0024    /dollar\n");
  fprintf(f, "16#0025    /percent\n");
  fprintf(f, "16#0026    /ampersand\n");
  fprintf(f, "16#0027    /quotesingle\n");
  fprintf(f, "16#0028    /parenleft\n");
  fprintf(f, "16#0029    /parenright\n");
  fprintf(f, "16#002A    /asterisk\n");
  fprintf(f, "16#002B    /plus\n");
  fprintf(f, "16#002C    /comma\n");
  fprintf(f, "16#002D    /hyphen\n");
  fprintf(f, "16#002E    /period\n");
  fprintf(f, "16#002F    /slash\n");
  fprintf(f, "16#0030    /zero\n");
  fprintf(f, "16#0031    /one\n");
  fprintf(f, "16#0032    /two\n");
  fprintf(f, "16#0033    /three\n");
  fprintf(f, "16#0034    /four\n");
  fprintf(f, "16#0035    /five\n");
  fprintf(f, "16#0036    /six\n");
  fprintf(f, "16#0037    /seven\n");
  fprintf(f, "16#0038    /eight\n");
  fprintf(f, "16#0039    /nine\n");
  fprintf(f, "16#003A    /colon\n");
  fprintf(f, "16#003B    /semicolon\n");
  fprintf(f, "16#003C    /less\n");
  fprintf(f, "16#003D    /equal\n");
  fprintf(f, "16#003E    /greater\n");
  fprintf(f, "16#003F    /question\n");
  fprintf(f, "16#0040    /at\n");
  fprintf(f, "16#0041    /A\n");
  fprintf(f, "16#0042    /B\n");
  fprintf(f, "16#0043    /C\n");
  fprintf(f, "16#0044    /D\n");
  fprintf(f, "16#0045    /E\n");
  fprintf(f, "16#0046    /F\n");
  fprintf(f, "16#0047    /G\n");
  fprintf(f, "16#0048    /H\n");
  fprintf(f, "16#0049    /I\n");
  fprintf(f, "16#004A    /J\n");
  fprintf(f, "16#004B    /K\n");
  fprintf(f, "16#004C    /L\n");
  fprintf(f, "16#004D    /M\n");
  fprintf(f, "16#004E    /N\n");
  fprintf(f, "16#004F    /O\n");
  fprintf(f, "16#0050    /P\n");
  fprintf(f, "16#0051    /Q\n");
  fprintf(f, "16#0052    /R\n");
  fprintf(f, "16#0053    /S\n");
  fprintf(f, "16#0054    /T\n");
  fprintf(f, "16#0055    /U\n");
  fprintf(f, "16#0056    /V\n");
  fprintf(f, "16#0057    /W\n");
  fprintf(f, "16#0058    /X\n");
  fprintf(f, "16#0059    /Y\n");
  fprintf(f, "16#005A    /Z\n");
  fprintf(f, "16#005B    /bracketleft\n");
  fprintf(f, "16#005C    /backslash\n");
  fprintf(f, "16#005D    /bracketright\n");
  fprintf(f, "16#005E    /asciicircum\n");
  fprintf(f, "16#005F    /underscore\n");
  fprintf(f, "16#0060    /grave\n");
  fprintf(f, "16#0061    /a\n");
  fprintf(f, "16#0062    /b\n");
  fprintf(f, "16#0063    /c\n");
  fprintf(f, "16#0064    /d\n");
  fprintf(f, "16#0065    /e\n");
  fprintf(f, "16#0066    /f\n");
  fprintf(f, "16#0067    /g\n");
  fprintf(f, "16#0068    /h\n");
  fprintf(f, "16#0069    /i\n");
  fprintf(f, "16#006A    /j\n");
  fprintf(f, "16#006B    /k\n");
  fprintf(f, "16#006C    /l\n");
  fprintf(f, "16#006D    /m\n");
  fprintf(f, "16#006E    /n\n");
  fprintf(f, "16#006F    /o\n");
  fprintf(f, "16#0070    /p\n");
  fprintf(f, "16#0071    /q\n");
  fprintf(f, "16#0072    /r\n");
  fprintf(f, "16#0073    /s\n");
  fprintf(f, "16#0074    /t\n");
  fprintf(f, "16#0075    /u\n");
  fprintf(f, "16#0076    /v\n");
  fprintf(f, "16#0077    /w\n");
  fprintf(f, "16#0078    /x\n");
  fprintf(f, "16#0079    /y\n");
  fprintf(f, "16#007A    /z\n");
  fprintf(f, "16#007B    /braceleft\n");
  fprintf(f, "16#007C    /bar\n");
  fprintf(f, "16#007D    /braceright\n");
  fprintf(f, "16#007E    /asciitilde\n");
  fprintf(f, "16#00A0    /space\n");
  fprintf(f, "16#00A1    /exclamdown\n");
  fprintf(f, "16#00A2    /cent\n");
  fprintf(f, "16#00A3    /sterling\n");
  fprintf(f, "16#00A4    /currency\n");
  fprintf(f, "16#00A5    /yen\n");
  fprintf(f, "16#00A6    /brokenbar\n");
  fprintf(f, "16#00A7    /section\n");
  fprintf(f, "16#00A8    /dieresis\n");
  fprintf(f, "16#00A9    /copyright\n");
  fprintf(f, "16#00AA    /ordfeminine\n");
  fprintf(f, "16#00AB    /guillemotleft\n");
  fprintf(f, "16#00AC    /logicalnot\n");
  fprintf(f, "16#00AD    /hyphen\n");
  fprintf(f, "16#00AE    /registered\n");
  fprintf(f, "16#00AF    /macron\n");
  fprintf(f, "16#00B0    /degree\n");
  fprintf(f, "16#00B1    /plusminus\n");
  fprintf(f, "16#00B2    /twosuperior\n");
  fprintf(f, "16#00B3    /threesuperior\n");
  fprintf(f, "16#00B4    /acute\n");
  fprintf(f, "16#00B5    /mu\n");
  fprintf(f, "16#00B6    /paragraph\n");
  fprintf(f, "16#00B7    /periodcentered\n");
  fprintf(f, "16#00B8    /cedilla\n");
  fprintf(f, "16#00B9    /onesuperior\n");
  fprintf(f, "16#00BA    /ordmasculine\n");
  fprintf(f, "16#00BB    /guillemotright\n");
  fprintf(f, "16#00BC    /onequarter\n");
  fprintf(f, "16#00BD    /onehalf\n");
  fprintf(f, "16#00BE    /threequarters\n");
  fprintf(f, "16#00BF    /questiondown\n");
  fprintf(f, "16#00C0    /Agrave\n");
  fprintf(f, "16#00C1    /Aacute\n");
  fprintf(f, "16#00C2    /Acircumflex\n");
  fprintf(f, "16#00C3    /Atilde\n");
  fprintf(f, "16#00C4    /Adieresis\n");
  fprintf(f, "16#00C5    /Aring\n");
  fprintf(f, "16#00C6    /AE\n");
  fprintf(f, "16#00C7    /Ccedilla\n");
  fprintf(f, "16#00C8    /Egrave\n");
  fprintf(f, "16#00C9    /Eacute\n");
  fprintf(f, "16#00CA    /Ecircumflex\n");
  fprintf(f, "16#00CB    /Edieresis\n");
  fprintf(f, "16#00CC    /Igrave\n");
  fprintf(f, "16#00CD    /Iacute\n");
  fprintf(f, "16#00CE    /Icircumflex\n");
  fprintf(f, "16#00CF    /Idieresis\n");
  fprintf(f, "16#00D0    /Eth\n");
  fprintf(f, "16#00D1    /Ntilde\n");
  fprintf(f, "16#00D2    /Ograve\n");
  fprintf(f, "16#00D3    /Oacute\n");
  fprintf(f, "16#00D4    /Ocircumflex\n");
  fprintf(f, "16#00D5    /Otilde\n");
  fprintf(f, "16#00D6    /Odieresis\n");
  fprintf(f, "16#00D7    /multiply\n");
  fprintf(f, "16#00D8    /Oslash\n");
  fprintf(f, "16#00D9    /Ugrave\n");
  fprintf(f, "16#00DA    /Uacute\n");
  fprintf(f, "16#00DB    /Ucircumflex\n");
  fprintf(f, "16#00DC    /Udieresis\n");
  fprintf(f, "16#00DD    /Yacute\n");
  fprintf(f, "16#00DE    /Thorn\n");
  fprintf(f, "16#00DF    /germandbls\n");
  fprintf(f, "16#00E0    /agrave\n");
  fprintf(f, "16#00E1    /aacute\n");
  fprintf(f, "16#00E2    /acircumflex\n");
  fprintf(f, "16#00E3    /atilde\n");
  fprintf(f, "16#00E4    /adieresis\n");
  fprintf(f, "16#00E5    /aring\n");
  fprintf(f, "16#00E6    /ae\n");
  fprintf(f, "16#00E7    /ccedilla\n");
  fprintf(f, "16#00E8    /egrave\n");
  fprintf(f, "16#00E9    /eacute\n");
  fprintf(f, "16#00EA    /ecircumflex\n");
  fprintf(f, "16#00EB    /edieresis\n");
  fprintf(f, "16#00EC    /igrave\n");
  fprintf(f, "16#00ED    /iacute\n");
  fprintf(f, "16#00EE    /icircumflex\n");
  fprintf(f, "16#00EF    /idieresis\n");
  fprintf(f, "16#00F0    /eth\n");
  fprintf(f, "16#00F1    /ntilde\n");
  fprintf(f, "16#00F2    /ograve\n");
  fprintf(f, "16#00F3    /oacute\n");
  fprintf(f, "16#00F4    /ocircumflex\n");
  fprintf(f, "16#00F5    /otilde\n");
  fprintf(f, "16#00F6    /odieresis\n");
  fprintf(f, "16#00F7    /divide\n");
  fprintf(f, "16#00F8    /oslash\n");
  fprintf(f, "16#00F9    /ugrave\n");
  fprintf(f, "16#00FA    /uacute\n");
  fprintf(f, "16#00FB    /ucircumflex\n");
  fprintf(f, "16#00FC    /udieresis\n");
  fprintf(f, "16#00FD    /yacute\n");
  fprintf(f, "16#00FE    /thorn\n");
  fprintf(f, "16#00FF    /ydieresis\n");
  fprintf(f, "16#0100    /Amacron\n");
  fprintf(f, "16#0101    /amacron\n");
  fprintf(f, "16#0102    /Abreve\n");
  fprintf(f, "16#0103    /abreve\n");
  fprintf(f, "16#0104    /Aogonek\n");
  fprintf(f, "16#0105    /aogonek\n");
  fprintf(f, "16#0106    /Cacute\n");
  fprintf(f, "16#0107    /cacute\n");
  fprintf(f, "16#0108    /Ccircumflex\n");
  fprintf(f, "16#0109    /ccircumflex\n");
  fprintf(f, "16#010A    /Cdotaccent\n");
  fprintf(f, "16#010B    /cdotaccent\n");
  fprintf(f, "16#010C    /Ccaron\n");
  fprintf(f, "16#010D    /ccaron\n");
  fprintf(f, "16#010E    /Dcaron\n");
  fprintf(f, "16#010F    /dcaron\n");
  fprintf(f, "16#0110    /Dcroat\n");
  fprintf(f, "16#0111    /dcroat\n");
  fprintf(f, "16#0112    /Emacron\n");
  fprintf(f, "16#0113    /emacron\n");
  fprintf(f, "16#0114    /Ebreve\n");
  fprintf(f, "16#0115    /ebreve\n");
  fprintf(f, "16#0116    /Edotaccent\n");
  fprintf(f, "16#0117    /edotaccent\n");
  fprintf(f, "16#0118    /Eogonek\n");
  fprintf(f, "16#0119    /eogonek\n");
  fprintf(f, "16#011A    /Ecaron\n");
  fprintf(f, "16#011B    /ecaron\n");
  fprintf(f, "16#011C    /Gcircumflex\n");
  fprintf(f, "16#011D    /gcircumflex\n");
  fprintf(f, "16#011E    /Gbreve\n");
  fprintf(f, "16#011F    /gbreve\n");
  fprintf(f, "16#0120    /Gdotaccent\n");
  fprintf(f, "16#0121    /gdotaccent\n");
  fprintf(f, "16#0122    /Gcommaaccent\n");
  fprintf(f, "16#0123    /gcommaaccent\n");
  fprintf(f, "16#0124    /Hcircumflex\n");
  fprintf(f, "16#0125    /hcircumflex\n");
  fprintf(f, "16#0126    /Hbar\n");
  fprintf(f, "16#0127    /hbar\n");
  fprintf(f, "16#0128    /Itilde\n");
  fprintf(f, "16#0129    /itilde\n");
  fprintf(f, "16#012A    /Imacron\n");
  fprintf(f, "16#012B    /imacron\n");
  fprintf(f, "16#012C    /Ibreve\n");
  fprintf(f, "16#012D    /ibreve\n");
  fprintf(f, "16#012E    /Iogonek\n");
  fprintf(f, "16#012F    /iogonek\n");
  fprintf(f, "16#0130    /Idotaccent\n");
  fprintf(f, "16#0131    /dotlessi\n");
  fprintf(f, "16#0132    /IJ\n");
  fprintf(f, "16#0133    /ij\n");
  fprintf(f, "16#0134    /Jcircumflex\n");
  fprintf(f, "16#0135    /jcircumflex\n");
  fprintf(f, "16#0136    /Kcommaaccent\n");
  fprintf(f, "16#0137    /kcommaaccent\n");
  fprintf(f, "16#0138    /kgreenlandic\n");
  fprintf(f, "16#0139    /Lacute\n");
  fprintf(f, "16#013A    /lacute\n");
  fprintf(f, "16#013B    /Lcommaaccent\n");
  fprintf(f, "16#013C    /lcommaaccent\n");
  fprintf(f, "16#013D    /Lcaron\n");
  fprintf(f, "16#013E    /lcaron\n");
  fprintf(f, "16#013F    /Ldot\n");
  fprintf(f, "16#0140    /ldot\n");
  fprintf(f, "16#0141    /Lslash\n");
  fprintf(f, "16#0142    /lslash\n");
  fprintf(f, "16#0143    /Nacute\n");
  fprintf(f, "16#0144    /nacute\n");
  fprintf(f, "16#0145    /Ncommaaccent\n");
  fprintf(f, "16#0146    /ncommaaccent\n");
  fprintf(f, "16#0147    /Ncaron\n");
  fprintf(f, "16#0148    /ncaron\n");
  fprintf(f, "16#0149    /napostrophe\n");
  fprintf(f, "16#014A    /Eng\n");
  fprintf(f, "16#014B    /eng\n");
  fprintf(f, "16#014C    /Omacron\n");
  fprintf(f, "16#014D    /omacron\n");
  fprintf(f, "16#014E    /Obreve\n");
  fprintf(f, "16#014F    /obreve\n");
  fprintf(f, "16#0150    /Ohungarumlaut\n");
  fprintf(f, "16#0151    /ohungarumlaut\n");
  fprintf(f, "16#0152    /OE\n");
  fprintf(f, "16#0153    /oe\n");
  fprintf(f, "16#0154    /Racute\n");
  fprintf(f, "16#0155    /racute\n");
  fprintf(f, "16#0156    /Rcommaaccent\n");
  fprintf(f, "16#0157    /rcommaaccent\n");
  fprintf(f, "16#0158    /Rcaron\n");
  fprintf(f, "16#0159    /rcaron\n");
  fprintf(f, "16#015A    /Sacute\n");
  fprintf(f, "16#015B    /sacute\n");
  fprintf(f, "16#015C    /Scircumflex\n");
  fprintf(f, "16#015D    /scircumflex\n");
  fprintf(f, "16#015E    /Scedilla\n");
  fprintf(f, "16#015F    /scedilla\n");
  fprintf(f, "16#0160    /Scaron\n");
  fprintf(f, "16#0161    /scaron\n");
  fprintf(f, "16#0162    /Tcommaaccent\n");
  fprintf(f, "16#0163    /tcommaaccent\n");
  fprintf(f, "16#0164    /Tcaron\n");
  fprintf(f, "16#0165    /tcaron\n");
  fprintf(f, "16#0166    /Tbar\n");
  fprintf(f, "16#0167    /tbar\n");
  fprintf(f, "16#0168    /Utilde\n");
  fprintf(f, "16#0169    /utilde\n");
  fprintf(f, "16#016A    /Umacron\n");
  fprintf(f, "16#016B    /umacron\n");
  fprintf(f, "16#016C    /Ubreve\n");
  fprintf(f, "16#016D    /ubreve\n");
  fprintf(f, "16#016E    /Uring\n");
  fprintf(f, "16#016F    /uring\n");
  fprintf(f, "16#0170    /Uhungarumlaut\n");
  fprintf(f, "16#0171    /uhungarumlaut\n");
  fprintf(f, "16#0172    /Uogonek\n");
  fprintf(f, "16#0173    /uogonek\n");
  fprintf(f, "16#0174    /Wcircumflex\n");
  fprintf(f, "16#0175    /wcircumflex\n");
  fprintf(f, "16#0176    /Ycircumflex\n");
  fprintf(f, "16#0177    /ycircumflex\n");
  fprintf(f, "16#0178    /Ydieresis\n");
  fprintf(f, "16#0179    /Zacute\n");
  fprintf(f, "16#017A    /zacute\n");
  fprintf(f, "16#017B    /Zdotaccent\n");
  fprintf(f, "16#017C    /zdotaccent\n");
  fprintf(f, "16#017D    /Zcaron\n");
  fprintf(f, "16#017E    /zcaron\n");
  fprintf(f, "16#017F    /longs\n");
  fprintf(f, "16#0192    /florin\n");
  fprintf(f, "16#01A0    /Ohorn\n");
  fprintf(f, "16#01A1    /ohorn\n");
  fprintf(f, "16#01AF    /Uhorn\n");
  fprintf(f, "16#01B0    /uhorn\n");
  fprintf(f, "16#01E6    /Gcaron\n");
  fprintf(f, "16#01E7    /gcaron\n");
  fprintf(f, "16#01FA    /Aringacute\n");
  fprintf(f, "16#01FB    /aringacute\n");
  fprintf(f, "16#01FC    /AEacute\n");
  fprintf(f, "16#01FD    /aeacute\n");
  fprintf(f, "16#01FE    /Oslashacute\n");
  fprintf(f, "16#01FF    /oslashacute\n");
  fprintf(f, "16#0218    /Scommaaccent\n");
  fprintf(f, "16#0219    /scommaaccent\n");
  fprintf(f, "16#021A    /Tcommaaccent\n");
  fprintf(f, "16#021B    /tcommaaccent\n");
  fprintf(f, "16#02BC    /afii57929\n");
  fprintf(f, "16#02BD    /afii64937\n");
  fprintf(f, "16#02C6    /circumflex\n");
  fprintf(f, "16#02C7    /caron\n");
  fprintf(f, "16#02C9    /macron\n");
  fprintf(f, "16#02D8    /breve\n");
  fprintf(f, "16#02D9    /dotaccent\n");
  fprintf(f, "16#02DA    /ring\n");
  fprintf(f, "16#02DB    /ogonek\n");
  fprintf(f, "16#02DC    /tilde\n");
  fprintf(f, "16#02DD    /hungarumlaut\n");
  fprintf(f, "16#0300    /gravecomb\n");
  fprintf(f, "16#0301    /acutecomb\n");
  fprintf(f, "16#0303    /tildecomb\n");
  fprintf(f, "16#0309    /hookabovecomb\n");
  fprintf(f, "16#0323    /dotbelowcomb\n");
  fprintf(f, "16#0384    /tonos\n");
  fprintf(f, "16#0385    /dieresistonos\n");
  fprintf(f, "16#0386    /Alphatonos\n");
  fprintf(f, "16#0387    /anoteleia\n");
  fprintf(f, "16#0388    /Epsilontonos\n");
  fprintf(f, "16#0389    /Etatonos\n");
  fprintf(f, "16#038A    /Iotatonos\n");
  fprintf(f, "16#038C    /Omicrontonos\n");
  fprintf(f, "16#038E    /Upsilontonos\n");
  fprintf(f, "16#038F    /Omegatonos\n");
  fprintf(f, "16#0390    /iotadieresistonos\n");
  fprintf(f, "16#0391    /Alpha\n");
  fprintf(f, "16#0392    /Beta\n");
  fprintf(f, "16#0393    /Gamma\n");
  fprintf(f, "16#0394    /Delta\n");
  fprintf(f, "16#0395    /Epsilon\n");
  fprintf(f, "16#0396    /Zeta\n");
  fprintf(f, "16#0397    /Eta\n");
  fprintf(f, "16#0398    /Theta\n");
  fprintf(f, "16#0399    /Iota\n");
  fprintf(f, "16#039A    /Kappa\n");
  fprintf(f, "16#039B    /Lambda\n");
  fprintf(f, "16#039C    /Mu\n");
  fprintf(f, "16#039D    /Nu\n");
  fprintf(f, "16#039E    /Xi\n");
  fprintf(f, "16#039F    /Omicron\n");
  fprintf(f, "16#03A0    /Pi\n");
  fprintf(f, "16#03A1    /Rho\n");
  fprintf(f, "16#03A3    /Sigma\n");
  fprintf(f, "16#03A4    /Tau\n");
  fprintf(f, "16#03A5    /Upsilon\n");
  fprintf(f, "16#03A6    /Phi\n");
  fprintf(f, "16#03A7    /Chi\n");
  fprintf(f, "16#03A8    /Psi\n");
  fprintf(f, "16#03A9    /Omega\n");
  fprintf(f, "16#03AA    /Iotadieresis\n");
  fprintf(f, "16#03AB    /Upsilondieresis\n");
  fprintf(f, "16#03AC    /alphatonos\n");
  fprintf(f, "16#03AD    /epsilontonos\n");
  fprintf(f, "16#03AE    /etatonos\n");
  fprintf(f, "16#03AF    /iotatonos\n");
  fprintf(f, "16#03B0    /upsilondieresistonos\n");
  fprintf(f, "16#03B1    /alpha\n");
  fprintf(f, "16#03B2    /beta\n");
  fprintf(f, "16#03B3    /gamma\n");
  fprintf(f, "16#03B4    /delta\n");
  fprintf(f, "16#03B5    /epsilon\n");
  fprintf(f, "16#03B6    /zeta\n");
  fprintf(f, "16#03B7    /eta\n");
  fprintf(f, "16#03B8    /theta\n");
  fprintf(f, "16#03B9    /iota\n");
  fprintf(f, "16#03BA    /kappa\n");
  fprintf(f, "16#03BB    /lambda\n");
  fprintf(f, "16#03BC    /mu\n");
  fprintf(f, "16#03BD    /nu\n");
  fprintf(f, "16#03BE    /xi\n");
  fprintf(f, "16#03BF    /omicron\n");
  fprintf(f, "16#03C0    /pi\n");
  fprintf(f, "16#03C1    /rho\n");
  fprintf(f, "16#03C2    /sigma1\n");
  fprintf(f, "16#03C3    /sigma\n");
  fprintf(f, "16#03C4    /tau\n");
  fprintf(f, "16#03C5    /upsilon\n");
  fprintf(f, "16#03C6    /phi\n");
  fprintf(f, "16#03C7    /chi\n");
  fprintf(f, "16#03C8    /psi\n");
  fprintf(f, "16#03C9    /omega\n");
  fprintf(f, "16#03CA    /iotadieresis\n");
  fprintf(f, "16#03CB    /upsilondieresis\n");
  fprintf(f, "16#03CC    /omicrontonos\n");
  fprintf(f, "16#03CD    /upsilontonos\n");
  fprintf(f, "16#03CE    /omegatonos\n");
  fprintf(f, "16#03D1    /theta1\n");
  fprintf(f, "16#03D2    /Upsilon1\n");
  fprintf(f, "16#03D5    /phi1\n");
  fprintf(f, "16#03D6    /omega1\n");
  fprintf(f, "16#0401    /afii10023\n");
  fprintf(f, "16#0402    /afii10051\n");
  fprintf(f, "16#0403    /afii10052\n");
  fprintf(f, "16#0404    /afii10053\n");
  fprintf(f, "16#0405    /afii10054\n");
  fprintf(f, "16#0406    /afii10055\n");
  fprintf(f, "16#0407    /afii10056\n");
  fprintf(f, "16#0408    /afii10057\n");
  fprintf(f, "16#0409    /afii10058\n");
  fprintf(f, "16#040A    /afii10059\n");
  fprintf(f, "16#040B    /afii10060\n");
  fprintf(f, "16#040C    /afii10061\n");
  fprintf(f, "16#040E    /afii10062\n");
  fprintf(f, "16#040F    /afii10145\n");
  fprintf(f, "16#0410    /afii10017\n");
  fprintf(f, "16#0411    /afii10018\n");
  fprintf(f, "16#0412    /afii10019\n");
  fprintf(f, "16#0413    /afii10020\n");
  fprintf(f, "16#0414    /afii10021\n");
  fprintf(f, "16#0415    /afii10022\n");
  fprintf(f, "16#0416    /afii10024\n");
  fprintf(f, "16#0417    /afii10025\n");
  fprintf(f, "16#0418    /afii10026\n");
  fprintf(f, "16#0419    /afii10027\n");
  fprintf(f, "16#041A    /afii10028\n");
  fprintf(f, "16#041B    /afii10029\n");
  fprintf(f, "16#041C    /afii10030\n");
  fprintf(f, "16#041D    /afii10031\n");
  fprintf(f, "16#041E    /afii10032\n");
  fprintf(f, "16#041F    /afii10033\n");
  fprintf(f, "16#0420    /afii10034\n");
  fprintf(f, "16#0421    /afii10035\n");
  fprintf(f, "16#0422    /afii10036\n");
  fprintf(f, "16#0423    /afii10037\n");
  fprintf(f, "16#0424    /afii10038\n");
  fprintf(f, "16#0425    /afii10039\n");
  fprintf(f, "16#0426    /afii10040\n");
  fprintf(f, "16#0427    /afii10041\n");
  fprintf(f, "16#0428    /afii10042\n");
  fprintf(f, "16#0429    /afii10043\n");
  fprintf(f, "16#042A    /afii10044\n");
  fprintf(f, "16#042B    /afii10045\n");
  fprintf(f, "16#042C    /afii10046\n");
  fprintf(f, "16#042D    /afii10047\n");
  fprintf(f, "16#042E    /afii10048\n");
  fprintf(f, "16#042F    /afii10049\n");
  fprintf(f, "16#0430    /afii10065\n");
  fprintf(f, "16#0431    /afii10066\n");
  fprintf(f, "16#0432    /afii10067\n");
  fprintf(f, "16#0433    /afii10068\n");
  fprintf(f, "16#0434    /afii10069\n");
  fprintf(f, "16#0435    /afii10070\n");
  fprintf(f, "16#0436    /afii10072\n");
  fprintf(f, "16#0437    /afii10073\n");
  fprintf(f, "16#0438    /afii10074\n");
  fprintf(f, "16#0439    /afii10075\n");
  fprintf(f, "16#043A    /afii10076\n");
  fprintf(f, "16#043B    /afii10077\n");
  fprintf(f, "16#043C    /afii10078\n");
  fprintf(f, "16#043D    /afii10079\n");
  fprintf(f, "16#043E    /afii10080\n");
  fprintf(f, "16#043F    /afii10081\n");
  fprintf(f, "16#0440    /afii10082\n");
  fprintf(f, "16#0441    /afii10083\n");
  fprintf(f, "16#0442    /afii10084\n");
  fprintf(f, "16#0443    /afii10085\n");
  fprintf(f, "16#0444    /afii10086\n");
  fprintf(f, "16#0445    /afii10087\n");
  fprintf(f, "16#0446    /afii10088\n");
  fprintf(f, "16#0447    /afii10089\n");
  fprintf(f, "16#0448    /afii10090\n");
  fprintf(f, "16#0449    /afii10091\n");
  fprintf(f, "16#044A    /afii10092\n");
  fprintf(f, "16#044B    /afii10093\n");
  fprintf(f, "16#044C    /afii10094\n");
  fprintf(f, "16#044D    /afii10095\n");
  fprintf(f, "16#044E    /afii10096\n");
  fprintf(f, "16#044F    /afii10097\n");
  fprintf(f, "16#0451    /afii10071\n");
  fprintf(f, "16#0452    /afii10099\n");
  fprintf(f, "16#0453    /afii10100\n");
  fprintf(f, "16#0454    /afii10101\n");
  fprintf(f, "16#0455    /afii10102\n");
  fprintf(f, "16#0456    /afii10103\n");
  fprintf(f, "16#0457    /afii10104\n");
  fprintf(f, "16#0458    /afii10105\n");
  fprintf(f, "16#0459    /afii10106\n");
  fprintf(f, "16#045A    /afii10107\n");
  fprintf(f, "16#045B    /afii10108\n");
  fprintf(f, "16#045C    /afii10109\n");
  fprintf(f, "16#045E    /afii10110\n");
  fprintf(f, "16#045F    /afii10193\n");
  fprintf(f, "16#0462    /afii10146\n");
  fprintf(f, "16#0463    /afii10194\n");
  fprintf(f, "16#0472    /afii10147\n");
  fprintf(f, "16#0473    /afii10195\n");
  fprintf(f, "16#0474    /afii10148\n");
  fprintf(f, "16#0475    /afii10196\n");
  fprintf(f, "16#0490    /afii10050\n");
  fprintf(f, "16#0491    /afii10098\n");
  fprintf(f, "16#04D9    /afii10846\n");
  fprintf(f, "16#05B0    /afii57799\n");
  fprintf(f, "16#05B1    /afii57801\n");
  fprintf(f, "16#05B2    /afii57800\n");
  fprintf(f, "16#05B3    /afii57802\n");
  fprintf(f, "16#05B4    /afii57793\n");
  fprintf(f, "16#05B5    /afii57794\n");
  fprintf(f, "16#05B6    /afii57795\n");
  fprintf(f, "16#05B7    /afii57798\n");
  fprintf(f, "16#05B8    /afii57797\n");
  fprintf(f, "16#05B9    /afii57806\n");
  fprintf(f, "16#05BB    /afii57796\n");
  fprintf(f, "16#05BC    /afii57807\n");
  fprintf(f, "16#05BD    /afii57839\n");
  fprintf(f, "16#05BE    /afii57645\n");
  fprintf(f, "16#05BF    /afii57841\n");
  fprintf(f, "16#05C0    /afii57842\n");
  fprintf(f, "16#05C1    /afii57804\n");
  fprintf(f, "16#05C2    /afii57803\n");
  fprintf(f, "16#05C3    /afii57658\n");
  fprintf(f, "16#05D0    /afii57664\n");
  fprintf(f, "16#05D1    /afii57665\n");
  fprintf(f, "16#05D2    /afii57666\n");
  fprintf(f, "16#05D3    /afii57667\n");
  fprintf(f, "16#05D4    /afii57668\n");
  fprintf(f, "16#05D5    /afii57669\n");
  fprintf(f, "16#05D6    /afii57670\n");
  fprintf(f, "16#05D7    /afii57671\n");
  fprintf(f, "16#05D8    /afii57672\n");
  fprintf(f, "16#05D9    /afii57673\n");
  fprintf(f, "16#05DA    /afii57674\n");
  fprintf(f, "16#05DB    /afii57675\n");
  fprintf(f, "16#05DC    /afii57676\n");
  fprintf(f, "16#05DD    /afii57677\n");
  fprintf(f, "16#05DE    /afii57678\n");
  fprintf(f, "16#05DF    /afii57679\n");
  fprintf(f, "16#05E0    /afii57680\n");
  fprintf(f, "16#05E1    /afii57681\n");
  fprintf(f, "16#05E2    /afii57682\n");
  fprintf(f, "16#05E3    /afii57683\n");
  fprintf(f, "16#05E4    /afii57684\n");
  fprintf(f, "16#05E5    /afii57685\n");
  fprintf(f, "16#05E6    /afii57686\n");
  fprintf(f, "16#05E7    /afii57687\n");
  fprintf(f, "16#05E8    /afii57688\n");
  fprintf(f, "16#05E9    /afii57689\n");
  fprintf(f, "16#05EA    /afii57690\n");
  fprintf(f, "16#05F0    /afii57716\n");
  fprintf(f, "16#05F1    /afii57717\n");
  fprintf(f, "16#05F2    /afii57718\n");
  fprintf(f, "16#060C    /afii57388\n");
  fprintf(f, "16#061B    /afii57403\n");
  fprintf(f, "16#061F    /afii57407\n");
  fprintf(f, "16#0621    /afii57409\n");
  fprintf(f, "16#0622    /afii57410\n");
  fprintf(f, "16#0623    /afii57411\n");
  fprintf(f, "16#0624    /afii57412\n");
  fprintf(f, "16#0625    /afii57413\n");
  fprintf(f, "16#0626    /afii57414\n");
  fprintf(f, "16#0627    /afii57415\n");
  fprintf(f, "16#0628    /afii57416\n");
  fprintf(f, "16#0629    /afii57417\n");
  fprintf(f, "16#062A    /afii57418\n");
  fprintf(f, "16#062B    /afii57419\n");
  fprintf(f, "16#062C    /afii57420\n");
  fprintf(f, "16#062D    /afii57421\n");
  fprintf(f, "16#062E    /afii57422\n");
  fprintf(f, "16#062F    /afii57423\n");
  fprintf(f, "16#0630    /afii57424\n");
  fprintf(f, "16#0631    /afii57425\n");
  fprintf(f, "16#0632    /afii57426\n");
  fprintf(f, "16#0633    /afii57427\n");
  fprintf(f, "16#0634    /afii57428\n");
  fprintf(f, "16#0635    /afii57429\n");
  fprintf(f, "16#0636    /afii57430\n");
  fprintf(f, "16#0637    /afii57431\n");
  fprintf(f, "16#0638    /afii57432\n");
  fprintf(f, "16#0639    /afii57433\n");
  fprintf(f, "16#063A    /afii57434\n");
  fprintf(f, "16#0640    /afii57440\n");
  fprintf(f, "16#0641    /afii57441\n");
  fprintf(f, "16#0642    /afii57442\n");
  fprintf(f, "16#0643    /afii57443\n");
  fprintf(f, "16#0644    /afii57444\n");
  fprintf(f, "16#0645    /afii57445\n");
  fprintf(f, "16#0646    /afii57446\n");
  fprintf(f, "16#0647    /afii57470\n");
  fprintf(f, "16#0648    /afii57448\n");
  fprintf(f, "16#0649    /afii57449\n");
  fprintf(f, "16#064A    /afii57450\n");
  fprintf(f, "16#064B    /afii57451\n");
  fprintf(f, "16#064C    /afii57452\n");
  fprintf(f, "16#064D    /afii57453\n");
  fprintf(f, "16#064E    /afii57454\n");
  fprintf(f, "16#064F    /afii57455\n");
  fprintf(f, "16#0650    /afii57456\n");
  fprintf(f, "16#0651    /afii57457\n");
  fprintf(f, "16#0652    /afii57458\n");
  fprintf(f, "16#0660    /afii57392\n");
  fprintf(f, "16#0661    /afii57393\n");
  fprintf(f, "16#0662    /afii57394\n");
  fprintf(f, "16#0663    /afii57395\n");
  fprintf(f, "16#0664    /afii57396\n");
  fprintf(f, "16#0665    /afii57397\n");
  fprintf(f, "16#0666    /afii57398\n");
  fprintf(f, "16#0667    /afii57399\n");
  fprintf(f, "16#0668    /afii57400\n");
  fprintf(f, "16#0669    /afii57401\n");
  fprintf(f, "16#066A    /afii57381\n");
  fprintf(f, "16#066D    /afii63167\n");
  fprintf(f, "16#0679    /afii57511\n");
  fprintf(f, "16#067E    /afii57506\n");
  fprintf(f, "16#0686    /afii57507\n");
  fprintf(f, "16#0688    /afii57512\n");
  fprintf(f, "16#0691    /afii57513\n");
  fprintf(f, "16#0698    /afii57508\n");
  fprintf(f, "16#06A4    /afii57505\n");
  fprintf(f, "16#06AF    /afii57509\n");
  fprintf(f, "16#06BA    /afii57514\n");
  fprintf(f, "16#06D2    /afii57519\n");
  fprintf(f, "16#06D5    /afii57534\n");
  fprintf(f, "16#1E80    /Wgrave\n");
  fprintf(f, "16#1E81    /wgrave\n");
  fprintf(f, "16#1E82    /Wacute\n");
  fprintf(f, "16#1E83    /wacute\n");
  fprintf(f, "16#1E84    /Wdieresis\n");
  fprintf(f, "16#1E85    /wdieresis\n");
  fprintf(f, "16#1EF2    /Ygrave\n");
  fprintf(f, "16#1EF3    /ygrave\n");
  fprintf(f, "16#200C    /afii61664\n");
  fprintf(f, "16#200D    /afii301\n");
  fprintf(f, "16#200E    /afii299\n");
  fprintf(f, "16#200F    /afii300\n");
  fprintf(f, "16#2012    /figuredash\n");
  fprintf(f, "16#2013    /endash\n");
  fprintf(f, "16#2014    /emdash\n");
  fprintf(f, "16#2015    /afii00208\n");
  fprintf(f, "16#2017    /underscoredbl\n");
  fprintf(f, "16#2018    /quoteleft\n");
  fprintf(f, "16#2019    /quoteright\n");
  fprintf(f, "16#201A    /quotesinglbase\n");
  fprintf(f, "16#201B    /quotereversed\n");
  fprintf(f, "16#201C    /quotedblleft\n");
  fprintf(f, "16#201D    /quotedblright\n");
  fprintf(f, "16#201E    /quotedblbase\n");
  fprintf(f, "16#2020    /dagger\n");
  fprintf(f, "16#2021    /daggerdbl\n");
  fprintf(f, "16#2022    /bullet\n");
  fprintf(f, "16#2024    /onedotenleader\n");
  fprintf(f, "16#2025    /twodotenleader\n");
  fprintf(f, "16#2026    /ellipsis\n");
  fprintf(f, "16#202C    /afii61573\n");
  fprintf(f, "16#202D    /afii61574\n");
  fprintf(f, "16#202E    /afii61575\n");
  fprintf(f, "16#2030    /perthousand\n");
  fprintf(f, "16#2032    /minute\n");
  fprintf(f, "16#2033    /second\n");
  fprintf(f, "16#2039    /guilsinglleft\n");
  fprintf(f, "16#203A    /guilsinglright\n");
  fprintf(f, "16#203C    /exclamdbl\n");
  fprintf(f, "16#2044    /fraction\n");
  fprintf(f, "16#2070    /zerosuperior\n");
  fprintf(f, "16#2074    /foursuperior\n");
  fprintf(f, "16#2075    /fivesuperior\n");
  fprintf(f, "16#2076    /sixsuperior\n");
  fprintf(f, "16#2077    /sevensuperior\n");
  fprintf(f, "16#2078    /eightsuperior\n");
  fprintf(f, "16#2079    /ninesuperior\n");
  fprintf(f, "16#207D    /parenleftsuperior\n");
  fprintf(f, "16#207E    /parenrightsuperior\n");
  fprintf(f, "16#207F    /nsuperior\n");
  fprintf(f, "16#2080    /zeroinferior\n");
  fprintf(f, "16#2081    /oneinferior\n");
  fprintf(f, "16#2082    /twoinferior\n");
  fprintf(f, "16#2083    /threeinferior\n");
  fprintf(f, "16#2084    /fourinferior\n");
  fprintf(f, "16#2085    /fiveinferior\n");
  fprintf(f, "16#2086    /sixinferior\n");
  fprintf(f, "16#2087    /seveninferior\n");
  fprintf(f, "16#2088    /eightinferior\n");
  fprintf(f, "16#2089    /nineinferior\n");
  fprintf(f, "16#208D    /parenleftinferior\n");
  fprintf(f, "16#208E    /parenrightinferior\n");
  fprintf(f, "16#20A1    /colonmonetary\n");
  fprintf(f, "16#20A3    /franc\n");
  fprintf(f, "16#20A4    /lira\n");
  fprintf(f, "16#20A7    /peseta\n");
  fprintf(f, "16#20AA    /afii57636\n");
  fprintf(f, "16#20AB    /dong\n");
  fprintf(f, "16#20AC    /Euro\n");
  fprintf(f, "16#2105    /afii61248\n");
  fprintf(f, "16#2111    /Ifraktur\n");
  fprintf(f, "16#2113    /afii61289\n");
  fprintf(f, "16#2116    /afii61352\n");
  fprintf(f, "16#2118    /weierstrass\n");
  fprintf(f, "16#211C    /Rfraktur\n");
  fprintf(f, "16#211E    /prescription\n");
  fprintf(f, "16#2122    /trademark\n");
  fprintf(f, "16#2126    /Omega\n");
  fprintf(f, "16#212E    /estimated\n");
  fprintf(f, "16#2135    /aleph\n");
  fprintf(f, "16#2153    /onethird\n");
  fprintf(f, "16#2154    /twothirds\n");
  fprintf(f, "16#215B    /oneeighth\n");
  fprintf(f, "16#215C    /threeeighths\n");
  fprintf(f, "16#215D    /fiveeighths\n");
  fprintf(f, "16#215E    /seveneighths\n");
  fprintf(f, "16#2190    /arrowleft\n");
  fprintf(f, "16#2191    /arrowup\n");
  fprintf(f, "16#2192    /arrowright\n");
  fprintf(f, "16#2193    /arrowdown\n");
  fprintf(f, "16#2194    /arrowboth\n");
  fprintf(f, "16#2195    /arrowupdn\n");
  fprintf(f, "16#21A8    /arrowupdnbse\n");
  fprintf(f, "16#21B5    /carriagereturn\n");
  fprintf(f, "16#21D0    /arrowdblleft\n");
  fprintf(f, "16#21D1    /arrowdblup\n");
  fprintf(f, "16#21D2    /arrowdblright\n");
  fprintf(f, "16#21D3    /arrowdbldown\n");
  fprintf(f, "16#21D4    /arrowdblboth\n");
  fprintf(f, "16#2200    /universal\n");
  fprintf(f, "16#2202    /partialdiff\n");
  fprintf(f, "16#2203    /existential\n");
  fprintf(f, "16#2205    /emptyset\n");
  fprintf(f, "16#2206    /Delta\n");
  fprintf(f, "16#2207    /gradient\n");
  fprintf(f, "16#2208    /element\n");
  fprintf(f, "16#2209    /notelement\n");
  fprintf(f, "16#220B    /suchthat\n");
  fprintf(f, "16#220F    /product\n");
  fprintf(f, "16#2211    /summation\n");
  fprintf(f, "16#2212    /minus\n");
  fprintf(f, "16#2215    /fraction\n");
  fprintf(f, "16#2217    /asteriskmath\n");
  fprintf(f, "16#2219    /periodcentered\n");
  fprintf(f, "16#221A    /radical\n");
  fprintf(f, "16#221D    /proportional\n");
  fprintf(f, "16#221E    /infinity\n");
  fprintf(f, "16#221F    /orthogonal\n");
  fprintf(f, "16#2220    /angle\n");
  fprintf(f, "16#2227    /logicaland\n");
  fprintf(f, "16#2228    /logicalor\n");
  fprintf(f, "16#2229    /intersection\n");
  fprintf(f, "16#222A    /union\n");
  fprintf(f, "16#222B    /integral\n");
  fprintf(f, "16#2234    /therefore\n");
  fprintf(f, "16#223C    /similar\n");
  fprintf(f, "16#2245    /congruent\n");
  fprintf(f, "16#2248    /approxequal\n");
  fprintf(f, "16#2260    /notequal\n");
  fprintf(f, "16#2261    /equivalence\n");
  fprintf(f, "16#2264    /lessequal\n");
  fprintf(f, "16#2265    /greaterequal\n");
  fprintf(f, "16#2282    /propersubset\n");
  fprintf(f, "16#2283    /propersuperset\n");
  fprintf(f, "16#2284    /notsubset\n");
  fprintf(f, "16#2286    /reflexsubset\n");
  fprintf(f, "16#2287    /reflexsuperset\n");
  fprintf(f, "16#2295    /circleplus\n");
  fprintf(f, "16#2297    /circlemultiply\n");
  fprintf(f, "16#22A5    /perpendicular\n");
  fprintf(f, "16#22C5    /dotmath\n");
  fprintf(f, "16#2302    /house\n");
  fprintf(f, "16#2310    /revlogicalnot\n");
  fprintf(f, "16#2320    /integraltp\n");
  fprintf(f, "16#2321    /integralbt\n");
  fprintf(f, "16#2329    /angleleft\n");
  fprintf(f, "16#232A    /angleright\n");
  fprintf(f, "16#2500    /SF100000\n");
  fprintf(f, "16#2502    /SF110000\n");
  fprintf(f, "16#250C    /SF010000\n");
  fprintf(f, "16#2510    /SF030000\n");
  fprintf(f, "16#2514    /SF020000\n");
  fprintf(f, "16#2518    /SF040000\n");
  fprintf(f, "16#251C    /SF080000\n");
  fprintf(f, "16#2524    /SF090000\n");
  fprintf(f, "16#252C    /SF060000\n");
  fprintf(f, "16#2534    /SF070000\n");
  fprintf(f, "16#253C    /SF050000\n");
  fprintf(f, "16#2550    /SF430000\n");
  fprintf(f, "16#2551    /SF240000\n");
  fprintf(f, "16#2552    /SF510000\n");
  fprintf(f, "16#2553    /SF520000\n");
  fprintf(f, "16#2554    /SF390000\n");
  fprintf(f, "16#2555    /SF220000\n");
  fprintf(f, "16#2556    /SF210000\n");
  fprintf(f, "16#2557    /SF250000\n");
  fprintf(f, "16#2558    /SF500000\n");
  fprintf(f, "16#2559    /SF490000\n");
  fprintf(f, "16#255A    /SF380000\n");
  fprintf(f, "16#255B    /SF280000\n");
  fprintf(f, "16#255C    /SF270000\n");
  fprintf(f, "16#255D    /SF260000\n");
  fprintf(f, "16#255E    /SF360000\n");
  fprintf(f, "16#255F    /SF370000\n");
  fprintf(f, "16#2560    /SF420000\n");
  fprintf(f, "16#2561    /SF190000\n");
  fprintf(f, "16#2562    /SF200000\n");
  fprintf(f, "16#2563    /SF230000\n");
  fprintf(f, "16#2564    /SF470000\n");
  fprintf(f, "16#2565    /SF480000\n");
  fprintf(f, "16#2566    /SF410000\n");
  fprintf(f, "16#2567    /SF450000\n");
  fprintf(f, "16#2568    /SF460000\n");
  fprintf(f, "16#2569    /SF400000\n");
  fprintf(f, "16#256A    /SF540000\n");
  fprintf(f, "16#256B    /SF530000\n");
  fprintf(f, "16#256C    /SF440000\n");
  fprintf(f, "16#2580    /upblock\n");
  fprintf(f, "16#2584    /dnblock\n");
  fprintf(f, "16#2588    /block\n");
  fprintf(f, "16#258C    /lfblock\n");
  fprintf(f, "16#2590    /rtblock\n");
  fprintf(f, "16#2591    /ltshade\n");
  fprintf(f, "16#2592    /shade\n");
  fprintf(f, "16#2593    /dkshade\n");
  fprintf(f, "16#25A0    /filledbox\n");
  fprintf(f, "16#25A1    /H22073\n");
  fprintf(f, "16#25AA    /H18543\n");
  fprintf(f, "16#25AB    /H18551\n");
  fprintf(f, "16#25AC    /filledrect\n");
  fprintf(f, "16#25B2    /triagup\n");
  fprintf(f, "16#25BA    /triagrt\n");
  fprintf(f, "16#25BC    /triagdn\n");
  fprintf(f, "16#25C4    /triaglf\n");
  fprintf(f, "16#25CA    /lozenge\n");
  fprintf(f, "16#25CB    /circle\n");
  fprintf(f, "16#25CF    /H18533\n");
  fprintf(f, "16#25D8    /invbullet\n");
  fprintf(f, "16#25D9    /invcircle\n");
  fprintf(f, "16#25E6    /openbullet\n");
  fprintf(f, "16#263A    /smileface\n");
  fprintf(f, "16#263B    /invsmileface\n");
  fprintf(f, "16#263C    /sun\n");
  fprintf(f, "16#2640    /female\n");
  fprintf(f, "16#2642    /male\n");
  fprintf(f, "16#2660    /spade\n");
  fprintf(f, "16#2663    /club\n");
  fprintf(f, "16#2665    /heart\n");
  fprintf(f, "16#2666    /diamond\n");
  fprintf(f, "16#266A    /musicalnote\n");
  fprintf(f, "16#266B    /musicalnotedbl\n");
  fprintf(f, "16#F6BE    /dotlessj\n");
  fprintf(f, "16#F6BF    /LL\n");
  fprintf(f, "16#F6C0    /ll\n");
  fprintf(f, "16#F6C1    /Scedilla\n");
  fprintf(f, "16#F6C2    /scedilla\n");
  fprintf(f, "16#F6C3    /commaaccent\n");
  fprintf(f, "16#F6C4    /afii10063\n");
  fprintf(f, "16#F6C5    /afii10064\n");
  fprintf(f, "16#F6C6    /afii10192\n");
  fprintf(f, "16#F6C7    /afii10831\n");
  fprintf(f, "16#F6C8    /afii10832\n");
  fprintf(f, "16#F6C9    /Acute\n");
  fprintf(f, "16#F6CA    /Caron\n");
  fprintf(f, "16#F6CB    /Dieresis\n");
  fprintf(f, "16#F6CC    /DieresisAcute\n");
  fprintf(f, "16#F6CD    /DieresisGrave\n");
  fprintf(f, "16#F6CE    /Grave\n");
  fprintf(f, "16#F6CF    /Hungarumlaut\n");
  fprintf(f, "16#F6D0    /Macron\n");
  fprintf(f, "16#F6D1    /cyrBreve\n");
  fprintf(f, "16#F6D2    /cyrFlex\n");
  fprintf(f, "16#F6D3    /dblGrave\n");
  fprintf(f, "16#F6D4    /cyrbreve\n");
  fprintf(f, "16#F6D5    /cyrflex\n");
  fprintf(f, "16#F6D6    /dblgrave\n");
  fprintf(f, "16#F6D7    /dieresisacute\n");
  fprintf(f, "16#F6D8    /dieresisgrave\n");
  fprintf(f, "16#F6D9    /copyrightserif\n");
  fprintf(f, "16#F6DA    /registerserif\n");
  fprintf(f, "16#F6DB    /trademarkserif\n");
  fprintf(f, "16#F6DC    /onefitted\n");
  fprintf(f, "16#F6DD    /rupiah\n");
  fprintf(f, "16#F6DE    /threequartersemdash\n");
  fprintf(f, "16#F6DF    /centinferior\n");
  fprintf(f, "16#F6E0    /centsuperior\n");
  fprintf(f, "16#F6E1    /commainferior\n");
  fprintf(f, "16#F6E2    /commasuperior\n");
  fprintf(f, "16#F6E3    /dollarinferior\n");
  fprintf(f, "16#F6E4    /dollarsuperior\n");
  fprintf(f, "16#F6E5    /hypheninferior\n");
  fprintf(f, "16#F6E6    /hyphensuperior\n");
  fprintf(f, "16#F6E7    /periodinferior\n");
  fprintf(f, "16#F6E8    /periodsuperior\n");
  fprintf(f, "16#F6E9    /asuperior\n");
  fprintf(f, "16#F6EA    /bsuperior\n");
  fprintf(f, "16#F6EB    /dsuperior\n");
  fprintf(f, "16#F6EC    /esuperior\n");
  fprintf(f, "16#F6ED    /isuperior\n");
  fprintf(f, "16#F6EE    /lsuperior\n");
  fprintf(f, "16#F6EF    /msuperior\n");
  fprintf(f, "16#F6F0    /osuperior\n");
  fprintf(f, "16#F6F1    /rsuperior\n");
  fprintf(f, "16#F6F2    /ssuperior\n");
  fprintf(f, "16#F6F3    /tsuperior\n");
  fprintf(f, "16#F6F4    /Brevesmall\n");
  fprintf(f, "16#F6F5    /Caronsmall\n");
  fprintf(f, "16#F6F6    /Circumflexsmall\n");
  fprintf(f, "16#F6F7    /Dotaccentsmall\n");
  fprintf(f, "16#F6F8    /Hungarumlautsmall\n");
  fprintf(f, "16#F6F9    /Lslashsmall\n");
  fprintf(f, "16#F6FA    /OEsmall\n");
  fprintf(f, "16#F6FB    /Ogoneksmall\n");
  fprintf(f, "16#F6FC    /Ringsmall\n");
  fprintf(f, "16#F6FD    /Scaronsmall\n");
  fprintf(f, "16#F6FE    /Tildesmall\n");
  fprintf(f, "16#F6FF    /Zcaronsmall\n");
  fprintf(f, "16#F721    /exclamsmall\n");
  fprintf(f, "16#F724    /dollaroldstyle\n");
  fprintf(f, "16#F726    /ampersandsmall\n");
  fprintf(f, "16#F730    /zerooldstyle\n");
  fprintf(f, "16#F731    /oneoldstyle\n");
  fprintf(f, "16#F732    /twooldstyle\n");
  fprintf(f, "16#F733    /threeoldstyle\n");
  fprintf(f, "16#F734    /fouroldstyle\n");
  fprintf(f, "16#F735    /fiveoldstyle\n");
  fprintf(f, "16#F736    /sixoldstyle\n");
  fprintf(f, "16#F737    /sevenoldstyle\n");
  fprintf(f, "16#F738    /eightoldstyle\n");
  fprintf(f, "16#F739    /nineoldstyle\n");
  fprintf(f, "16#F73F    /questionsmall\n");
  fprintf(f, "16#F760    /Gravesmall\n");
  fprintf(f, "16#F761    /Asmall\n");
  fprintf(f, "16#F762    /Bsmall\n");
  fprintf(f, "16#F763    /Csmall\n");
  fprintf(f, "16#F764    /Dsmall\n");
  fprintf(f, "16#F765    /Esmall\n");
  fprintf(f, "16#F766    /Fsmall\n");
  fprintf(f, "16#F767    /Gsmall\n");
  fprintf(f, "16#F768    /Hsmall\n");
  fprintf(f, "16#F769    /Ismall\n");
  fprintf(f, "16#F76A    /Jsmall\n");
  fprintf(f, "16#F76B    /Ksmall\n");
  fprintf(f, "16#F76C    /Lsmall\n");
  fprintf(f, "16#F76D    /Msmall\n");
  fprintf(f, "16#F76E    /Nsmall\n");
  fprintf(f, "16#F76F    /Osmall\n");
  fprintf(f, "16#F770    /Psmall\n");
  fprintf(f, "16#F771    /Qsmall\n");
  fprintf(f, "16#F772    /Rsmall\n");
  fprintf(f, "16#F773    /Ssmall\n");
  fprintf(f, "16#F774    /Tsmall\n");
  fprintf(f, "16#F775    /Usmall\n");
  fprintf(f, "16#F776    /Vsmall\n");
  fprintf(f, "16#F777    /Wsmall\n");
  fprintf(f, "16#F778    /Xsmall\n");
  fprintf(f, "16#F779    /Ysmall\n");
  fprintf(f, "16#F77A    /Zsmall\n");
  fprintf(f, "16#F7A1    /exclamdownsmall\n");
  fprintf(f, "16#F7A2    /centoldstyle\n");
  fprintf(f, "16#F7A8    /Dieresissmall\n");
  fprintf(f, "16#F7AF    /Macronsmall\n");
  fprintf(f, "16#F7B4    /Acutesmall\n");
  fprintf(f, "16#F7B8    /Cedillasmall\n");
  fprintf(f, "16#F7BF    /questiondownsmall\n");
  fprintf(f, "16#F7E0    /Agravesmall\n");
  fprintf(f, "16#F7E1    /Aacutesmall\n");
  fprintf(f, "16#F7E2    /Acircumflexsmall\n");
  fprintf(f, "16#F7E3    /Atildesmall\n");
  fprintf(f, "16#F7E4    /Adieresissmall\n");
  fprintf(f, "16#F7E5    /Aringsmall\n");
  fprintf(f, "16#F7E6    /AEsmall\n");
  fprintf(f, "16#F7E7    /Ccedillasmall\n");
  fprintf(f, "16#F7E8    /Egravesmall\n");
  fprintf(f, "16#F7E9    /Eacutesmall\n");
  fprintf(f, "16#F7EA    /Ecircumflexsmall\n");
  fprintf(f, "16#F7EB    /Edieresissmall\n");
  fprintf(f, "16#F7EC    /Igravesmall\n");
  fprintf(f, "16#F7ED    /Iacutesmall\n");
  fprintf(f, "16#F7EE    /Icircumflexsmall\n");
  fprintf(f, "16#F7EF    /Idieresissmall\n");
  fprintf(f, "16#F7F0    /Ethsmall\n");
  fprintf(f, "16#F7F1    /Ntildesmall\n");
  fprintf(f, "16#F7F2    /Ogravesmall\n");
  fprintf(f, "16#F7F3    /Oacutesmall\n");
  fprintf(f, "16#F7F4    /Ocircumflexsmall\n");
  fprintf(f, "16#F7F5    /Otildesmall\n");
  fprintf(f, "16#F7F6    /Odieresissmall\n");
  fprintf(f, "16#F7F8    /Oslashsmall\n");
  fprintf(f, "16#F7F9    /Ugravesmall\n");
  fprintf(f, "16#F7FA    /Uacutesmall\n");
  fprintf(f, "16#F7FB    /Ucircumflexsmall\n");
  fprintf(f, "16#F7FC    /Udieresissmall\n");
  fprintf(f, "16#F7FD    /Yacutesmall\n");
  fprintf(f, "16#F7FE    /Thornsmall\n");
  fprintf(f, "16#F7FF    /Ydieresissmall\n");
  fprintf(f, "16#F8E5    /radicalex\n");
  fprintf(f, "16#F8E6    /arrowvertex\n");
  fprintf(f, "16#F8E7    /arrowhorizex\n");
  fprintf(f, "16#F8E8    /registersans\n");
  fprintf(f, "16#F8E9    /copyrightsans\n");
  fprintf(f, "16#F8EA    /trademarksans\n");
  fprintf(f, "16#F8EB    /parenlefttp\n");
  fprintf(f, "16#F8EC    /parenleftex\n");
  fprintf(f, "16#F8ED    /parenleftbt\n");
  fprintf(f, "16#F8EE    /bracketlefttp\n");
  fprintf(f, "16#F8EF    /bracketleftex\n");
  fprintf(f, "16#F8F0    /bracketleftbt\n");
  fprintf(f, "16#F8F1    /bracelefttp\n");
  fprintf(f, "16#F8F2    /braceleftmid\n");
  fprintf(f, "16#F8F3    /braceleftbt\n");
  fprintf(f, "16#F8F4    /braceex\n");
  fprintf(f, "16#F8F5    /integralex\n");
  fprintf(f, "16#F8F6    /parenrighttp\n");
  fprintf(f, "16#F8F7    /parenrightex\n");
  fprintf(f, "16#F8F8    /parenrightbt\n");
  fprintf(f, "16#F8F9    /bracketrighttp\n");
  fprintf(f, "16#F8FA    /bracketrightex\n");
  fprintf(f, "16#F8FB    /bracketrightbt\n");
  fprintf(f, "16#F8FC    /bracerighttp\n");
  fprintf(f, "16#F8FD    /bracerightmid\n");
  fprintf(f, "16#F8FE    /bracerightbt\n");
  fprintf(f, "16#FB00    /ff\n");
  fprintf(f, "16#FB01    /fi\n");
  fprintf(f, "16#FB02    /fl\n");
  fprintf(f, "16#FB03    /ffi\n");
  fprintf(f, "16#FB04    /ffl\n");
  fprintf(f, "16#FB1F    /afii57705\n");
  fprintf(f, "16#FB2A    /afii57694\n");
  fprintf(f, "16#FB2B    /afii57695\n");
  fprintf(f, "16#FB35    /afii57723\n");
  fprintf(f, "16#FB4B    /afii57700\n");
  fprintf(f, ">>    def\n");

  fprintf(f, "10 dict dup begin\n");
  fprintf(f, "  /FontType 3 def\n");
  fprintf(f, "  /FontMatrix [.001 0 0 .001 0 0 ] def\n");
  fprintf(f, "  /FontBBox [0 0 100 100] def\n");
  fprintf(f, "  /Encoding 256 array def\n");
  fprintf(f, "  0 1 255 {Encoding exch /.notdef put} for\n");
  fprintf(f, "  Encoding 97 /openbox put\n");
  fprintf(f, "  /CharProcs 2 dict def\n");
  fprintf(f, "  CharProcs begin\n");
  fprintf(f, "    /.notdef { } def\n");
  fprintf(f, "    /openbox\n");
  fprintf(f, "      { newpath\n");
  fprintf(f, "          90 30 moveto  90 670 lineto\n");
  fprintf(f, "          730 670 lineto  730 30 lineto\n");
  fprintf(f, "        closepath\n");
  fprintf(f, "        60 setlinewidth\n");
  fprintf(f, "        stroke } def\n");
  fprintf(f, "  end\n");
  fprintf(f, "  /BuildChar\n");
  fprintf(f, "    { 1000 0 0\n");
  fprintf(f, "	0 750 750\n");
  fprintf(f, "        setcachedevice\n");
  fprintf(f, "	exch begin\n");
  fprintf(f, "        Encoding exch get\n");
  fprintf(f, "        CharProcs exch get\n");
  fprintf(f, "	end\n");
  fprintf(f, "	exec\n");
  fprintf(f, "    } def\n");
  fprintf(f, "end\n");
  fprintf(f, "/NoglyphFont exch definefont pop\n");
  fprintf(f, "\n");

  fprintf(f, "/mbshow {                       %% num\n");
  fprintf(f, "    8 array                     %% num array\n");
  fprintf(f, "    -1                          %% num array counter\n");
  fprintf(f, "    {\n");
  fprintf(f, "        dup 7 ge { exit } if\n");
  fprintf(f, "        1 add                   %% num array counter\n");
  fprintf(f, "        2 index 16#100 mod      %% num array counter mod\n");
  fprintf(f, "        3 copy put pop          %% num array counter\n");
  fprintf(f, "        2 index 16#100 idiv     %% num array counter num\n");
  fprintf(f, "        dup 0 le\n");
  fprintf(f, "        {\n");
  fprintf(f, "            pop exit\n");
  fprintf(f, "        } if\n");
  fprintf(f, "        4 -1 roll pop\n");
  fprintf(f, "        3 1 roll\n");
  fprintf(f, "    } loop                      %% num array counter\n");
  fprintf(f, "    3 -1 roll pop               %% array counter\n");
  fprintf(f, "    dup 1 add string            %% array counter string\n");
  fprintf(f, "    0 1 3 index\n");
  fprintf(f, "    {                           %% array counter string index\n");
  fprintf(f, "        2 index 1 index sub     %% array counter string index sid\n");
  fprintf(f, "        4 index 3 2 roll get    %% array counter string sid byte\n");
  fprintf(f, "        2 index 3 1 roll put    %% array counter string\n");
  fprintf(f, "    } for\n");
  fprintf(f, "    show pop pop\n");
  fprintf(f, "} def\n");

  fprintf(f, "/draw_undefined_char\n");
  fprintf(f, "{\n");
  fprintf(f, "  /NoglyphFont findfont csize scalefont setfont (a) show\n");
  fprintf(f, "} bind def\n");
  fprintf(f, "\n");
  fprintf(f, "/real_unicodeshow\n");
  fprintf(f, "{\n");
  fprintf(f, "  /ccode exch def\n");
  fprintf(f, "  /Unicodedict where {\n");
  fprintf(f, "    pop\n");
  fprintf(f, "    Unicodedict ccode known {\n");
  fprintf(f, "      /cwidth {currentfont /ScaleMatrix get 0 get} def \n");
  fprintf(f, "      /cheight cwidth def \n");
  fprintf(f, "      gsave\n");
  fprintf(f, "      currentpoint translate\n");
  fprintf(f, "      cwidth 1056 div cheight 1056 div scale\n");
  fprintf(f, "      2 -2 translate\n");
  fprintf(f, "      ccode Unicodedict exch get\n");
  fprintf(f, "      cvx exec\n");
  fprintf(f, "      grestore\n");
  fprintf(f, "      currentpoint exch cwidth add exch moveto\n");
  fprintf(f, "      true\n");
  fprintf(f, "    } {\n");
  fprintf(f, "      false\n");
  fprintf(f, "    } ifelse\n");
  fprintf(f, "  } {\n");
  fprintf(f, "    false\n");
  fprintf(f, "  } ifelse\n");
  fprintf(f, "} bind def\n");
  fprintf(f, "\n");
  fprintf(f, "/real_unicodeshow_native\n");
  fprintf(f, "{\n");
  fprintf(f, "  /ccode exch def\n");
  fprintf(f, "  /NativeFont where {\n");
  fprintf(f, "    pop\n");
  fprintf(f, "    NativeFont findfont /FontName get /Courier eq {\n");
  fprintf(f, "      false\n");
  fprintf(f, "    } {\n");
  fprintf(f, "      NativeFont findfont csize scalefont setfont\n");
  fprintf(f, "      /Unicode2NativeDict where {\n");
  fprintf(f, "        pop\n");
  fprintf(f, "        Unicode2NativeDict ccode known {\n");
  fprintf(f, "          Unicode2NativeDict ccode get show\n");
  fprintf(f, "          true\n");
  fprintf(f, "        } {\n");
  fprintf(f, "          false\n");
  fprintf(f, "        } ifelse\n");
  fprintf(f, "      } {\n");
  fprintf(f, "	  false\n");
  fprintf(f, "      } ifelse\n");
  fprintf(f, "    } ifelse\n");
  fprintf(f, "  } {\n");
  fprintf(f, "    false\n");
  fprintf(f, "  } ifelse\n");
  fprintf(f, "} bind def\n");
  fprintf(f, "\n");
  fprintf(f, "/real_glyph_unicodeshow\n");
  fprintf(f, "{\n");
  fprintf(f, "  /ccode exch def\n");
  fprintf(f, "      /UniDict where {\n");
  fprintf(f, "        pop\n");
  fprintf(f, "        UniDict ccode known {\n");
  fprintf(f, "          UniDict ccode get glyphshow\n");
  fprintf(f, "          true\n");
  fprintf(f, "        } {\n");
  fprintf(f, "          false\n");
  fprintf(f, "        } ifelse\n");
  fprintf(f, "      } {\n");
  fprintf(f, "	  false\n");
  fprintf(f, "      } ifelse\n");
  fprintf(f, "} bind def\n");
  fprintf(f, "/real_unicodeshow_cid\n");
  fprintf(f, "{\n");
  fprintf(f, "  /ccode exch def\n");
  fprintf(f, "  /UCS2Font where {\n");
  fprintf(f, "    pop\n");
  fprintf(f, "    UCS2Font findfont /FontName get /Courier eq {\n");
  fprintf(f, "      false\n");
  fprintf(f, "    } {\n");
  fprintf(f, "      UCS2Font findfont csize scalefont setfont\n");
  fprintf(f, "      ccode mbshow\n");
  fprintf(f, "      true\n");
  fprintf(f, "    } ifelse\n");
  fprintf(f, "  } {\n");
  fprintf(f, "    false\n");
  fprintf(f, "  } ifelse\n");
  fprintf(f, "} bind def\n");
  fprintf(f, "\n");
  fprintf(f, "/unicodeshow \n");
  fprintf(f, "{\n");
  fprintf(f, "  /cfont currentfont def\n");
  fprintf(f, "  /str exch def\n");
  fprintf(f, "  /i 0 def\n");
  fprintf(f, "  str length /ls exch def\n");
  fprintf(f, "  {\n");
  fprintf(f, "    i 1 add ls ge {exit} if\n");
  fprintf(f, "    str i get /c1 exch def\n");
  fprintf(f, "    str i 1 add get /c2 exch def\n");
  fprintf(f, "    /c c2 256 mul c1 add def\n");
  fprintf(f, "    c2 1 ge \n");
  fprintf(f, "    {\n");
  fprintf(f, "      c unicodeshow1\n");
  fprintf(f, "      {\n");
  fprintf(f, "        %% do nothing\n");
  fprintf(f, "      } {\n");
  fprintf(f, "        c real_unicodeshow_cid	%% try CID \n");
  fprintf(f, "        {\n");
  fprintf(f, "          %% do nothing\n");
  fprintf(f, "        } {\n");
  fprintf(f, "          c unicodeshow2\n");
  fprintf(f, "          {\n");
  fprintf(f, "            %% do nothing\n");
  fprintf(f, "          } {\n");
  fprintf(f, "            draw_undefined_char\n");
  fprintf(f, "          } ifelse\n");
  fprintf(f, "        } ifelse\n");
  fprintf(f, "      } ifelse\n");
  fprintf(f, "    } {\n");
  fprintf(f, "      %% ascii\n");
  fprintf(f, "      cfont setfont\n");
  fprintf(f, "      str i 1 getinterval show\n");
  fprintf(f, "    } ifelse\n");
  fprintf(f, "    /i i 2 add def\n");
  fprintf(f, "  } loop\n");
  fprintf(f, "}  bind def\n");
  fprintf(f, "\n");

  fprintf(f, "/u2nadd {Unicode2NativeDict 3 1 roll put} bind def\n");
  fprintf(f, "\n");

  fprintf(f, "/Unicode2NativeDictdef 0 dict def\n");
  fprintf(f, "/default_ls {\n");
  fprintf(f, "  /Unicode2NativeDict Unicode2NativeDictdef def\n");
  fprintf(f, "  /UCS2Font   /Courier def\n");
  fprintf(f, "  /NativeFont /Courier def\n");
  fprintf(f, "  /unicodeshow1 { real_glyph_unicodeshow } bind def\n");
  fprintf(f, "  /unicodeshow2 { real_unicodeshow_native } bind def\n");
  fprintf(f, "} bind def\n");

  fprintf(f, "\n");

  // read the printer properties file
  InitUnixPrinterProps();

  // setup prolog for each langgroup
  initlanggroup();

  fprintf(f, "%%%%EndProlog\n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::begin_page()
{
FILE *f;

  f = mPrintContext->prSetup->tmpBody;
  fprintf(f, "%%%%Page: %d %d\n", mPageNumber, mPageNumber);
  fprintf(f, "%%%%BeginPageSetup\n");
  if(mPrintSetup->num_copies != 1) {
    fprintf(f, "1 dict dup /NumCopies %d put setpagedevice\n",
      mPrintSetup->num_copies);
  }
  fprintf(f,"/pagelevel save def\n");
  if (mPrintContext->prSetup->landscape){
    fprintf(f, "%d 0 translate 90 rotate\n",PAGE_TO_POINT_I(mPrintContext->prSetup->height));
  }
  fprintf(f, "%d 0 translate\n", PAGE_TO_POINT_I(mPrintContext->prSetup->left));
  fprintf(f, "0 %d translate\n", PAGE_TO_POINT_I(mPrintContext->prSetup->top));
  fprintf(f, "%%%%EndPageSetup\n");
#if 0
  annotate_page( mPrintContext->prSetup->header, 0, -1, pn);
#endif
  fprintf(f, "newpath 0 %d moveto ", PAGE_TO_POINT_I(mPrintContext->prSetup->top));
  fprintf(f, "%d 0 rlineto 0 %d rlineto -%d 0 rlineto ",
			PAGE_TO_POINT_I(mPrintContext->prInfo->page_width),
			PAGE_TO_POINT_I(mPrintContext->prInfo->page_height),
			PAGE_TO_POINT_I(mPrintContext->prInfo->page_width));
  fprintf(f, " closepath clip newpath\n");

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

  while ((length = fread(buffer, 1, sizeof(buffer),
          mPrintContext->prSetup->tmpBody)) > 0)  {
    fwrite(buffer, 1, length, f);
  }

  if (mPrintSetup->tmpBody) {
    fclose(mPrintSetup->tmpBody);
    mPrintSetup->tmpBody = nsnull;
  }
  if (mPrintSetup->tmpBody_filename)
    free((void *)mPrintSetup->tmpBody_filename);
  
  // n_pages is zero so use mPageNumber
  fprintf(f, "%%%%Trailer\n");
  fprintf(f, "%%%%Pages: %d\n", (int) mPageNumber - 1);
  fprintf(f, "%%%%EOF\n");

#ifdef VMS
  if ( mPrintSetup->print_cmd != nsnull ) {
    char VMSPrintCommand[1024];
    if (mPrintSetup->out) {
      fclose(mPrintSetup->out);
      mPrintSetup->out = nsnull;
    }  
    PR_snprintf(VMSPrintCommand, sizeof(VMSPrintCommand), "%s %s.",
      mPrintSetup->print_cmd, mPrintSetup->filename);
    PR_LOG(nsPostScriptObjLM, PR_LOG_DEBUG, ("VMS print command '%s'\n", VMSPrintCommand));
    // FixMe: Check for error and return one of NS_ERROR_GFX_PRINTER_* on demand  
    system(VMSPrintCommand);
    free((void *)mPrintSetup->filename);
  }
#endif

  if (mPrintSetup->filename) {
    PR_LOG(nsPostScriptObjLM, PR_LOG_DEBUG, ("print to file completed.\n"));
  }  
  else {
    PR_LOG(nsPostScriptObjLM, PR_LOG_DEBUG, ("piping job to '%s'\n", mPrintSetup->print_cmd));

    FILE  *pipe;
    char   buf[256];
    size_t len;
        
    pipe = popen(mPrintSetup->print_cmd, "w");
    /* XXX: We should look at |errno| in this case and return something
     * more specific here... */
    if (!pipe)
      return NS_ERROR_GFX_PRINTER_CMD_FAILURE;
    
    size_t job_size = 0;
    
    /* Reset file pointer to the beginning of the temp file... */
    fseek(mPrintSetup->out, 0, SEEK_SET);

    do {
      len = fread(buf, 1, sizeof(buf), mPrintSetup->out);
      fwrite(buf, 1, len, pipe);
      
      job_size += len;
    } while(len == sizeof(buf));

    pclose(pipe);
    PR_LOG(nsPostScriptObjLM, PR_LOG_DEBUG, ("piping done, copied %ld bytes.\n", job_size));
    if (errno != 0) {
      return NS_ERROR_GFX_PRINTER_CMD_FAILURE;
    }
  }
  
  return NS_OK;
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
nsPostScriptObj::moveto(int x, int y)
{
  XL_SET_NUMERIC_LOCALE();
  y -= mPrintContext->prInfo->page_topy;

  // invert y
 // y = (mPrintContext->prInfo->page_height - y - 1) + mPrintContext->prSetup->bottom;

  y = (mPrintContext->prInfo->page_height - y - 1);
  
  fprintf(mPrintContext->prSetup->tmpBody, "%g %g moveto\n",
		PAGE_TO_POINT_F(x), PAGE_TO_POINT_F(y));
  XL_RESTORE_NUMERIC_LOCALE();
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::moveto_loc(int x, int y)
{
  /* This routine doesn't care about the clip region in the page */

  XL_SET_NUMERIC_LOCALE();

  // invert y
  y = (mPrintContext->prSetup->height - y - 1);

  fprintf(mPrintContext->prSetup->tmpBody, "%g %g moveto\n",
		PAGE_TO_POINT_F(x), PAGE_TO_POINT_F(y));
  XL_RESTORE_NUMERIC_LOCALE();
}


/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::lineto( int aX1, int aY1)
{
  XL_SET_NUMERIC_LOCALE();

  aY1 -= mPrintContext->prInfo->page_topy;
  //aY1 = (mPrintContext->prInfo->page_height - aY1 - 1) + mPrintContext->prSetup->bottom;
  aY1 = (mPrintContext->prInfo->page_height - aY1 - 1)  ;

  fprintf(mPrintContext->prSetup->tmpBody, "%g %g lineto\n",
		PAGE_TO_POINT_F(aX1), PAGE_TO_POINT_F(aY1));

  XL_RESTORE_NUMERIC_LOCALE();
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::ellipse( int aWidth, int aHeight)
{
  XL_SET_NUMERIC_LOCALE();

  // Ellipse definition
  fprintf(mPrintContext->prSetup->tmpBody,"%g %g ",
                PAGE_TO_POINT_F(aWidth)/2, PAGE_TO_POINT_F(aHeight)/2);
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
nsPostScriptObj::arc( int aWidth, int aHeight,float aStartAngle,float aEndAngle)
{

  XL_SET_NUMERIC_LOCALE();
  // Arc definition
  fprintf(mPrintContext->prSetup->tmpBody,"%g %g ",
                PAGE_TO_POINT_F(aWidth)/2, PAGE_TO_POINT_F(aHeight)/2);
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
nsPostScriptObj::box( int aW, int aH)
{
  XL_SET_NUMERIC_LOCALE();
  fprintf(mPrintContext->prSetup->tmpBody,
          "%g 0 rlineto 0 %g rlineto %g 0 rlineto ",
          PAGE_TO_POINT_F(aW), -PAGE_TO_POINT_F(aH), -PAGE_TO_POINT_F(aW));
  XL_RESTORE_NUMERIC_LOCALE();
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::box_subtract( int aW, int aH)
{
  XL_SET_NUMERIC_LOCALE();
  fprintf(mPrintContext->prSetup->tmpBody,
          "0 %g rlineto %g 0 rlineto 0 %g rlineto  ",
          PAGE_TO_POINT_F(-aH), PAGE_TO_POINT_F(aW), PAGE_TO_POINT_F(aH));
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
nsPostScriptObj::line( int aX1, int aY1, int aX2, int aY2, int aThick)
{
  XL_SET_NUMERIC_LOCALE();
  fprintf(mPrintContext->prSetup->tmpBody,
          "gsave %g setlinewidth\n ",PAGE_TO_POINT_F(aThick));

  aY1 -= mPrintContext->prInfo->page_topy;
 // aY1 = (mPrintContext->prInfo->page_height - aY1 - 1) + mPrintContext->prSetup->bottom;
  aY1 = (mPrintContext->prInfo->page_height - aY1 - 1) ;
  aY2 -= mPrintContext->prInfo->page_topy;
 // aY2 = (mPrintContext->prInfo->page_height - aY2 - 1) + mPrintContext->prSetup->bottom;
  aY2 = (mPrintContext->prInfo->page_height - aY2 - 1) ;

  fprintf(mPrintContext->prSetup->tmpBody, "%g %g moveto %g %g lineto\n",
		    PAGE_TO_POINT_F(aX1), PAGE_TO_POINT_F(aY1),
		    PAGE_TO_POINT_F(aX2), PAGE_TO_POINT_F(aY2));
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
nsPostScriptObj::translate(int x, int y)
{
    XL_SET_NUMERIC_LOCALE();
    y -= mPrintContext->prInfo->page_topy;
    // Y inversion
    //y = (mPrintContext->prInfo->page_height - y - 1) + mPrintContext->prSetup->bottom;
    y = (mPrintContext->prInfo->page_height - y - 1) ;

    fprintf(mPrintContext->prSetup->tmpBody, "%g %g translate\n", PAGE_TO_POINT_F(x), PAGE_TO_POINT_F(y));
    XL_RESTORE_NUMERIC_LOCALE();
}


/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *  Special notes, this on window will blow up since we can not get the bits in a DDB
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::grayimage(nsIImage *aImage,int aX,int aY, int aWidth,int aHeight)
{
PRInt32 rowData,bytes_Per_Pix,x,y;
PRInt32 width,height,bytewidth,cbits,n;
PRUint8 *theBits,*curline;
PRBool isTopToBottom;
PRInt32 sRow, eRow, rStep; 

  XL_SET_NUMERIC_LOCALE();
  bytes_Per_Pix = aImage->GetBytesPix();

  if(bytes_Per_Pix == 1)
    return ;

  rowData = aImage->GetLineStride();
  height = aImage->GetHeight();
  width = aImage->GetWidth();
  bytewidth = 3*width;
  cbits = 8;

  FILE *f = mPrintContext->prSetup->tmpBody;
  fprintf(f, "gsave\n");
  fprintf(f, "/rowdata %d string def\n",bytewidth/3);
  translate(aX, aY + aHeight);
  fprintf(f, "%g %g scale\n",
          PAGE_TO_POINT_F(aWidth), PAGE_TO_POINT_F(aHeight));
  fprintf(f, "%d %d ", width, height);
  fprintf(f, "%d ", cbits);
  fprintf(f, "[%d 0 0 %d 0 0]\n", width,height);
  fprintf(f, " { currentfile rowdata readhexstring pop }\n");
  fprintf(f, " image\n");

  aImage->LockImagePixels(PR_FALSE);
  theBits = aImage->GetBits();
  n = 0;
  if ( ( isTopToBottom = aImage->GetIsRowOrderTopToBottom()) == PR_TRUE ) {
	sRow = height - 1;
        eRow = 0;
        rStep = -1;
  } else {
	sRow = 0;
        eRow = height;
        rStep = 1;
  }

  y = sRow;
  while ( 1 ) {
    curline = theBits + (y*rowData);
    for(x=0;x<bytewidth;x+=3){
      if (n > 71) {
          fprintf(mPrintContext->prSetup->tmpBody,"\n");
          n = 0;
      }
      fprintf(mPrintContext->prSetup->tmpBody, "%02x", (int) (0xff & *curline));
      curline+=3; 
      n += 2;
    }
    y += rStep;
    if ( isTopToBottom == PR_TRUE && y < eRow ) break;
    if ( isTopToBottom == PR_FALSE && y >= eRow ) break;
  }
  aImage->UnlockImagePixels(PR_FALSE);

  fprintf(mPrintContext->prSetup->tmpBody, "\ngrestore\n");
  XL_RESTORE_NUMERIC_LOCALE();

}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *  Special notes, this on window will blow up since we can not get the bits in a DDB
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::colorimage(nsIImage *aImage,int aX,int aY, int aWidth,int aHeight)
{
PRInt32 rowData,bytes_Per_Pix,x,y;
PRInt32 width,height,bytewidth,cbits,n;
PRUint8 *theBits,*curline;
PRBool isTopToBottom;
PRInt32 sRow, eRow, rStep; 

  XL_SET_NUMERIC_LOCALE();

  if(mPrintSetup->color == PR_FALSE ){
    this->grayimage(aImage,aX,aY,aWidth,aHeight);
    return;
  }

  bytes_Per_Pix = aImage->GetBytesPix();

  if(bytes_Per_Pix == 1)
    return ;

  rowData = aImage->GetLineStride();
  height = aImage->GetHeight();
  width = aImage->GetWidth();
  bytewidth = 3*width;
  cbits = 8;

  FILE *f = mPrintContext->prSetup->tmpBody;
  fprintf(f, "gsave\n");
  fprintf(f, "/rowdata %d string def\n",bytewidth);
  translate(aX, aY + aHeight);
  fprintf(f, "%g %g scale\n",
          PAGE_TO_POINT_F(aWidth), PAGE_TO_POINT_F(aHeight));
  fprintf(f, "%d %d ", width, height);
  fprintf(f, "%d ", cbits);
  fprintf(f, "[%d 0 0 %d 0 0]\n", width,height);
  fprintf(f, " { currentfile rowdata readhexstring pop }\n");
  fprintf(f, " false 3 colorimage\n");

  aImage->LockImagePixels(PR_FALSE);
  theBits = aImage->GetBits();
  n = 0;
  if ( ( isTopToBottom = aImage->GetIsRowOrderTopToBottom()) == PR_TRUE ) {
	sRow = height - 1;
        eRow = 0;
        rStep = -1;
  } else {
	sRow = 0;
        eRow = height;
        rStep = 1;
  }

  y = sRow;
  while ( 1 ) {
    curline = theBits + (y*rowData);
    for(x=0;x<bytewidth;x++){
      if (n > 71) {
          fprintf(f,"\n");
          n = 0;
      }
      fprintf(f, "%02x", (int) (0xff & *curline++));
      n += 2;
    }
    y += rStep;
    if ( isTopToBottom == PR_TRUE && y < eRow ) break;
    if ( isTopToBottom == PR_FALSE && y >= eRow ) break;
  }
  aImage->UnlockImagePixels(PR_FALSE);

  fprintf(f, "\ngrestore\n");
  XL_RESTORE_NUMERIC_LOCALE();

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
    greyBrightness=(NS_PS_RED(aColor)+NS_PS_GREEN(aColor)+NS_PS_BLUE(aColor))/3;
    fprintf(mPrintContext->prSetup->tmpBody,"%3.2f %3.2f %3.2f setrgbcolor\n",
    greyBrightness,greyBrightness,greyBrightness);
  } else {
    fprintf(mPrintContext->prSetup->tmpBody,"%3.2f %3.2f %3.2f setrgbcolor\n",
    NS_PS_RED(aColor), NS_PS_GREEN(aColor), NS_PS_BLUE(aColor));
  }

  XL_RESTORE_NUMERIC_LOCALE();
}


void nsPostScriptObj::setfont(const nsCString aFontName, PRUint32 aHeight)
{
  fprintf(mPrintContext->prSetup->tmpBody,
          "/%s findfont %d scalefont setfont\n",
          aFontName.get(),
          NS_TWIPS_TO_POINTS(aHeight));
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
  fprintf(mPrintContext->prSetup->tmpBody,"%d",NS_TWIPS_TO_POINTS(aHeight));
	
  
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
nsPostScriptObj::InitUnixPrinterProps()
{
  nsCOMPtr<nsIPersistentProperties> printerprops_tmp;
  const char propertyURL[] = "resource:/res/unixpsfonts.properties";
  nsCOMPtr<nsIURI> uri;
  NS_ENSURE_SUCCESS(NS_NewURI(getter_AddRefs(uri), NS_LITERAL_CSTRING(propertyURL)), PR_FALSE);
  nsCOMPtr<nsIInputStream> in;
  NS_ENSURE_SUCCESS(NS_OpenURI(getter_AddRefs(in), uri), PR_FALSE);
  NS_ENSURE_SUCCESS(nsComponentManager::CreateInstance(
    NS_PERSISTENTPROPERTIES_CONTRACTID, nsnull,
    NS_GET_IID(nsIPersistentProperties), getter_AddRefs(printerprops_tmp)),
    PR_FALSE);
  NS_ENSURE_SUCCESS(printerprops_tmp->Load(in), PR_FALSE);
  mPrinterProps = printerprops_tmp;
  return PR_TRUE;
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
      nsICharsetConverterManager *ccMain = nsnull;
      nsICharsetAlias *aliasmgr = nsnull;
      res = nsServiceManager::GetService(kCharsetConverterManagerCID,
	   kICharsetConverterManagerIID, (nsISupports **) & ccMain);
      if (NS_SUCCEEDED(res) && ccMain) {
        res = nsServiceManager::GetService(NS_CHARSETALIAS_CONTRACTID,
	  NS_GET_IID(nsICharsetAlias), (nsISupports **) & aliasmgr);
	if (NS_SUCCEEDED(res) && aliasmgr) {
	  res = aliasmgr->GetPreferred(NS_ConvertASCIItoUCS2(psnativecode), str);
	  if (NS_SUCCEEDED(res)) {
	    res = ccMain->GetUnicodeEncoder(&str, &linfo->mEncoder);
	  }
          nsServiceManager::ReleaseService(NS_CHARSETALIAS_CONTRACTID, aliasmgr);
	}
        nsServiceManager::ReleaseService(kCharsetConverterManagerCID, ccMain);
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
