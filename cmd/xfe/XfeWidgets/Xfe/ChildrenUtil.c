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
/* Name:		<Xfe/ChildrenUtil.c>									*/
/* Description:	Children misc utilities source.							*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <Xfe/XfeP.h>

#include <Xfe/PrimitiveP.h>
#include <Xfe/ManagerP.h>


/*----------------------------------------------------------------------*/
/*																		*/
/* Children access														*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Cardinal
XfeNumChildren(Widget w)
{
	assert( XtIsComposite(w) );

	return _XfemNumChildren(w);
}
/*----------------------------------------------------------------------*/
/* extern */ WidgetList
XfeChildren(Widget w)
{
	assert( XtIsComposite(w) );

	return _XfemChildren(w);
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeChildrenIndex(Widget w,Cardinal i)
{
	assert( XtIsComposite(w) );

	return _XfeChildrenIndex(w,i);
}
/*----------------------------------------------------------------------*/
/* extern */ Cardinal
XfeChildrenCountAlive(Widget w)
{
	Cardinal	num_alive = 0;
	Cardinal	i;

	assert( w != NULL );

	for (i = 0; i < _XfemNumChildren(w); i ++)
	{
		if (_XfeIsAlive(_XfeChildrenIndex(w,i)))
		{
			num_alive++;
		}
	}		

	return num_alive;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Popup children access												*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ Cardinal
XfeNumPopups(Widget w)
{
	return _XfeNumPopups(w);
}
/*----------------------------------------------------------------------*/
/* extern */ WidgetList
XfePopupList(Widget w)
{
	return _XfePopupList(w);
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfePopupListIndex(Widget w,Cardinal i)
{
	return _XfePopupListIndex(w,i);
}
/*----------------------------------------------------------------------*/
/* extern */ Cardinal
XfePopupListCountAlive(Widget w)
{
	Cardinal	num_alive = 0;
	Cardinal	i;

	assert( w != NULL );

	for (i = 0; i < _XfeNumPopups(w); i ++)
	{
		if (_XfeIsAlive(_XfePopupListIndex(w,i)))
		{
			num_alive++;
		}
	}		

	return num_alive;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Children access														*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeChildrenGet(Widget w,WidgetList * children,Cardinal * num_children)
{
    assert( _XfeIsAlive(w) );
    assert( XtIsComposite(w) );
	assert( (children != NULL) || (num_children != NULL) );

	if (children)
	{
		*children		= _XfemChildren(w);
	}

	if (num_children)
	{
		*num_children	= _XfemNumChildren(w);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ Cardinal
XfeChildrenGetNumManaged(Widget w)
{
	Cardinal i;
	Cardinal num_managed = 0;

    assert( _XfeIsAlive(w) );
    assert( XtIsComposite(w) );

	for (i = 0; i < _XfemNumChildren(w); i++)
	{
 		if (_XfeIsManaged(_XfeChildrenIndex(w,i)))
		{
			num_managed++;
		}
	}

	return num_managed;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeChildrenGetLast(Widget w)
{
    assert( _XfeIsAlive(w) );
    assert( XtIsComposite(w) );

	if (_XfemChildren(w) && _XfemNumChildren(w))
	{
		return _XfemChildren(w)[ _XfemNumChildren(w) - 1 ];
	}

	return NULL;
}
/*----------------------------------------------------------------------*/
/* extern */ int
XfeChildGetIndex(Widget w)
{
	Widget parent = XtParent(w);

    assert( _XfeIsAlive(w) );

	parent = XtParent(w);

    assert( _XfeIsAlive(parent) );
	
	if (_XfemNumChildren(parent) && _XfemChildren(parent))
	{
		Cardinal	i = 0;

		for (i = 0; i < _XfemNumChildren(parent); i++)
		{
			if (_XfemChildren(parent)[i] == w)
			{
				return i;
			}
		}
	}

	return -1;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Destruction															*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeChildrenDestroy(Widget w)
{
	Widget *		children;
	Cardinal		i;

    assert( _XfeIsAlive(w) );
    assert( XtIsComposite(w) );

	/* Make sure the widget has children */
	if (!_XfemNumChildren(w))
	{
		return;
	}

	/* Allocate space for a temporary children array */
	children = (Widget *) XtMalloc(sizeof(Widget) * _XfemNumChildren(w));

	/* Copy the widget ids */
	for (i = 0; i < _XfemNumChildren(w); i++)
	{
		children[i] = _XfemChildren(w)[i];
	}

	/* Destroy the children */
	for (i = 0; i < _XfemNumChildren(w); i++)
	{
		if (_XfeIsAlive(children[i]))
		{
			XtDestroyWidget(children[i]);
		}
	}

	XtFree((char *) children);

	/* Update an flush in case the destruction takes a long time */
	XmUpdateDisplay(w);
	XFlush(XtDisplay(w));
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Callbacks / Event handlers											*/
/*																		*/
/*----------------------------------------------------------------------*/
/* extern */ void
XfeChildrenAddCallback(Widget			w,
					   String			callback_name,
					   XtCallbackProc	callback,
					   XtPointer		data)
{
	Widget *		children;
	Cardinal		num_children;
	Cardinal		i;

    assert( _XfeIsAlive(w) );
    assert( XtIsComposite(w) );

	XfeChildrenGet(w,&children,&num_children);

	for (i = 0; i < num_children; i++)
	{
		assert( _XfeIsAlive(children[i]) );

		XtAddCallback(children[i],callback_name,callback,data);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeChildrenAddEventHandler(Widget			w,
						   EventMask		event_mask,
						   Boolean			nonmaskable,
						   XtEventHandler	proc,
						   XtPointer		data)
{
	Widget *		children;
	Cardinal		num_children;
	Cardinal		i;

    assert( _XfeIsAlive(w) );
    assert( XtIsComposite(w) );

	XfeChildrenGet(w,&children,&num_children);

	for (i = 0; i < num_children; i++)
	{
		assert( _XfeIsAlive(children[i]) );

		XtAddEventHandler(children[i],event_mask,nonmaskable,proc,data);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeChildrenRemoveCallback(Widget			w,
						  String			callback_name,
						  XtCallbackProc	callback,
						  XtPointer			data)
{
	Widget *		children;
	Cardinal		num_children;
	Cardinal		i;

    assert( _XfeIsAlive(w) );
    assert( XtIsComposite(w) );

	XfeChildrenGet(w,&children,&num_children);

	for (i = 0; i < num_children; i++)
	{
		assert( _XfeIsAlive(children[i]) );

		XtRemoveCallback(children[i],callback_name,callback,data);
	}
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeChildrenRemoveEventHandler(Widget			w,
							  EventMask			event_mask,
							  Boolean			nonmaskable,
							  XtEventHandler	proc,
							  XtPointer			data)
{
	Widget *		children;
	Cardinal		num_children;
	Cardinal		i;

    assert( _XfeIsAlive(w) );
    assert( XtIsComposite(w) );

	XfeChildrenGet(w,&children,&num_children);

	for (i = 0; i < num_children; i++)
	{
		if (_XfeIsAlive(w))
		{
			XtRemoveEventHandler(children[i],event_mask,nonmaskable,proc,data);
		}

		XtRemoveEventHandler(children[i],event_mask,nonmaskable,proc,data);
	}
}
/*----------------------------------------------------------------------*/

