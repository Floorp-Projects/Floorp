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
/*---------------------------------------*/
/*																		*/
/* Name:		PersonalToolbar.cpp										*/
/* Description:	XFE_PersonalToolbar component source.					*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/



#include "structs.h"
#include "xfe.h"
#include "xpassert.h"
#include "bkmks.h"
#include "felocale.h"
#include "intl_csi.h"
#include "prefapi.h"
#include "View.h"
#include "PersonalToolbar.h"
#include "MozillaApp.h"
#include "BookmarkFrame.h"
#include "BookmarkMenu.h"
#include "IconGroup.h"
#include "ToolbarDrop.h"
#include "Logo.h"
#include "PopupMenu.h"
#include "prefapi.h"

#include <Xfe/ToolItem.h>
#include <Xfe/ToolBar.h>

#define DEFAULT_TOOLBAR_FOLDER_NAME		"Personal Toolbar Folder"
#define MIN_TOOLBAR_HEIGHT				26
#define MAX_CHILD_WIDTH					100
#define PERSONAL_TOOLBAR_NAME			"personalToolbar"
#define LOGO_NAME						"logo"

/* static */ MenuSpec 
XFE_PersonalToolbar::m_popupMenuSpec[] = 
{
	{ xfeCmdPersonalToolbarAddBookmark,		PUSHBUTTON },
	{ xfeCmdPersonalToolbarEditItem,		PUSHBUTTON },
	{ xfeCmdPersonalToolbarRemoveItem,		PUSHBUTTON },
	MENU_SEPARATOR,
	{ xfeCmdPersonalToolbarItemProperties,	PUSHBUTTON },
	{ NULL }
};

//////////////////////////////////////////////////////////////////////////
XFE_PersonalToolbar::XFE_PersonalToolbar(MWContext *	bookmarkContext,
										 XFE_Toolbox *	parent_toolbox,
										 const char *	name,
										 XFE_Frame *	frame) :
	XFE_ToolboxItem(frame,parent_toolbox),
	XFE_BookmarkBase(bookmarkContext,frame,False,True),
	m_toolBarFolder(NULL),
	m_dropTargetItem(NULL),
	m_dropTargetLocation(XmINDICATOR_LOCATION_NONE),
	m_popup(NULL)
{
	XP_ASSERT( name != NULL );

	XP_ASSERT( getBookmarkContext() != NULL );

	// Obtain the toolbar folder
	m_toolBarFolder = XFE_PersonalToolbar::getToolbarFolder();

	// Create the base widget - a tool item
	m_widget = XtVaCreateWidget(name,
								xfeToolItemWidgetClass,
								parent_toolbox->getBaseWidget(),
								XmNuserData,			this,
								NULL);
	// Create the toolbar
	m_toolBar = 
		XtVaCreateManagedWidget(PERSONAL_TOOLBAR_NAME,
								xfeToolBarWidgetClass,
								m_widget,
								XmNusePreferredWidth,		False,
								XmNusePreferredHeight,		True,
								XmNchildForceWidth,			False,
								XmNchildForceHeight,		True,
								XmNchildUsePreferredWidth,	True,
								XmNchildUsePreferredHeight,	False,
//								XmNminHeight,				MIN_TOOLBAR_HEIGHT,
								NULL);
	
	// Create the logo
	m_logo = new XFE_Logo(getFrame(),m_widget,LOGO_NAME);

	m_logo->setSize(XFE_ANIMATION_SMALL);

	// Attach and configure the logo
	configureLogo();

	// Update the appearace for the first time
	updateAppearance();

	// Make sure the toolbar is not highlighted first
	setRaised(False);
	
	// register and enable the drop site
	m_dropSite = new XFE_PersonalDrop(m_toolBar,this);
	m_dropSite->enable();

	// Configure the drop site
	Arg			xargs[1];
	Cardinal	n = 0;

	XtSetArg(xargs[n],XmNanimationStyle,	XmDRAG_UNDER_NONE);		n++;
	
	m_dropSite->update(xargs,n);

	// Register personal toolbar widgets for dragging
	addDragWidget(getBaseWidget());
	addDragWidget(getToolBarWidget());
  
	// Install the destruction handler
	installDestroyHandler();

	// Force the items to update
	reallyUpdateRoot();

	// If we are supposed to have a toolbar folder, but it does not
	// exist, then we need to create one and populate it with the default
	// crap from marketing
	if (XFE_PersonalToolbar::hasToolbarFolder() && !m_toolBarFolder)
	{
		addDefaultToolbarFolder();

		XP_ASSERT( isToolbarFolderValid() );

		if (isToolbarFolderValid())
		{
			addDefaultPersonalEntries("personal_toolbar",m_toolBarFolder);
		}
	}

	// Add popup callback to toolbar
	XtAddCallback(m_toolBar,
				  XmNbutton3DownCallback,
				  &XFE_PersonalToolbar::popupCB,
				  (XtPointer) this);
}
//////////////////////////////////////////////////////////////////////////
XFE_PersonalToolbar::~XFE_PersonalToolbar()
{
	if (m_dropSite)
	{
		delete m_dropSite;
	}

	if (m_popup)
	{
		delete m_popup;
	}
}
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_PersonalToolbar::prepareToUpdateRoot()
{
	// Force the items to update
	reallyUpdateRoot();
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_PersonalToolbar::reallyUpdateRoot()
{
	BM_Entry * newFolder = XFE_PersonalToolbar::getToolbarFolder();

	// Check for changes in the toolbar folder
	if (m_toolBarFolder != newFolder)
	{
#ifdef DEBUG_slamm
      XP_ASSERT(newFolder);
#endif
      if (newFolder)
        XFE_PersonalToolbar::setToolbarFolder(newFolder,False);  
      
      m_toolBarFolder = newFolder;
	}

	// Since a lot of changes will happen to the toolbar, we tell it to 
	// any geometry or layout changes.  We will force these to occur later.
	XtVaSetValues(m_toolBar,XmNignoreConfigure,True,NULL);

	// Destroy the current widgets
	destroyToolbarWidgets();

	// If the toolbar folder is valid, populate the toolbar
	if (isToolbarFolderValid())
	{
		// Access the first entry in the personal toolbar folder
		BM_Entry * entry = BM_GetChildren(m_toolBarFolder);
		
		while (entry)
		{
			Widget item = NULL;
			
			// Headers
			if (BM_IsHeader(entry))
			{
				item = createXfeCascade(m_toolBar,entry);
			}
			// Separators
			else if (BM_IsSeparator(entry))
			{
				item = createSeparator(m_toolBar);
			}
			// Normal items
			else
			{
				item = createXfeButton(m_toolBar,entry);
			}
			
			XP_ASSERT( XfeIsAlive(item) );
			
			XtManageChild(item);
			
			entry = BM_GetNext(entry);
		}
	}

	// Tell the toolbar not too ignore geometry or layout changes anymore
	XtVaSetValues(m_toolBar,XmNignoreConfigure,False,NULL);

	// Force the toolbar to reconfigure.
	XfeManagerLayout(m_toolBar);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_PersonalToolbar::configureXfeButton(Widget item,BM_Entry *entry)
{
	if (fe_globalPrefs.toolbar_style == BROWSER_TOOLBAR_TEXT_ONLY)
	{
		XtVaSetValues(item,
					  XmNpixmap,			XmUNSPECIFIED_PIXMAP,
					  XmNpixmapMask,		XmUNSPECIFIED_PIXMAP,
					  NULL);

		XtVaSetValues(item,XmNbuttonLayout,XmBUTTON_LABEL_ONLY,NULL);
	}
	else
	{
		Pixmap pixmap;
		Pixmap pixmapMask;

		getPixmapsForEntry(entry,&pixmap,&pixmapMask,NULL,NULL);
		
		XtVaSetValues(item,
					  XmNpixmap,		pixmap,
					  XmNpixmapMask,	pixmapMask,
					  XmNbuttonLayout,	XmBUTTON_LABEL_ON_RIGHT,
					  NULL);
	}

	// Add popup callback to item
	XtAddCallback(item,
				  XmNbutton3DownCallback,
				  &XFE_PersonalToolbar::popupCB,
				  (XtPointer) this);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_PersonalToolbar::configureXfeCascade(Widget item,BM_Entry * entry)
{
	if (fe_globalPrefs.toolbar_style == BROWSER_TOOLBAR_TEXT_ONLY)
	{
		XtVaSetValues(item,
					  XmNpixmap,			XmUNSPECIFIED_PIXMAP,
					  XmNarmedPixmap,		XmUNSPECIFIED_PIXMAP,
					  XmNpixmapMask,		XmUNSPECIFIED_PIXMAP,
					  XmNarmedPixmapMask,	XmUNSPECIFIED_PIXMAP,
					  NULL);

//		XtVaSetValues(item,XmNbuttonLayout,XmBUTTON_LABEL_ONLY,NULL);
	}
	else
	{
		Pixmap		pixmap;
		Pixmap		armedPixmap;
		Pixmap		pixmapMask;
		Pixmap		armedPixmapMask;
		Arg			av[4];
		Cardinal	ac = 0;

		getPixmapsForEntry(entry,&pixmap,&pixmapMask,
						   &armedPixmap,&armedPixmapMask);


		XtSetArg(av[ac],XmNpixmap,			pixmap); ac++;
		XtSetArg(av[ac],XmNpixmapMask,		pixmapMask); ac++;

		// Only show the aremd pixmap/mask if this entry has children
		if (XfeIsAlive(XfeCascadeGetSubMenu(item)))
		{
			XtSetArg(av[ac],XmNarmedPixmap,		armedPixmap); ac++;
			XtSetArg(av[ac],XmNarmedPixmapMask,	armedPixmapMask); ac++;
		}

		XtSetValues(item,av,ac);

//		XtVaSetValues(item,XmNbuttonLayout,XmBUTTON_LABEL_ON_RIGHT,NULL);
	}

	// Add popup callback to item
	XtAddCallback(item,
				  XmNbutton3DownCallback,
				  &XFE_PersonalToolbar::popupCB,
				  (XtPointer) this);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_PersonalToolbar::updateAppearance()
{
	if (fe_globalPrefs.toolbar_style == BROWSER_TOOLBAR_TEXT_ONLY)
	{
		XtVaSetValues(m_toolBar,XmNbuttonLayout,XmBUTTON_LABEL_ONLY,NULL);
	}
	else
	{
		XtVaSetValues(m_toolBar,XmNbuttonLayout,XmBUTTON_LABEL_ON_RIGHT,NULL);
	}

	reallyUpdateRoot();
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_PersonalToolbar::updateToolbarFolderName()
{
	// Access the new toolbar folder 
	BM_Entry * newFolder = XFE_PersonalToolbar::getToolbarFolder();

	// Make sure the folder has indeed changed
	if (m_toolBarFolder != newFolder)
	{
		// Assign the new toolbar folder
		m_toolBarFolder = newFolder;
		
		// Update
		reallyUpdateRoot();
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_PersonalToolbar::addDefaultPersonalEntries(char *		configString,
											   BM_Entry *	header)
{
	int			i = 0;
	XP_Bool		ok = PREF_NOERROR;
	char *		label;
	char *		url;
	BM_Entry *	entry;
	char		curConfigString[256];

	XP_SPRINTF(curConfigString,"%s.item",configString);

    for (i=0; ok==PREF_NOERROR; i++)
	{
		label = url = NULL;

		ok = PREF_CopyIndexConfigString(curConfigString,i,"label",&label);

		if (ok == PREF_NOERROR)
		{
			ok = PREF_CopyIndexConfigString(curConfigString,i,"url",&url);
			if(ok == PREF_NOERROR)
			{	
				// if it's a folder we need to first fill it in
				if(XP_STRCMP(url, "FOLDER")== 0)
				{
					entry = BM_NewHeader(label);

					char buf[256];

					XP_SPRINTF(buf,"%s_%d",curConfigString,i);

					addDefaultPersonalEntries(buf,entry);
				}
				// otherwise just create a new url
				else
				{
					entry = BM_NewUrl(label,url,NULL,0);
				}

				//add the new entry
				BM_AppendToHeader(getBookmarkContext(),header,entry);
			}

		}

		if(label)
		{
			XP_FREE(label);
		}

		if(url)
		{
			XP_FREE(url);
		}
	}
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_PersonalToolbar::getToolBarWidget()
{
	return m_toolBar;
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_PersonalToolbar::getLastItem()
{
	return XfeToolBarGetLastItem(m_toolBar);
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_PersonalToolbar::getIndicatorItem()
{
	return XfeToolBarGetIndicatorItem(m_toolBar);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_PersonalToolbar::setRaised(XP_Bool state)
{
	XP_ASSERT( XfeIsAlive(m_toolBar) );

	XtVaSetValues(m_toolBar,XmNraised,state,NULL);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_PersonalToolbar::addEntry(const char *	address,
							  const char *	title,
							  BM_Date		lastAccess)
{
	XP_ASSERT( address != NULL );
	XP_ASSERT( title != NULL );

	// If there is no personal toolbar folder add a default one
	if (!m_toolBarFolder)
	{
		addDefaultToolbarFolder();
	}

 	XP_ASSERT( isToolbarFolderValid() );

	if (!isToolbarFolderValid())
	{
		return;
	}

	BM_Entry * entry = BM_NewUrl(title,address,NULL,lastAccess);

	BM_AppendToHeader(getBookmarkContext(),m_toolBarFolder,entry);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_PersonalToolbar::destroyToolbarWidgets()
{
	XP_ASSERT( XfeIsAlive(m_toolBar) );

 	WidgetList		children;
 	Cardinal		num_children;

 	XfeChildrenGet(m_toolBar,&children,&num_children);	

 	// Get rid of the previous items we created
 	if (children && num_children)
	{
		XtUnmanageChildren(children,num_children);
      
		XfeDestroyMenuWidgetTree(children,num_children,True);
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_PersonalToolbar::addDefaultToolbarFolder()
{
	// First, look for a folder called "Personal Toolbar Folder"
	BM_Entry * folder = getTopLevelFolder(DEFAULT_TOOLBAR_FOLDER_NAME);

	if (!folder)
	{
		folder = BM_NewHeader(DEFAULT_TOOLBAR_FOLDER_NAME);
			
		BM_AppendToHeader(getBookmarkContext(),
						  getRootFolder(),
//						  BM_GetAddHeader(getBookmarkContext()),
						  folder);
	}

	XFE_PersonalToolbar::setToolbarFolder(folder,True);
}
//////////////////////////////////////////////////////////////////////////
XP_Bool
XFE_PersonalToolbar::isToolbarFolderValid()
{
 	return ((m_toolBarFolder != NULL) && BM_IsHeader(m_toolBarFolder));
}
//////////////////////////////////////////////////////////////////////////

/* static */ void
XFE_PersonalToolbar::setToolbarFolder(BM_Entry * entry,XP_Bool notify)
{
	char *	name = "";
	XP_Bool	has_folder = False;

	// Assign the name if the entry is valid
	if (entry && BM_IsHeader(entry))
	{
		name = BM_GetName(entry);
		has_folder = True;
	}

	// Set the preferences
    StrAllocCopy(fe_globalPrefs.personal_toolbar_folder, name);
    fe_globalPrefs.has_toolbar_folder = has_folder;

	// Notify mozilla app that the personal toolbar folder has changed.
	if (notify)
	{
		XFE_MozillaApp::theApp()->notifyInterested(
			XFE_MozillaApp::personalToolbarFolderChanged);
	}
}
//////////////////////////////////////////////////////////////////////////
/* static */ BM_Entry *
XFE_PersonalToolbar::getToolbarFolder()
{
	BM_Entry * result = NULL;

	if (!XFE_BookmarkFrame::main_bm_context)
	{
		return NULL;
	}

	// Access the toolbar folder name
	char * name = XFE_PersonalToolbar::getToolbarFolderName();

	// Try to find the folder to match the name
	if (name)
	{
		BM_Entry * root = BM_GetRoot(XFE_BookmarkFrame::main_bm_context);
		
		XP_ASSERT( root != NULL );
		
		if (root)
		{
			result = XFE_BookmarkBase::BM_FindFolderByName(root,name);
		}
	}

	return result;
}
//////////////////////////////////////////////////////////////////////////
/* static */ XP_Bool
XFE_PersonalToolbar::hasToolbarFolder()
{
 	// Find out if we do have a toolbar folder
    return fe_globalPrefs.has_toolbar_folder;
}
//////////////////////////////////////////////////////////////////////////
/* static */ char *
XFE_PersonalToolbar::getToolbarFolderName()
{
	// Make sure the main bookmark context is valid
	if (!XFE_BookmarkFrame::main_bm_context)
	{
		return NULL;
	}

	// Make sure we have a toolbar folder defined
	if (!XFE_PersonalToolbar::hasToolbarFolder())
	{
		return NULL;
	}

	// Find out the name of the toolbar folder
    return fe_globalPrefs.personal_toolbar_folder;
}
//////////////////////////////////////////////////////////////////////////


//
// DND feedback methods
//
Widget
XFE_PersonalToolbar::getDropTargetItem()
{
	return m_dropTargetItem;
}
//////////////////////////////////////////////////////////////////////////
unsigned char
XFE_PersonalToolbar::getDropTargetLocation()
{
	return m_dropTargetLocation;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_PersonalToolbar::setDropTargetItem(Widget item,int x)
{
	assert( XfeIsAlive(item) );

	m_dropTargetItem = item;
	
	m_dropTargetLocation = XfeToolBarXYToIndicatorLocation(m_toolBar,
														   m_dropTargetItem,
														   x,0);
	
	int position = XfeChildGetIndex(m_dropTargetItem);

	XtVaSetValues(m_toolBar,
				  XmNindicatorPosition,		position,
				  XmNindicatorLocation,		m_dropTargetLocation,
				  NULL);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_PersonalToolbar::clearDropTargetItem()
{
	m_dropTargetItem = NULL;

	XtVaSetValues(m_toolBar,
				  XmNindicatorPosition,		XmINDICATOR_DONT_SHOW,
				  NULL);
}
//////////////////////////////////////////////////////////////////////////


//
// Popup menu stuff
//
/* static */ void
XFE_PersonalToolbar::popupCB(Widget		w,
							 XtPointer	clientData,
							 XtPointer	callData)
{
	XFE_PersonalToolbar *	pt = (XFE_PersonalToolbar *) clientData;
	XmAnyCallbackStruct *	cbs = (XmAnyCallbackStruct *) callData;
	XEvent *				event = cbs->event;

	pt->handlePopup(w,event);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_PersonalToolbar::handlePopup(Widget w,XEvent * event)
{
	if (!m_popup)
	{
		m_popup = new XFE_PopupMenu("popup",getFrame(),FE_GetToplevelWidget());
		
		m_popup->setMenuSpec(XFE_PersonalToolbar::m_popupMenuSpec);
	}

	if (XfeIsButton(w))
	{
		printf("Button Popup(%s)\n",XtName(w));
	}
	else if (XfeIsToolBar(w))
	{
		printf("ToolBar Popup(%s)\n",XtName(w));
	}

	m_popup->position(event);
	m_popup->show();

//	BM_Entry * entry;
}
//////////////////////////////////////////////////////////////////////////

