/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

 
#ifndef _PSOBJ_H_
#define _PSOBJ_H_


#include "xp_core.h"
#ifdef __cplusplus
#include "nsColor.h"
#include "nsCoord.h"
#include "nsString.h"

#include "nsCOMPtr.h"
#include "nsIPref.h"
#include "nsHashtable.h"
#include "nsIUnicodeEncoder.h"

#include "nsIDeviceContextSpecPS.h"
#include "nsIPersistentProperties2.h"

class nsIImage;
#endif

#define NS_LETTER_SIZE    0
#define NS_LEGAL_SIZE     1
#define NS_EXECUTIVE_SIZE 2
#define NS_A4_SIZE	  3
#define NS_A3_SIZE	  4

#define NS_PORTRAIT  0
#define NS_LANDSCAPE 1

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

#ifdef __cplusplus
typedef struct PS_LangGroupInfo_ {
  nsIUnicodeEncoder *mEncoder;
  nsHashtable       *mU2Ntable;
} PS_LangGroupInfo;
#endif

typedef struct LineRecord_struct LineRecord;

/*
** Used to store state needed while translation is in progress
*/
struct PrintInfo_ {
  /*	for table printing */
  int32	page_height;	/* Size of printable area on page  */
  int32	page_width;	  /* Size of printable area on page  */
  int32	page_break;	  /* Current page bottom  */
  int32 page_topy;	  /* Current page top  */
  int phase;

	PageBreaks *pages;	/* Contains extents of each page  */

  int pt_size;		    /* Size of above table  */
  int n_pages;		    /* # of valid entries in above table */

  char*	doc_title;	/* best guess at title */
  int32 doc_width;	/* Total document width */
  int32 doc_height;	/* Total document height */

#ifdef LATER
  THIS IS GOING TO BE DELETED XXXXX
  float	scale;		
  int32	pre_start;	
  int32	pre_end;	
  XP_List	*interesting;	
  XP_Bool	in_pre;		
#endif

  /* These fields are used only by the text translator */
  char *line;		          /* Pointer to data for the current line */
  XP_Bool in_table;	      /* True when caching lines in a table */
  XP_Bool first_line_p;		/* delete all this */

  int table_top,table_bottom;
  LineRecord *saved_lines;	/* cached lines for tables  */
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
  
  const char* header;
  const char* footer;

  int *sizes;
  XP_Bool reverse;              /* Output order */
  XP_Bool color;                /* Image output */
  XP_Bool deep_color;		        /* 24 bit color output */
  XP_Bool landscape;            /* Rotated output */
  XP_Bool underline;            /* underline links */
  XP_Bool scale_images;         /* Scale unsized images which are too big */
  XP_Bool scale_pre;		        /* do the pre-scaling thing */
  float dpi;                    /* dpi for externally sized items */
  float rules;			            /* Scale factor for rulers */
  int n_up;                     /* cool page combining */
  int bigger;                   /* Used to init sizes if sizesin NULL */
  int paper_size;               /* Paper Size(letter,legal,exec,a4,a3) */

  const char* prefix;           /* For text xlate, prepended to each line */
  const char* eol;              /* For text translation, line terminator  */
  const char* bullet;           /* What char to use for bullets */

  struct URL_Struct_ *url;      /* url of doc being translated */
  FILE *out;                  /* Where to send the output */
  char *filename;               /* output file name, if any */
  XL_CompletionRoutine completion; /* Called when translation finished */
  void* carg;                   /* Data saved for completion routine */
  int status;                   /* Status of URL on completion */
#ifdef VMS
  char *print_cmd;		/* print command issued in dtor*/
#endif

	/* "other" font is for encodings other than iso-8859-1 */
  char *otherFontName[N_FONTS];		   
  /* name of "other" PostScript font */
  PS_FontInfo *otherFontInfo[N_FONTS];	   
  /* font info parsed from "other" afm file */
  int16 otherFontCharSetID;	    /* charset ID of "other" font */

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

#ifdef __cplusplus
class nsPostScriptObj
{


public:
  nsPostScriptObj();
  ~nsPostScriptObj();
  
  
  /** ---------------------------------------------------
   *  Init PostScript Object 
   *	@update 3/19/99 dwc
   */
  nsresult Init( nsIDeviceContextSpecPS *aSpec, PRUnichar * aTitle);
  /** ---------------------------------------------------
   *  Start a postscript page
   *	@update 2/1/99 dwc
   */
  void begin_page();
  /** ---------------------------------------------------
   *  end the current postscript page
   *	@update 2/1/99 dwc
   */
  void end_page();
  /** ---------------------------------------------------
   *  start the current document
   *	@update 2/1/99 dwc
   */
  void begin_document();

  /** ---------------------------------------------------
   *  end the current document
   *	@update 2/1/99 dwc
   */
  void end_document();
  /** ---------------------------------------------------
   *  move the cursor to this location
   *	@update 2/1/99 dwc
   */
  void moveto(int aX, int aY);
  /** ---------------------------------------------------
   *  move the cursor to this location
   *	@update 2/1/99 dwc
   */
  void moveto_loc(int aX, int aY);
  /** ---------------------------------------------------
   *  put down a line from the current cursor to the x and y location
   *	@update 2/1/99 dwc
   */
  void lineto(int aX, int aY);
  /** ---------------------------------------------------
   *  close the current postscript path, basically will return to the starting point
   *	@update 2/1/99 dwc
   */
  void closepath();
  /** ---------------------------------------------------
   *  create an elliptical path
   *	@update 2/1/99 dwc
   *  @param aWidth - Width of the ellipse
   *  @param aHeight - Height of the ellipse
   */
  void ellipse(int aWidth, int aHeight);
  /** ---------------------------------------------------
   *  create an elliptical path
   *	@update 2/1/99 dwc
   *  @param aWidth - Width of the ellipse
   *  @param aHeight - Height of the ellipse
   */
  void arc(int aWidth, int aHeight,float aStartAngle,float aEndAngle);
  /** ---------------------------------------------------
   *  create a retangular path
   *	@update 2/1/99 dwc
   */
  void box(int aWidth, int aHeight);
  /** ---------------------------------------------------
   *  create a retangular path, but winding the opposite way of a normal path, for clipping
   *	@update 2/1/99 dwc
   */
  void box_subtract(int aWidth, int aHeight);
  /** ---------------------------------------------------
   *  Draw a postscript line
   *	@update 2/1/99 dwc
   */
  void line(int aX1, int aY1, int aX2, int aY2, int aThink);
  /** ---------------------------------------------------
   *  strock the current path
   *	@update 2/1/99 dwc
   */
  void stroke();
  /** ---------------------------------------------------
   *  fill the current path
   *	@update 2/1/99 dwc
   */
  void fill();
  /** ---------------------------------------------------
   *  push the current graphics state onto the postscript stack
   *	@update 2/1/99 dwc
   */
  void graphics_save();
  /** ---------------------------------------------------
   *  pop the graphics state off of the postscript stack
   *	@update 2/1/99 dwc
   */
  void graphics_restore();
  /** ---------------------------------------------------
   *  output a color postscript image
   *	@update 2/1/99 dwc
   */
  void colorimage(nsIImage *aImage,int aX, int aY, int aWidth, int aHeight);

  /** ---------------------------------------------------
   *  output a grayscale postscript image
   *	@update 9/1/99 dwc
   */
  void grayimage(nsIImage *aImage,int aX, int aY, int aWidth, int aHeight);

  /** ---------------------------------------------------
   *  ???
   *	@update 2/1/99 dwc
   */
  void begin_squished_text( float aSqeeze);
  /** ---------------------------------------------------
   *  ???
   *	@update 2/1/99 dwc
   */
  void end_squished_text();
  /** ---------------------------------------------------
   *  Get rid of data structures for the postscript
   *	@update 2/1/99 dwc
   */
  void finalize_translation();
  /** ---------------------------------------------------
   *  ???
   *	@update 2/1/99 dwc
   */
  void annotate_page( const char*, int, int, int);
  /** ---------------------------------------------------
   *  translate the current coordinate system
   *	@update 2/1/99 dwc
   */
  void translate(int aX, int aY);
  /** ---------------------------------------------------
   *  Issue a PS show command, which causes image to be rastered
   *	@update 2/1/99 dwc
   */
  void show(const char* aText, int aLen, const char *aAlign);
  /** ---------------------------------------------------
   *  This version takes an Unicode string. 
   *	@update 3/22/2000 yueheng.xu@intel.com
   */
  void show(const PRUnichar* aText, int aLen, const char *aAlign);
  /** ---------------------------------------------------
   *  set the clipping path to the current path using the winding rule
   *	@update 2/1/99 dwc
   */
  void clip();
  /** ---------------------------------------------------
   *  set the clipping path to the current path using the even/odd rule
   *	@update 2/1/99 dwc
   */
  void eoclip(); 
  /** ---------------------------------------------------
   *  start a new path
   *	@update 2/1/99 dwc
   */
  void newpath();
  /** ---------------------------------------------------
   *  reset the current postsript clip path to the page
   *	@update 2/1/99 dwc
   */
  void initclip();
  /** ---------------------------------------------------
   *  make the current postscript path the current postscript clip path
   *	@update 2/1/99 dwc
   */
  void clippath();
  /** ---------------------------------------------------
   *  set the color
   *	@update 2/1/99 dwc
   */
  void setcolor(nscolor aTheColor);
  /** ---------------------------------------------------
   *  Set up the font
   *	@update 2/1/99 dwc
   */
  void setscriptfont(PRInt16 aFontIndex,const nsString &aFamily,nscoord aHeight, PRUint8 aStyle, PRUint8 aVariant, PRUint16 aWeight, PRUint8 decorations);
  /** ---------------------------------------------------
   *  output a postscript comment
   *	@update 2/1/99 dwc
   */
  void comment(const char *aTheComment);
  /** ---------------------------------------------------
   *  setup language group
   *	@update 5/30/00 katakai
   */
  void setlanggroup(nsIAtom* aLangGroup);
  /** ---------------------------------------------------
   *  prepare conversion table for native ps fonts
   *	@update 6/1/2000 katakai
   */
  void preshow(const PRUnichar* aText, int aLen);

  FILE * GetPrintFile();
  PRBool  InitUnixPrinterProps();
  PRBool  GetUnixPrinterSetting(const nsCAutoString&, char**);
private:
  PSContext             *mPrintContext;
  PrintSetup            *mPrintSetup;
  PRUint16              mPageNumber;
  nsCOMPtr<nsIPersistentProperties> mPrinterProps;
  char                  *mTitle;



  /** ---------------------------------------------------
   *  Set up the postscript
   *	@update 2/1/99 dwc
   */
  void initialize_translation(PrintSetup* aPi);
  /** ---------------------------------------------------
   *  initialize language group
   *	@update 5/30/00 katakai
   */
  void initlanggroup();

};

#endif /* __cplusplus */

#endif
