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

#include "xlate_i.h"
#include "isotab.c"
#include "locale.h"

#define RED_PART 0.299		/* Constants for converting RGB to greyscale */
#define GREEN_PART 0.587
#define BLUE_PART 0.114

#define XL_SET_NUMERIC_LOCALE() char* cur_locale = setlocale(LC_NUMERIC, "C")
#define XL_RESTORE_NUMERIC_LOCALE() setlocale(LC_NUMERIC, cur_locale)

/*
** These two functions swap values around in order to deal with page
** rotation.
*/

void xl_initialize_translation(MWContext *cx, PrintSetup* pi)
{
  PrintSetup *dup = XP_NEW(PrintSetup);
  *dup = *pi;
  cx->prSetup = dup;
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

void xl_finalize_translation(MWContext *cx)
{
    XP_DELETE(cx->prSetup);
    cx->prSetup = NULL;
}

void xl_begin_document(MWContext *cx)
{
    int i;
    XP_File f;

    f = cx->prSetup->out;
    XP_FilePrintf(f, "%%!PS-Adobe-3.0\n");
    XP_FilePrintf(f, "%%%%BoundingBox: %d %d %d %d\n",
        PAGE_TO_POINT_I(cx->prSetup->left),
	PAGE_TO_POINT_I(cx->prSetup->bottom),
	PAGE_TO_POINT_I(cx->prSetup->width-cx->prSetup->right),
	PAGE_TO_POINT_I(cx->prSetup->height-cx->prSetup->top));
    XP_FilePrintf(f, "%%%%Creator: Mozilla (NetScape) HTML->PS\n");
    XP_FilePrintf(f, "%%%%DocumentData: Clean7Bit\n");
    XP_FilePrintf(f, "%%%%Orientation: %s\n",
        (cx->prSetup->width < cx->prSetup->height) ? "Portrait" : "Landscape");
    XP_FilePrintf(f, "%%%%Pages: %d\n", (int) cx->prInfo->n_pages);
    if (cx->prSetup->reverse)
	XP_FilePrintf(f, "%%%%PageOrder: Descend\n");
    else
	XP_FilePrintf(f, "%%%%PageOrder: Ascend\n");
    XP_FilePrintf(f, "%%%%Title: %s\n", cx->prInfo->doc_title);
#ifdef NOTYET
    XP_FilePrintf(f, "%%%%For: %n", user_name_stuff);
#endif
    XP_FilePrintf(f, "%%%%EndComments\n");
    
    XP_FilePrintf(f, "%%%%BeginProlog\n");
    XP_FilePrintf(f, "[");
    for (i = 0; i < 256; i++)
      {
	if (*isotab[i] == '\0')
	  XP_FilePrintf(f, " /.notdef");
	else
	  XP_FilePrintf(f, " /%s", isotab[i]);
	if (( i % 10) == 9)
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
    if (cx->prSetup->otherFontName)
      XP_FilePrintf(f, "/of { /%s findfont exch scalefont "
		     "setfont } bind def\n", cx->prSetup->otherFontName);
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

void xl_begin_page(MWContext *cx, int pn)
{
  XP_File f;

  f = cx->prSetup->out;
  pn++;
  XP_FilePrintf(f, "%%%%Page: %d %d\n", pn, pn);
  XP_FilePrintf(f, "%%%%BeginPageSetup\n/pagelevel save def\n");
  if (cx->prSetup->landscape)
    XP_FilePrintf(f, "%d 0 translate 90 rotate\n",
		  PAGE_TO_POINT_I(cx->prSetup->height));
  XP_FilePrintf(f, "%d 0 translate\n", PAGE_TO_POINT_I(cx->prSetup->left));
  XP_FilePrintf(f, "%%%%EndPageSetup\n");
#if 0
  xl_annotate_page(cx, cx->prSetup->header, 0, -1, pn);
#endif
  XP_FilePrintf(f, "newpath 0 %d moveto ", PAGE_TO_POINT_I(cx->prSetup->bottom));
  XP_FilePrintf(f, "%d 0 rlineto 0 %d rlineto -%d 0 rlineto ",
			PAGE_TO_POINT_I(cx->prInfo->page_width),
			PAGE_TO_POINT_I(cx->prInfo->page_height),
			PAGE_TO_POINT_I(cx->prInfo->page_width));
  XP_FilePrintf(f, " closepath clip newpath\n");
}

void xl_end_page(MWContext *cx, int pn)
{
#if 0
  xl_annotate_page(cx, cx->prSetup->footer,
		   cx->prSetup->height-cx->prSetup->bottom-cx->prSetup->top,
		   1, pn);
  XP_FilePrintf(cx->prSetup->out, "pagelevel restore\nshowpage\n");
#endif

  XP_FilePrintf(cx->prSetup->out, "pagelevel restore\n");
  xl_annotate_page(cx, cx->prSetup->header, cx->prSetup->top/2, -1, pn);
  xl_annotate_page(cx, cx->prSetup->footer,
				   cx->prSetup->height - cx->prSetup->bottom/2,
				   1, pn);
  XP_FilePrintf(cx->prSetup->out, "showpage\n");
}

void xl_end_document(MWContext *cx)
{
    XP_FilePrintf(cx->prSetup->out, "%%%%EOF\n");
}

void xl_moveto(MWContext* cx, int x, int y)
{
  XL_SET_NUMERIC_LOCALE();
  y -= cx->prInfo->page_topy;
  /*
  ** Agh! Y inversion again !!
  */
  y = (cx->prInfo->page_height - y - 1) + cx->prSetup->bottom;

  XP_FilePrintf(cx->prSetup->out, "%g %g moveto\n",
		PAGE_TO_POINT_F(x), PAGE_TO_POINT_F(y));
  XL_RESTORE_NUMERIC_LOCALE();
}

void xl_moveto_loc(MWContext* cx, int x, int y)
{
  /* This routine doesn't care about the clip region in the page */

  XL_SET_NUMERIC_LOCALE();

  /*
  ** Agh! Y inversion again !!
  */
  y = (cx->prSetup->height - y - 1);

  XP_FilePrintf(cx->prSetup->out, "%g %g moveto\n",
		PAGE_TO_POINT_F(x), PAGE_TO_POINT_F(y));
  XL_RESTORE_NUMERIC_LOCALE();
}

void xl_translate(MWContext* cx, int x, int y)
{
    XL_SET_NUMERIC_LOCALE();
    y -= cx->prInfo->page_topy;
    /*
    ** Agh! Y inversion again !!
    */
    y = (cx->prInfo->page_height - y - 1) + cx->prSetup->bottom;

    XP_FilePrintf(cx->prSetup->out, "%g %g translate\n", PAGE_TO_POINT_F(x), PAGE_TO_POINT_F(y));
    XL_RESTORE_NUMERIC_LOCALE();
}

void xl_show(MWContext *cx, char* txt, int len, char *align)
{
    XP_File f;

    f = cx->prSetup->out;
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

void xl_circle(MWContext* cx, int w, int h)
{
  XL_SET_NUMERIC_LOCALE();
  XP_FilePrintf(cx->prSetup->out, "%g %g c ", PAGE_TO_POINT_F(w)/2.0, PAGE_TO_POINT_F(h)/2.0);
  XL_RESTORE_NUMERIC_LOCALE();
}

void xl_box(MWContext* cx, int w, int h)
{
  XL_SET_NUMERIC_LOCALE();
  XP_FilePrintf(cx->prSetup->out, "%g 0 rlineto 0 %g rlineto %g 0 rlineto closepath ",
      PAGE_TO_POINT_F(w), -PAGE_TO_POINT_F(h), -PAGE_TO_POINT_F(w));
  XL_RESTORE_NUMERIC_LOCALE();
}

MODULE_PRIVATE void
xl_draw_border(MWContext *cx, int x, int y, int w, int h, int thick)
{
  XL_SET_NUMERIC_LOCALE();
  XP_FilePrintf(cx->prSetup->out, "gsave %g setlinewidth\n ",
		  PAGE_TO_POINT_F(thick));
  xl_moveto(cx, x, y);
  xl_box(cx, w, h);
  xl_stroke(cx);
  XP_FilePrintf(cx->prSetup->out, "grestore\n");
  XL_RESTORE_NUMERIC_LOCALE();
}

MODULE_PRIVATE void
xl_draw_3d_border(MWContext *cx, int x, int y, int w, int h, int thick, int tl, int br)
{
  int llx, lly;

  XL_SET_NUMERIC_LOCALE();
  XP_FilePrintf(cx->prSetup->out, "gsave\n ");

  /* lower left */
  llx = x;
  lly = y + h;

  /* top left */
  xl_moveto(cx, llx, lly);
  XP_FilePrintf(cx->prSetup->out, 
				"%g %g rlineto 0 %g rlineto %g 0 rlineto %g %g rlineto %g 0 rlineto closepath\n",
				PAGE_TO_POINT_F(thick), PAGE_TO_POINT_F(thick),
				PAGE_TO_POINT_F(h-2*thick),
				PAGE_TO_POINT_F(w-2*thick),
				PAGE_TO_POINT_F(thick), PAGE_TO_POINT_F(thick),
				-PAGE_TO_POINT_F(w));
  XP_FilePrintf(cx->prSetup->out, ".%d setgray fill\n", tl);

  /* bottom right */
  xl_moveto(cx, llx, lly);
  XP_FilePrintf(cx->prSetup->out, 
				"%g %g rlineto %g 0 rlineto 0 %g rlineto %g %g rlineto 0 %g rlineto closepath\n",
				PAGE_TO_POINT_F(thick),	PAGE_TO_POINT_F(thick),
				PAGE_TO_POINT_F(w-2*thick),
				PAGE_TO_POINT_F(h-2*thick),
				PAGE_TO_POINT_F(thick), PAGE_TO_POINT_F(thick),
				-PAGE_TO_POINT_F(h));
  XP_FilePrintf(cx->prSetup->out, ".%d setgray fill\n", br);

  XP_FilePrintf(cx->prSetup->out, "grestore\n");
  XL_RESTORE_NUMERIC_LOCALE();
}

MODULE_PRIVATE void
xl_draw_3d_radiobox(MWContext *cx, int x, int y, int w, int h, int thick,
					int top, int bottom, int center)
{
  int lx, ly;

  XL_SET_NUMERIC_LOCALE();
  XP_FilePrintf(cx->prSetup->out, "gsave\n ");

  /* left */
  lx = x;
  ly = y + h/2;

  /* bottom */
  xl_moveto(cx, lx, ly);
  XP_FilePrintf(cx->prSetup->out, 
				"%g %g rlineto %g %g rlineto %g 0 rlineto %g %g rlineto %g %g rlineto closepath\n",
				PAGE_TO_POINT_F(w/2), -PAGE_TO_POINT_F(h/2),
				PAGE_TO_POINT_F(w/2), PAGE_TO_POINT_F(h/2),
				-PAGE_TO_POINT_F(thick),
				-PAGE_TO_POINT_F(w/2-thick),-PAGE_TO_POINT_F(h/2-thick),
	            -PAGE_TO_POINT_F(w/2-thick), PAGE_TO_POINT_F(h/2-thick));
  XP_FilePrintf(cx->prSetup->out, ".%d setgray fill\n", bottom);

  /* top  */
  xl_moveto(cx, lx, ly);
  XP_FilePrintf(cx->prSetup->out, 
				"%g %g rlineto %g %g rlineto %g 0 rlineto %g %g rlineto %g %g rlineto closepath\n",
				PAGE_TO_POINT_F(w/2), PAGE_TO_POINT_F(h/2),
				PAGE_TO_POINT_F(w/2), -PAGE_TO_POINT_F(h/2),
				-PAGE_TO_POINT_F(thick),
				-PAGE_TO_POINT_F(w/2-thick), PAGE_TO_POINT_F(h/2-thick),
	            -PAGE_TO_POINT_F(w/2-thick), -PAGE_TO_POINT_F(h/2-thick));
  XP_FilePrintf(cx->prSetup->out, ".%d setgray fill\n", top);

  /* center */
  if (center != 10) {
	  xl_moveto(cx, lx+thick, ly);
	  XP_FilePrintf(cx->prSetup->out, 
					"%g %g rlineto %g %g rlineto %g %g rlineto closepath\n",
					PAGE_TO_POINT_F(w/2-thick), -PAGE_TO_POINT_F(h/2-thick),
					PAGE_TO_POINT_F(w/2-thick), PAGE_TO_POINT_F(h/2-thick),
					-PAGE_TO_POINT_F(w/2-thick), PAGE_TO_POINT_F(h/2-thick));
	  XP_FilePrintf(cx->prSetup->out, ".%d setgray fill\n", center);
  }

  XP_FilePrintf(cx->prSetup->out, "grestore\n");
  XL_RESTORE_NUMERIC_LOCALE();
}

MODULE_PRIVATE void
xl_draw_3d_checkbox(MWContext *cx, int x, int y, int w, int h, int thick,
					int tl, int br, int center)
{
  int llx, lly;

  XL_SET_NUMERIC_LOCALE();
  XP_FilePrintf(cx->prSetup->out, "gsave\n ");

  /* lower left */
  llx = x;
  lly = y + h;

  /* top left */
  xl_moveto(cx, llx, lly);
  XP_FilePrintf(cx->prSetup->out, 
				"%g %g rlineto 0 %g rlineto %g 0 rlineto %g %g rlineto %g 0 rlineto closepath\n",
				PAGE_TO_POINT_F(thick), PAGE_TO_POINT_F(thick),
				PAGE_TO_POINT_F(h-2*thick),
				PAGE_TO_POINT_F(w-2*thick),
				PAGE_TO_POINT_F(thick), PAGE_TO_POINT_F(thick),
				-PAGE_TO_POINT_F(w));
  XP_FilePrintf(cx->prSetup->out, ".%d setgray fill\n", tl);

  /* bottom right */
  xl_moveto(cx, llx, lly);
  XP_FilePrintf(cx->prSetup->out, 
				"%g %g rlineto %g 0 rlineto 0 %g rlineto %g %g rlineto 0 %g rlineto closepath\n",
				PAGE_TO_POINT_F(thick),	PAGE_TO_POINT_F(thick),
				PAGE_TO_POINT_F(w-2*thick),
				PAGE_TO_POINT_F(h-2*thick),
				PAGE_TO_POINT_F(thick), PAGE_TO_POINT_F(thick),
				-PAGE_TO_POINT_F(h));
  XP_FilePrintf(cx->prSetup->out, ".%d setgray fill\n", br);

  /* center */
  if (center != 10) {
	  xl_moveto(cx, llx+thick, lly-thick);
	  XP_FilePrintf(cx->prSetup->out, 
					"0 %g rlineto %g 0 rlineto 0 %g rlineto closepath\n",
					PAGE_TO_POINT_F(h-2*thick),
					PAGE_TO_POINT_F(w-2*thick),
					-PAGE_TO_POINT_F(h-2*thick));
	  XP_FilePrintf(cx->prSetup->out, ".%d setgray fill\n", center);
  }

  XP_FilePrintf(cx->prSetup->out, "grestore\n");
  XL_RESTORE_NUMERIC_LOCALE();
}

extern void xl_draw_3d_arrow(MWContext *cx, int x , int y, int thick, int w,
							 int h, XP_Bool up, int left, int right, int base)
{
	int tx, ty;

	XL_SET_NUMERIC_LOCALE();
	XP_FilePrintf(cx->prSetup->out, "gsave\n ");

	if (up) {
		tx = x + w/2;
		ty = y;

		/* left */

		xl_moveto(cx, tx, ty);
		XP_FilePrintf(cx->prSetup->out, 
					  "%g %g rlineto %g %g rlineto %g %g rlineto closepath\n",
					  -PAGE_TO_POINT_F(w/2), -PAGE_TO_POINT_F(h),
					  PAGE_TO_POINT_F(thick), PAGE_TO_POINT_F(thick),
					  PAGE_TO_POINT_F(w/2-thick), PAGE_TO_POINT_F(h-2*thick));
		XP_FilePrintf(cx->prSetup->out, ".%d setgray fill\n", left);

		/* right */

		xl_moveto(cx, tx, ty);
		XP_FilePrintf(cx->prSetup->out, 
					  "%g %g rlineto %g %g rlineto %g %g rlineto closepath\n",
					  PAGE_TO_POINT_F(w/2), -PAGE_TO_POINT_F(h),
					  -PAGE_TO_POINT_F(thick), PAGE_TO_POINT_F(thick),
					  -PAGE_TO_POINT_F(w/2-thick), PAGE_TO_POINT_F(h-2*thick));
		XP_FilePrintf(cx->prSetup->out, ".%d setgray fill\n", right);

		/* base */

		xl_moveto(cx, tx-w/2, ty+h);
		XP_FilePrintf(cx->prSetup->out, 
					  "%g %g rlineto %g 0 rlineto %g %g rlineto closepath\n",
					  PAGE_TO_POINT_F(thick), PAGE_TO_POINT_F(thick),
					  PAGE_TO_POINT_F(w-2*thick),
					  PAGE_TO_POINT_F(thick), -PAGE_TO_POINT_F(thick));
		XP_FilePrintf(cx->prSetup->out, ".%d setgray fill\n", base);
	}
	else {
		tx = x + w/2;
		ty = y + h;

		/* left */

		xl_moveto(cx, tx, ty);
		XP_FilePrintf(cx->prSetup->out, 
					  "%g %g rlineto %g %g rlineto %g %g rlineto closepath\n",
					  -PAGE_TO_POINT_F(w/2), PAGE_TO_POINT_F(h),
					  PAGE_TO_POINT_F(thick), -PAGE_TO_POINT_F(thick),
					  PAGE_TO_POINT_F(w/2-thick), -PAGE_TO_POINT_F(h-2*thick));
		XP_FilePrintf(cx->prSetup->out, ".%d setgray fill\n", left);

		/* right */

		xl_moveto(cx, tx, ty);
		XP_FilePrintf(cx->prSetup->out, 
					  "%g %g rlineto %g %g rlineto %g %g rlineto closepath\n",
					  PAGE_TO_POINT_F(w/2), PAGE_TO_POINT_F(h),
					  -PAGE_TO_POINT_F(thick), -PAGE_TO_POINT_F(thick),
					  -PAGE_TO_POINT_F(w/2-thick), -PAGE_TO_POINT_F(h-2*thick));
		XP_FilePrintf(cx->prSetup->out, ".%d setgray fill\n", right);

		/* base */

		xl_moveto(cx, x, y);
		XP_FilePrintf(cx->prSetup->out, 
					  "%g %g rlineto %g 0 rlineto %g %g rlineto closepath\n",
					  PAGE_TO_POINT_F(thick), -PAGE_TO_POINT_F(thick),
					  PAGE_TO_POINT_F(w-2*thick),
					  PAGE_TO_POINT_F(thick), PAGE_TO_POINT_F(thick));
		XP_FilePrintf(cx->prSetup->out, ".%d setgray fill\n", base);
	}

  XP_FilePrintf(cx->prSetup->out, "grestore\n");
  XL_RESTORE_NUMERIC_LOCALE();
}

void xl_line(MWContext* cx, int x1, int y1, int x2, int y2, int thick)
{
  XL_SET_NUMERIC_LOCALE();
  XP_FilePrintf(cx->prSetup->out, "gsave %g setlinewidth\n ",
				PAGE_TO_POINT_F(thick));

  y1 -= cx->prInfo->page_topy;
  y1 = (cx->prInfo->page_height - y1 - 1) + cx->prSetup->bottom;
  y2 -= cx->prInfo->page_topy;
  y2 = (cx->prInfo->page_height - y2 - 1) + cx->prSetup->bottom;

  XP_FilePrintf(cx->prSetup->out, "%g %g moveto %g %g lineto\n",
		PAGE_TO_POINT_F(x1), PAGE_TO_POINT_F(y1),
		PAGE_TO_POINT_F(x2), PAGE_TO_POINT_F(y2));
  xl_stroke(cx);

  XP_FilePrintf(cx->prSetup->out, "grestore\n");
  XL_RESTORE_NUMERIC_LOCALE();
}

void xl_stroke(MWContext *cx)
{
    XP_FilePrintf(cx->prSetup->out, " stroke \n");
}

void xl_fill(MWContext *cx)
{
    XP_FilePrintf(cx->prSetup->out, " fill \n");
}

/*
** This function works, but is starting to show it's age, as the list
** of known problems grows:
**
** +  The images are completely uncompressed, which tends to generate
**    huge output files.  The university prototype browser uses RLE
**    compression on the images, which causes them to print slowly,
**    but is a huge win on some class of images.
** +  12 bit files can be constructed which print on any printer, but
**    not 24 bit files.
** +  The 12 bit code is careful not to create a string-per-row, unless
**    it is going through the compatibility conversion routine.
** +  The 1-bit code is completely unused and untested.
** +  It should squish the image if squishing is currently in effect.
*/

void xl_colorimage(MWContext *cx, int x, int y, int w, int h, IL_Pixmap *image,
                   IL_Pixmap *mask)
{
    uint8 pixmap_depth;
    int row, col, pps;
    int n;
    int16 *p12;
    int32 *p24;
    unsigned char *p8;
    int cbits;
    int rowdata, rowextra, row_ends_within_byte;
    XP_File f;
    NI_PixmapHeader *img_header = &image->header;

    XL_SET_NUMERIC_LOCALE();
    f = cx->prSetup->out;
    pps = 1;
    row_ends_within_byte = 0;
    pixmap_depth = img_header->color_space->pixmap_depth;

    /* Imagelib row data is aligned to 32-bit boundaries */
    if (pixmap_depth == 1) {
        rowdata = (img_header->width + 7)/8;
        rowextra = img_header->widthBytes - rowdata;
        cbits = 1;
        pps = 8;
    }
    else if (pixmap_depth == 16) {
        if (cx->prSetup->color) {
            rowdata = (img_header->width*12)/8;
            row_ends_within_byte = (img_header->width*12)%8 ? 1 : 0;
            cbits = 4;
        } else {
            cbits = 8;
            rowdata = img_header->width;
        }
        rowextra = img_header->widthBytes - img_header->width * 2;
    }
    else { /* depth assumed to be 32 */
        rowdata = 3 * img_header->width;
        rowextra = 0;
        cbits = 8;
    }

    assert(pixmap_depth == 16 || pixmap_depth == 32 || pixmap_depth == 1);
    XP_FilePrintf(f, "gsave\n");
    XP_FilePrintf(f, "/rowdata %d string def\n",
                  rowdata + row_ends_within_byte);
    xl_translate(cx, x, y + h);
    XP_FilePrintf(f, "%g %g scale\n", PAGE_TO_POINT_F(w), PAGE_TO_POINT_F(h));
    XP_FilePrintf(f, "%d %d ", img_header->width, img_header->height);
    XP_FilePrintf(f, "%d ", cbits);
    XP_FilePrintf(f, "[%d 0 0 %d 0 %d]\n", img_header->width,
                  -img_header->height, img_header->height);
    if (cx->prSetup->color && pixmap_depth == 16)
        XP_FilePrintf(f, " smartimage12rgb\n");
    else if (pixmap_depth == 32) {
        XP_FilePrintf(f, " { currentfile rowdata readhexstring pop }\n");
        XP_FilePrintf(f, " false 3 colorimage\n");
    } else {
        XP_FilePrintf(f, " { currentfile rowdata readhexstring pop }\n");
        XP_FilePrintf(f, " image\n");
    }

    n = 0;
    p8 = (unsigned char*) image->bits;
    p12 = (int16*) image->bits;
    p24 = (int32*) image->bits;
    for (row = 0; row < img_header->height; row++) {
        for (col = 0; col < img_header->width; col += pps) {
            switch ( pixmap_depth ) {
            case 16:
                if (cx->prSetup->color) {
                    if (n > 76) {
                        XP_FilePrintf(f, "\n");
                        n = 0;
                    }
                    XP_FilePrintf(f, "%03x", 0xfff & *p12++);
                    n += 3;
                } else {
                    unsigned char value;
                    value = (*p12 & 0x00f)/15.0 * GREEN_PART * 255.0
                        + ((((*p12 & 0x0f0)>>4)/15.0) * BLUE_PART * 255.0)
                        + ((((*p12 & 0xf00)>>8)/15.0) * RED_PART * 255.0);
                    p12++;
                    if (n > 76) {
                        XP_FilePrintf(f, "\n");
                        n = 0;
                    }
                    XP_FilePrintf(f, "%02X", value);
                    n += 2;
                }
                break;
            case 32:
                if (n > 71) {
                    XP_FilePrintf(f, "\n");
                    n = 0;
                }
                XP_FilePrintf(f, "%06x", (int) (0xffffff & *p24++));
                n += 6;
                break;
            case 1:
                if (n > 78) {
                    XP_FilePrintf(f, "\n");
                    n = 0;
                }
                XP_FilePrintf(f, "%02x", 0xff ^ *p8++);
                n += 2;
                break;
            }
        }
        if (row_ends_within_byte) {
            XP_FilePrintf(f, "0");
            n += 1;
        }
        p8 += rowextra;
        p12 = (int16*)((char*)p12 + rowextra);
    }
    XP_FilePrintf(f, "\ngrestore\n");
    XL_RESTORE_NUMERIC_LOCALE();
}

MODULE_PRIVATE void
xl_begin_squished_text(MWContext *cx, float scale)
{
    XL_SET_NUMERIC_LOCALE();
    XP_FilePrintf(cx->prSetup->out, "gsave %g 1 scale\n", scale);
    XL_RESTORE_NUMERIC_LOCALE();
}

MODULE_PRIVATE void
xl_end_squished_text(MWContext *cx)
{
    XP_FilePrintf(cx->prSetup->out, "grestore\n");
}
