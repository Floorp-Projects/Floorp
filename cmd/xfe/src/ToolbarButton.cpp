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

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// Name:        ToolbarButton.cpp                                       //
//                                                                      //
// Description:	XFE_ToolbarButton class implementation.                 //
//              A toolbar push button.                                  //
//                                                                      //
// Author:		Ramiro Estrugo <ramiro@netscape.com>                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "ToolbarButton.h"
#include "RDFUtils.h"
#include "BrowserFrame.h"			// for fe_reuseBrowser()

#include <Xfe/Button.h>

#include "prefapi.h"

//////////////////////////////////////////////////////////////////////////
XFE_ToolbarButton::XFE_ToolbarButton(XFE_Frame *		frame,
									 Widget				parent,
                                     HT_Resource		htResource,
									 const String		name) :
	XFE_ToolbarItem(frame,parent,htResource,name)
{
}
//////////////////////////////////////////////////////////////////////////
XFE_ToolbarButton::~XFE_ToolbarButton()
{
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Initialize
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarButton::initialize()
{
    Widget button = createBaseWidget(getParent(),getName());

	setBaseWidget(button);
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Sensitive interface
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarButton::setSensitive(Boolean state)
{
	XP_ASSERT( isAlive() );

	XfeSetPretendSensitive(m_widget,state);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ Boolean
XFE_ToolbarButton::isSensitive()
{
	XP_ASSERT( isAlive() );
	
	return XfeIsPretendSensitive(m_widget);
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Widget creation interface
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ Widget
XFE_ToolbarButton::createBaseWidget(Widget			parent,
									const String	name)
{
	XP_ASSERT( XfeIsAlive(parent) );
	XP_ASSERT( name != NULL );

	Widget button;

	button = XtVaCreateWidget(name,
							  xfeButtonWidgetClass,
							  parent,
							  NULL);

	return button;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Configure
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarButton::configure()
{
	XP_ASSERT( isAlive() );

	// TODO, all the toolbar item resizing magic
	// XtVaSetValues(m_widget,XmNforceDimensionToMax,False,NULL);

    // Set the item's label
    XFE_RDFUtils::setItemLabelString(getAncestorContext(),
									 m_widget,
									 getHtResource());

	// Set the item's style and layout
    int32			button_style = getButtonStyle();
	unsigned char	button_layout = styleToLayout(button_style);

    if (button_style == BROWSER_TOOLBAR_TEXT_ONLY)
	{
		XtVaSetValues(m_widget,
					  XmNpixmap,			XmUNSPECIFIED_PIXMAP,
					  XmNpixmapMask,		XmUNSPECIFIED_PIXMAP,
					  NULL);

		XtVaSetValues(m_widget, XmNbuttonLayout, button_layout, NULL);
	}
	else
	{
		Pixmap pixmap;
		Pixmap pixmapMask;

		getPixmapsForEntry(&pixmap,&pixmapMask,NULL,NULL);

        XtVaSetValues(m_widget,
                      XmNpixmap,		pixmap,
                      XmNpixmapMask,	pixmapMask,
                      XmNbuttonLayout,	button_layout,
                      NULL);
	}

}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// addCallbacks
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarButton::addCallbacks()
{
	XP_ASSERT( isAlive() );

	// Add activate callback
    XtAddCallback(m_widget,
				  XmNactivateCallback,
				  XFE_ToolbarButton::activateCB,
				  (XtPointer) this);

	// Add popup callback
	XtAddCallback(m_widget,
				  XmNbutton3DownCallback,
				  XFE_ToolbarButton::popupCB,
				  (XtPointer) this);
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Style and layout interface
//
//////////////////////////////////////////////////////////////////////////
int32
XFE_ToolbarButton::getButtonStyle()
{
	int32			button_style = BROWSER_TOOLBAR_TEXT_ONLY;
    void *			data = NULL;
	HT_Resource		entry = getHtResource();

	XP_ASSERT( entry != NULL ) ;

    // Get the Toolbar displaymode from HT
    HT_GetTemplateData(HT_TopNode(HT_GetView(entry)), 
					   gNavCenter->toolbarDisplayMode, 
					   HT_COLUMN_STRING, 
					   &data);

	// No value provided initially. So get it from prefs and set in HT
    if (!data)
    {
		/* int result = */ PREF_GetIntPref("browser.chrome.toolbar_style", 
									 &button_style);
		
       if (button_style == BROWSER_TOOLBAR_TEXT_ONLY)
	   {
		   HT_SetNodeData(HT_TopNode(HT_GetView(entry)), 
						  gNavCenter->toolbarDisplayMode, 
						  HT_COLUMN_STRING, 
						  "text");
	   }
       else if (button_style == BROWSER_TOOLBAR_ICONS_ONLY)
	   {
		   HT_SetNodeData(HT_TopNode(HT_GetView(entry)), 
						  gNavCenter->toolbarDisplayMode, 
						  HT_COLUMN_STRING, 
						  "pictures");
	   }
       else
	   {
		   HT_SetNodeData(HT_TopNode(HT_GetView(entry)), 
						  gNavCenter->toolbarDisplayMode, 
						  HT_COLUMN_STRING, 
						  "PicturesAndText");
	   }
    }
	// Value is found in HT
    else 
	{
		char * answer = (char *) data;
      
		if ((!XP_STRCASECMP(answer, "text")))
		{ 
			button_style = BROWSER_TOOLBAR_TEXT_ONLY;
		}
		else if ((!XP_STRCASECMP(answer, "icons")))
		{
			button_style = BROWSER_TOOLBAR_ICONS_ONLY;
		}
		else
		{
			button_style = BROWSER_TOOLBAR_ICONS_AND_TEXT;
		}
    }

	return button_style;
}
//////////////////////////////////////////////////////////////////////////
unsigned char
XFE_ToolbarButton::styleToLayout(int32 button_style)
{
	unsigned char		button_layout = XmBUTTON_LABEL_ONLY;
    void *				data = NULL;
	HT_Resource			entry = getHtResource();

	XP_ASSERT( entry != NULL ) ;

    if (button_style == BROWSER_TOOLBAR_ICONS_AND_TEXT) 
	{
		// Get the Toolbar bitmap position from HT */
		HT_GetTemplateData(HT_TopNode(HT_GetView(entry)), 
						   gNavCenter->toolbarBitmapPosition, 
						   HT_COLUMN_STRING, 
						   &data);

		if (data)
		{
			char * answer = (char *) data;

			if ((!XP_STRCASECMP(answer, "top")))
			{ 
				button_layout = XmBUTTON_LABEL_ON_BOTTOM;
			}
			else if ((!XP_STRCASECMP(answer, "side")))
			{
				button_layout = XmBUTTON_LABEL_ON_RIGHT;
			}
		}
		// Value not provided. It is top for command buttons and side 
		// for personal
		else 
		{
			if (XFE_RDFUtils::ht_IsFECommand(entry))
			{
				button_layout = XmBUTTON_LABEL_ON_BOTTOM;
			}
			else
			{
				button_layout = XmBUTTON_LABEL_ON_RIGHT;
			}
		}
	}
    else if (button_style == BROWSER_TOOLBAR_ICONS_ONLY)
    {
		button_layout = XmBUTTON_PIXMAP_ONLY;
    } 
    else if (button_style == BROWSER_TOOLBAR_TEXT_ONLY)
    {
		button_layout = XmBUTTON_LABEL_ONLY;
    }

	return button_layout;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_ToolbarButton::getPixmapsForEntry(Pixmap *    pixmapOut,
									  Pixmap *    maskOut,
									  Pixmap *    armedPixmapOut,
									  Pixmap *    armedMaskOut)
{
    IconGroup *		ig = NULL;
	HT_Resource		entry = getHtResource();

    // Right now the only way an entry can be NULL is for the 
    // bookmarkMoreButton which kinda looks and acts like a folder so
    // we use folder pixmaps for it
    if (!entry)
    {
        ig = &BM_Folder_group;
    }
    else
    {
#ifdef NOT_YET
        if (hasCustomIcon(entry)) {} /*else {*/
#endif /*NOT_YET*/
        if (XFE_RDFUtils::ht_IsFECommand(entry))
        {
            const char* url = HT_GetNodeURL(entry);
            
            ig = IconGroup_findGroupForName(url + 8);
        } 
        else
        {
            if (HT_IsContainer(entry))
            {
                XP_Bool is_menu = False;//(entry == getMenuFolder());
                XP_Bool is_add  = False;//(entry == getAddFolder());

            
                if (is_add && is_menu)     ig = &BM_NewAndMenuFolder_group;
                else if (is_add)           ig = &BM_NewFolder_group;
                else if (is_menu)          ig = &BM_MenuFolder_group;
                else                       ig = &BM_Folder_group;
            }
            else
            {
                int url_type = NET_URL_Type(HT_GetNodeURL(entry));
                
                if (url_type == IMAP_TYPE_URL || url_type == MAILBOX_TYPE_URL)
                    ig = &BM_MailBookmark_group;
                else if (url_type == NEWS_TYPE_URL)
                    ig = &BM_NewsBookmark_group;
                else
                    ig = &BM_Bookmark_group;
            }
        }
    }

    Pixmap pixmap          = XmUNSPECIFIED_PIXMAP;
    Pixmap mask            = XmUNSPECIFIED_PIXMAP;
    Pixmap armedPixmap     = XmUNSPECIFIED_PIXMAP;
    Pixmap armedMask       = XmUNSPECIFIED_PIXMAP;

    if (ig)
    {
		Widget ancestor_shell = getAncestorFrame()->getBaseWidget();

        IconGroup_createAllIcons(ig, 
                                 ancestor_shell,
                                 XfeForeground(ancestor_shell),
                                 XfeBackground(ancestor_shell));
        
        pixmap        = ig->pixmap_icon.pixmap;
        mask          = ig->pixmap_icon.mask;
        armedPixmap   = ig->pixmap_mo_icon.pixmap;
        armedMask     = ig->pixmap_mo_icon.mask;
    }

    if (pixmapOut)
    {
        *pixmapOut = pixmap;
    }

    if (maskOut)
    {
        *maskOut = mask;
    }

    if (armedPixmapOut)
    {
        *armedPixmapOut = armedPixmap;
    }

    if (armedMaskOut)
    {
        *armedMaskOut = armedMask;
    }

}

//////////////////////////////////////////////////////////////////////////
//
// Button callback interface
//
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarButton::activate()
{
	HT_Resource		entry = getHtResource();

	XP_ASSERT( entry != NULL );

	// Check for xfe commands
	if (XFE_RDFUtils::ht_IsFECommand(entry)) 
	{
		// Convert the entry name to a xfe command
		CommandType cmd = XFE_RDFUtils::ht_GetFECommand(entry);

		XP_ASSERT( cmd != NULL );

		// Fill in the command info
		XFE_CommandInfo info(XFE_COMMAND_EVENT_ACTION,
							 m_widget, 
							 NULL /*event*/,
							 NULL /*params*/,
							 0    /*num_params*/);

		XFE_Frame * frame = getAncestorFrame();

		XP_ASSERT( frame != NULL );
		
		// If the frame handles the command and its enabled, execute it.
		if (frame->handlesCommand(cmd,NULL,&info) &&
			frame->isCommandEnabled(cmd,NULL,&info))
		{
			xfe_ExecuteCommand(frame,cmd,NULL,&info);
			
			//_frame->notifyInterested(Command::commandDispatchedCallback, 
			//callData);
		}
	}
	// Other URLs
	else if (!HT_IsContainer(entry))
	{
		char *			address = HT_GetNodeURL(entry);
		URL_Struct *	url = NET_CreateURLStruct(address,NET_DONT_RELOAD);
		MWContext *		context = getAncestorContext();
		
		XP_ASSERT( context != NULL );
		
		fe_reuseBrowser(context,url);
	}
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarButton::popup()
{
	printf("popup(%s) - Write Me Please.\n",XtName(m_widget));
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Private callbacks
//
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_ToolbarButton::activateCB(Widget		/* w */,
							  XtPointer		clientData,
							  XtPointer		/* callData */)
{
	XFE_ToolbarButton * button = (XFE_ToolbarButton *) clientData;

	XP_ASSERT( button != NULL );

	button->activate();
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_ToolbarButton::popupCB(Widget			/* w */,
						   XtPointer		clientData,
						   XtPointer		/* callData */)
{
	XFE_ToolbarButton * button = (XFE_ToolbarButton *) clientData;

	XP_ASSERT( button != NULL );

	button->popup();
}
//////////////////////////////////////////////////////////////////////////
