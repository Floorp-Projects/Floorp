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
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */

#include "nscore.h"
#include "nsPostScriptObj.h"
#include "xp_mem.h"
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


#ifdef VMS
#include <stdlib.h>
#include "prprf.h"
#endif

extern "C" PS_FontInfo *PSFE_MaskToFI[N_FONTS];   // need fontmetrics.c

// These set the location to standard C and back
// which will keep the "." from converting to a "," 
// in certain languages for floating point output to postscript
#define XL_SET_NUMERIC_LOCALE() char* cur_locale = setlocale(LC_NUMERIC, "C")
#define XL_RESTORE_NUMERIC_LOCALE() setlocale(LC_NUMERIC, cur_locale)

#define NS_PS_RED(x) (((float)(NS_GET_R(x))) / 255.0) 
#define NS_PS_GREEN(x) (((float)(NS_GET_G(x))) / 255.0) 
#define NS_PS_BLUE(x) (((float)(NS_GET_B(x))) / 255.0) 
#define NS_TWIPS_TO_POINTS(x) ((x / 20))
#define NS_IS_BOLD(x) (((x) >= 401) ? 1 : 0) 

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_IID(kICharsetConverterManagerIID, NS_ICHARSETCONVERTERMANAGER_IID);

/* 
 * Paper Names 
 */
const char*const paper_string[]={ "Letter", "Legal", "Executive", "A4", "A3" };

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
PrintAsDSCTextline(FILE *f, char *text, int maxlen)
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
nsPostScriptObj::nsPostScriptObj()
{
	mPrintContext = nsnull;
	mPrintSetup = nsnull;

        nsServiceManager::GetService(kPrefCID, NS_GET_IID(nsIPref),
          (nsISupports**) &gPrefs);

	gLangGroups = new nsHashtable();
  mTitle = nsnull;
}

/** ---------------------------------------------------
 *  Destructor
 *	@update 2/1/99 dwc
 */
nsPostScriptObj::~nsPostScriptObj()
{
  // The mPrintContext can be null
  // if  opening the PostScript document
  // fails.  Giving an invalid path, relative path
  // or a directory which the user does not have
  // write permissions for will fail to open a document
  // see bug 85535 
  if (mPrintContext) {
    // end the document
    end_document();
    finalize_translation();
    if ( mPrintSetup->filename != (char *) NULL )
      fclose( mPrintSetup->out );
    else {
#if defined(XP_OS2_VACPP) || defined(XP_PC)
      // pclose not defined OS2TODO
#else
	pclose( mPrintSetup->out );
#endif
    }
#ifdef VMS
    if ( mPrintSetup->print_cmd != (char *) NULL ) {
      char VMSPrintCommand[1024];
      PR_snprintf(VMSPrintCommand, sizeof(VMSPrintCommand), "%s /delete %s.",
        mPrintSetup->print_cmd, mPrintSetup->filename);
      // FixMe: Check for error and return one of NS_ERROR_GFX_PRINTER_* on demand  
      system(VMSPrintCommand);
      free(mPrintSetup->filename);
    }
#endif
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
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
nsresult 
nsPostScriptObj::Init( nsIDeviceContextSpecPS *aSpec, PRUnichar * aTitle )
{
  PRBool  isGray, isAPrinter, isFirstPageFirst;
  int     printSize;
  int     landscape;
  float   fwidth, fheight;
  char    *buf;

  PrintInfo* pi = new PrintInfo(); 
  mPrintSetup = new PrintSetup();

  if( (nsnull!=pi) && (nsnull!=mPrintSetup) ){
    memset(mPrintSetup, 0, sizeof(struct PrintSetup_));

    mPrintSetup->color = PR_TRUE;              // Image output 
    mPrintSetup->deep_color = PR_TRUE;         // 24 bit color output 
    mPrintSetup->paper_size = NS_LEGAL_SIZE;   // Paper Size(letter,legal,exec,a4,a3)
    mPrintSetup->reverse = 0;                  // Output order, 0 is acsending 
    if ( aSpec != nsnull ) {
      aSpec->GetGrayscale( isGray );
      if ( isGray == PR_TRUE ) {
        mPrintSetup->color = PR_FALSE; 
        mPrintSetup->deep_color = PR_FALSE; 
      }


      aSpec->GetFirstPageFirst( isFirstPageFirst );
      if ( isFirstPageFirst == PR_FALSE )
        mPrintSetup->reverse = 1;
      aSpec->GetSize( printSize );
      mPrintSetup->paper_size = printSize;
      aSpec->GetToPrinter( isAPrinter );
      if ( isAPrinter == PR_TRUE ) {
#ifndef VMS
        aSpec->GetCommand( &buf );
#if defined(XP_OS2_VACPP) || defined(XP_PC)
	mPrintSetup->out = NULL;
        // popen not defined OS2TODO
#else
        mPrintSetup->out = popen( buf, "w" );
#endif
        mPrintSetup->filename = (char *) NULL;  
#else
        // We can not open a pipe and print the contents of it. Instead
        // we have to print to a file and then print that.
        aSpec->GetCommand( &mPrintSetup->print_cmd );
        mPrintSetup->filename = tempnam("SYS$SCRATCH:","MOZ_P");
        mPrintSetup->out = fopen(mPrintSetup->filename, "w");
#endif
      } else {
        aSpec->GetPath( &buf );
        mPrintSetup->filename = buf;          
        mPrintSetup->out = fopen(mPrintSetup->filename, "w"); 
        if (!mPrintSetup->out)
          return NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE;
      }
    } else 
        return NS_ERROR_FAILURE;

    /* make sure the open worked */

    if (!mPrintSetup->out)
      return NS_ERROR_GFX_PRINTER_CMD_FAILURE;
      
    mPrintContext = new PSContext();
    memset(mPrintContext, 0, sizeof(struct PSContext_));
    memset(pi, 0, sizeof(struct PrintInfo_));

    mPrintSetup->dpi = 72.0f;                  // dpi for externally sized items 
    aSpec->GetPageDimensions( fwidth, fheight );
    mPrintSetup->width = (int)(fwidth * mPrintSetup->dpi);
    mPrintSetup->height = (int)(fheight * mPrintSetup->dpi);
#ifdef DEBUG
    printf("\nPreWidth = %f PreHeight = %f\n",fwidth,fheight);
    printf("\nWidth = %d Height = %d\n",mPrintSetup->width,mPrintSetup->height);
#endif
    mPrintSetup->header = "header";
    mPrintSetup->footer = "footer";
    mPrintSetup->sizes = NULL;

    aSpec->GetLandscape( landscape );
    mPrintSetup->landscape = (landscape) ? PR_TRUE : PR_FALSE; // Rotated output 
    //mPrintSetup->landscape = PR_FALSE;

    mPrintSetup->underline = PR_TRUE;             // underline links 
    mPrintSetup->scale_images = PR_TRUE;          // Scale unsized images which are too big 
    mPrintSetup->scale_pre = PR_FALSE;		        // do the pre-scaling thing 
    // scale margins (specified in inches) to dots.

    mPrintSetup->top = (int) 0;     
    mPrintSetup->bottom = (int) 0;
    mPrintSetup->left = (int) 0;
    mPrintSetup->right = (int) 0; 
#ifdef DEBUG
    printf( "dpi %f top %d bottom %d left %d right %d\n", mPrintSetup->dpi, mPrintSetup->top, mPrintSetup->bottom, mPrintSetup->left, mPrintSetup->right );
#endif
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
    mPrintSetup->completion = NULL;            // Called when translation finished 
    mPrintSetup->carg = NULL;                  // Data saved for completion routine 
    mPrintSetup->status = 0;                   // Status of URL on completion 
	                                    // "other" font is for encodings other than iso-8859-1 
    mPrintSetup->otherFontName[0] = NULL;		   
  				                            // name of "other" PostScript font 
    mPrintSetup->otherFontInfo[0] = NULL;	   
    // font info parsed from "other" afm file 
    mPrintSetup->otherFontCharSetID = 0;	      // charset ID of "other" font 
    //mPrintSetup->cx = NULL;                  // original context, if available 
    pi->page_height = mPrintSetup->height * 10;	// Size of printable area on page 
    pi->page_width = mPrintSetup->width * 10;	// Size of printable area on page 
    pi->page_break = 0;	              // Current page bottom 
    pi->page_topy = 0;	              // Current page top 
    pi->phase = 0;

 
    pi->pages=NULL;		                // Contains extents of each page 

    pi->pt_size = 0;		              // Size of above table 
    pi->n_pages = 0;	        	      // # of valid entries in above table 

    mTitle = nsnull;
    if(nsnull != aTitle){
      mTitle = ToNewCString(nsDependentString(aTitle));
    }

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
  XP_DELETE(mPrintContext->prSetup);
  mPrintContext->prSetup = NULL;
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::initialize_translation(PrintSetup* pi)
{
  PrintSetup *dup = XP_NEW(PrintSetup);
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
  fprintf(f, "%%%%Creator: Mozilla (NetScape) HTML->PS\n");
  fprintf(f, "%%%%DocumentData: Clean8Bit\n");
  fprintf(f, "%%%%DocumentPaperSizes: %s\n",
	            paper_string[mPrintContext->prSetup->paper_size]);
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
  fprintf(f, "  /unicodeshow1 { real_unicodeshow } bind def\n");
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

  f = mPrintContext->prSetup->out;
  fprintf(f, "%%%%Page: %d %d\n", mPageNumber, mPageNumber);
  fprintf(f, "%%%%BeginPageSetup\n/pagelevel save def\n");
  if (mPrintContext->prSetup->landscape){
    fprintf(f, "%d 0 translate 90 rotate\n",PAGE_TO_POINT_I(mPrintContext->prSetup->height));
  }
  fprintf(f, "%d 0 translate\n", PAGE_TO_POINT_I(mPrintContext->prSetup->left));
  fprintf(f, "0 %d translate\n", -PAGE_TO_POINT_I(mPrintContext->prSetup->top));
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

  fprintf(mPrintContext->prSetup->out, "pagelevel restore\n");
  annotate_page(mPrintContext->prSetup->header, mPrintContext->prSetup->top/2, -1, mPageNumber);
  annotate_page( mPrintContext->prSetup->footer,
				   mPrintContext->prSetup->height - mPrintContext->prSetup->bottom/2,
				   1, mPageNumber);
  fprintf(mPrintContext->prSetup->out, "showpage\n");
  mPageNumber++;
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::end_document()
{
  FILE *f;

  f = mPrintContext->prSetup->out;
  // n_pages is zero so use mPageNumber
  fprintf(f, "%%%%Trailer\n");
  fprintf(f, "%%%%Pages: %d\n", (int) mPageNumber - 1);
  fprintf(f, "%%%%EOF\n");
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

  f = mPrintContext->prSetup->out;
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
  FILE *f = mPrintContext->prSetup->out;
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
nsPostScriptObj::show(const PRUnichar* txt, int len, const char *align)
{
 FILE *f = mPrintContext->prSetup->out;
 unsigned char highbyte, lowbyte;
 PRUnichar uch;

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
  
  fprintf(mPrintContext->prSetup->out, "%g %g moveto\n",
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

  fprintf(mPrintContext->prSetup->out, "%g %g moveto\n",
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

  fprintf(mPrintContext->prSetup->out, "%g %g lineto\n",
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
  fprintf(mPrintContext->prSetup->out,"%g %g ",PAGE_TO_POINT_F(aWidth)/2, PAGE_TO_POINT_F(aHeight)/2);
  fprintf(mPrintContext->prSetup->out, 
                " matrix currentmatrix currentpoint translate\n");
  fprintf(mPrintContext->prSetup->out, 
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
  fprintf(mPrintContext->prSetup->out,"%g %g ",PAGE_TO_POINT_F(aWidth)/2, PAGE_TO_POINT_F(aHeight)/2);
  fprintf(mPrintContext->prSetup->out, 
                " matrix currentmatrix currentpoint translate\n");
  fprintf(mPrintContext->prSetup->out, 
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
  fprintf(mPrintContext->prSetup->out, "%g 0 rlineto 0 %g rlineto %g 0 rlineto ",
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
  fprintf(mPrintContext->prSetup->out,"0 %g rlineto %g 0 rlineto 0 %g rlineto  ",
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
  fprintf(mPrintContext->prSetup->out, " clip \n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::eoclip()
{
  fprintf(mPrintContext->prSetup->out, " eoclip \n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::clippath()
{
  fprintf(mPrintContext->prSetup->out, " clippath \n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::newpath()
{
  fprintf(mPrintContext->prSetup->out, " newpath \n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::closepath()
{
  fprintf(mPrintContext->prSetup->out, " closepath \n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::initclip()
{
  fprintf(mPrintContext->prSetup->out, " initclip \n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::line( int aX1, int aY1, int aX2, int aY2, int aThick)
{
  XL_SET_NUMERIC_LOCALE();
  fprintf(mPrintContext->prSetup->out, "gsave %g setlinewidth\n ",PAGE_TO_POINT_F(aThick));

  aY1 -= mPrintContext->prInfo->page_topy;
 // aY1 = (mPrintContext->prInfo->page_height - aY1 - 1) + mPrintContext->prSetup->bottom;
  aY1 = (mPrintContext->prInfo->page_height - aY1 - 1) ;
  aY2 -= mPrintContext->prInfo->page_topy;
 // aY2 = (mPrintContext->prInfo->page_height - aY2 - 1) + mPrintContext->prSetup->bottom;
  aY2 = (mPrintContext->prInfo->page_height - aY2 - 1) ;

  fprintf(mPrintContext->prSetup->out, "%g %g moveto %g %g lineto\n",
		    PAGE_TO_POINT_F(aX1), PAGE_TO_POINT_F(aY1),
		    PAGE_TO_POINT_F(aX2), PAGE_TO_POINT_F(aY2));
  stroke();

  fprintf(mPrintContext->prSetup->out, "grestore\n");
  XL_RESTORE_NUMERIC_LOCALE();
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::stroke()
{
  fprintf(mPrintContext->prSetup->out, " stroke \n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::fill()
{
  fprintf(mPrintContext->prSetup->out, " fill \n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::graphics_save()
{
  fprintf(mPrintContext->prSetup->out, " gsave \n");
}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::graphics_restore()
{
  fprintf(mPrintContext->prSetup->out, " grestore \n");
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

    fprintf(mPrintContext->prSetup->out, "%g %g translate\n", PAGE_TO_POINT_F(x), PAGE_TO_POINT_F(y));
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

  fprintf(mPrintContext->prSetup->out, "gsave\n");
  fprintf(mPrintContext->prSetup->out, "/rowdata %d string def\n",bytewidth/3);
  translate(aX, aY + aHeight);
  fprintf(mPrintContext->prSetup->out, "%g %g scale\n", PAGE_TO_POINT_F(aWidth), PAGE_TO_POINT_F(aHeight));
  fprintf(mPrintContext->prSetup->out, "%d %d ", width, height);
  fprintf(mPrintContext->prSetup->out, "%d ", cbits);
  //fprintf(mPrintContext->prSetup->out, "[%d 0 0 %d 0 %d]\n", width,-height, height);
  fprintf(mPrintContext->prSetup->out, "[%d 0 0 %d 0 0]\n", width,height);
  fprintf(mPrintContext->prSetup->out, " { currentfile rowdata readhexstring pop }\n");
  fprintf(mPrintContext->prSetup->out, " image\n");

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
          fprintf(mPrintContext->prSetup->out,"\n");
          n = 0;
      }
      fprintf(mPrintContext->prSetup->out, "%02x", (int) (0xff & *curline));
      curline+=3; 
      n += 2;
    }
    y += rStep;
    if ( isTopToBottom == PR_TRUE && y < eRow ) break;
    if ( isTopToBottom == PR_FALSE && y >= eRow ) break;
  }

  fprintf(mPrintContext->prSetup->out, "\ngrestore\n");
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

  fprintf(mPrintContext->prSetup->out, "gsave\n");
  fprintf(mPrintContext->prSetup->out, "/rowdata %d string def\n",bytewidth);
  translate(aX, aY + aHeight);
  fprintf(mPrintContext->prSetup->out, "%g %g scale\n", PAGE_TO_POINT_F(aWidth), PAGE_TO_POINT_F(aHeight));
  fprintf(mPrintContext->prSetup->out, "%d %d ", width, height);
  fprintf(mPrintContext->prSetup->out, "%d ", cbits);
  //fprintf(mPrintContext->prSetup->out, "[%d 0 0 %d 0 %d]\n", width,-height, height);
  fprintf(mPrintContext->prSetup->out, "[%d 0 0 %d 0 0]\n", width,height);
  fprintf(mPrintContext->prSetup->out, " { currentfile rowdata readhexstring pop }\n");
  fprintf(mPrintContext->prSetup->out, " false 3 colorimage\n");

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
          fprintf(mPrintContext->prSetup->out,"\n");
          n = 0;
      }
      fprintf(mPrintContext->prSetup->out, "%02x", (int) (0xff & *curline++));
      n += 2;
    }
    y += rStep;
    if ( isTopToBottom == PR_TRUE && y < eRow ) break;
    if ( isTopToBottom == PR_FALSE && y >= eRow ) break;
  }

  fprintf(mPrintContext->prSetup->out, "\ngrestore\n");
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
    fprintf(mPrintContext->prSetup->out,"%3.2f %3.2f %3.2f setrgbcolor\n",
    greyBrightness,greyBrightness,greyBrightness);
  } else {
    fprintf(mPrintContext->prSetup->out,"%3.2f %3.2f %3.2f setrgbcolor\n",
    NS_PS_RED(aColor), NS_PS_GREEN(aColor), NS_PS_BLUE(aColor));
  }

  XL_RESTORE_NUMERIC_LOCALE();
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
  fprintf(mPrintContext->prSetup->out,"%d",NS_TWIPS_TO_POINTS(aHeight));
	
  
  if( aFontIndex >= 0) {
    postscriptFont = aFontIndex;
  } else {
    postscriptFont = 0;
  }


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

	 fprintf(mPrintContext->prSetup->out, " f%d\n", postscriptFont);


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

  fprintf(mPrintContext->prSetup->out,"%%%s\n", aTheComment);

}

/** ---------------------------------------------------
 *  See documentation in nsPostScriptObj.h
 *	@update 5/30/00 katakai
 */
void
nsPostScriptObj::setlanggroup(nsIAtom * aLangGroup)
{
  FILE *f = mPrintContext->prSetup->out;

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
  nsAutoString propertyURL;
  propertyURL.AssignWithConversion("resource:/res/unixpsfonts.properties");
  nsCOMPtr<nsIURI> uri;
  NS_ENSURE_SUCCESS(NS_NewURI(getter_AddRefs(uri), propertyURL), PR_FALSE);
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

  nsAutoString key;
  key.AssignWithConversion(aKey.get());
  nsAutoString oValue;
  nsresult res = mPrinterProps->GetStringProperty(key, oValue);
  if (!NS_SUCCEEDED(res)) {
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
  {"print.psnativefont.ja", "Ryumin-Light-EUC-H"},
  {"print.psnativecode.ja", "euc-jp"},
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

/* make <langgroup>_ls define for each LangGroup here */
static void PrefEnumCallback(const char *aName, void *aClosure)
{
  nsPostScriptObj *psObj = (nsPostScriptObj*)aClosure;
  FILE *f = psObj->GetPrintFile();

  nsAutoString lang; lang.AssignWithConversion(aName);

  if (strstr(aName, "print.psnativefont.")) {
    lang.Cut(0, 19);
  } else if (strstr(aName, "print.psunicodefont.")) {
    lang.Cut(0, 20);
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
  nsCAutoString namepsnativefont("print.psnativefont.");
  namepsnativefont.AppendWithConversion(lang);
  gPrefs->CopyCharPref(namepsnativefont.get(), getter_Copies(psnativefont));

  nsCAutoString namepsnativecode("print.psnativecode.");
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
    nsCAutoString namepsfontorder("print.psfontorder.");
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
  nsCAutoString namepsunicodefont("print.psunicodefont.");
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
  gPrefs->EnumerateChildren("print.psnativefont.",
	    PrefEnumCallback, (void *) this);

  gPrefs->EnumerateChildren("print.psunicodefont.",
	    PrefEnumCallback, (void *) this);
}
