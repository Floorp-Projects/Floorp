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
/* Name:		<XfeTest/TestMenu.c>									*/
/* Description:	Xfe widget menu test funcs.								*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#include <XfeTest/XfeTest.h>

static WidgetClass	type_to_class			(XfeMenuItemType item_type);
static String		type_to_action_cb_name	(XfeMenuItemType item_type);

/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeMenuItemCreate(Widget pw,XfeMenuItem data,XtPointer client_data)
{
	Widget			item = NULL;
	WidgetClass		wc = NULL;

	assert( XfeIsAlive(pw) );
	assert( XmIsRowColumn(pw) );
	assert( data != NULL );
	assert( data->name != NULL );

	wc = type_to_class(data->type);

	assert( wc != NULL );

	if (client_data)
	{
		data->client_data = client_data;
	}

	if (data->type == XfeMENU_PANE)
	{
		XfeMenuPaneRec pane_data;

		pane_data.name = data->name;
		pane_data.items = data->pane_items;

		item = XfeMenuPaneCreate(pw,&pane_data,client_data);
	}
	else
	{
		item = XtVaCreateManagedWidget(data->name,wc,pw,NULL);

		if (data->type == XfeMENU_SEP)
		{
			XtVaSetValues(item,XmNshadowThickness,3,NULL);
		}
		else if (data->type == XfeMENU_RADIO)
		{
			XtVaSetValues(item,XmNindicatorType,XmONE_OF_MANY,NULL);
			XtVaSetValues(pw,XmNradioBehavior,True,NULL);
		}
		else if (data->type == XfeMENU_TOGGLE)
		{
			XtVaSetValues(item,XmNindicatorType,XmN_OF_MANY,NULL);
			XtVaSetValues(pw,XmNradioBehavior,False,NULL);
		}

		if (data->action_cb)
		{
			String cb_name = type_to_action_cb_name(data->type);
		
			XtAddCallback(item,cb_name,data->action_cb,data->client_data);
		}
	}

	return item;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeMenuItemCreate2(Widget				pw,
				   String				name,
				   XfeMenuItemType		type,
				   XtCallbackProc		action_cb,
				   XfeMenuItemRec *		pane_items,
				   XtPointer			client_data)
{
	XfeMenuItemRec menu_item;

	assert( XfeIsAlive(pw) );
	assert( XmIsRowColumn(pw) );
	assert( name != NULL );

	menu_item.name			= name;
	menu_item.type			= type;
	menu_item.action_cb		= action_cb;
	menu_item.pane_items	= pane_items;
	menu_item.client_data	= client_data;

	return XfeMenuItemCreate(pw,&menu_item,client_data);
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeMenuPaneCreate(Widget pw,XfeMenuPane data,XtPointer client_data)
{
	Widget			pane = NULL;
	Widget			cascade;
	char			buf[1024];
	
	assert( XfeIsAlive(pw) );
	assert( XmIsRowColumn(pw) );
	assert( data != NULL );

	sprintf(buf,"%s_pulldown",data->name);

	pane = XmCreatePulldownMenu(pw,buf,NULL,0);

	XtVaSetValues(pane,XmNtearOffModel,XmTEAR_OFF_ENABLED,NULL);

	cascade = XtVaCreateManagedWidget(data->name,
									  xmCascadeButtonWidgetClass,
									  pw,
									  XmNsubMenuId,		pane,
									  NULL);
	if (data->items)
	{
		XfeMenuItem item = data->items;

		while(item && item->name)
		{
			XfeMenuItemCreate(pane,item,client_data);

			item++;
		}
	}

	if ((strcmp(data->name,"help") == 0) || 
		(strcmp(data->name,"Help") == 0))
	{
		XtVaSetValues(pw,XmNmenuHelpWidget,cascade,NULL);
	}

	return cascade;
}
/*----------------------------------------------------------------------*/
extern Widget
XfeMenuPaneCreate2(Widget			pw,
				   String			name,
				   XfeMenuItemRec * items,
				   XtPointer		client_data)
{
	XfeMenuPaneRec menu_pane;

	assert( XfeIsAlive(pw) );
	assert( XmIsRowColumn(pw) );
	assert( name != NULL );

	menu_pane.name	= name;
	menu_pane.items	= items;

	return XfeMenuPaneCreate(pw,&menu_pane,client_data);
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfeMenuBarCreate(Widget				pw,
				 String				name,
				 XfeMenuPaneRec *	items,
				 XtPointer			client_data,
				 ArgList			av,
				 Cardinal			ac)
{
	Widget menu_bar = NULL;
	
	assert( XfeIsAlive(pw) );
	assert( name != NULL );

	menu_bar = XmCreateMenuBar(pw,name,av,ac);

	if (items)
	{
		XfeMenuPane pane = items;

		while(pane && pane->name)
		{
			XfeMenuPaneCreate(menu_bar,pane,client_data);

			pane++;
		}
	}

	XtManageChild(menu_bar);

	return menu_bar;
}
/*----------------------------------------------------------------------*/
/* extern */ Widget
XfePopupMenuCreate(Widget			pw,
				   String			name,
				   XfeMenuPaneRec *	items,
				   XtPointer		client_data,
				   ArgList			av,
				   Cardinal			ac)
{
	Widget popup_menu = NULL;
	
	assert( XfeIsAlive(pw) );
	assert( name != NULL );

	popup_menu = XmCreatePopupMenu(pw,name,av,ac);

	XtVaSetValues(popup_menu,XmNtearOffModel,XmTEAR_OFF_ENABLED,NULL);

	if (items)
	{
		XfeMenuPane pane = items;

		while(pane && pane->name)
		{
			XfeMenuPaneCreate(popup_menu,pane,client_data);

			pane++;
		}
	}

	return popup_menu;
}
/*----------------------------------------------------------------------*/
/* extern */ String
XfeNameIndex(String prefix,Cardinal i)
{
	static char buf[4096];

	sprintf(buf,"%s%d",prefix,i);

	return buf;
}
/*----------------------------------------------------------------------*/
/* extern */ String
XfeNameString(String prefix,String name)
{
	static char buf[4096];

	sprintf(buf,"%s%s",prefix,name);

	return buf;
}
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
static WidgetClass
type_to_class(XfeMenuItemType item_type)
{
	WidgetClass wc = NULL;

	switch(item_type)
	{
	case XfeMENU_BM_PANE:		wc = xfeBmCascadeWidgetClass;		break;
	case XfeMENU_BM_PUSH:		wc = xfeBmButtonWidgetClass;		break;
	case XfeMENU_PANE:			wc = xmCascadeButtonWidgetClass;	break;
	case XfeMENU_LABEL:			wc = xmLabelWidgetClass;			break;
	case XfeMENU_PUSH:			wc = xmPushButtonWidgetClass;		break;
	case XfeMENU_SEP:			wc = xmSeparatorWidgetClass;		break;
	case XfeMENU_TOGGLE:		wc = xmToggleButtonWidgetClass;		break;
	case XfeMENU_RADIO:			wc = xmToggleButtonWidgetClass;		break;
	}

	return wc;
}
/*----------------------------------------------------------------------*/
static String
type_to_action_cb_name(XfeMenuItemType item_type)
{
	String name = NULL;

	switch(item_type)
	{
	case XfeMENU_BM_PANE:		name = XmNactivateCallback;		break;
	case XfeMENU_BM_PUSH:		name = XmNactivateCallback;		break;
	case XfeMENU_PANE:			name = XmNactivateCallback;		break;
	case XfeMENU_LABEL:			name = NULL;					break;
	case XfeMENU_PUSH:			name = XmNactivateCallback;		break;
	case XfeMENU_RADIO:			name = XmNvalueChangedCallback;	break;
	case XfeMENU_SEP:			name = NULL;					break;
	case XfeMENU_TOGGLE:		name = XmNvalueChangedCallback;	break;
	}

	return name;
}
/*----------------------------------------------------------------------*/
