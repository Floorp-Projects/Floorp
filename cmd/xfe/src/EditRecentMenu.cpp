/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#include "EditRecentMenu.h"
#include "MozillaApp.h"
#include "View.h"
#include "EditorFrame.h"
#include "felocale.h"
#include "intl_csi.h"

#include <Xm/PushBG.h>
#include <Xm/SeparatoG.h>
#include <Xfe/Cascade.h>

#include "edt.h"
#include "xeditor.h"

#include "xpgetstr.h"			// for XP_GetString()

#define MAX_MENU_ITEM_LENGTH				40
#define MAX_NUM_RECENT_MENU_ITEMS			10

extern int XFE_UNTITLED;

//////////////////////////////////////////////////////////////////////////
//
// Used to pass data to the callbacks
//
//////////////////////////////////////////////////////////////////////////
typedef struct
{
	XFE_EditRecentMenu *	object;		// Object
	char *	                pUrl;		// the URL...
} _OpenCallbackStruct;
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
XFE_EditRecentMenu::XFE_EditRecentMenu(XFE_Frame *	frame,
									   Widget		cascade,
									   XP_Bool      openPage) :
	_frame(frame),
	_cascade(cascade),
	_openPage(openPage),
	_itemCount(0)
{
	// Obtain the submenu
	XtVaGetValues(_cascade,XmNsubMenuId,&_submenu,NULL);

	XP_ASSERT( _submenu != NULL );

	XtAddCallback(_cascade,
				  XmNdestroyCallback,
				  &XFE_EditRecentMenu::cascadeDestroyCB,
				  (XtPointer) this);
	
	XtAddCallback(_cascade,
				  XmNcascadingCallback,
				  &XFE_EditRecentMenu::cascadingCB,
				  (XtPointer) this);

	// Make the delay large to get the desired "hold" effect
	XtVaSetValues(_cascade,XmNmappingDelay,300,NULL);
}
//////////////////////////////////////////////////////////////////////////
XFE_EditRecentMenu::~XFE_EditRecentMenu()
{
}
//////////////////////////////////////////////////////////////////////////
void
XFE_EditRecentMenu::generate(Widget		cascade,
							 XtPointer	data,
							 XFE_Frame *	frame)
{
	XFE_EditRecentMenu * obj;

	obj = new XFE_EditRecentMenu(frame,cascade, (int) data);

#ifdef DEBUG_rhess2
	fprintf(stderr, "generate::[ %p ] - [ %p ][ %p ][ %p ]\n", 
			obj, cascade, data, frame);
#endif
}
//////////////////////////////////////////////////////////////////////////
//
// Cascade callbacks
//
//////////////////////////////////////////////////////////////////////////
void
XFE_EditRecentMenu::cascadeDestroyCB(Widget,XtPointer clientData,XtPointer)
{
	XFE_EditRecentMenu * obj = (XFE_EditRecentMenu*)clientData;
	
	XP_ASSERT( obj != NULL );
	
	delete obj;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_EditRecentMenu::cascadingCB(Widget,XtPointer clientData,XtPointer)
{
	XFE_EditRecentMenu *obj = (XFE_EditRecentMenu*)clientData;

	XP_ASSERT( obj != NULL );
	
	obj->cascading();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_EditRecentMenu::cascading()
{
	destroyItems();

	fillSubmenu();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_EditRecentMenu::destroyItems()
{
	Widget *kids = 0;
	Cardinal nkids = 0;

	// XtUnrealizeWidget (_submenu);

	XtVaGetValues (_submenu, 
				   XmNchildren, &kids, 
				   XmNnumChildren, &nkids, 
				   0);

	if (nkids) {
		//		kids = &(kids[m_firstslot]);
		//		nkids -= m_firstslot;
		
		if (nkids) { 
			XtUnmanageChildren (kids, nkids);
					
			XfeDestroyMenuWidgetTree(kids,nkids,False);
#ifdef DEBUG_rhess2
			fprintf(stderr, "destroyItems::[ %d ]\n", nkids);
#endif			
		}
	}
	else {
#ifdef DEBUG_rhess2
		fprintf(stderr, "destroyItems::[ NULL ]\n");
#endif			
	}

	_itemCount = 0;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_EditRecentMenu::fillSubmenu()
{
	char * pUrl = NULL;
	int32     i = 0;

	XP_ASSERT( _frame != NULL );

	MWContext * context = _frame->getContext();

	XP_ASSERT( context != NULL );

	if (!_frame || !context)
	{
		return;
	}
	
	// XtRealizeWidget (_submenu);

	if (_openPage) {
		Widget separator = NULL;

		addItem(0, xfeCmdOpenPage, NULL);

		separator = XmCreateSeparatorGadget(_submenu, "separator", NULL, 0);

		XtVaSetValues(separator, 
					  XmNshadowThickness, 2,
					  NULL);
		XtManageChild(separator);
	}

	for( i = 0; i < MAX_NUM_RECENT_MENU_ITEMS; i++ )
	{
		if(EDT_GetEditHistory(context, i, &pUrl, NULL))
        {
            // Condense in case URL is too long for the menu
            // Note: pUrl is static string - don't free it

			addItem(i,NULL, pUrl);
			_itemCount++;
        }
    }
#ifdef DEBUG_rhess2
	fprintf(stderr, "fillSubmenu::[ %d ]\n", _itemCount);
#endif			
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Menu item callbacks
//
//////////////////////////////////////////////////////////////////////////
void
XFE_EditRecentMenu::itemActivateCB(Widget,XtPointer clientData,XtPointer)
{
	_OpenCallbackStruct * data = (_OpenCallbackStruct *) clientData;

	XP_ASSERT( data != NULL );
	XP_ASSERT( data->object != NULL );
	
	data->object->itemActivate(data->pUrl);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_EditRecentMenu::itemArmCB(Widget,XtPointer clientData,XtPointer)
{
	_OpenCallbackStruct * data = (_OpenCallbackStruct *) clientData;

	XP_ASSERT( data != NULL );
	XP_ASSERT( data->object != NULL );
	
	data->object->itemArm(data->pUrl);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_EditRecentMenu::itemDisarmCB(Widget,XtPointer clientData,XtPointer)
{
	_OpenCallbackStruct * data = (_OpenCallbackStruct *) clientData;

	XP_ASSERT( data != NULL );
	XP_ASSERT( data->object != NULL );
	
	data->object->itemDisarm();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_EditRecentMenu::itemDestroyCB(Widget,XtPointer clientData,XtPointer)
{
	_OpenCallbackStruct * data = (_OpenCallbackStruct *) clientData;

	XP_ASSERT( data != NULL );

	XP_FREE(data);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_EditRecentMenu::itemActivate(char * pUrl)
{
	XP_ASSERT( _frame != NULL );

	MWContext *		context = _frame->getContext();

	if (pUrl) {
		if (_frame->handlesCommand(xfeCmdEditPage,(void *) pUrl)) {
			if (EDT_IS_NEW_DOCUMENT(context) && !EDT_DirtyFlag(context)) {
				URL_Struct* url = NET_CreateURLStruct(pUrl, NET_NORMAL_RELOAD);
				XFE_EditorFrame *theFrame = (XFE_EditorFrame*) _frame;
				
				theFrame->getURL(url);
			}
			else {
				fe_EditorEdit(context, _frame, /*chromespec=*/NULL, pUrl);
			}
		}
	}
	else {
		fe_OpenURLDialog(context);
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_EditRecentMenu::itemArm(char* /*pUrl*/)
{
	XP_ASSERT( _frame != NULL );
	//
	// d'oh - what's supposed to happen now?...
	//
}
//////////////////////////////////////////////////////////////////////////
void
XFE_EditRecentMenu::itemDisarm()
{
	XP_ASSERT( _frame != NULL );

	_frame->notifyInterested(XFE_View::statusNeedsUpdatingMidTruncated,
							 (void*) "");
}
//////////////////////////////////////////////////////////////////////////
void
XFE_EditRecentMenu::addItem(int		position,
							char *    name,
							char *	pUrl)
{
	char*  xt_name = NULL;
	char*    title = pUrl;

	// Make sure the title is valid
	if (!name && !pUrl)
	{
		return;
	}

	if (name) {
		xt_name = name;
	}
	else {
		xt_name = xfeCmdOpenTargetUrl;
	}

	_OpenCallbackStruct * data;

	// Create a new bookmark data structure for the callbacks
	data = XP_NEW_ZAP(_OpenCallbackStruct);

	data->object	= this;
	data->pUrl		= pUrl;

	// Create the new item
	Widget item = XtVaCreateManagedWidget(xt_name,
										  xmPushButtonGadgetClass,
										  _submenu,
										  NULL);
	// Add callbacks to item
	XtAddCallback(item,
				  XmNactivateCallback,
				  &XFE_EditRecentMenu::itemActivateCB,
				  (XtPointer) data);

	XtAddCallback(item,
				  XmNarmCallback,
				  &XFE_EditRecentMenu::itemArmCB,
				  (XtPointer) data);

	XtAddCallback(item,
				  XmNdisarmCallback,
				  &XFE_EditRecentMenu::itemDisarmCB,
				  (XtPointer) data);

	if (title) {
		// Format the title
		char * buffer = PR_smprintf("%2d  %s",position + 1,title);

		XP_ASSERT( buffer != NULL );

		// Truncate the title and install it on the item
		if (buffer)	{
			MWContext *			context = _frame->getContext();
			INTL_CharSetInfo	c = LO_GetDocumentCharacterSetInfo(context);
			XmFontList			font_list;
			
			INTL_MidTruncateString(INTL_GetCSIWinCSID(c), 
								   buffer, 
								   buffer,
								   MAX_MENU_ITEM_LENGTH);
			
			
			XmString label = fe_ConvertToXmString((unsigned char *) buffer,
												  INTL_GetCSIWinCSID(c), 
												  NULL, 
												  XmFONT_IS_FONT,
												  &font_list);
			
			if (label)
				{
					XtVaSetValues(item,XmNlabelString,label,NULL);
					
					XmStringFree(label);
				}
			
			// Free the allocated memory
			XP_FREE(buffer);
		}
	}
}
//////////////////////////////////////////////////////////////////////////
