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

 
#ifndef _PSOBJ_H_
#define _PSOBJ_H_


#include "xp_core.h"
#include "xp_file.h"
#include "ntypes.h"
#include "net.h"
#include "nscolor.h"
#include "nscoord.h"


class nsIImage;


#define NS_LETTER_SIZE    0
#define NS_LEGAL_SIZE     1
#define NS_EXECUTIVE_SIZE 2

#define PAGE_WIDTH 612 // Points
#define PAGE_HEIGHT 792 //Points
#define N_FONTS 8
#define INCH_TO_PAGE(f) ((int) (.5 + (f)*720))
#define PAGE_TO_POINT_I(f) ((int) ((f) / 10.0))
#define PAGE_TO_POINT_F(f) ((f) / 10.0)
#define POINT_TO_PAGE(p) ((p)*10)

typedef void (*XL_CompletionRoutine)(void*);


typedef struct {
    short llx, lly, urx, ury;
} PS_BBox;

typedef struct {
	short wx, wy;
	PS_BBox charBBox;
} PS_CharInfo;

typedef struct {
    char *name;
    PS_BBox fontBBox;
    short upos, uthick;
    PS_CharInfo chars[256];
} PS_FontInfo;

typedef struct page_breaks {
    int32 y_top;
    int32 y_break;
} PageBreaks;

typedef struct LineRecord_struct LineRecord;

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

typedef struct PrintInfo_ PrintInfo;

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
  int paper_size;                  /* Paper Size(letter,legal,exec,a4) */

  char* prefix;                    /* For text xlate, prepended to each line */
  char* eol;			   /* For text translation, line terminator */
  char* bullet;                    /* What char to use for bullets */

  struct URL_Struct_ *url;         /* url of doc being translated */
  XP_File out;                     /* Where to send the output */
  char *filename;                  /* output file name, if any */
  XL_CompletionRoutine completion; /* Called when translation finished */
  void* carg;                      /* Data saved for completion routine */
  int status;                      /* Status of URL on completion */

		/* "other" font is for encodings other than iso-8859-1 */
  char *otherFontName[N_FONTS];		   
  				/* name of "other" PostScript font */
  PS_FontInfo *otherFontInfo[N_FONTS];	   
  				/* font info parsed from "other" afm file */
  int16 otherFontCharSetID;	   /* charset ID of "other" font */

  //MWContext *cx;                   /* original context, if available */
};

typedef struct PrintSetup_ PrintSetup;

struct PSContext_{

    char        *url;         /* URL of current document */
    char        * name;	      /* name of this context */
    char        * title;		  /* title (if supplied) of current document */
    PrintSetup	*prSetup;	    /* Info about print job */
    PrintInfo	  *prInfo;	    /* State information for printing process */
};
typedef struct PSContext_ PSContext;

class nsPostScriptObj
{


public:
  nsPostScriptObj();
  ~nsPostScriptObj();


  void begin_page();
  void end_page();
  void end_document();
  void moveto(int x, int y);
  void moveto_loc(int x, int y);
  void lineto(int x1, int y1);
  void closepath();
  void circle(int w, int h);
  void box(int w, int h);
  void box_subtract(int w, int h);
  void line(int x1, int y1, int x2, int y2, int thick);
  void stroke();
  void fill();
  void graphics_save();
  void graphics_restore();
  void colorimage(nsIImage *aImage,int x, int y, int w, int h);
  void begin_squished_text( float);
  void end_squished_text();
  void finalize_translation();
  void annotate_page( char*, int, int, int);
  void translate(int x, int y);
  void show(char* txt, int len, char *align);
  void clip();
  void newpath();
  void initclip();
  void setcolor(nscolor aTheColor);
  void setscriptfont(nscoord aHeight, PRUint8 aStyle, PRUint8 aVariant, PRUint16 aWeight, PRUint8 decorations);

private:
  PSContext             *mPrintContext;
  PrintSetup            *mPrintSetup;
  PRUint16              mPageNumber;


  void initialize_translation(PrintSetup* pi);
  void begin_document();

};



#endif