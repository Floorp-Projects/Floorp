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


/*
** External points of interest for the translation library
*/

#ifndef XLATE_H
#define XLATE_H

typedef void (*XL_CompletionRoutine)(PrintSetup*);
typedef void* XL_TextTranslation;
typedef void* XL_PostscriptTranslation;

XP_BEGIN_PROTOS
extern void XL_InitializePrintSetup(PrintSetup *p);
extern void XL_InitializeTextSetup(PrintSetup *p);
extern void XL_TranslatePostscript(MWContext*, URL_Struct *u,
                                   SHIST_SavedData *sd, PrintSetup*p);
extern XL_TextTranslation
  XL_TranslateText(MWContext *, URL_Struct *u, PrintSetup*p);
extern void XL_GetTextImage(LO_ImageStruct *image);
extern void
 XL_DisplayTextImage(MWContext *cx, int iLocation, LO_ImageStruct *img);
extern XP_Bool XP_CheckElementSpan(MWContext*, int top, int height);
extern void XP_InitializePrintInfo(MWContext *);
extern void XP_CleanupPrintInfo(MWContext *);
extern void XP_DrawForPrint(MWContext *, int );
extern void XP_LayoutForPrint(MWContext *cx, int32 doc_height);
XP_END_PROTOS

typedef struct LineRecord_struct LineRecord;

/*
** PAGE coordinates are 720/inch, layout happens in this space
** POINT coordinates are 72/inch, the printer wants these
*/
#define INCH_TO_PAGE(f) ((int) (.5 + (f)*720))
#define PAGE_TO_POINT_I(f) ((int) ((f) / 10.0))
#define PAGE_TO_POINT_F(f) ((f) / 10.0)
#define POINT_TO_PAGE(p) ((p)*10)

/*
** Used to pass info into text and/or postscript translation
*/
struct PrintSetup_ {
  int top;                        /* Margins  (PostScript Only) */
  int bottom;
  int left;
  int right;

  int width;                       /* Paper size, # of cols for text xlate */
  int height;

  char* header;
  char* footer;

  int *sizes;
  XP_Bool reverse;                 /* Output order */
  XP_Bool color;                   /* Image output */
  XP_Bool deep_color;		   /* 24 bit color output */
  XP_Bool landscape;               /* Rotated output */
  XP_Bool underline;               /* underline links */
  XP_Bool scale_images;            /* Scale unsized images which are too big */
  XP_Bool scale_pre;		   /* do the pre-scaling thing */
  float dpi;                       /* dpi for externally sized items */
  float rules;			   /* Scale factor for rulers */
  int n_up;                        /* cool page combining */
  int bigger;                      /* Used to init sizes if sizesin NULL */

  char* prefix;                    /* For text xlate, prepended to each line */
  char* eol;			   /* For text translation, line terminator */
  char* bullet;                    /* What char to use for bullets */

  struct URL_Struct_ *url;         /* url of doc being translated */
  XP_File out;                     /* Where to send the output */
  char *filename;                  /* output file name, if any */
  XL_CompletionRoutine completion; /* Called when translation finished */
  void* carg;                      /* Data saved for completion routine */
  int status;                      /* Status of URL on completion */

				   /* "other" font is typically East Asian */
  char *otherFontName;		   /* name of "other" PostScript font */
  int16 otherFontCharSetID;	   /* charset ID of "other" font */
  int otherFontWidth;		   /* width of "other" font (square) */
  int otherFontAscent;		   /* Ascent of "other" font (square) */

  MWContext *cx;                   /* original context, if available */
};

#define XL_LOADING_PHASE 1
#define XL_LAYOUT_PHASE 2
#define XL_DRAW_PHASE 3

typedef struct page_breaks {
    int32 y_top;
    int32 y_break;
} PageBreaks;

/*
** Used to store state needed while translation is in progress
*/
struct PrintInfo_ {
	/*
	** BEGIN SPECIAL
	**	If using the table print code, the following fields must
	**	be properly set up.
	*/
  int32	page_height;	/* Size of printable area on page */
  int32	page_width;	/* Size of printable area on page */
  int32	page_break;	/* Current page bottom */
  int32 page_topy;	/* Current page top */
  int phase;
	/*
	** CONTINUE SPECIAL
	**	The table print code maintains these
	*/

	PageBreaks *pages;		/* Contains extents of each page */

  int pt_size;		/* Size of above table */
  int n_pages;		/* # of valid entries in above table */
	/*
	** END SPECIAL
	*/

    /*
    ** AAAOOOGAH
    **
    ** These are used to cache values from the originating context's
    ** function table
    */
  void (*scnatt)(MWContext*);   /* SetCallNetlibAllTheTime */
  void (*ccnatt)(MWContext*);   /* CLearCallNetlibAllTheTime */

  char*	doc_title;	/* best guess at title */
  int32 doc_width;	/* Total document width */
  int32 doc_height;	/* Total document height */

#ifdef LATER
  float	scale;		/* for shrinking pre areas */
  int32	pre_start;	/* First y of current pre section */
  int32	pre_end;	/* Last y of current pre section */
  XP_List	*interesting;	/* List of pre's about which I care */
  XP_Bool	in_pre;		/* True when inside a <pre> section */
#endif

/*
** These fields are used only by the text translator
*/
  char *line;		/* Pointer to data for the current line */
  XP_Bool in_table;	/* True when caching lines in a table */
  XP_Bool first_line_p;		/* true when the first line has not yet been
							   output - this is a kludge for the mail
							   citation code. */
  int table_top,	/* Size of the table being cached */
      table_bottom;
  LineRecord *saved_lines;	/* cached lines for tables */
  int last_y;		/* Used to track blank lines */
};

#endif /* XLATE_H */

