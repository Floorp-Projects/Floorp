/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   outline.c --- the outline widget hack.
   Created: Terry Weissman <terry@netscape.com>, 24-Jun-95.
 */


#include "mozilla.h"
#include "xfe.h"
#include <msgcom.h>

#include "outline.h"
#include "dragdrop.h"

#include <Xfe/Xfe.h>			/* for xfe widgets and utilities */

#ifndef _STDC_
#define _STDC_ 1
#define HACKED_STDC 1
#endif

#define XmLBACKWARDS_COMPATIBILITY

#include <XmL/GridP.h>
#include <Xm/Label.h>		/* For the drag window pixmap label */

#ifdef HACKED_STDC
#undef HACKED_STDC
#undef _STDC_
#endif

#include "icons.h"
#include "icondata.h"

#ifdef FREEIF
#undef FREEIF
#endif
#define FREEIF(obj) do { if (obj) { XP_FREE (obj); obj = 0; }} while (0)

#define PIXMAP_WIDTH 18		/* #### */
#define PIXMAP_INDENT 10

#define LINE_AREA_WIDTH 7

#define FLIPPY_WIDTH 14		/* #### */

#define MIN_COLUMN_WIDTH 10	/* So that the user can manipulate it. */
#define MAX_COLUMN_WIDTH 10000	/* So that we don't overflow any 16-bit
				   numbers. */

static Widget DataWidget = NULL;

typedef struct fe_OutlineInfo {
  Widget widget;
  Widget drag_widget;
  int numcolumns;
  int numrows;
  fe_OutlineGetDataFunc datafunc;
  fe_OutlineClickFunc clickfunc;
  fe_OutlineIconClickFunc iconfunc;
  MSG_Pane *pane;
  void* closure;

  int lastx;
  int lasty;
  Time lastdowntime;
  Time lastuptime;
  XP_Bool ignoreevents;
  EventMask activity;

  /* indented column specific stuff. */
  int *column_indented;	/* the columns that are to be indented. */
  int indent_amount;		/* the amount to indent each subline. */

  /* drag specific stuff. */
  XP_Bool drag_enabled;
  fe_icon *drag_icon;
  void* dragscrolltimer;
  int dragscrolldir;
  int dragrow;
  XP_Bool dragrowbox;
  GC draggc;
  fe_dnd_Type drag_type;

  XP_Bool allowiconresize;
  int iconcolumnwidth;
  int* columnwidths;
  int* columnIndex;		/* Which column to display where.  If
				   columnIndex[1] == 3, then display the third
				   column's data in column 1.  Column zero is
				   the icon column.*/
  char** posinfo;		/* Pointer to a string where we keep the
				   current column ordering and widths, in the
				   form to be saved in the preferences file.*/
  int dragcolumn;
  Pixmap dragColumnPixmap;
  unsigned char clickrowtype;
  const char** headers;		/* What headers to display on the columns.
				   Entry zero is for the icon column. */
  XP_Bool* headerhighlight;

  void* visibleTimer;
  int visibleLine;

  fe_OutlineDesc Data;
  int DataRow;
} fe_OutlineInfo;


static fe_icon pixmapFlippyFolded = { 0 };
static fe_icon pixmapFlippyExpanded = { 0 };

#if 0
static fe_icon pixmapMarked = { 0 };
static fe_icon pixmapUnmarked = { 0 };
#endif

static fe_dnd_Source dragsource = {0};

/* static variables to make the conversion from our style
   to fontlist tags easier (and less of a memory problem.) */

static char *ITALIC_STYLE = "ITALIC";
static char *BOLD_STYLE = "BOLD";
static char *DEFAULT_STYLE = XmFONTLIST_DEFAULT_TAG; /* Needs to be this for i18n to work. */

static char *
fe_outline_style_to_tag(fe_OutlineTextStyle style)
{
  switch (style) 
    {
    case FE_OUTLINE_Italic:
      return ITALIC_STYLE;
    case FE_OUTLINE_Bold:
      return BOLD_STYLE;
    case FE_OUTLINE_Default:
      return DEFAULT_STYLE;
    default:
      XP_ASSERT(0);
      return DEFAULT_STYLE;
    }
}

static void
fe_make_outline_drag_widget (fe_OutlineInfo *info,
			     fe_OutlineDesc *data, int x, int y)
{
#if 0
  Display *dpy = XtDisplay (info->widget);
  XmString str;
  int str_w, str_h;
#endif
  Visual *v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;
  Widget label;
  XmFontList fontList;
  /*fe_icon *icon;*/
  Widget shell;
  Pixmap dragPixmap;

  if (dragsource.widget) return;

  shell = info->widget;
  while (XtParent(shell) && !XtIsShell(shell)) {
    shell = XtParent(shell);
  }

  XtVaGetValues (shell, XtNvisual, &v, XtNcolormap, &cmap,
		 XtNdepth, &depth, 0);

  XtVaGetValues (info->widget, XmNfontList, &fontList, NULL);

  /* ### free the string
     str = XmStringCreateLocalized(data->str[5]);
     str_w = XmStringWidth (fontList, str);
     str_h = XmStringHeight (fontList, str);
     */

  dragsource.type = info->clickrowtype == XmCONTENT ? info->drag_type : FE_DND_COLUMN;
  dragsource.closure = info->widget;

  if (info->dragColumnPixmap)
    {
      XFreePixmap(XtDisplay(info->widget), info->dragColumnPixmap);
      info->dragColumnPixmap = 0;
    }

  if (dragsource.type == FE_DND_COLUMN) {
    XRectangle cellrect;
    XGCValues gcv;

    XmLGridRowColumnToXY(info->widget, 
			 XmHEADING, 0, 
			 XmCONTENT, info->dragcolumn, 
			 True, &cellrect);

    /* if the column extends past the right hand side of the widget, clip
       it. */
    {
      Dimension clipwidth;
      Widget vertscrollbar;

      XtVaGetValues(info->widget,
		    XmNverticalScrollBar, &vertscrollbar,
		    NULL);

      clipwidth = XfeWidth(info->widget);

      if (vertscrollbar && XtIsManaged(vertscrollbar))
	clipwidth -= XfeWidth(vertscrollbar);

      if (cellrect.x + cellrect.width > (int)clipwidth)
	cellrect.width = clipwidth - cellrect.x;
    }

#ifndef DONT_DRAG_COLUMNS
    cellrect.height = XfeHeight(info->widget) - cellrect.y;
#endif

    if (!info->draggc)
      info->draggc = XCreateGC(XtDisplay(info->widget),
			       XtWindow(info->widget),
			       0, &gcv);

    dragPixmap = info->dragColumnPixmap = XCreatePixmap(XtDisplay(info->widget),
							XtWindow(info->widget),
							cellrect.width,
							cellrect.height,
							depth);

    XCopyArea(XtDisplay(info->widget), XtWindow(info->widget),
	      info->dragColumnPixmap, info->draggc,
	      cellrect.x, cellrect.y, cellrect.width, cellrect.height,
	      0,0);

    dragsource.widget = XtVaCreateWidget ("drag_win",
					  overrideShellWidgetClass,
					  info->widget,
					  XmNwidth, cellrect.width,
					  XmNheight, cellrect.height,
					  XmNvisual, v,
					  XmNcolormap, cmap,
					  XmNdepth, depth,
					  XmNborderWidth, 0,
					  NULL);
  } else {

    XP_ASSERT(info->drag_icon != NULL);
    if (info->drag_icon == NULL) return;

    dragPixmap = info->drag_icon->pixmap;

    dragsource.widget = XtVaCreateWidget ("drag_win",
					  overrideShellWidgetClass,
					  info->widget,
					  XmNwidth, info->drag_icon->width,
					  XmNheight, info->drag_icon->height,
					  XmNvisual, v,
					  XmNcolormap, cmap,
					  XmNdepth, depth,
					  XmNborderWidth, 0,
					  NULL);
  }


  label = XtVaCreateManagedWidget ("label",
				   xmLabelWidgetClass,
				   dragsource.widget,
				   XmNlabelType, XmPIXMAP,
				   XmNlabelPixmap, dragPixmap,
				   NULL);

  /* XmStringFree(str); */
}


static void
fe_destroy_outline_drag_widget (void)
{
  if (!dragsource.widget) return;
  XtDestroyWidget (dragsource.widget);
  dragsource.widget = NULL;
}

static fe_OutlineInfo*
fe_get_info(Widget outline)
{
  fe_OutlineInfo* result = NULL;
  XtVaGetValues(outline, XmNuserData, &result, 0);
  assert(result->widget == outline);
  return result;
}


static void
PixmapDraw(Widget w, Pixmap pixmap, Pixmap mask,
	   int pixmapWidth, int pixmapHeight, unsigned char alignment, GC gc,
	   XRectangle *rect, XRectangle *clipRect)
{
  Display *dpy;
  Window win;
  int needsClip;
  int x, y, width, height;

  if (pixmap == XmUNSPECIFIED_PIXMAP) return;
  if (rect->width <= 4 || rect->height <= 4) return;
  if (clipRect->width < 3 || clipRect->height < 3) return;
  if (!XtIsRealized(w)) return;
  dpy = XtDisplay(w);
  win = XtWindow(w);

  width = pixmapWidth;
  height = pixmapHeight;
  if (!width || !height) {
    alignment = XmALIGNMENT_TOP_LEFT;
    width = clipRect->width - 4;
    height = clipRect->height - 4;
  }

  if (alignment == XmALIGNMENT_TOP_LEFT ||
      alignment == XmALIGNMENT_LEFT ||
      alignment == XmALIGNMENT_BOTTOM_LEFT) {
    x = rect->x + 2;
  } else if (alignment == XmALIGNMENT_TOP ||
	     alignment == XmALIGNMENT_CENTER ||
	     alignment == XmALIGNMENT_BOTTOM) {
    x = rect->x + ((int)rect->width - width) / 2;
  } else {
    x = rect->x + rect->width - width - 2;
  }

  if (alignment == XmALIGNMENT_TOP ||
      alignment == XmALIGNMENT_TOP_LEFT ||
      alignment == XmALIGNMENT_TOP_RIGHT) {
    y = rect->y + 2;
  } else if (alignment == XmALIGNMENT_LEFT ||
	     alignment == XmALIGNMENT_CENTER ||
	     alignment == XmALIGNMENT_RIGHT) {
    y = rect->y + ((int)rect->height - height) / 2;
  } else {
    y = rect->y + rect->height - height - 2;
  }

  needsClip = 1;
  if (clipRect->x <= x &&
      clipRect->y <= y &&
      clipRect->x + clipRect->width >= x + width &&
      clipRect->y + clipRect->height >= y + height) {
    needsClip = 0;
  }

  if (needsClip) {
    XSetClipRectangles(dpy, gc, 0, 0, clipRect, 1, Unsorted);
  } else if (mask) {
    XSetClipMask(dpy, gc, mask);
    XSetClipOrigin(dpy, gc, x, y);
  }
  XSetGraphicsExposures(dpy, gc, False);
  XCopyArea(dpy, pixmap, win, gc, 0, 0, width, height, x, y);
  XSetGraphicsExposures(dpy, gc, True);
  if (needsClip || mask) {
    XSetClipMask(dpy, gc, None);
  }
}




extern XmString fe_StringChopCreate(char* message, char* tag,
				    XmFontList font_list, int maxwidth);

static void
fe_outline_celldraw(Widget widget, XtPointer clientData, XtPointer callData)
{
  fe_OutlineInfo* info = (fe_OutlineInfo*) clientData;
  XmLGridCallbackStruct* call = (XmLGridCallbackStruct*) callData;
  XmLGridDrawStruct *draw = call->drawInfo;
  Boolean drawSelected = draw->drawSelected;
  XmLCGridRow *row;
  XmLCGridColumn *column;
  unsigned char alignment;
  Pixel fg;
  XmFontList fontList;
  XmString str = NULL;
  const char* ptr;
  fe_icon* data = NULL;
  XRectangle r, textRect;
  int sourcecol;

  if (call->rowType != XmCONTENT) return;
  
  row = XmLGridGetRow(widget, call->rowType, call->row);
  column = XmLGridGetColumn(widget, call->columnType, call->column);

  /* fill in useful things into the data sent to the datafunc. */
  info->Data.column_headers = (const char **)info->headers;
  info->Data.numcolumns = info->numcolumns;
  info->Data.selected = (XP_Bool)drawSelected;

  if (DataWidget != widget || call->row != info->DataRow) {
    if (!(*info->datafunc)(widget, info->closure, call->row, &info->Data, 
			   0)) {
      info->DataRow = -1;
      return;
    }
    DataWidget = widget;
    info->DataRow = call->row;
  }

  sourcecol = info->columnIndex[call->column];

  if (sourcecol == 0) 
    {
      /* Draw the flippy, if any. */
      if (info->Data.flippy != FE_OUTLINE_Leaf) 
        {
          switch (info->Data.flippy) {
          case FE_OUTLINE_Folded:	data = &pixmapFlippyFolded; break;
          case FE_OUTLINE_Expanded:	data = &pixmapFlippyExpanded; break;
          default:
	    XP_ASSERT(0);
          }
          r = *draw->cellRect;
          r.width = data->width;
          PixmapDraw(widget, data->pixmap, data->mask, data->width, data->height,
		     XmALIGNMENT_LEFT, draw->gc, &r, call->clipRect);
        }
  
#if 0
      /* Draw the indented icon */
      r = *draw->cellRect;
      data = fe_outline_lookup_icon(Data.icon);
      XP_ASSERT(data);
      if (!data) return;
    
      r.x += Data.depth * PIXMAP_INDENT + FLIPPY_WIDTH;
      r.width = data->width;
      if (r.x + r.width > draw->cellRect->x + draw->cellRect->width) {
	char buf[10];
	XRectangle rect;
	rect = *draw->cellRect;
	rect.width -= r.width;
	PR_snprintf(buf, sizeof (buf), "%d", Data.depth);
        XtVaGetValues(widget,
		      XmNrowPtr, row,
		      XmNcolumnPtr, column,
		      XmNcellForeground, &fg,
		      XmNcellFontList, &fontList,
		      NULL);
        str = XmStringCreate(buf, DEFAULT_STYLE);
        XSetForeground(XtDisplay(widget), draw->gc,
		       drawSelected ? draw->selectForeground : fg);
        XmLStringDraw(widget, str, draw->stringDirection, fontList,
		      XmALIGNMENT_RIGHT, draw->gc, &rect, &rect);
        r.x = draw->cellRect->x + draw->cellRect->width - r.width;
      }
      PixmapDraw(widget, data->pixmap, data->mask, data->width, data->height,
		 XmALIGNMENT_LEFT, draw->gc, &r, call->clipRect);
#endif

    } 
  else /* sourcecol != 0 */
    {
      ptr = info->Data.str[sourcecol - 1];

      /* if we're the indented column, we push everything over by the indented
	 amount -- the depth * the indent_amount */
      if (info->column_indented[call->column] & FE_OUTLINE_IndentedColumn)
	{
	  Dimension indent_amount = info->Data.depth * info->indent_amount;

	  draw->cellRect->x += indent_amount;
	  draw->cellRect->width -= indent_amount;
	}

      /* if there's an icon, we need to push the text over. */
      if (info->Data.type[sourcecol - 1] & FE_OUTLINE_Icon)
	{
          PixmapDraw(widget, info->Data.icons[sourcecol - 1]->pixmap, 
		     info->Data.icons[sourcecol - 1]->mask, 
		     info->Data.icons[sourcecol - 1]->width, 
		     info->Data.icons[sourcecol - 1]->height,
		     XmALIGNMENT_LEFT, draw->gc, draw->cellRect,
		     call->clipRect);

	  textRect.x = draw->cellRect->x + info->Data.icons[sourcecol - 1]->width + 4;
	  textRect.y = draw->cellRect->y;
	  textRect.width = draw->cellRect->width - info->Data.icons[sourcecol - 1]->height - 4;
	  textRect.height = draw->cellRect->height;
	}
      else
	{
	  textRect = *draw->cellRect;
	}

      if (info->Data.type[sourcecol - 1] & FE_OUTLINE_String)
        {
          XtVaGetValues(widget,
		        XmNrowPtr, row,
		        XmNcolumnPtr, column,
		        XmNcellForeground, &fg,
		        XmNcellAlignment, &alignment,
		        XmNcellFontList, &fontList,
		        NULL);
          if (call->clipRect->width > 4) 
            {
	      /* Impose some spacing between columns.  What a hack. ### */
	      call->clipRect->width -= 4;

	      if (info->Data.type[sourcecol - 1] == FE_OUTLINE_ChoppedString) 
                {
	          str = fe_StringChopCreate((char*) ptr, fe_outline_style_to_tag(info->Data.style),
					    fontList,
				            call->clipRect->width);
	        } 
              else 
                {
	          str = XmStringCreate((char *) ptr, fe_outline_style_to_tag(info->Data.style));
	        }
	      
	      XSetForeground(XtDisplay(widget), draw->gc,
		             drawSelected ? draw->selectForeground : fg);
	      XmLStringDraw(widget, str, draw->stringDirection, fontList, alignment,
		            draw->gc, &textRect, call->clipRect);
	      call->clipRect->width += 4;
            }
        } 
    }
  if (call->row == info->dragrow && sourcecol > 0) 
    {
      int y;
      int x2 = draw->cellRect->x + draw->cellRect->width - 1;
      XP_Bool rightedge = FALSE;
      if (call->column == info->numcolumns) 
        {
          rightedge = TRUE;
          if (str) 
            {
	      int xx = draw->cellRect->x + XmStringWidth(fontList, str) + 4;
	      if (x2 > xx) x2 = xx;
            }
        }
      if (info->draggc == NULL) 
        {
          XGCValues gcv;
#if 0
          Pixel foreground;
#endif
          gcv.foreground = fg;
          info->draggc = XCreateGC(XtDisplay(widget), XtWindow(widget),
			           GCForeground, &gcv);
        }
      y = draw->cellRect->y + draw->cellRect->height - 1;
      XDrawLine(XtDisplay(widget), XtWindow(widget), info->draggc,
	        draw->cellRect->x, y, x2, y);
      if (info->dragrowbox) 
        {
          int y0 = draw->cellRect->y;
          if (call->column == 1) 
            {
	      XDrawLine(XtDisplay(widget), XtWindow(widget), info->draggc,
		        draw->cellRect->x, y0, draw->cellRect->x, y);
            }
          if (rightedge) 
            {
	      XDrawLine(XtDisplay(widget), XtWindow(widget), info->draggc,
		        x2, y0, x2, y);
            }	
          XDrawLine(XtDisplay(widget), XtWindow(widget), info->draggc,
		    draw->cellRect->x, y0, x2, y0);
    
        }
    }
  if (str) XmStringFree(str);
}


static void
fe_outline_click(Widget widget, XtPointer clientData, XtPointer callData)
{
  fe_OutlineInfo* info = (fe_OutlineInfo*) clientData;
  XmLGridCallbackStruct* call = (XmLGridCallbackStruct*) callData;
  XEvent* event;
  int row;
  unsigned int state;
  int sourcecol = info->columnIndex[call->column];

  event = call->event;
  if (!event)
    state = 0;
  else if (event->type == ButtonPress || event->type == ButtonRelease)
    state = event->xbutton.state;
  else if (event->type == KeyPress || event->type == KeyRelease)
    state = event->xkey.state;
  else
    state = 0;

  if (call->rowType == XmHEADING) 
    row = -1;
  else
    row = call->row;
  (*info->clickfunc)(widget, info->closure, row,
		     sourcecol - 1,
		     info->headers ? info->headers[sourcecol] : NULL,
		     1, call->reason == XmCR_ACTIVATE ? 2 : 1,
		     (state & ShiftMask) != 0, (state & ControlMask) != 0,
		     0);
}


static int last_motion_x = 0;
static int last_motion_y = 0;

static void
UpdateData (Widget widget, fe_OutlineInfo *info, int row)
{
  if (widget != DataWidget || row != info->DataRow) {
    if (!(*info->datafunc)(widget, info->closure, row, &info->Data, 0)) {
      return;
    }
    DataWidget = widget;
    info->DataRow = row;
  }
}

static XP_Bool
RowIsSelected(fe_OutlineInfo* info, int row)
{
  int* position;
  int n = XmLGridGetSelectedRowCount(info->widget);
  XP_Bool result = FALSE;
  int i;
  if (n > 0) {
    position = XP_ALLOC(sizeof(int) * n);
    if (position) {
      XmLGridGetSelectedRows(info->widget, position, n);
      for (i=0 ; i<n ; i++) {
	if (position[i] == row) {
	  result = TRUE;
	  break;
	}
      }
      XP_FREE(position);
    }
  }
  return result;
}


static void
fe_outline_visible_timer(void* closure)
{
  fe_OutlineInfo* info = (fe_OutlineInfo*) closure;
  info->visibleTimer = NULL;
  if (info->visibleLine >= 0) {
    /* First and check that the user isn't still busy with his mouse.  If
       he is, then we'll do this stuff when activity goes to 0. */
    if (info->activity == 0) {
      fe_OutlineMakeVisible(info->widget, info->visibleLine);
      info->visibleLine = -1;
    }
  }
}


static void
SendClick(fe_OutlineInfo* info, XEvent* event, XP_Bool only_if_not_selected)
{
  int x = event->xbutton.x;
  int y = event->xbutton.y;
  int numclicks = 1;
  unsigned char rowtype;
  unsigned char coltype;
  int row;
  int column;
  unsigned int state = 0;

  if (!only_if_not_selected &&
      abs(x - info->lastx) < 5 && abs(y - info->lasty) < 5 &&
      (info->lastdowntime - info->lastuptime <=
       XtGetMultiClickTime(XtDisplay(info->widget)))) {
    numclicks = 2;		/* ### Allow triple clicks? */
  }
  info->lastx = x;
  info->lasty = y;

  if (XmLGridXYToRowColumn(info->widget, x, y,
			   &rowtype, &row, &coltype, &column) >= 0) {
    if (rowtype == XmHEADING) row = -1;
    if (coltype == XmCONTENT) {
      info->activity = ButtonPressMask;
      info->ignoreevents = True;
      if (column < 1 && !only_if_not_selected && row >= 0) {
	UpdateData(info->widget, info, row);
	if (1 /*### Pixel compare the click with where we drew icon*/) {
	  if (numclicks == 1) {
	    (*info->iconfunc)(info->widget, info->closure, row, 0);
	  }
	}
	return;
      }
      state = event->xbutton.state;
      if (!only_if_not_selected || !RowIsSelected(info, row)) {
	int sourcecol = info->columnIndex[column];
	if (numclicks == 1) {
	  /* The user just clicked.  If he's in the middle of a double-click,
	     then we don't want calls to fe_OutlineMakeVisible to cause things
	     to scroll before the double-click finishes.  So, we set a
	     timer to go off; if fe_OutlineMakeVisible gets called before the
	     timer expires, we put off the call until the timer goes off. */
	  if (info->visibleTimer) {
	    FE_ClearTimeout(info->visibleTimer);
	  }
	  info->visibleTimer =
	    FE_SetTimeout(fe_outline_visible_timer, info,
			  XtGetMultiClickTime(XtDisplay(info->widget)) + 10);
	}	
	if (row != -1)
	  fe_OutlineSelect(info->widget, row, True);

	(*info->clickfunc)(info->widget, info->closure, row, sourcecol - 1,
			   info->headers ? info->headers[sourcecol] : NULL,
			   event->xbutton.button, numclicks,
			   (state & ShiftMask) != 0,
			   (state & ControlMask) != 0, 0);

      }
    }
  }
}



static void
ButtonEvent(Widget widget, XtPointer closure, XEvent* event, Boolean* c)
{
  fe_OutlineInfo* info = (fe_OutlineInfo*) closure;
  int x = event->xbutton.x;
  int y = event->xbutton.y;
  unsigned char rowtype;
  unsigned char coltype;
  int row;
  int column;

  info->ignoreevents = False;

  switch (event->type) {
  case ButtonPress:
    /* Always ignore btn3. Btn3 is for popups. - dp */
    if (event->xbutton.button == 3) break;

    if (XmLGridXYToRowColumn(info->widget, x, y,
			     &rowtype, &row, &coltype, &column) >= 0) {
      if (rowtype == XmHEADING) {
	if (XmLGridPosIsResize(info->widget, x, y)) {
	  return;
	}
      }
      info->clickrowtype = rowtype;
      info->dragcolumn = column;
      info->activity |= ButtonPressMask;
      info->ignoreevents = True;
    }
    last_motion_x = x;
    last_motion_y = y;
    info->lastdowntime = event->xbutton.time;
    break;

  case ButtonRelease:

    /* fe_SetCursor (info->context, False); */

    if (info->activity & ButtonPressMask) {
      if (info->activity & PointerMotionMask) {
	/* handle the drop */

	fe_dnd_DoDrag(&dragsource, event, FE_DND_DROP);
	fe_dnd_DoDrag(&dragsource, event, FE_DND_END);

	fe_destroy_outline_drag_widget();

      } else {
	SendClick(info, event, FALSE);
      }
    }
    info->lastuptime = event->xbutton.time;
    info->activity = 0;
    if (info->visibleLine >= 0 && info->visibleTimer == NULL) {
      fe_OutlineMakeVisible(info->widget, info->visibleLine);
      info->visibleLine = -1;
    }

    break;

  case MotionNotify:
    if (!info->drag_enabled)
      break;

    if (!(info->activity & PointerMotionMask) &&
	(abs(x - last_motion_x) < 5 && abs(y - last_motion_y) < 5)) {
      /* We aren't yet dragging, and the mouse hasn't moved enough for
	 this to be considered a drag. */
      break;
    }


    if (info->activity & ButtonPressMask) {
      /* ok, the pointer moved while a button was held.
       * we're gonna drag some stuff.
       */
      info->ignoreevents = True;
      if (!(info->activity & PointerMotionMask)) {
	if (info->drag_type == FE_DND_NONE &&
	    info->clickrowtype == XmCONTENT) {
	  /* We don't do drag'n'drop in this context.  Do any any visibility
	     scrolling that we may have been putting off til the end of user
	     activity. */
	  info->activity = 0;
	  if (info->visibleLine >= 0 && info->visibleTimer == NULL) {
	    fe_OutlineMakeVisible(info->widget, info->visibleLine);
	    info->visibleLine = -1;
	  }
	  break;
	}

	/* First time.  If the item we're pointing at isn't
	   selected, deliver a click message for it (which ought to
	   select it.) */
	  
	if (info->clickrowtype == XmCONTENT) {
	  /* Hack things so we click where the mouse down was, not where
	     the first notify event was.  Bleah. */
	  event->xbutton.x = last_motion_x;
	  event->xbutton.y = last_motion_y;
	  SendClick(info, event, TRUE);
	  event->xbutton.x = x;
	  event->xbutton.y = y;
	}

	/* Create a drag source. */
	fe_make_outline_drag_widget (info, &info->Data, x, y);
	fe_dnd_DoDrag(&dragsource, event, FE_DND_START);
	info->activity |= PointerMotionMask;
      }
	
      fe_dnd_DoDrag(&dragsource, event, FE_DND_DRAG);

      /* Now, force all the additional mouse motion events that are
	 lingering around on the server to come to us, so that Xt can
	 compress them away.  Yes, XSync really does improve performance
	 in this case, not hurt it. */
      XSync(XtDisplay(info->widget), False);

    }
    last_motion_x = x;
    last_motion_y = y;
    break;
  }
  if (info->ignoreevents) *c = False;    
}

static void
fe_outline_destroy_cb(Widget widget, XtPointer clientData, XtPointer callData)
{
  fe_OutlineInfo *info = (fe_OutlineInfo*)clientData;

  /* first we must have something to free... */
  XP_ASSERT(info);

  /* we must also be sure we're freeing the right stuff. */
  XP_ASSERT(info->widget == widget);

  FREEIF(info->Data.type);
  FREEIF(info->Data.icons);
  FREEIF(info->Data.str);

  FREEIF(info->columnIndex);
  FREEIF(info->column_indented);
  FREEIF(info->headerhighlight);

  XP_FREE(info);
}

static void
fe_set_default_column_widths(Widget widget) {
  fe_OutlineInfo* info = fe_get_info(widget);
  int i;
  short avgwidth, avgheight;
  XmFontList fontList;
  XtVaGetValues(widget, XmNfontList, &fontList, NULL);
  XmLFontListGetDimensions(fontList, &avgwidth, &avgheight, TRUE);

  for (i=0 ; i<info->numcolumns ; i++) {
    int width = info->columnwidths[i] * avgwidth;
    if (width < MIN_COLUMN_WIDTH) width = MIN_COLUMN_WIDTH;
    if (width > MAX_COLUMN_WIDTH) width = MAX_COLUMN_WIDTH;
    info->columnIndex[i] = i;

    if (i == 0) info->iconcolumnwidth = width;

    XtVaSetValues(widget,
		  XmNcolumn, i,
		  XmNcolumnSizePolicy, XmCONSTANT,
		  XmNcolumnWidth, width,
		  NULL);
  }
}

Widget
fe_GridCreate(MWContext* context, Widget parent, String name,
	      ArgList av, Cardinal ac,
	      int maxindentdepth,
	      int numcolumns, int* columnwidths,
	      fe_OutlineGetDataFunc datafunc,
	      fe_OutlineClickFunc clickfunc,
	      fe_OutlineIconClickFunc iconfunc,
	      void* closure,
	      char** posinfo, int tag, XP_Bool isOutline)
{
  Widget result;
  fe_OutlineInfo* info;

  XP_ASSERT(numcolumns >= 0);

  info = XP_NEW_ZAP(fe_OutlineInfo);
  if (info == NULL) return NULL; /* ### */

  XtSetArg(av[ac], XmNuserData, info); ac++;

  info->numcolumns = numcolumns;

  info->datafunc = datafunc;
  info->clickfunc = clickfunc;
  info->iconfunc = iconfunc;
  info->closure = closure;
  info->dragrow = -1;
  info->posinfo = posinfo;
  info->visibleLine = -1;
  info->DataRow = -1;

  info->Data.type = XP_CALLOC(numcolumns, sizeof(fe_OutlineType));
  info->Data.icons = XP_CALLOC(numcolumns, sizeof(fe_icon*));
  info->Data.str = XP_CALLOC(numcolumns, sizeof(char*));

  info->lastx = -999;
  info->columnwidths = columnwidths;

  XtSetArg(av[ac], XmNselectionPolicy, XmSELECT_MULTIPLE_ROW); ac++;

  if (isOutline) {
    columnwidths[numcolumns - 1] = 9999; /* Make the last column always really
					    wide, so we always are ready to
					    show something no matter how wide
					    the window gets. */

    if ((context->type == MWContextMail 
	 && fe_globalPrefs.mail_pane_style == FE_PANES_HORIZONTAL)
	|| ((context->type == MWContextNews
	     && fe_globalPrefs.news_pane_style == FE_PANES_HORIZONTAL))) {
      XtSetArg(av[ac], XmNhorizontalSizePolicy, XmCONSTANT); ac++;
    }
    else {
      XtSetArg(av[ac], XmNhorizontalSizePolicy, XmVARIABLE); ac++;
    }
  } else {
    XtSetArg(av[ac], XmNhorizontalSizePolicy, XmCONSTANT); ac++;
  }

  result = XmLCreateGrid(parent, name, av, ac);
  info->widget = result;

  XtAddCallback(result, XmNdestroyCallback, fe_outline_destroy_cb, info);

  XtVaSetValues(result,
		XmNcellDefaults, True,
		XmNcellLeftBorderType, XmBORDER_NONE,
		XmNcellRightBorderType, XmBORDER_NONE,
		XmNcellTopBorderType, XmBORDER_NONE,
		XmNcellBottomBorderType, XmBORDER_NONE,
		XmNcellAlignment, XmALIGNMENT_LEFT,
		0);

  XmLGridAddColumns(result, XmCONTENT, -1, info->numcolumns);

  info->allowiconresize = (maxindentdepth > 0);

  info->columnIndex = XP_CALLOC(numcolumns, sizeof(int));

  fe_set_default_column_widths(result);

  XtInsertEventHandler(result, ButtonPressMask | ButtonReleaseMask
		       | PointerMotionMask, False,
		       ButtonEvent, info, XtListHead);

  XtAddCallback(result, XmNcellDrawCallback, fe_outline_celldraw, info);
  /*  XtAddCallback(result, XmNselectCallback, fe_outline_click, info);
      XtAddCallback(result, XmNactivateCallback, fe_outline_click, info);*/

  /* initialize the column indentation stuff. */
  info->column_indented = XP_CALLOC(numcolumns, sizeof(int *));
  info->indent_amount = 10;

  /* initialize the drag and drop stuff. */
  fe_OutlineDisableDrag(result);
  info->dragColumnPixmap = 0;

  if (!pixmapFlippyFolded.pixmap) {
    Pixel pixel;
    XtVaGetValues(result, XmNbackground, &pixel, NULL);

    fe_MakeIcon(context, pixel, &pixmapFlippyFolded, NULL,
		HFolder.width, HFolder.height,
		HFolder.mono_bits, HFolder.color_bits, HFolder.mask_bits,
		FALSE);
    fe_MakeIcon(context, pixel, &pixmapFlippyExpanded, NULL,
		HFolderO.width, HFolderO.height,
		HFolderO.mono_bits, HFolderO.color_bits, HFolderO.mask_bits,
		FALSE);
#if 0 
    /* no marks. */
    fe_MakeIcon(context, pixel, &pixmapMarked, NULL,
		HMarked.width, HMarked.height,
		HMarked.mono_bits, HMarked.color_bits, HMarked.mask_bits,
		FALSE);
    fe_MakeIcon(context, pixel, &pixmapUnmarked, NULL,
		HUMarked.width, HUMarked.height,
		HUMarked.mono_bits, HUMarked.color_bits, HUMarked.mask_bits,
		FALSE);
#endif
  }

  return result;
}



static void
fe_outline_remember_columns(Widget widget)
{
  fe_OutlineInfo* info = fe_get_info(widget);
  XmLCGridColumn* col;
  int i;
  Dimension width;
  char* ptr;
  int length = 0;
  FREEIF(*(info->posinfo));
  for (i=0 ; i < info->numcolumns ; i++) {
    length += strlen(info->headers[i]) + 14;
  }
  *(info->posinfo) = XP_ALLOC(length);
  ptr = *(info->posinfo);
  for (i=0 ; i<info->numcolumns ; i++) {
    col = XmLGridGetColumn(widget, XmCONTENT, i);
    XtVaGetValues(widget, XmNcolumnPtr, col, XmNcolumnWidth, &width, 0);
    PR_snprintf(ptr, *(info->posinfo) + length - ptr,
		"%s:%d;", info->headers[info->columnIndex[i]],
		(int) width);
    ptr += strlen(ptr);
  }
  if (ptr > *(info->posinfo)) ptr[-1] = '\0';
}


static void
fe_outline_resize(Widget widget, XtPointer clientData, XtPointer callData)
{
  /* The user has resized a column.  Unfortunately, if they resize it
     to width 0, they will never be able to grab it to resize it
     bigger.  So, we will implement a minimum width per column. */

  fe_OutlineInfo* info = (fe_OutlineInfo*) clientData;
  XmLGridCallbackStruct* call = (XmLGridCallbackStruct*) callData;
  XmLCGridColumn* col;
  Dimension width;
  if (call->reason != XmCR_RESIZE_COLUMN) return;
  if (!info->allowiconresize && call->column == 0) {
    XtVaSetValues(widget, XmNcolumn, call->column, XmNcolumnWidth,
		  info->iconcolumnwidth, 0);
  } else {
    col = XmLGridGetColumn(widget, call->columnType, call->column);
    XtVaGetValues(widget, XmNcolumnPtr, col, XmNcolumnWidth, &width, 0);
    if (width < MIN_COLUMN_WIDTH) {
      XtVaSetValues(widget, XmNcolumn, call->column,
		    XmNcolumnWidth, MIN_COLUMN_WIDTH, 0);
    }
  }
  fe_outline_remember_columns(widget);
}

void
fe_OutlineSetMaxDepth(Widget outline, int maxdepth)
{
  fe_OutlineInfo* info = fe_get_info(outline);
  int value = (maxdepth - 1) * PIXMAP_INDENT + PIXMAP_WIDTH + FLIPPY_WIDTH;
  XP_ASSERT(!info->allowiconresize);
  XP_ASSERT(maxdepth > 0);
  if (value != info->iconcolumnwidth) {
    info->iconcolumnwidth = value;
    XtVaSetValues(outline, XmNcolumn, 0, XmNcolumnWidth, value, 0);
  }
}


static XmString
fe_outline_get_header(char *widget, char *header, XP_Bool highlight)
{
  char		clas[512];
  XrmDatabase	db;
  char		name[512];
  char		*type;
  XrmValue	value;
  XmString	xms;

  db = XtDatabase(fe_display);
  (void) PR_snprintf(clas, sizeof(clas), "%s.MailNewsColumns.Pane.Column",
		     fe_progclass);
  (void) PR_snprintf(name, sizeof(name), "%s.mailNewsColumns.%s.%s",
		     fe_progclass, widget, header);
  if (XrmGetResource(db, name, clas, &type, &value))
    {
      xms = XmStringCreate((char *) value.addr,
			   highlight ? BOLD_STYLE : DEFAULT_STYLE);
    }
  else
    {
      xms = XmStringCreate(header,
			   highlight ? BOLD_STYLE : DEFAULT_STYLE);
    }

  return xms;
}


void 
fe_OutlineSetHeaders(Widget widget, fe_OutlineHeaderDesc *desc)
{				/* Fix i18n in here ### */
  fe_OutlineInfo* info = fe_get_info(widget);
  int i;
  XmString str;
  char* ptr;
  char* ptr2;
  char* ptr3;
  int value;
  int width;

  ptr = *info->posinfo;
  for (i=0 ; i<info->numcolumns ; i++) {
    if (ptr == NULL) {
      FREEIF(*info->posinfo);
      break;
    }
    ptr2 = XP_STRCHR(ptr, ';');
    if (ptr2) *ptr2 = '\0';
    ptr3 = XP_STRCHR(ptr, ':');
    if (!ptr3) {
      FREEIF(*info->posinfo);
      break;
    }
    *ptr3 = '\0';
    for (value = 0 ; value < info->numcolumns ; value++) {
      if (strcmp(desc->header_strings[value], ptr) == 0) break;
    }
    if (value > info->numcolumns) {
      FREEIF(*info->posinfo);
      break;
    }
    info->columnIndex[i] = value;
    width = atoi(ptr3 + 1);
    *ptr3 = ':';
    if (i == info->numcolumns) width = MAX_COLUMN_WIDTH; /* Last column is
							    always huge.*/
    if (width < MIN_COLUMN_WIDTH) width = MIN_COLUMN_WIDTH;
    if (width > MAX_COLUMN_WIDTH) width = MAX_COLUMN_WIDTH;
    XtVaSetValues(widget,
		  XmNcolumn, i,
		  XmNcolumnSizePolicy, XmCONSTANT,
		  XmNcolumnWidth, width,
		  0);
    if (ptr2) *ptr2++ = ';';
    ptr = ptr2;
  }

  if (*info->posinfo) {
    /* Check that every column was mentioned, and no duplicates occurred. */
    int* check = XP_CALLOC(info->numcolumns, sizeof(int));
    for (i=0 ; i<info->numcolumns ; i++) check[i] = 0;
    for (i=0 ; i<info->numcolumns ; i++) {
      int w = info->columnIndex[i];
      if (w < 0 || w > info->numcolumns || check[w]) {
	FREEIF(*info->posinfo);
	break;
      }
      check[w] = 1;
    }
    XP_FREE(check);
  }

  if (!*info->posinfo) fe_set_default_column_widths(widget);

  info->headers = desc->header_strings;
  info->headerhighlight = (XP_Bool*)
    XP_ALLOC((info->numcolumns) * sizeof(XP_Bool));
  XP_MEMSET(info->headerhighlight, 0,
	    (info->numcolumns) * sizeof(XP_Bool));
  XmLGridAddRows(widget, XmHEADING, 0, 1);
  for (i=0 ; i<info->numcolumns ; i++) {
    char* name = (char *) desc->header_strings[info->columnIndex[i]];

    if (desc->type[info->columnIndex[i]] & FE_OUTLINE_String) 
      {
	str = fe_outline_get_header(XtName(widget), name, 0);
	XtVaSetValues(widget, XmNcolumn, i, XmNrowType, XmHEADING, XmNrow, 0,
		      XmNcellLeftBorderType, XmBORDER_LINE,
		      XmNcellRightBorderType, XmBORDER_LINE,
		      XmNcellTopBorderType, XmBORDER_LINE,
		      XmNcellBottomBorderType, XmBORDER_LINE,
		      XmNcellString, str, 0);
	XmStringFree(str);
      } 
    else if (desc->type[info->columnIndex[i]] & FE_OUTLINE_Icon) 
      {
	XtVaSetValues(widget, XmNcolumn, i, XmNrowType, XmHEADING, XmNrow, 0,
		      XmNcellLeftBorderType, XmBORDER_LINE,
		      XmNcellRightBorderType, XmBORDER_LINE,
		      XmNcellTopBorderType, XmBORDER_LINE,
		      XmNcellBottomBorderType, XmBORDER_LINE,
		      XmNcellType, XmPIXMAP_CELL, XmNcellPixmap, desc->icons[info->columnIndex[i]]->pixmap,
		      0);
      }
   
    /* keep track of the column information -- whether or not to indent, draw
       lines, etc. */
    info->column_indented[i] = desc->type[info->columnIndex[i]];
  }
  XtVaSetValues(widget, XmNallowColumnResize, True, 0);
  XtAddCallback(widget, XmNresizeCallback, fe_outline_resize, info);
}


void
fe_OutlineChangeHeaderLabel(Widget widget, const char* headername,
			    const char* label)
{
  fe_OutlineInfo* info = fe_get_info(widget);
  int i;
  int j;
  XmString str;
  if (label == NULL) label = headername;
  for (i=0 ; i<info->numcolumns ; i++) {
    unsigned char celltype;
    XtVaGetValues(widget, XmNcellType, &celltype, 0);

    if (XP_STRCMP(info->headers[i], headername) == 0) {
      for (j=0 ; j<info->numcolumns ; j++) {
	if (info->columnIndex[j] != i) continue;
	str = XmStringCreate((char*/*NOOP*/)label,
			     info->headerhighlight[i] ? BOLD_STYLE : DEFAULT_STYLE);
	XtVaSetValues(widget, XmNcolumn, j, XmNrowType, XmHEADING, XmNrow, 0,
		      XmNcellString, str, 
		      XmNcellType, XmSTRING_CELL,
		      XmNcellEditable, False, /* hmm.. */
		      0);
	XmStringFree(str);
	return;
      }
    }
  }
  XP_ASSERT(0);
}


void
fe_OutlineSetHeaderHighlight(Widget widget, const char* header, XP_Bool value)
{
  fe_OutlineInfo* info = fe_get_info(widget);
  int i, j;
  XmString str;
  for (i=0 ; i<info->numcolumns ; i++) {
    if (XP_STRCMP(info->headers[i], header) == 0) {
      if (info->headerhighlight[i] == value) return;
      info->headerhighlight[i] = value;
      for (j=0 ; j<info->numcolumns ; j++) {
	if (info->columnIndex[j] == i) {
	  str = fe_outline_get_header(XtName(widget), (char *)info->headers[i], value);
	  XtVaSetValues(widget, XmNcolumn, j, XmNrowType, XmHEADING,
			XmNrow, 0, XmNcellString, str, 0);
	  XmStringFree(str);
	  return;
	}
      }
      XP_ASSERT(0);
    }
  }
  /* should we really do this?
   - toshok 
  XP_ASSERT(0);
  */
}



void
fe_OutlineChange(Widget widget, int first, int length, int newnumrows)
{
  fe_OutlineInfo* info = fe_get_info(widget);
  int i;
  info->DataRow = -1;
  if (newnumrows != info->numrows) {
    if (newnumrows > info->numrows) {
      XmLGridAddRows(widget, XmCONTENT, -1, newnumrows - info->numrows);
    } else {
      XmLGridDeleteRows(widget, XmCONTENT, newnumrows,
			info->numrows - newnumrows);
    }
    info->numrows = newnumrows;
    length = newnumrows - first;
  }
  if (first == 0 && length == newnumrows) {
    XmLGridRedrawAll(widget);
  } else {
    for (i=first ; i<first + length ; i++) {
      XmLGridRedrawRow(widget, XmCONTENT, i);
    }
  }
  XFlush(XtDisplay(widget));
}


void
fe_OutlineSelect(Widget widget, int row, Boolean exclusive)
{
  if (exclusive) XmLGridDeselectAllRows(widget, False);
  XmLGridSelectRow(widget, row, False);
}


void
fe_OutlineUnselect(Widget widget, int row)
{
  XmLGridDeselectRow(widget, row, False);
}

void
fe_OutlineUnselectAll(Widget widget)
{
  XmLGridDeselectAllRows(widget, False);
}


void
fe_OutlineMakeVisible(Widget widget, int visible)
{
  fe_OutlineInfo* info = fe_get_info(widget);
  int firstrow, lastrow;
  XRectangle rect;
  Dimension height, shadowthickness;
  if (visible < 0) return;
  if (info->visibleTimer) {
    info->visibleLine = visible;
    return;
  }
  XtVaGetValues(widget, XmNscrollRow, &firstrow, XmNheight, &height,
		XmNshadowThickness, &shadowthickness, NULL);
  height -= shadowthickness;
  for (lastrow = firstrow + 1 ; ; lastrow++) {
    if (XmLGridRowColumnToXY(widget, XmCONTENT, lastrow, XmCONTENT, 0,
			     True, &rect) < 0)
      {
	break;
      }
    if (rect.y + rect.height >= (int)height) break;
  }
  if (visible >= firstrow && visible < lastrow) return;
  firstrow = visible - ((lastrow - firstrow) / 2);
  if (firstrow < 0) firstrow = 0;
  XtVaSetValues(widget, XmNscrollRow, firstrow, 0);
}


int
fe_OutlineGetSelection(Widget outline, MSG_ViewIndex* sellist, int sizesellist)
{
  int count = XmLGridGetSelectedRowCount(outline);
  if (sellist && count > 0) {
    int status;
    XP_ASSERT(count <= sizesellist);
    if (sizeof(MSG_ViewIndex) == sizeof(int)) {
      status = XmLGridGetSelectedRows(outline, (int*) sellist, count);
    } else {
      int* positions = (int*) XP_ALLOC(count * sizeof(int));
      int i;
      status = XmLGridGetSelectedRows(outline, positions, count);
      if (status == 0) {
	for (i=0 ; i<count ; i++) {
	  sellist[i] = positions[i];
	}
      }
      XP_FREE(positions);
    }
    if (status < 0) return status;
  }
  return count;
}


int
fe_OutlineRootCoordsToRow(Widget outline, int x, int y, XP_Bool* nearbottom)
{
  Position rootx;
  Position rooty;
  int row;
  int column;
  unsigned char rowtype;
  unsigned char coltype;
  XtTranslateCoords(outline, 0, 0, &rootx, &rooty);
  x -= rootx;
  y -= rooty;
  if (x < 0 || y < 0 ||
      x >= XfeWidth(outline) || y >= XfeHeight(outline)) {
    return -1;
  }
  if (XmLGridXYToRowColumn(outline, x, y, &rowtype, &row,
			   &coltype, &column) < 0) {
    return -1;
  }
  if (rowtype != XmCONTENT || coltype != XmCONTENT) return -1;
  if (nearbottom) {
    int row2;
    *nearbottom = (XmLGridXYToRowColumn(outline, x, y + 5, &rowtype, &row2,
					&coltype, &column) >= 0 &&
		   row2 > row);
  }
  return row;
}


#define SCROLLMARGIN 50
#define INITIALWAIT 500
#define REPEATINTERVAL 100

static void
fe_outline_drag_scroll(void* closure)
{
  fe_OutlineInfo* info = (fe_OutlineInfo*) closure;
  int row;
  info->dragscrolltimer = FE_SetTimeout(fe_outline_drag_scroll, info,
					REPEATINTERVAL);
  XtVaGetValues(info->widget, XmNscrollRow, &row, 0);
  row += info->dragscrolldir;
  XtVaSetValues(info->widget, XmNscrollRow, row, 0);
}

void
fe_OutlineHandleDragEvent(Widget outline, XEvent* event, fe_dnd_Event type,
			  fe_dnd_Source* source)
{
  fe_OutlineInfo* info = fe_get_info(outline);
  XP_Bool doscroll = FALSE;
  Position rootx;
  Position rooty;
  int x, y;
  unsigned char rowtype;
  unsigned char coltype;
  int row;
  int column;
  XmLCGridColumn* col;
  Dimension width;
  int i;
  int delta;
  int tmp;
  int total;

  if (source->type == FE_DND_COLUMN) {
    if (type == FE_DND_DROP && source->closure == outline) 
      {
	/* first we make sure that the drop happens within a valid row/column
	   pair. */

	XtTranslateCoords(outline, 0, 0, &rootx, &rooty);

	x = event->xbutton.x_root - rootx;
	y = event->xbutton.y_root - rooty;
	
	if (XmLGridXYToRowColumn(info->widget, x, y,
				 &rowtype, &row, &coltype, &column) < 0)
	  return;
	
	if (column != info->dragcolumn) 
	  {
	    /* Get rid of the hack that makes the last column really wide.  Make
	       it be exactly as wide as it appears, so that if it no longer
	       ends up as the last column, it has a reasonable width. */
	    total = XfeWidth(outline);
	    for (i=0 ; i<info->numcolumns ; i++) 
	      {
		col = XmLGridGetColumn(outline, XmCONTENT, i);
		XtVaGetValues(outline,
			      XmNcolumnPtr, col,
			      XmNcolumnWidth, &width, 0
			      );
		total -= ((int) width); /* Beware any unsigned lossage... */
	      }
	    if (total < MIN_COLUMN_WIDTH) total = MIN_COLUMN_WIDTH;
	    width = total;
	    if (width > MAX_COLUMN_WIDTH) width = MAX_COLUMN_WIDTH;
	    XtVaSetValues(outline, XmNcolumn, info->numcolumns - 1,
			  XmNcolumnWidth, width, 0);

	    if (info->dragcolumn < column) 
	      {
		delta = 1;
	      } 
	    else 
	      {
		delta = -1;
	      }

	    /* save off the column number we dragged */
	    tmp = info->columnIndex[info->dragcolumn];

	    /* move all the other columns out of the way. */
	    for (i=info->dragcolumn ; i != column ; i += delta) 
	      {
		info->columnIndex[i] = info->columnIndex[i + delta];
	      }
	    
	    /* replace the column we dragged in the place we dropped
	       it. */
	    info->columnIndex[column] = tmp;
	    
	    XmLGridMoveColumns(outline, column, info->dragcolumn, 1);
	    
	    /* Now restore the hack of having the last column be wide. */
	    XtVaSetValues(outline, XmNcolumn, info->numcolumns - 1,
			  XmNcolumnWidth, MAX_COLUMN_WIDTH, 0);
	    
	    fe_outline_remember_columns(outline);
	  }
      }

    return;
  }
  
  if (type != FE_DND_DRAG && type != FE_DND_END) return;

  if (type == FE_DND_DRAG) {
    XtTranslateCoords(outline, 0, 0, &rootx, &rooty);
    x = event->xbutton.x_root - rootx;
    y = event->xbutton.y_root - rooty;
    doscroll = (x >= 0 && x < XfeWidth(outline) &&
		((y < 0 && y >= -SCROLLMARGIN) ||
		 (y >= XfeHeight(outline) &&
		  y < XfeHeight(outline) + SCROLLMARGIN)));
    info->dragscrolldir = y > XfeHeight(outline) / 2 ? 1 : -1;
  }
  if (!doscroll) {
    if (info->dragscrolltimer) {
      FE_ClearTimeout(info->dragscrolltimer);
      info->dragscrolltimer = NULL;
    }
  } else {
    if (!info->dragscrolltimer) {
      info->dragscrolltimer = FE_SetTimeout(fe_outline_drag_scroll, info,
					    INITIALWAIT);
    }
  }
}


void
fe_OutlineSetDragFeedback(Widget outline, int row, XP_Bool usebox)
{
  fe_OutlineInfo* info = fe_get_info(outline);
  int old = info->dragrow;
  if (old == row && info->dragrowbox == usebox) return;
  info->dragrowbox = usebox;
  info->dragrow = row;
  if (old >= 0) XmLGridRedrawRow(outline, XmCONTENT, old);
  if (row >= 0 && row != old) XmLGridRedrawRow(outline, XmCONTENT, row);
}


Widget
fe_OutlineCreate(MWContext* context, Widget parent, String name,
		 ArgList av, Cardinal ac,
		 int maxindentdepth,
		 int numcolumns, int* columnwidths,
		 fe_OutlineGetDataFunc datafunc,
		 fe_OutlineClickFunc clickfunc,
		 fe_OutlineIconClickFunc iconfunc,
		 void* closure,
		 char** posinfo, int tag)
{
  /*int i;*/

  return fe_GridCreate(context, parent, name, av, ac, maxindentdepth,
		       numcolumns, columnwidths, datafunc, clickfunc, 
		       iconfunc, closure, posinfo, tag, True); 
  /* last var = isOutline: attach TRUE for Outline, attach FALSE for Grid */
}

void fe_OutlineEnableDrag(Widget outline, fe_icon *dragicon, fe_dnd_Type dragtype)
{
  fe_OutlineInfo* info = fe_get_info(outline);

  info->drag_enabled = True;
  info->drag_icon = dragicon;
  info->drag_type = dragtype;
}

void fe_OutlineDisableDrag(Widget outline)
{
  fe_OutlineInfo* info = fe_get_info(outline);
  
  info->drag_enabled = False;
  info->drag_icon = NULL;
  info->drag_type = FE_DND_NONE;
}
