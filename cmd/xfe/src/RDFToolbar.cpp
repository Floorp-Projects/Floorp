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
/* 
   RDFToolbar.cpp - Toolbars supplied by RDF
   Created: Stephen Lamm <slamm@netscape.com>, 13-Aug-1998
 */

#include "RDFToolbar.h"
#include "Logo.h"

#include <Xfe/ToolItem.h>
#include <Xfe/ToolBar.h>

#if DEBUG_slamm
#define D(x) x
#else
#define D(x)
#endif

#define MIN_TOOLBAR_HEIGHT				26
#define MAX_CHILD_WIDTH					100
#define LOGO_NAME						"logo"
#define TOOLBAR_NAME					"toolbar"

XFE_RDFToolbar::XFE_RDFToolbar(XFE_Frame * frame, 
                               XFE_Toolbox * toolbox,
                               HT_View view)
    : XFE_ToolboxItem(frame, toolbox),
      XFE_RDFMenuToolbarBase(frame, FALSE /*only headers*/, TRUE /*fancy*/),
      _frame(frame)
{
	m_widget = XtVaCreateWidget(HT_GetViewName(view),
                   xfeToolItemWidgetClass,
                   toolbox->getBaseWidget(),
                   XmNuserData, this,
                   NULL);

	// Create the toolbar
	_toolbar = XtVaCreateManagedWidget(TOOLBAR_NAME,
                   xfeToolBarWidgetClass,
                   m_widget,
                   XmNusePreferredWidth,         False,
                   XmNusePreferredHeight,        True,
                   XmNchildForceWidth,           False,
                   XmNchildForceHeight,          True,
                   XmNchildUsePreferredWidth,    True,
                   XmNchildUsePreferredHeight,   False,
                   NULL);

	// Create the logo
	m_logo = new XFE_Logo(getFrame(),m_widget,LOGO_NAME);

	m_logo->setSize(XFE_ANIMATION_SMALL);

	// Attach and configure the logo
	configureLogo();

	// Make sure the toolbar is not highlighted first
	setRaised(False);

	// Install the destruction handler
	installDestroyHandler();

    setHTView(view);

    show();

#ifdef NOT_YET
	_frame->registerInterest(XFE_View::chromeNeedsUpdating,
                             this,
                             (XFE_FunctionNotification)update_cb);

	_frame->registerInterest(XFE_View::commandNeedsUpdating,
                             this,
                             (XFE_FunctionNotification)updateCommand_cb);

    XFE_MozillaApp::theApp()->registerInterest(XFE_View::commandNeedsUpdating,
											   this,
											   (XFE_FunctionNotification)updateCommand_cb);

    XFE_MozillaApp::theApp()->registerInterest(XFE_MozillaApp::updateToolbarAppearance,
                                               this,
                                               (XFE_FunctionNotification)updateToolbarAppearance_cb);
#endif
}


XFE_RDFToolbar::~XFE_RDFToolbar() 
{
#ifdef NOT_YET
	_frame->unregisterInterest(XFE_View::chromeNeedsUpdating,
                               this,
                               (XFE_FunctionNotification)update_cb);

	_frame->unregisterInterest(XFE_View::commandNeedsUpdating,
                               this,
                               (XFE_FunctionNotification)updateCommand_cb);

    XFE_MozillaApp::theApp()->unregisterInterest(XFE_View::commandNeedsUpdating,
												 this,
												 (XFE_FunctionNotification)updateCommand_cb);

    XFE_MozillaApp::theApp()->unregisterInterest(XFE_MozillaApp::updateToolbarAppearance,
                                                 this,
                                                 (XFE_FunctionNotification)updateToolbarAppearance_cb);
#endif
}
#ifdef NOT_YET
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_RDFToolbar, updateCommand)(XFE_NotificationCenter */*obj*/, 
					      void */*clientData*/, 
					      void *callData)
{
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_RDFToolbar, update)(XFE_NotificationCenter */*obj*/, 
									   void */*clientData*/, 
									   void */*callData*/)
{
  update();
}
//////////////////////////////////////////////////////////////////////////
XFE_CALLBACK_DEFN(XFE_RDFToolbar, updateToolbarAppearance)(XFE_NotificationCenter */*obj*/, 
									   void */*clientData*/, 
									   void */*callData*/)
{
	updateAppearance();
}
#endif /*NOT_YET*/
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFToolbar::update()
{
#ifdef NOT_YET
	// Make sure the toolbar is alive
    if (!XfeIsAlive(_toolbar))
    {
        return;
    }
  
	Widget *		children;
	Cardinal		num_children;
	Cardinal		i;

	XfeChildrenGet(_toolbar,&children,&num_children);

	for (i = 0; i < num_children; i ++)
	{
		if (XfeIsButton(children[i]))
        {
            
        }

    }
#endif /*NOT_YET*/
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFToolbar::notify(HT_Resource n, HT_Event whatHappened)
{
  D(debugEvent(n, whatHappened,"Toolbar"););
  switch (whatHappened) {
  case HT_EVENT_NODE_ADDED:
      addItem(n);
      break;
  default:
      // Fall through and let the base class handle this.
    break;
  }
  XFE_RDFMenuToolbarBase::notify(n, whatHappened);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFToolbar::setRaised(XP_Bool state)
{
	XP_ASSERT( XfeIsAlive(_toolbar) );

	XtVaSetValues(_toolbar,XmNraised,state,NULL);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFToolbar::destroyToolbarWidgets()
{
	XP_ASSERT( XfeIsAlive(_toolbar) );

 	WidgetList		children;
 	Cardinal		num_children;

 	XfeChildrenGet(_toolbar,&children,&num_children);	

 	// Get rid of the previous items we created
 	if (children && num_children)
	{
		XtUnmanageChildren(children,num_children);
      
		XfeDestroyMenuWidgetTree(children,num_children,True);
	}
}
//////////////////////////////////////////////////////////////////////////
XP_Bool
XFE_RDFToolbar::isToolbarFolderValid()
{
	HT_Resource toolbarRoot = getRootFolder();
 	return ((toolbarRoot != NULL) && HT_IsContainer(toolbarRoot));
}
//////////////////////////////////////////////////////////////////////////
void
XFE_RDFToolbar::addItem(HT_Resource node)
{
    Widget item = NULL;

    HT_Resource   toolbarRoot   = getRootFolder();
    HT_Resource   parent        = HT_GetParent(node);

    // This node doesn't belong here.
    if (toolbarRoot != parent) return;
        
    // Headers
    if (HT_IsContainer(node))
    {
        item = createXfeCascade(_toolbar, node);
    }
    // Separators
    else if (HT_IsSeparator(node))
    {
        item = createSeparator(_toolbar);
    }
    // Normal items
    else
    {
        item = createXfeButton(_toolbar, node);
    }
    
    XP_ASSERT( XfeIsAlive(item) );
    
    XtManageChild(item);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFToolbar::prepareToUpdateRoot()
{
	// Force the items to update
	updateRoot();
}
//////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFToolbar::updateRoot()
{
	// Since a lot of changes will happen to the toolbar, we tell it to 
	// any geometry or layout changes.  We will force these to occur later.
	XtVaSetValues(_toolbar,XmNignoreConfigure,True,NULL);

	// Destroy the current widgets
	destroyToolbarWidgets();

	// If the toolbar folder is valid, populate the toolbar
	if (isToolbarFolderValid())
	{
        HT_Resource toolbarRoot = getRootFolder();

        HT_SetOpenState(toolbarRoot, PR_TRUE);
        HT_Cursor child_cursor = HT_NewCursor(toolbarRoot);
        HT_Resource child;
		while ( (child = HT_GetNextItem(child_cursor)) )
		{
			addItem(child);
		}
	}

	// Tell the toolbar not too ignore geometry or layout changes anymore
	XtVaSetValues(_toolbar,XmNignoreConfigure,False,NULL);

	// Force the toolbar to reconfigure.
	XfeManagerLayout(_toolbar);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFToolbar::configureXfeButton(Widget item,HT_Resource entry)
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
		
        if (ht_IsFECommand(entry))
        {
            XtVaSetValues(item,
                          XmNpixmap,		pixmap,
                          XmNpixmapMask,	pixmapMask,
                          XmNbuttonLayout,	XmBUTTON_LABEL_ON_BOTTOM,
                          NULL);
        }
        else
        {
            XtVaSetValues(item,
                          XmNpixmap,		pixmap,
                          XmNpixmapMask,	pixmapMask,
                          XmNbuttonLayout,	XmBUTTON_LABEL_ON_RIGHT,
                          NULL);
        }
	}

#ifdef NOT_YET
	// Add popup callback to item
	XtAddCallback(item,
				  XmNbutton3DownCallback,
				  &XFE_RDFToolbar::popupCB,
				  (XtPointer) this);
#endif
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFToolbar::configureXfeCascade(Widget item,HT_Resource entry)
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

#ifdef NOT_YET
	// Add popup callback to item
	XtAddCallback(item,
				  XmNbutton3DownCallback,
				  &XFE_RDFToolbar::popupCB,
				  (XtPointer) this);
#endif
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_RDFToolbar::updateAppearance()
{
	if (fe_globalPrefs.toolbar_style == BROWSER_TOOLBAR_TEXT_ONLY)
	{
		XtVaSetValues(_toolbar,XmNbuttonLayout,XmBUTTON_LABEL_ONLY,NULL);
	}
	else
	{
		XtVaSetValues(_toolbar,XmNbuttonLayout,XmBUTTON_LABEL_ON_RIGHT,NULL);
	}

	updateRoot();
}
//////////////////////////////////////////////////////////////////////////
