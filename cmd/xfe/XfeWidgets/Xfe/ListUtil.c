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
/*-----------------------------------------*/
/*																		*/
/* Name:		<Xfe/ListUtil.c>										*/
/* Description:	List misc utilities source.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/XfeP.h>
#include <Xfe/PrimitiveP.h>
#include <Xfe/ManagerP.h>
#include <Xfe/ListUtilP.h>

#include <Xm/ScrolledWP.h>

#define SW_ACCESS(_w,_m) (((XmScrolledWindowWidget) _w ) -> swindow . _m)

/*----------------------------------------------------------------------*/
/*																		*/
/* XmList utilities.													*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ int
XfeListGetItemCount(Widget list)
{
	assert( _XfeIsAlive(list) );
	assert( XmIsList(list) );

	return _XfeXmListAccess(list,itemCount);
}
/*----------------------------------------------------------------------*/
/* extern */ int
XfeListGetSelectedItemPos(Widget list)
{
	int *		sel_items;
	int			sel_item_count;
	int			position = -1;

	if (XmListGetSelectedPos(list,&sel_items,&sel_item_count))
	{
		if (sel_items)
		{
			position = sel_items[0];
		
			XtFree((char *) sel_items);
		}
	}

	return position;
}
/*----------------------------------------------------------------------*/
/* extern */ XmString
XfeListGetSelectedItem(Widget list)
{
	XmString	item = NULL;
	XmString *	sel_items;
	int			sel_item_count;

	XtVaGetValues(list,
				  XmNselectedItems,		&sel_items,
				  XmNselectedItemCount,	&sel_item_count,
				  NULL);

	if (sel_item_count)
	{
		item = sel_items[0];
	}

	return item;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeListSelectItemAtPos(Widget list,int position)
{
	assert( _XfeIsAlive(list) );
	assert( XmIsList(list) );

	if (position > 0)
	{
		XmListSelectPos(list,position,False);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeListDeselectItemAtPos(Widget list,int position)
{
	assert( _XfeIsAlive(list) );
	assert( XmIsList(list) );

	if (position > 0)
	{
		XmListDeselectPos(list,position);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ int
XfeListGetItemAtY(Widget list,Position y)
{
	int position;

	assert( _XfeIsAlive(list) );
	assert( XmIsList(list) );

	position = XmListYToPos(list,y);
 	
	if ((position == 0) && (y <= _XfeHeight(list)))
	{
		position = XfeListGetItemCount(list);
	}

	return position;
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeListGetMaxItemHeight(Widget list)
{
	assert( _XfeIsAlive(list) );
	assert( XmIsList(list) );

	return _XfeXmListAccess(list,MaxItemHeight);
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeListGetMaxItemWidth(Widget list)
{
	Dimension	max_width = 0;
	Cardinal	i;

	assert( _XfeIsAlive(list) );
	assert( XmIsList(list) );

    if (_XfeXmListAccess(list,InternalList) && 
		_XfeXmListAccess(list,itemCount))
    {
		for (i = 0; i < _XfeXmListAccess(list,itemCount); i++)
		{
			if (_XfeXmListAccess(list,InternalList)[i]->width > max_width)
			{
				max_width = _XfeXmListAccess(list,InternalList)[i]->width;
			}
		}
    }

	return max_width;
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeListGetSpacing(Widget list)
{
	assert( _XfeIsAlive(list) );
	assert( XmIsList(list) );

	return _XfeXmListAccess(list,ItemSpacing);
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeListGetMarginHeight(Widget list)
{
	assert( _XfeIsAlive(list) );
	assert( XmIsList(list) );

	return _XfeXmListAccess(list,margin_height);
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeListGetMarginWidth(Widget list)
{
	assert( _XfeIsAlive(list) );
	assert( XmIsList(list) );

	return _XfeXmListAccess(list,margin_width);
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeListPreferredGeometry(Widget			list,
						 Dimension *	width_out,
						 Dimension *	height_out)
{
	Dimension	width;
	Dimension	height;
	Cardinal	item_count;

	short spacing;

	assert( _XfeIsAlive(list) );
	assert( XmIsList(list) );
	assert( width_out != NULL || height_out != NULL );

	item_count = _XfeXmListAccess(list,itemCount);

	width = XfeListGetMaxItemWidth(list);

	spacing = _XfeXmListAccess(list,ItemSpacing);

	height = 
		item_count * XfeListGetMaxItemHeight(list) +
		item_count * 2 * _XfeHighlightThickness(list) +
		(item_count - 1) * _XfeXmListAccess(list,ItemSpacing);

	printf("spacing = %d\n",spacing);
	printf("highlight = %d\n",_XfeHighlightThickness(list));
	printf("item_count = %d\n",item_count);
	printf("height = %d\n",height);
		
	if (height_out)
	{
		*height_out = height;
	}

	if (width_out)
	{
		*width_out = width;
	}

}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* XmScrolledWindow utilities.											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeScrolledWindowGetVSB(Widget sw)
{
	assert( _XfeIsAlive(sw) );
	assert( XmIsScrolledWindow(sw) );

	return (Widget) SW_ACCESS(sw,vScrollBar);
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeScrolledWindowGetHSB(Widget sw)
{
	assert( _XfeIsAlive(sw) );
	assert( XmIsScrolledWindow(sw) );

	return (Widget) SW_ACCESS(sw,hScrollBar);

}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeScrolledWindowGetMarginHeight(Widget sw)
{
	assert( _XfeIsAlive(sw) );
	assert( XmIsScrolledWindow(sw) );

	return SW_ACCESS(sw,HeightPad);
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeScrolledWindowGetMarginWidth(Widget sw)
{
	assert( _XfeIsAlive(sw) );
	assert( XmIsScrolledWindow(sw) );

	return SW_ACCESS(sw,WidthPad);
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeScrolledWindowGetSpacing(Widget sw)
{
	assert( _XfeIsAlive(sw) );
	assert( XmIsScrolledWindow(sw) );

	return SW_ACCESS(sw,pad);
}
/*----------------------------------------------------------------------*/
