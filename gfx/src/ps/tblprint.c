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
** This file contains a couple of routines which might be useful
** in a cross platform way for dealing with the calculations
** involved in table printing.
*/

#include "xlate_i.h"

PRIVATE XP_Bool
too_big(MWContext* cx, int size)
{
  return size >= (cx->prInfo->page_height/8)*7;
}

/*
** Called by the FE_Display* functions with the Y span of the item to be
** displayed.  Used to find page breaks and to reject items which are
** not on the current page.
**
** Returns:
** 	TRUE means display the item
** 	FALSE means do not display the item
**
** Requires:
**	cx->prInfo to point to a record which contains the following
**	information:
**
**		page_topy:	DOC Y coord of top of this page.
**		page_break:	During layout, initialize this to
**				page_topy+page_height;
**		page_height:	The physical height of a piece of paper,
**				minus top and bottom margins.
**		phase:		XL_<mumble>_PHASE
**
**	If invoked with XP_LayoutForPrint and XP_DrawForPrint, this
**	will be done automatically.
*/

PUBLIC XP_Bool
XP_CheckElementSpan(MWContext*cx, int top, int height)
{
  int bottom;

  /*
  ** During load phase, or if this doesn't appear to be a print
  ** related call, we do nothing.
  */
  
  if (cx->prInfo == NULL || cx->prInfo->phase == 0)
  	return TRUE;
  	
  if (cx->prInfo->phase == XL_LOADING_PHASE)
    return FALSE;

  bottom = top + height - 1;

  /*
  ** Layout phase is used purely to compute the page breaks for each page.
  ** we always return FALSE, but look carefully at items which span the
  ** page boundry and pick for the actual page break the y minimum of
  ** "acceptable" items which span the physical page boundry.
  */
  if (cx->prInfo->phase == XL_LAYOUT_PHASE)
  {
    if (!too_big(cx, height)
	&& top >= cx->prInfo->page_topy
	&& top < cx->prInfo->page_topy + cx->prInfo->page_height
	&& bottom >= cx->prInfo->page_topy + cx->prInfo->page_height)
    {
      /*
      ** This item won't fit on this page, and is small enough to consider
      ** breaking a page for.
      */
      if (top < cx->prInfo->page_break) {
	cx->prInfo->page_break = top;
      }
    }
    return FALSE;
  }
  
  /*
  ** Which leaves only the actual generation phase.
  */

  /*
  ** Trivial reject of items which aren't on the page at all
  */
  if (bottom < cx->prInfo->page_topy || top > cx->prInfo->page_break) {
    return FALSE;
  }

  /*
  ** Complete acceptance of items which fall entirely on the current page
  */
  if (top >= cx->prInfo->page_topy && bottom <= cx->prInfo->page_break) {
    return TRUE;
  }

  /*
  ** Also print items which overhang the soft boundry, but not the hard
  ** page boundry.
  */
  if (bottom < cx->prInfo->page_topy + cx->prInfo->page_height)
  {
    return TRUE;
  }

  /*
  ** It straddles the page break.  If it's big, then print it, and let it
  ** get chopped off.  If it is small, it will display on the next page.
  */
  return too_big(cx, height);
}

/*
** Call to set up the page counting fields in the printinfo structure
*/
PUBLIC void
XP_InitializePrintInfo(MWContext *cx)
{
  if (cx->prInfo == NULL) {
    PrintInfo *p;

    p = XP_NEW_ZAP(PrintInfo);
    if (!p) return;
    cx->prInfo = p;
  }
  cx->prInfo->n_pages = 0;
  cx->prInfo->phase = XL_DRAW_PHASE;
}

PUBLIC void
XP_CleanupPrintInfo(MWContext *cx)
{
  PrintInfo *p = cx->prInfo;

  if (p) {
    if (p->pages) {
      free(p->pages);
      p->pages = NULL;
    }
    free(p);
    cx->prInfo = NULL;
  }
}

PUBLIC void
XP_LayoutForPrint(MWContext *cx, int32 doc_height)
{
  int32 y;
  int saveit;

  saveit = cx->prInfo->phase;
  cx->prInfo->phase = XL_LAYOUT_PHASE;
  y = 0;
  while (y < doc_height) {
    cx->prInfo->page_topy = y;
    cx->prInfo->page_break = y + cx->prInfo->page_height;
    LO_RefreshArea(cx, 0, y,
		       cx->prInfo->page_width, cx->prInfo->page_height);
    if (cx->prInfo->n_pages >= cx->prInfo->pt_size) {
      cx->prInfo->pt_size += 100;
      cx->prInfo->pages = (PageBreaks*)realloc(
				  cx->prInfo->pages,
				  cx->prInfo->pt_size * sizeof (PageBreaks));
		if ( !cx->prInfo->pages )
		{
			extern int MK_OUT_OF_MEMORY;
			cx->prInfo->n_pages = MK_OUT_OF_MEMORY;
			return;
		}
    }
    cx->prInfo->pages[cx->prInfo->n_pages].y_top = y;
    cx->prInfo->pages[cx->prInfo->n_pages].y_break = cx->prInfo->page_break-1;
    cx->prInfo->n_pages++;
    y = cx->prInfo->page_break;
  }
  cx->prInfo->phase = saveit;
}

PUBLIC void
XP_DrawForPrint(MWContext *cx, int page)
{
  int32 t;
  int saveit;

  saveit = cx->prInfo->phase;
  cx->prInfo->phase = XL_DRAW_PHASE;

  t = cx->prInfo->pages[page].y_top;
  cx->prInfo->page_topy = t;
  cx->prInfo->page_break = cx->prInfo->pages[page].y_break;
  LO_RefreshArea(cx,
      0, t,
      cx->prInfo->page_width, cx->prInfo->page_height);
  cx->prInfo->phase = saveit;
}
