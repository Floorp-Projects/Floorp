/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nscore.h"
#include "nsPostScriptObj.h"
#include "xp_mem.h"
#include "libi18n.h"
#include "isotab.c"
#include "nsFont.h"
#include "nsIImage.h"

 
extern "C" PS_FontInfo *PSFE_MaskToFI[N_FONTS];   // need fontmetrics.c

#define XL_SET_NUMERIC_LOCALE() char* cur_locale = setlocale(LC_NUMERIC, "C")
#define XL_RESTORE_NUMERIC_LOCALE() setlocale(LC_NUMERIC, cur_locale)
#define NS_PS_RED(x) (((float)(NS_GET_R(x))) / 255.0) 
#define NS_PS_GREEN(x) (((float)(NS_GET_G(x))) / 255.0) 
#define NS_PS_BLUE(x) (((float)(NS_GET_B(x))) / 255.0) 
#define NS_TWIPS_TO_POINTS(x) ((x / 20))
#define NS_IS_BOLD(x) (((x) >= 500) ? 1 : 0) 

/* 
 * Paper Names 
 */
char* paper_string[]={ "Letter", "Legal", "Executive", "A4" };


/** ---------------------------------------------------
 *  Default Constructor
 *	@update 2/1/99 dwc
 */
nsPostScriptObj::nsPostScriptObj()
{
PrintInfo* pi = new PrintInfo();
PrintSetup* ps = new PrintSetup();

  mPrintContext = new PSContext();
  memset(mPrintContext, 0, sizeof(struct PSContext_));
  memset(ps, 0, sizeof(struct PrintSetup_));
  memset(pi, 0, sizeof(struct PrintInfo_));
 
  ps->top = 0;                        /* Margins  (PostScript Only) */
  ps->bottom = 0;
  ps->left = 0;
  ps->right = 0;
  ps->width = PAGE_WIDTH;            /* Paper size, # of cols for text xlate */
  ps->height = PAGE_HEIGHT;
  ps->header = "header";
  ps->footer = "footer";
  ps->sizes = NULL;
  ps->reverse = 0;                 /* Output order, 0 is acsending */
  ps->color = TRUE;                /* Image output */
  ps->deep_color = TRUE;		      /* 24 bit color output */
  ps->landscape = FALSE;           /* Rotated output */
  ps->underline = TRUE;            /* underline links */
  ps->scale_images = TRUE;           /* Scale unsized images which are too big */
  ps->scale_pre = FALSE;		      /* do the pre-scaling thing */
  ps->dpi = 72.0f;                 /* dpi for externally sized items */
  ps->rules = 1.0f;			          /* Scale factor for rulers */
  ps->n_up = 0;                        /* cool page combining */
  ps->bigger = 1;                      /* Used to init sizes if sizesin NULL */
  ps->paper_size = NS_LEGAL_SIZE;     /* Paper Size(letter,legal,exec,a4) */
  ps->prefix = "";                    /* For text xlate, prepended to each line */
  ps->eol = "";			   /* For text translation, line terminator */
  ps->bullet = "+";                    /* What char to use for bullets */

  URL_Struct_* url = new URL_Struct_;
  memset(url, 0, sizeof(URL_Struct_));
  ps->url = url;         /* url of doc being translated */
  char filename[30];
  static char g_nsPostscriptFileCount = 0; //('a');
  char ext[30];
  sprintf(ext,"%d",g_nsPostscriptFileCount);
  sprintf(filename,"file%s.ps", ext); 
  g_nsPostscriptFileCount++;
  ps->out = fopen(filename , "w");                     /* Where to send the output */
  ps->filename = filename;                  /* output file name, if any */
  ps->completion = NULL; /* Called when translation finished */
  ps->carg = NULL;                      /* Data saved for completion routine */
  ps->status = 0;                      /* Status of URL on completion */
		/* "other" font is for encodings other than iso-8859-1 */
  ps->otherFontName[0] = NULL;		   
  				/* name of "other" PostScript font */
  ps->otherFontInfo[0] = NULL;	   
  				/* font info parsed from "other" afm file */
  ps->otherFontCharSetID = 0;	   /* charset ID of "other" font */
  //ps->cx = NULL;                   /* original context, if available */

  pi->page_height = PAGE_HEIGHT * 10;	/* Size of printable area on page */
  pi->page_width = PAGE_WIDTH * 10;	/* Size of printable area on page */
  pi->page_break = 0;	/* Current page bottom */
  pi->page_topy = 0;	/* Current page top */
  pi->phase = 0;
	/*
	** CONTINUE SPECIAL
	**	The table print code maintains these
	*/
 
  pi->pages=NULL;		/* Contains extents of each page */

  pi->pt_size = 0;		/* Size of above table */
  pi->n_pages = 0;		/* # of valid entries in above table */
	/*
	** END SPECIAL
	*/

  pi->doc_title="Test Title";	/* best guess at title */
  pi->doc_width = 0;	/* Total document width */
  pi->doc_height = 0;	/* Total document height */

  mPrintContext->prInfo = pi;

  // begin the document
  initialize_translation(ps);

  begin_document();	
  mPrintSetup = ps;
  mPageNumber = 1;  // we are on the first page

}


/** ---------------------------------------------------
 *  Destructor
 *	@update 2/1/99 dwc
 */
nsPostScriptObj::~nsPostScriptObj()
{

  // end the document
  end_document();

  finalize_translation();

  // Cleanup things allocated along the way
  if (nsnull != mPrintContext){
	 if (nsnull != mPrintContext->prInfo)
		 delete mPrintContext->prInfo;

     if (nsnull != mPrintContext->prSetup)
		 delete mPrintContext->prSetup;

     delete mPrintContext;
  }

  if (nsnull != mPrintSetup)
	  delete mPrintSetup;

}

/** ---------------------------------------------------
 *  Get rid of data structures for the postscript
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::finalize_translation()
{
    XP_DELETE(mPrintContext->prSetup);
    mPrintContext->prSetup = NULL;
}

/** ---------------------------------------------------
 *  Set up the postscript
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
  if (pi->landscape)
  {
    dup->height = POINT_TO_PAGE(pi->width);
    dup->width = POINT_TO_PAGE(pi->height);
    /* XXX Should I swap the margins too ??? */
  }	
}

/** ---------------------------------------------------
 *  Open a document
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::begin_document()
{
    int i;
    XP_File f;
    char* charset_name = NULL;

    f = mPrintContext->prSetup->out;
    XP_FilePrintf(f, "%%!PS-Adobe-3.0\n");
    XP_FilePrintf(f, "%%%%BoundingBox: %d %d %d %d\n",
        PAGE_TO_POINT_I(mPrintContext->prSetup->left),
	PAGE_TO_POINT_I(mPrintContext->prSetup->bottom),
	PAGE_TO_POINT_I(mPrintContext->prSetup->width-mPrintContext->prSetup->right),
	PAGE_TO_POINT_I(mPrintContext->prSetup->height-mPrintContext->prSetup->top));
    XP_FilePrintf(f, "%%%%Creator: Mozilla (NetScape) HTML->PS\n");
    XP_FilePrintf(f, "%%%%DocumentData: Clean8Bit\n");
    XP_FilePrintf(f, "%%%%DocumentPaperSizes: %s\n",
	 paper_string[mPrintContext->prSetup->paper_size]);
    XP_FilePrintf(f, "%%%%Orientation: %s\n",
        (mPrintContext->prSetup->width < mPrintContext->prSetup->height) ? "Portrait" : "Landscape");
    XP_FilePrintf(f, "%%%%Pages: %d\n", (int) mPrintContext->prInfo->n_pages);
    if (mPrintContext->prSetup->reverse)
	XP_FilePrintf(f, "%%%%PageOrder: Descend\n");
    else
	XP_FilePrintf(f, "%%%%PageOrder: Ascend\n");
    XP_FilePrintf(f, "%%%%Title: %s\n", mPrintContext->prInfo->doc_title);
#ifdef NOTYET
    XP_FilePrintf(f, "%%%%For: %n", user_name_stuff);
#endif
    XP_FilePrintf(f, "%%%%EndComments\n");

    /* general comments: Mozilla-specific */
    XP_FilePrintf(f, "\n%% MozillaURL: %s\n", mPrintContext->prSetup->url->address);
    /* get charset name of non-latin1 fonts */
    /* for external filters, supply information */
    if (mPrintContext->prSetup->otherFontName[0] || mPrintContext->prSetup->otherFontInfo[0]){
      INTL_CharSetIDToName(mPrintContext->prSetup->otherFontCharSetID, charset_name);
      XP_FilePrintf(f, "%% MozillaCharsetName: %s\n\n", charset_name);
    }else
      /* default: iso-8859-1 */
      XP_FilePrintf(f, "%% MozillaCharsetName: iso-8859-1\n\n");
    
    /* now begin prolog */
    XP_FilePrintf(f, "%%%%BeginProlog\n");
    XP_FilePrintf(f, "[");
    for (i = 0; i < 256; i++)
      {
	if (*isotab[i] == '\0')
	  XP_FilePrintf(f, " /.notdef");
	else
	  XP_FilePrintf(f, " /%s", isotab[i]);
	if (( i % 6) == 5)
	  XP_FilePrintf(f, "\n");
      }
    XP_FilePrintf(f, "] /isolatin1encoding exch def\n");
    XP_FilePrintf(f, "/c { matrix currentmatrix currentpoint translate\n");
    XP_FilePrintf(f, "     3 1 roll scale newpath 0 0 1 0 360 arc setmatrix } bind def\n");
    for (i = 0; i < N_FONTS; i++)
	XP_FilePrintf(f, 
	    "/F%d\n"
	    "    /%s findfont\n"
	    "    dup length dict begin\n"
	    "	{1 index /FID ne {def} {pop pop} ifelse} forall\n"
	    "	/Encoding isolatin1encoding def\n"
	    "    currentdict end\n"
	    "definefont pop\n"
	    "/f%d { /F%d findfont exch scalefont setfont } bind def\n",
		i, PSFE_MaskToFI[i]->name, i, i);
    for (i = 0; i < N_FONTS; i++)
      if (mPrintContext->prSetup->otherFontName[i]) {
	  XP_FilePrintf(f, 
	    	"/of%d { /%s findfont exch scalefont setfont } bind def\n",
		i, mPrintContext->prSetup->otherFontName[i]);
/*          XP_FilePrintf(f, "/of /of1;\n", mPrintContext->prSetup->otherFontName); */
      }
    XP_FilePrintf(f, "/rhc {\n");
    XP_FilePrintf(f, "    {\n");
    XP_FilePrintf(f, "        currentfile read {\n");
    XP_FilePrintf(f, "	    dup 97 ge\n");
    XP_FilePrintf(f, "		{ 87 sub true exit }\n");
    XP_FilePrintf(f, "		{ dup 48 ge { 48 sub true exit } { pop } ifelse }\n");
    XP_FilePrintf(f, "	    ifelse\n");
    XP_FilePrintf(f, "	} {\n");
    XP_FilePrintf(f, "	    false\n");
    XP_FilePrintf(f, "	    exit\n");
    XP_FilePrintf(f, "	} ifelse\n");
    XP_FilePrintf(f, "    } loop\n");
    XP_FilePrintf(f, "} bind def\n");
    XP_FilePrintf(f, "\n");
    XP_FilePrintf(f, "/cvgray { %% xtra_char npix cvgray - (string npix long)\n");
    XP_FilePrintf(f, "    dup string\n");
    XP_FilePrintf(f, "    0\n");
    XP_FilePrintf(f, "    {\n");
    XP_FilePrintf(f, "	rhc { cvr 4.784 mul } { exit } ifelse\n");
    XP_FilePrintf(f, "	rhc { cvr 9.392 mul } { exit } ifelse\n");
    XP_FilePrintf(f, "	rhc { cvr 1.824 mul } { exit } ifelse\n");
    XP_FilePrintf(f, "	add add cvi 3 copy put pop\n");
    XP_FilePrintf(f, "	1 add\n");
    XP_FilePrintf(f, "	dup 3 index ge { exit } if\n");
    XP_FilePrintf(f, "    } loop\n");
    XP_FilePrintf(f, "    pop\n");
    XP_FilePrintf(f, "    3 -1 roll 0 ne { rhc { pop } if } if\n");
    XP_FilePrintf(f, "    exch pop\n");
    XP_FilePrintf(f, "} bind def\n");
    XP_FilePrintf(f, "\n");
    XP_FilePrintf(f, "/smartimage12rgb { %% w h b [matrix] smartimage12rgb -\n");
    XP_FilePrintf(f, "    /colorimage where {\n");
    XP_FilePrintf(f, "	pop\n");
    XP_FilePrintf(f, "	{ currentfile rowdata readhexstring pop }\n");
    XP_FilePrintf(f, "	false 3\n");
    XP_FilePrintf(f, "	colorimage\n");
    XP_FilePrintf(f, "    } {\n");
    XP_FilePrintf(f, "	exch pop 8 exch\n");
    XP_FilePrintf(f, "	3 index 12 mul 8 mod 0 ne { 1 } { 0 } ifelse\n");
    XP_FilePrintf(f, "	4 index\n");
    XP_FilePrintf(f, "	6 2 roll\n");
    XP_FilePrintf(f, "	{ 2 copy cvgray }\n");
    XP_FilePrintf(f, "	image\n");
    XP_FilePrintf(f, "	pop pop\n");
    XP_FilePrintf(f, "    } ifelse\n");
    XP_FilePrintf(f, "} def\n");
    XP_FilePrintf(f,"/cshow { dup stringwidth pop 2 div neg 0 rmoveto show } bind def\n");  
     XP_FilePrintf(f,"/rshow { dup stringwidth pop neg 0 rmoveto show } bind def\n");
    XP_FilePrintf(f, "/BeginEPSF {\n");
    XP_FilePrintf(f, "  /b4_Inc_state save def\n");
    XP_FilePrintf(f, "  /dict_count countdictstack def\n");
    XP_FilePrintf(f, "  /op_count count 1 sub def\n");
    XP_FilePrintf(f, "  userdict begin\n");
    XP_FilePrintf(f, "  /showpage {} def\n");
    XP_FilePrintf(f, "  0 setgray 0 setlinecap 1 setlinewidth 0 setlinejoin\n");
    XP_FilePrintf(f, "  10 setmiterlimit [] 0 setdash newpath\n");
    XP_FilePrintf(f, "  /languagelevel where\n");
    XP_FilePrintf(f, "  { pop languagelevel 1 ne\n");
    XP_FilePrintf(f, "    { false setstrokeadjust false setoverprint } if\n");
    XP_FilePrintf(f, "  } if\n");
    XP_FilePrintf(f, "} bind def\n");
    XP_FilePrintf(f, "/EndEPSF {\n");
    XP_FilePrintf(f, "  count op_count sub {pop} repeat\n");
    XP_FilePrintf(f, "  countdictstack dict_count sub {end} repeat\n");
    XP_FilePrintf(f, "  b4_Inc_state restore\n");
    XP_FilePrintf(f, "} bind def\n");
   XP_FilePrintf(f, "%%%%EndProlog\n");
}

/** ---------------------------------------------------
 *  Open a page
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::begin_page()
{
  XP_File f;

  f = mPrintContext->prSetup->out;
  XP_FilePrintf(f, "%%%%Page: %d %d\n", mPageNumber, mPageNumber);
  XP_FilePrintf(f, "%%%%BeginPageSetup\n/pagelevel save def\n");
  if (mPrintContext->prSetup->landscape)
    XP_FilePrintf(f, "%d 0 translate 90 rotate\n",
		  PAGE_TO_POINT_I(mPrintContext->prSetup->height));
  XP_FilePrintf(f, "%d 0 translate\n", PAGE_TO_POINT_I(mPrintContext->prSetup->left));
  XP_FilePrintf(f, "%%%%EndPageSetup\n");
#if 0
  annotate_page( mPrintContext->prSetup->header, 0, -1, pn);
#endif
  XP_FilePrintf(f, "newpath 0 %d moveto ", PAGE_TO_POINT_I(mPrintContext->prSetup->bottom));
  XP_FilePrintf(f, "%d 0 rlineto 0 %d rlineto -%d 0 rlineto ",
			PAGE_TO_POINT_I(mPrintContext->prInfo->page_width),
			PAGE_TO_POINT_I(mPrintContext->prInfo->page_height),
			PAGE_TO_POINT_I(mPrintContext->prInfo->page_width));
  XP_FilePrintf(f, " closepath clip newpath\n");
}

/** ---------------------------------------------------
 *  Close a page
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::end_page()
{
#if 0
  annotate_page( mPrintContext->prSetup->footer,
		   mPrintContext->prSetup->height-mPrintContext->prSetup->bottom-mPrintContext->prSetup->top,
		   1, pn);
  XP_FilePrintf(mPrintContext->prSetup->out, "pagelevel restore\nshowpage\n");
#endif

  XP_FilePrintf(mPrintContext->prSetup->out, "pagelevel restore\n");
  annotate_page(mPrintContext->prSetup->header, mPrintContext->prSetup->top/2, -1, mPageNumber);
  annotate_page( mPrintContext->prSetup->footer,
				   mPrintContext->prSetup->height - mPrintContext->prSetup->bottom/2,
				   1, mPageNumber);
  XP_FilePrintf(mPrintContext->prSetup->out, "showpage\n");
  mPageNumber++;
}

/** ---------------------------------------------------
 *  End a document
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::end_document()
{
    XP_FilePrintf(mPrintContext->prSetup->out, "%%%%EOF\n");
}


/** ---------------------------------------------------
 *  Reset the cliping path
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::annotate_page(char *aTemplate, int y, int delta_dir, int pn)
{
}

/** ---------------------------------------------------
 *  Issue a PS show command, which causes image to be rastered
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::show(char* txt, int len, char *align)
{
    XP_File f;

    f = mPrintContext->prSetup->out;
    XP_FilePrintf(f, "(");
    while (len-- > 0) {
	switch (*txt) {
	    case '(':
	    case ')':
	    case '\\':
		XP_FilePrintf(f, "\\%c", *txt);
		break;
	    default:
		if (*txt < ' ' || (*txt & 0x80))
		  XP_FilePrintf(f, "\\%o", *txt & 0xff);
		else
		  XP_FilePrintf(f, "%c", *txt);
		break;
	}
	txt++;
    }
    XP_FilePrintf(f, ") %sshow\n", align);
}

/** ---------------------------------------------------
 *  Output a PS moveto command
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::moveto(int x, int y)
{
  XL_SET_NUMERIC_LOCALE();
  y -= mPrintContext->prInfo->page_topy;
  /*
  ** Agh! Y inversion again !!
  */
  y = (mPrintContext->prInfo->page_height - y - 1) + mPrintContext->prSetup->bottom;

  XP_FilePrintf(mPrintContext->prSetup->out, "%g %g moveto\n",
		PAGE_TO_POINT_F(x), PAGE_TO_POINT_F(y));
  XL_RESTORE_NUMERIC_LOCALE();
}

/** ---------------------------------------------------
 *  Move the PS cursor to a location, see documentation in nsPostScript.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::moveto_loc(int x, int y)
{
  /* This routine doesn't care about the clip region in the page */

  XL_SET_NUMERIC_LOCALE();

  /*
  ** Agh! Y inversion again !!
  */
  y = (mPrintContext->prSetup->height - y - 1);

  XP_FilePrintf(mPrintContext->prSetup->out, "%g %g moveto\n",
		PAGE_TO_POINT_F(x), PAGE_TO_POINT_F(y));
  XL_RESTORE_NUMERIC_LOCALE();
}


/** ---------------------------------------------------
 *  Draw a PS line,see documentation in nsPostScript.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::lineto( int x1, int y1)
{
  XL_SET_NUMERIC_LOCALE();

  y1 -= mPrintContext->prInfo->page_topy;
  y1 = (mPrintContext->prInfo->page_height - y1 - 1) + mPrintContext->prSetup->bottom;

  XP_FilePrintf(mPrintContext->prSetup->out, "%g %g lineto\n",
		PAGE_TO_POINT_F(x1), PAGE_TO_POINT_F(y1));

  XL_RESTORE_NUMERIC_LOCALE();
}

/** ---------------------------------------------------
 *  Draw a PS circle,see documentation in nsPostScript.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::circle( int aW, int aH)
{
  XL_SET_NUMERIC_LOCALE();
  XP_FilePrintf(mPrintContext->prSetup->out, "%g %g c ", PAGE_TO_POINT_F(aW)/2.0, PAGE_TO_POINT_F(aH)/2.0);
  XL_RESTORE_NUMERIC_LOCALE();
}

/** ---------------------------------------------------
 *  Draw a PS rectangle,see documentation in nsPostScript.h
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::box( int aW, int aH)
{
  XL_SET_NUMERIC_LOCALE();
  XP_FilePrintf(mPrintContext->prSetup->out, "%g 0 rlineto 0 %g rlineto %g 0 rlineto ",
      PAGE_TO_POINT_F(aW), -PAGE_TO_POINT_F(aH), -PAGE_TO_POINT_F(aW));
  XL_RESTORE_NUMERIC_LOCALE();
}

/** ---------------------------------------------------
 *  Create a box, but draw the path in the counterclockwise direction
 *  so it will subtract using the winding run if inside another path
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::box_subtract( int aW, int aH)
{
  XL_SET_NUMERIC_LOCALE();
  XP_FilePrintf(mPrintContext->prSetup->out,"0 %g rlineto %g 0 rlineto 0 %g rlineto  ",
      PAGE_TO_POINT_F(-aH), PAGE_TO_POINT_F(aW), PAGE_TO_POINT_F(aH));
  XL_RESTORE_NUMERIC_LOCALE();
}

/** ---------------------------------------------------
 *  Set the clipping using the last path closed
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::clip()
{
  XP_FilePrintf(mPrintContext->prSetup->out, " clip \n");
}

/** ---------------------------------------------------
 *  Open a new path
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::newpath()
{
  XP_FilePrintf(mPrintContext->prSetup->out, " newpath \n");
}

/** ---------------------------------------------------
 *  Close the currently open path
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::closepath()
{
  XP_FilePrintf(mPrintContext->prSetup->out, " closepath \n");
}

/** ---------------------------------------------------
 *  Reset the cliping path
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::initclip()
{
  XP_FilePrintf(mPrintContext->prSetup->out, " initclip \n");
}

/** ---------------------------------------------------
 *  Reset the cliping path
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::line( int x1, int y1, int x2, int y2, int thick)
{
  XL_SET_NUMERIC_LOCALE();
  XP_FilePrintf(mPrintContext->prSetup->out, "gsave %g setlinewidth\n ",
				PAGE_TO_POINT_F(thick));

  y1 -= mPrintContext->prInfo->page_topy;
  y1 = (mPrintContext->prInfo->page_height - y1 - 1) + mPrintContext->prSetup->bottom;
  y2 -= mPrintContext->prInfo->page_topy;
  y2 = (mPrintContext->prInfo->page_height - y2 - 1) + mPrintContext->prSetup->bottom;

  XP_FilePrintf(mPrintContext->prSetup->out, "%g %g moveto %g %g lineto\n",
		PAGE_TO_POINT_F(x1), PAGE_TO_POINT_F(y1),
		PAGE_TO_POINT_F(x2), PAGE_TO_POINT_F(y2));
  stroke();

  XP_FilePrintf(mPrintContext->prSetup->out, "grestore\n");
  XL_RESTORE_NUMERIC_LOCALE();
}

/** ---------------------------------------------------
 *  Reset the cliping path
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::stroke()
{
  XP_FilePrintf(mPrintContext->prSetup->out, " stroke \n");
}

/** ---------------------------------------------------
 *  Reset the cliping path
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::fill()
{
  XP_FilePrintf(mPrintContext->prSetup->out, " fill \n");
}

/** ---------------------------------------------------
 *  Reset the cliping path
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::graphics_save()
{
   XP_FilePrintf(mPrintContext->prSetup->out, " gsave \n");
}

/** ---------------------------------------------------
 *  Reset the cliping path
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::graphics_restore()
{
  XP_FilePrintf(mPrintContext->prSetup->out, " grestore \n");
}

/** ---------------------------------------------------
 *  translate in PS space
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::translate(int x, int y)
{
    XL_SET_NUMERIC_LOCALE();
    y -= mPrintContext->prInfo->page_topy;
    /*
    ** Agh! Y inversion again !!
    */
    y = (mPrintContext->prInfo->page_height - y - 1) + mPrintContext->prSetup->bottom;

    XP_FilePrintf(mPrintContext->prSetup->out, "%g %g translate\n", PAGE_TO_POINT_F(x), PAGE_TO_POINT_F(y));
    XL_RESTORE_NUMERIC_LOCALE();
}

/** ---------------------------------------------------
 *  output an image in 24 
 *	@update 2/1/99 dwc
 */
void 
nsPostScriptObj::colorimage(nsIImage *aImage,int aX,int aY, int aWidth,int aHeight)
{
PRInt32 rowData,bytes_Per_Pix,x,y;
PRInt32 width,height,bytewidth,cbits,n;
PRUint8 *theBits,*curline;

  XL_SET_NUMERIC_LOCALE();
  bytes_Per_Pix = aImage->GetBytesPix();

  if(bytes_Per_Pix == 1)
    return ;

  rowData = aImage->GetLineStride();
  height = aImage->GetHeight();
  width = aImage->GetWidth();
  bytewidth = 3*width;
  cbits = 8;


  XP_FilePrintf(mPrintContext->prSetup->out, "gsave\n");
  XP_FilePrintf(mPrintContext->prSetup->out, "/rowdata %d string def\n",bytewidth);
  translate(aX, aY + aHeight);
  XP_FilePrintf(mPrintContext->prSetup->out, "%g %g scale\n", PAGE_TO_POINT_F(aWidth), PAGE_TO_POINT_F(aHeight));
  XP_FilePrintf(mPrintContext->prSetup->out, "%d %d ", width, height);
  XP_FilePrintf(mPrintContext->prSetup->out, "%d ", cbits);
  //XP_FilePrintf(mPrintContext->prSetup->out, "[%d 0 0 %d 0 %d]\n", width,-height, height);
  XP_FilePrintf(mPrintContext->prSetup->out, "[%d 0 0 %d 0 0]\n", width,height);
  XP_FilePrintf(mPrintContext->prSetup->out, " { currentfile rowdata readhexstring pop }\n");
  XP_FilePrintf(mPrintContext->prSetup->out, " false 3 colorimage\n");


  theBits = aImage->GetBits();
  n = 0;
  for(y=0;y<height;y++){
    curline = theBits + (y*rowData);
    for(x=0;x<bytewidth;x++){
      if (n > 71) {
          XP_FilePrintf(mPrintContext->prSetup->out,"\n");
          n = 0;
      }
      XP_FilePrintf(mPrintContext->prSetup->out, "%02x", (int) (0xff & *curline++));
      n += 2;
    }
  }


  XP_FilePrintf(mPrintContext->prSetup->out, "\ngrestore\n");
  XL_RESTORE_NUMERIC_LOCALE();

}

/** ---------------------------------------------------
 *  Reset the cliping path
 *	@update 2/1/99 dwc
 */
void
nsPostScriptObj::setcolor(nscolor aColor)
{
  XP_FilePrintf(mPrintContext->prSetup->out,"%3.2f %3.2f %3.2f setrgbcolor\n", NS_PS_RED(aColor), NS_PS_GREEN(aColor),
		  NS_PS_BLUE(aColor));
}


/** ---------------------------------------------------
 *  See documentation
 *	@update 2/1/98 dwc
 */
void 
nsPostScriptObj::setscriptfont(nscoord aHeight, PRUint8 aStyle, 
											 PRUint8 aVariant, PRUint16 aWeight, PRUint8 decorations)
{
int postscriptFont = 0;

  XP_FilePrintf(mPrintContext->prSetup->out,"%d",NS_TWIPS_TO_POINTS(aHeight));
	
  //XXX:PS Add bold, italic and other settings here
	switch(aStyle){
	  case NS_FONT_STYLE_NORMAL :
		  if (NS_IS_BOLD(aWeight)) {
		    postscriptFont = 1;   // NORMAL BOLD
      } else {
	        postscriptFont = 0; // NORMAL
		  }
	  break;

	  case NS_FONT_STYLE_ITALIC:
		  if (NS_IS_BOLD(aWeight)) {		  
		    postscriptFont = 3; // BOLD ITALIC
      } else {			  
		    postscriptFont = 2; // ITALIC
		  }
	  break;

	  case NS_FONT_STYLE_OBLIQUE:
		  if (NS_IS_BOLD(aWeight)) {	
	        postscriptFont = 7;   // COURIER-BOLD OBLIQUE
      } else {	
	        postscriptFont = 6;   // COURIER OBLIQUE
		  }
	    break;
	}

	 XP_FilePrintf(mPrintContext->prSetup->out, " f%d\n", postscriptFont);

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
