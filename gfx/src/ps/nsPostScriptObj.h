/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * ***** END LICENSE BLOCK ***** */

 
#ifndef _PSOBJ_H_
#define _PSOBJ_H_

#include "prtypes.h"  /* for int32 & friends */
#ifdef __cplusplus
#include "nsColor.h"
#include "nsCoord.h"
#include "nsRect.h"
#include "nsString.h"

#include "nsCOMPtr.h"
#include "nsIPref.h"
#include "nsHashtable.h"
#include "nsIUnicodeEncoder.h"

#include "nsIDeviceContextSpecPS.h"
#include "nsIPersistentProperties2.h"
#include "nsTempfilePS.h"
#include "nsEPSObjectPS.h"

class nsIImage;
class nsIAtom;
#endif

#include <stdio.h>

#define N_FONTS 8
#define INCH_TO_PAGE(f) ((int) (.5 + (f)*720))

typedef int XP_Bool;

typedef void (*XL_CompletionRoutine)(void*);

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
  const char *doc_title; /* best guess at title */

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
  nscoord width;                /* Page size, in twips, as oriented for */
  nscoord height;               /* this print job. */
  
  const char* header;
  const char* footer;
  const char* paper_name;

  int *sizes;
  XP_Bool reverse;              /* Output order */
  XP_Bool color;                /* Image output */
  XP_Bool deep_color;		        /* 24 bit color output */
  XP_Bool landscape;            /* Rotated output */
  XP_Bool underline;            /* underline links */
  XP_Bool scale_images;         /* Scale unsized images which are too big */
  XP_Bool scale_pre;		        /* do the pre-scaling thing */
  float rules;			            /* Scale factor for rulers */
  int n_up;                     /* cool page combining */
  int bigger;                   /* Used to init sizes if sizesin NULL */
  const char* prefix;           /* For text xlate, prepended to each line */
  const char* eol;              /* For text translation, line terminator  */
  const char* bullet;           /* What char to use for bullets */

  struct URL_Struct_ *url;      /* url of doc being translated */
  XL_CompletionRoutine completion; /* Called when translation finished */
  void* carg;                   /* Data saved for completion routine */
  int status;                   /* Status of URL on completion */
  int num_copies;               /* Number of copies of job to print */
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

struct PSBoundingBox {          /* For BeginEPSF() */
  float llx, lly, urx, ury;
};

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
  nsresult Init( nsIDeviceContextSpecPS *aSpec );
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
   *  end the current document
   *	@update 2/1/99 dwc
   */
  nsresult end_document();

  /** ---------------------------------------------------
   *  Write the document prolog to the given file handle
   *  @param File handle which should receive the prolog.
   */
  void write_prolog(FILE *aHandle);

  /** ---------------------------------------------------
   *  Write the document script (body) to the given file handle.
   *  @param File handle which should receive the prolog.
   *  @return NS_OK or a suitable error code for I/O errors.
   */
  nsresult write_script(FILE *aHandle);

  /** ---------------------------------------------------
   *  move the current point to this location
   *	@update 9/30/2003 kherron
   *	@param  aX   X coordinate
   *	        aY   Y coordinate
   *    @return VOID
   */
  void moveto(nscoord aX, nscoord aY);
  /** ---------------------------------------------------
   *  Add a line to the current path, from the current point
   *  to the specified point.
   *	@update 9/30/2003
   *	@param  aX   X coordinate
   *	        aY   Y coordinate
   *	@return VOID
   */
  void lineto(nscoord aX, nscoord aY);
  /** ---------------------------------------------------
   *  close the current path.
   *	@update 2/1/99 dwc
   */
  void closepath();
  /** ---------------------------------------------------
   *  create an elliptical path
   *	@update 2/1/99 dwc
   *  @param aWidth - Width of the ellipse
   *  @param aHeight - Height of the ellipse
   */
  void arc(nscoord aWidth, nscoord aHeight,float aStartAngle,float aEndAngle);
  /** ---------------------------------------------------
   *  create a retangular path. The current point is at
   *  the top left corner.
   *	@update 9/30/2003 kherron
   *	@param aX      Starting point X
   *	@param aY      Starting point Y
   *	@param aWidth  Distance rightwards.
   *	@param aHeight Distance downwards.
   */
  void box(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
  /** ---------------------------------------------------
   *  create a retangular path, drawing counterclockwise.
   *  The current point is at the top left corner.
   *	@update 9/30/2003 kherron
   *	@param aWidth  Distance rightwards.
   *	@param aHeight Distance downwards.
   */
  void box_subtract(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
  /** ---------------------------------------------------
   *  Draw (stroke) a line.
   *	@update 9/30/2003 kherron
   *	@param  aX1     Starting point X
   *	        aY1     Starting point Y
   *	        aX2     Ending point X
   *	        aY2     Ending point Y
   *	        aThick  Line thickness
   */
  void line(nscoord aX1, nscoord aY1, nscoord aX2, nscoord aY2, nscoord aThick);
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
  void draw_image(nsIImage *anImage,
      const nsRect& sRect, const nsRect& iRect, const nsRect& dRect);

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
   *  Output postscript to scale the current coordinate system
   *	@param aX   X scale factor
   *	@param aY   Y scale factor
   */
  void scale(float aX, float aY);
  /** ---------------------------------------------------
   *  translate the current coordinate system
   *	@update 9/30/2003
   *	@param aX   Distance to move X coordinates
   *	       aY   Distance to move Y coordinates
   */
  void translate(nscoord aX, nscoord aY);
  /** ---------------------------------------------------
   *  Issue a PS show command, which causes image to be rastered
   *	@update 2/1/99 dwc
   */
  void show(const char* aText, int aLen, const char *aAlign);
  /** ---------------------------------------------------
   *  This version takes an Unicode string. 
   *	@update 3/22/2000 yueheng.xu@intel.com
   */
  void show(const PRUnichar* aText, int aLen, const char *aAlign, int aType);
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
   *  Set up the font
   *    @update 12/17/2002 louie
   */
  void setfont(const nsCString aFontName, PRUint32 aHeight);
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

  /** ---------------------------------------------------
   *  Render an encapsulated postscript object into the document
   *	@update 3/6/2004 kherron
   *	@param aRect Rectangle in which to render the EPS
   *	@param anEPS Object to render
   *	@return NS_OK if the object was copied successfully.
   */
  nsresult render_eps(const nsRect& aRect, nsEPSObjectPS &anEPS);
  
  void settitle(PRUnichar * aTitle);

  /** ---------------------------------------------------
   *  Retrieve the handle for the temp file holding the
   *  document script.
   *  @return the script file handle.
   */
  FILE * GetScriptHandle() { return mScriptFP; }

  PRBool  GetUnixPrinterSetting(const nsCAutoString&, char**);
  PrintSetup            *mPrintSetup;
private:
  PSContext             *mPrintContext;
  PRUint16              mPageNumber;
  nsCOMPtr<nsIPersistentProperties> mPrinterProps;
  char                  *mTitle;
  nsTempfilePS          mTempfileFactory;
  nsCOMPtr<nsILocalFile> mDocScript;
  FILE                  *mScriptFP;


  /** ---------------------------------------------------
   *  Set up the postscript
   *	@update 2/1/99 dwc
   */
  void initialize_translation(PrintSetup* aPi);
  /** ---------------------------------------------------
   *  initialize language group
   *	@update 5/17/2004 kherron
   *	@param aHandle File handle to write langgroup directives
   */
  void initlanggroup(FILE *aHandle);

};

#endif /* __cplusplus */

#endif
