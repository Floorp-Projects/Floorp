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
   URLBar.cpp -- The text field chrome above an HTMLView, used in
               BrowserFrames.
   Created: Chris Toshok <toshok@netscape.com>, 1-Nov-96.
   */


#include "mozilla.h"
#include "xfe.h"
#include "hk_funcs.h"
#include "URLBar.h"

#include "Button.h"
#include "ProxyIcon.h"
#include "BookmarkMenu.h"	  // Need for bookmark context.
#include "MozillaApp.h"
#include "Logo.h"
#include "prefapi.h"
#include "felocale.h"

#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/TextF.h>

#include "DtWidgets/ComboBox.h"

#include <Xfe/ToolItem.h>
#include <Xfe/FrameShell.h>

#include "libmocha.h"   // For LM_StripWysiwygURLPrefix().

#include <xpgetstr.h>

#ifdef URLBAR_POPUP
#include "View.h"
#endif /* URLBAR_POPUP */

extern int XFE_URLBAR_FILE_HEADER;

extern "C" void fe_NeutralizeFocus(MWContext *context);

#define BOOKMARK_BUTTON_NAME		"bookmarkQuickfile"
#define LOCATION_LABEL_NAME			"urlLocationLabel"
#define LOCATION_PROXY_ICON_NAME	"urlLocationProxyIcon"
#define URL_COMBO_BOX_NAME			"urlComboBox"
#define URL_BAR_NAME				"urlBarItem"
#define MAIN_FORM_NAME				"urlBarMainForm"
#define LOGO_NAME					"logo"

const char *XFE_URLBar::navigateToURL = "XFE_URLBar::navigateToURL";

// Quickfile Menu Spec
/* static */ MenuSpec
XFE_URLBar::quickfile_menu_spec[] = 
{
	{
		xfeCmdAddBookmark,		
		PUSHBUTTON 
	},
	{ 
		"fileBookmarksSubmenu",     
		DYNA_FANCY_CASCADEBUTTON, 
		NULL, 
		NULL, 
		False, 
		(void *) True,					// Only headers 
		XFE_BookmarkMenu::generate
	},
	{
		xfeCmdOpenBookmarks,	
		PUSHBUTTON 
	},
	MENU_SEPARATOR,
	{ 
		"bookmarkPlaceHolder",
		DYNA_MENUITEMS,
		NULL,
		NULL,
		False,
		(void *) False,					// Only headers
		XFE_BookmarkMenu::generateQuickfile
	},
	{
		NULL 
	}
};


#ifdef URLBAR_POPUP
MenuSpec XFE_URLBar::ccp_popup_spec[] = {
  //  { xfeCmdUndo,  PUSHBUTTON },
  //  { xfeCmdRedo,  PUSHBUTTON },
  //  MENU_SEPARATOR,
  { xfeCmdCut,   PUSHBUTTON },
  { xfeCmdCopy,  PUSHBUTTON },
  { xfeCmdPaste, PUSHBUTTON },
  //  { xfeCmdDeleteItem, PUSHBUTTON },
  MENU_SEPARATOR,
  { xfeCmdSelectAll, PUSHBUTTON },
  { NULL },
};
#endif /* URLBAR_POPUP */

XFE_URLBar::XFE_URLBar(XFE_Frame *		parent_frame,
					   XFE_Toolbox *	parent_toolbox,
					   History_entry *	first_entry) :
	XFE_ToolboxItem(parent_frame,parent_toolbox),
	
	m_editedLabelString(NULL),
	m_uneditedLabelString(NULL),
	m_netsiteLabelString(NULL),
	
	m_parentFrame(parent_frame),
	
	m_bookmarkButton(NULL),
	m_locationProxyIcon(NULL),
	m_locationLabel(NULL),
	m_urlComboBox(NULL),
	
	m_labelBeingFrobbed(False),
	
	m_proxyIconDragSite(NULL),

	m_bookmarkDropSite(NULL)
	
#ifdef URLBAR_POPUP
    ,m_popup(NULL)
#endif /* URLBAR_POPUP */
{
	Widget main_form;

	// Create the base widget - a tool item
	m_widget = XtVaCreateWidget(URL_BAR_NAME,
								xfeToolItemWidgetClass,
								parent_toolbox->getBaseWidget(),
								XmNuserData,			this,
								NULL);

	// Create the main form
	main_form = XtVaCreateManagedWidget(MAIN_FORM_NAME,
										xmFormWidgetClass,
										m_widget,
										NULL);
	
	// Create the bookmark button
	createBookmarkButton(main_form);
	
	XP_ASSERT( m_bookmarkButton != NULL );

	// Show the bookmark button
	m_bookmarkButton->show();
	
	// Create the proxy icon
	createProxyIcon(main_form);
	
	XP_ASSERT( m_locationProxyIcon != NULL );
	XP_ASSERT( XfeIsAlive(m_locationLabel) );

	// Show the proxy icon and label
	m_locationProxyIcon->show();
	
	XtManageChild(m_locationLabel);

	// Create the url text combo box
	createUrlComboBox(main_form,first_entry);
	
	XP_ASSERT( XfeIsAlive(m_urlComboBox) );

	// Create the logo
	// Create the logo
	m_logo = new XFE_Logo(m_parentFrame,m_widget,LOGO_NAME);

	m_logo->setSize(XFE_ANIMATION_SMALL);

	// Show the url text
	XtManageChild(m_urlComboBox);

	// Attach and configure the logo
	configureLogo();

	// Show the main form
	XtManageChild(main_form);

	// Register url bar widgets for dragging
	addDragWidget(getFormWidget());
	addDragWidget(getUrlLabel());

	installDestroyHandler();

#ifdef URLBAR_POPUP
    Widget text_field;
	XtVaGetValues(m_urlComboBox, XmNtextField, &text_field, 0);

	// Add popup menu post event handler to floating task bar items
	XtAddEventHandler(
                      text_field,
                      ButtonPressMask,
                      True,
                      &XFE_URLBar::comboBoxButtonPressEH,
                      (XtPointer *)this
                      );
#endif /* URLBAR_POPUP */
}
//////////////////////////////////////////////////////////////////////////
void
XFE_URLBar::createBookmarkButton(Widget parent)
{
	XP_ASSERT( XfeIsAlive(parent) );
	XP_ASSERT( m_bookmarkButton == NULL );

	// Bookmark quickfile button.
	m_bookmarkButton = new XFE_Button(m_parentFrame,
									  parent,
									  BOOKMARK_BUTTON_NAME,
									  XFE_URLBar::quickfile_menu_spec,
									  &BM_QFile_group);
	
	// Configure the bookmark button
	m_bookmarkButton->setPretendSensitive(True);

	// Place the bookmark quickfile to the far left
	XtVaSetValues(m_bookmarkButton->getBaseWidget(),
				  XmNleftAttachment,		XmATTACH_FORM,
				  XmNrightAttachment,		XmATTACH_NONE,
				  XmNtopAttachment,			XmATTACH_FORM,
				  XmNbottomAttachment,		XmATTACH_NONE,
				  NULL);

	// Add the submenu torn callbacks
    XtAddCallback(m_bookmarkButton->getBaseWidget(),
				  XmNsubmenuTearCallback,
				  &XFE_URLBar::bookmarkCascadeTearCB,
				  (XtPointer) this);

	// Keep track of changes in the top level shell title
	Widget frame_shell = m_parentFrame->getBaseWidget();

	if (XfeIsAlive(frame_shell) && XfeIsFrameShell(frame_shell))
	{
		Boolean allow_tear;

		XtVaGetValues(m_bookmarkButton->getBaseWidget(),
					  XmNallowTearOff,&allow_tear,
					  NULL);

		if (allow_tear)
		{
			XtAddCallback(frame_shell,
						  XmNtitleChangedCallback,
						  &XFE_URLBar::frameTitleChangedCB,
						  (XtPointer) this);
		}
	}
	
	// The XFE_BookmarkMenu class overrides the XmNinstancePointer which was
	// originally set in XFE_Button.  This is really horrible.
	XFE_BookmarkMenu * quickfileMenu = 
		(XFE_BookmarkMenu *) XfeInstancePointer(m_bookmarkButton->getBaseWidget());

	XP_ASSERT( quickfileMenu != NULL );

	// Create and enable the quickfile drop site
	m_bookmarkDropSite = new 
		XFE_QuickfileDrop(m_bookmarkButton->getBaseWidget(),quickfileMenu);

	m_bookmarkDropSite->enable();
}
//////////////////////////////////////////////////////////////////////////

// URL Label string resources ( for Location: Netsite: etc )
/* static */ XtResource
XFE_URLBar::_urlLabelResources[] = 
{
	{
		"editedLabelString",
		XmCXmString,
		XmRXmString,
		sizeof(XmString),
		XtOffset(XFE_URLBar *, m_editedLabelString),
		XtRString,
		"Error!"
	},
	{
		"uneditedLabelString",
		XmCXmString,
		XmRXmString,
		sizeof(XmString),
		XtOffset(XFE_URLBar *, m_uneditedLabelString),
		XtRString,
		"Error!" 
	},
	{
		"netsiteLabelString",
		XmCXmString,
		XmRXmString,
		sizeof(XmString),
		XtOffset(XFE_URLBar *, m_netsiteLabelString),
		XtRString,
		"Error!" 
	}
};

void
XFE_URLBar::createProxyIcon(Widget parent)
{
	XP_ASSERT( XfeIsAlive(parent) );
	XP_ASSERT( m_locationProxyIcon == NULL );
	XP_ASSERT( m_locationLabel == NULL );

	// Proxy icon
	m_locationProxyIcon = new XFE_Button(m_parentFrame,
										 parent,
										 LOCATION_PROXY_ICON_NAME,
										 &LocationProxy_group);
	
	// Configure the bookmark button
	m_locationProxyIcon->setPretendSensitive(True);

	// Location label
	m_locationLabel = 
		XtVaCreateManagedWidget(LOCATION_LABEL_NAME, 
								xmLabelWidgetClass, 
								parent,
								NULL);

	// Load the url label strings
	XtGetApplicationResources(m_locationLabel,
							  (XtPointer) this,
							  XFE_URLBar::_urlLabelResources,
							  XtNumber(XFE_URLBar::_urlLabelResources),
							  0,
							  0);
	

	// Place the proxy icon right after the bookmark quickfile
	XtVaSetValues(m_locationProxyIcon->getBaseWidget(),
				  XmNleftAttachment,		XmATTACH_WIDGET,
				  XmNrightAttachment,		XmATTACH_NONE,
				  XmNtopAttachment,			XmATTACH_FORM,
				  XmNbottomAttachment,		XmATTACH_NONE,
				  XmNleftWidget,			m_bookmarkButton->getBaseWidget(),
				  NULL);

	// Place the location label right after the proxy icon
	XtVaSetValues(m_locationLabel,
				  XmNleftAttachment,		XmATTACH_WIDGET,
				  XmNleftWidget,		m_locationProxyIcon->getBaseWidget(),
				  XmNrightAttachment,		XmATTACH_NONE,
				  XmNtopAttachment,			XmATTACH_FORM,
				  XmNbottomAttachment,		XmATTACH_FORM,
				  NULL);

	m_proxyIconDragSite = new XFE_LocationDrag(m_locationProxyIcon->getBaseWidget());
	
	// Register proxy icon to have a tooltip.
	fe_WidgetAddToolTips(m_locationProxyIcon->getBaseWidget());
}
//////////////////////////////////////////////////////////////////////////
void
XFE_URLBar::createUrlComboBox(Widget parent,History_entry * first_entry)
{
	XP_ASSERT( XfeIsAlive(parent) );
	XP_ASSERT( m_urlComboBox == NULL );

	Arg			av[22];
	Cardinal	ac = 0;
	Visual *	visual = 0;
	Colormap	cmap = 0;
	Cardinal	depth = 0;
	Widget		text_field;
	
	XtVaGetValues(m_parentFrame->getBaseWidget(),
				  XtNvisual,	&visual, 
				  XtNcolormap,	&cmap,
				  XtNdepth,		&depth,
				  NULL);
		  
	ac = 0;
	XtSetArg(av[ac],XmNmoveSelectedItemUp,	True);					ac++;
	XtSetArg(av[ac],XmNarrowType,			XmMOTIF); 				ac++;
	XtSetArg(av[ac],XmNtype,				XmDROP_DOWN_COMBO_BOX); ac++;
	XtSetArg(av[ac],XmNvisibleItemCount,	10 );					ac++;
	XtSetArg(av[ac],XmNvisual,				visual);				ac++;
	XtSetArg(av[ac],XmNcolormap,			cmap);					ac++;
	XtSetArg(av[ac],XmNdepth,				depth);					ac++;
	XtSetArg(av[ac],XmNmarginHeight,		0);						ac++;
	XtSetArg(av[ac],XmNleftAttachment,		XmATTACH_WIDGET);		ac++;
	XtSetArg(av[ac],XmNleftWidget,			m_locationLabel);		ac++;
	XtSetArg(av[ac],XmNtopAttachment,		XmATTACH_FORM);			ac++;
	XtSetArg(av[ac],XmNbottomAttachment,	XmATTACH_FORM);			ac++;
	XtSetArg(av[ac],XmNrightAttachment,		XmATTACH_FORM);			ac++;
	XtSetArg(av[ac],XmNrightOffset,			0);						ac++;

	// The combo box calls XmNactivateCallback for both its text and
	// arrow.  This is very confusing since clicking on the arrow does
	// not really "activate" the combo box...so i added a combo box
	// boolean resource to avoid calling this callback.  This fixes bug
	// 71097 (browser reload by just posting the url combo box).
	XtSetArg(av[ac],XmNnoCallbackForArrow,	True);					ac++;

	/* Create a combobox for storing typed in url history */
	/* This replaces the old text field */
	/* Combobox is only for MWContextBrowser */
	m_urlComboBox = DtCreateComboBox(parent, URL_COMBO_BOX_NAME, av,ac);
	
	XtVaGetValues(m_urlComboBox, XmNtextField, &text_field, 0);
		  
	/* Loads the previous session history list from a 
	   designated file ("~/.netscape/history.list") */
	readUserHistory();
		  
	XtAddCallback(text_field, 
				  XmNmodifyVerifyCallback, 
				  url_text_modify_cb,
				  (XtPointer) this);

	String value = "";

	if (first_entry && first_entry->address)
	{
		value = first_entry->address;
	}
		  
	XtVaSetValues (text_field,
				   XmNcursorPosition,	0,
				   0);
	fe_SetTextField (text_field, value);
		  
	XtAddCallback(m_urlComboBox, 
				  XmNselectionCallback,
				  url_text_selection_cb,
				  (XtPointer) this);
	
	XtAddCallback(m_urlComboBox,
				  XmNactivateCallback,
				  url_text_cb,
				  (XtPointer) this);
}
//////////////////////////////////////////////////////////////////////////
XFE_URLBar::~XFE_URLBar()
{
#ifdef URLBAR_POPUP
    if (m_popup)
    {
        delete m_popup;
        m_popup = NULL;
    }
#endif /* URLBAR_POPUP */

	if (m_proxyIconDragSite) 
	{
		delete m_proxyIconDragSite;

		m_proxyIconDragSite = NULL;
	}

	if (m_bookmarkDropSite)
	{
		delete m_bookmarkDropSite;

		m_bookmarkDropSite = NULL;
	}
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_URLBar::getBookMarkButton()
{
	return m_bookmarkButton->getBaseWidget();
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_URLBar::getProxyIcon()
{
	return m_locationProxyIcon->getBaseWidget();
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_URLBar::getUrlLabel()
{
	return m_locationLabel;
}
//////////////////////////////////////////////////////////////////////////
Widget
XFE_URLBar::getFormWidget()
{
	return XtParent(m_locationLabel);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_URLBar::recordURL(URL_Struct *url)
{
  XmString str;
  int itemCount = 0;
  Widget list;
  int *posList;
  int poscnt;
  Boolean found = False;

  if ( !url->address ) return;
  str = XmStringCreateLtoR(url->address, XmSTRING_DEFAULT_CHARSET);
  XtVaGetValues(m_urlComboBox, XmNlist, &list, 0);

  found = XmListGetMatchPos(list, str, &posList, &poscnt);

  if ( found )
  {
      XmListDeletePos(list,posList[0]);
  }
  {
	XtVaGetValues(list, XmNitemCount, &itemCount, 0 );
        if ( itemCount >= 10 )
        {
	  XmListDeleteItemsPos(list,1,10);
        }
	XmListAddItem(list,str, 1);
	XmListSelectPos(list, 1, False);
        XmListSetPos(list, 1); /* Scroll to top */
  }
  
  /* Save to history */
  saveUserHistory();
}

void
XFE_URLBar::setURLString(URL_Struct *url)
{
  /* Fixing a bug in motif that sends junk values
     if SetValues is done on a widget that has
     modifyVerifyCallback
     And I really hate the way XtCallbackList is being
     handled. There is no way to easily copy one. I prefer
     to use explicitly XtCallbackRec * instead of hiding this
     and using XtCallbackList. Makes this code more readable.
     */

  /* update ProxyIcon drag object with new URL */
  if (m_proxyIconDragSite)
        m_proxyIconDragSite->setDragData(url);

  XtCallbackRec *cblist, *text_cb;
  int i;
  Widget text;
  
  XtVaGetValues(m_urlComboBox, XmNtextField, &text, 0);
  if (!text ) return;
  
  if (!url || (sploosh && url && !strcmp (url->address, sploosh))
      || (url && !strcmp (url->address, "about:blank")))
    url = 0;

  XP_ASSERT(!m_labelBeingFrobbed);
  /* Inhibit fe_url_text_modify_cb() for this call. */
  m_labelBeingFrobbed = True;
  XtVaSetValues (text, XmNcursorPosition, 0, 0);
  
  /* Get and save the modifyVerifyCallbacklist */
  XtVaGetValues (text, XmNmodifyVerifyCallback, &text_cb, 0);
  for(i=0; text_cb[i].callback != NULL; i++);
  i++;
  cblist = (XtCallbackRec *) malloc(sizeof(XtCallbackRec)*i);
  for(i=0; text_cb[i].callback != NULL; i++)
    cblist[i] = text_cb[i];
  cblist[i] = text_cb[i];
  
  /* NULL the modifyVerifyCallback before doing setvalues */
  XtVaSetValues (text, XmNmodifyVerifyCallback, NULL, 0);
  
  fe_SetTextField (text, 
				 (url && url->address ? 
							// Check for JavaScript wysiwyg mangling
							// Fixes bug 28035.
							(LM_StripWysiwygURLPrefix(url->address) ?
							 LM_StripWysiwygURLPrefix(url->address) : 
							 url->address) : 
							""));
  
  /* Setback the modifyVerifyCallback List from our saved List */
  XtVaSetValues (text, XmNmodifyVerifyCallback, cblist, 0);
  XP_FREE(cblist);

  m_labelBeingFrobbed = False;
  frob_label (False, url && url->is_netsite);  

#ifdef notyet
  fe_store_url_prop(context, url);
#endif /* notyet */
}


XP_Bool
XFE_URLBar::readUserHistory()
{
 XmString xmstr;
 char *value;
 char buffer[1024];
 FILE *fp = fopen(fe_globalPrefs.user_history_file,"r");

 if ( !fp )
    return FALSE;
 
 if ( !fgets(buffer, 1024, fp) )
	*buffer = 0;

 while ( fgets(buffer, 1024, fp ) )
 {
	value = XP_StripLine(buffer);

        if (strlen(value)==0 || *value == '#')
	   continue;
	xmstr = XmStringCreateLtoR(value, XmSTRING_DEFAULT_CHARSET);
	DtComboBoxAddItem( m_urlComboBox, xmstr,0, True );
        XmStringFree(xmstr);
 }
 fclose(fp);
 return TRUE;
}

/* For saving out the user typed in url history list to a file*/
XP_Bool
XFE_URLBar::saveUserHistory()
{
 int i,itemCount;
 XmStringTable itemList;
 char *urlStr;
 char *filename = fe_globalPrefs.user_history_file;
 FILE *fp = fopen(filename,"w");

 if ( !fp )
    return FALSE;

 fprintf (fp,
		  XP_GetString(XFE_URLBAR_FILE_HEADER),
		  fe_version);

 XtVaGetValues(m_urlComboBox, XmNitemCount, &itemCount, XmNitems, &itemList, NULL);
 for ( i = 0; i < itemCount; i++ )
 {
    XmStringGetLtoR( itemList[i], XmSTRING_DEFAULT_CHARSET, &urlStr);
    fputs(urlStr, fp);
    fputc('\n', fp);
 }
 fclose(fp);
 return TRUE;
}

void
XFE_URLBar::frob_label(XP_Bool edited_p,
					   XP_Bool netsite_p)
{
//	MWContext *context = m_parentFrame->getContext();
	XmString old_string = 0;
	XmString new_string = (edited_p
						   ? m_editedLabelString
						   : (netsite_p
							  ? m_netsiteLabelString
							  : m_uneditedLabelString));

	XP_ASSERT(new_string);

	XtVaGetValues (m_locationLabel, XmNlabelString, &old_string, 0);
	if (!XmStringByteCompare (old_string, new_string))
		XtVaSetValues (m_locationLabel, XmNlabelString, new_string, 0);
	XmStringFree (old_string);
}

void
XFE_URLBar::url_text(XtPointer callData)
{
  URL_Struct *url_struct;
  XmAnyCallbackStruct *cb = (XmAnyCallbackStruct *) callData;
  Widget text_field;
  char *text = 0;

  if (cb->reason != XmCR_ACTIVATE) abort ();

  XtVaGetValues (m_urlComboBox, XmNtextField, &text_field, 0);
  text = fe_GetTextField(text_field); /* return via XtMalloc */

  if (*text)
    {
      char *old_text = 0;
      char *new_text;
      int32 id;

      if (m_parentFrame->getContext() != NULL)
        {
          id = XP_CONTEXTID(m_parentFrame->getContext());
        }
      else
        {
          id = 0;
        }
      if (HK_CallHook(HK_LOCATION, NULL, id, text, &new_text))
        {
          if (new_text != NULL)
            {
              old_text = text;
              text = new_text;
            }
        }

      url_struct = NET_CreateURLStruct (text, NET_DONT_RELOAD);

      notifyInterested(XFE_URLBar::navigateToURL, (void*)url_struct);

      if (old_text)
        {
          XP_FREE(text);
          text = old_text;
        }
    }      

  fe_NeutralizeFocus(m_parentFrame->getContext());

  if ( text ) XtFree(text);  
}

void
XFE_URLBar::url_text_modify()
{
  if (!m_labelBeingFrobbed)
    frob_label (True, False);
}

void
XFE_URLBar::url_text_selection(XtPointer callData)
{
  XmString str;
  char* text = 0;
  URL_Struct *url_struct;
  DtComboBoxCallbackStruct *cbData = (DtComboBoxCallbackStruct *)callData;
  Widget list;
  
  if (cbData->reason == XmCR_SELECT )
    {
      str= cbData->item_or_text;
      XmStringGetLtoR( str, XmSTRING_DEFAULT_CHARSET, &text);
      XtVaGetValues(m_urlComboBox, XmNlist, &list, 0);
      if ( *text )
	{
	  url_struct = NET_CreateURLStruct (text, NET_DONT_RELOAD);
	  
	  notifyInterested(XFE_URLBar::navigateToURL, (void*)url_struct);
	} 
      
      XtFree(text);
    }
}

void
XFE_URLBar::url_text_cb(Widget, XtPointer closure, XtPointer callData)
{
  XFE_URLBar *obj = (XFE_URLBar*)closure;

  obj->url_text(callData);
}

void
XFE_URLBar::url_text_modify_cb (Widget, XtPointer closure, XtPointer)
{
  XFE_URLBar *obj = (XFE_URLBar*)closure;

  obj->url_text_modify();
}

void
XFE_URLBar::url_text_selection_cb(Widget, XtPointer closure, XtPointer callData)
{
  XFE_URLBar *obj = (XFE_URLBar*)closure;

  obj->url_text_selection(callData);
}

//////////////////////////////////////////////////////////////////////////
#ifdef URLBAR_POPUP
void
XFE_URLBar::doPopup(XEvent *event)
{
  int x, y, clickrow;
  BM_Entry *entryUnderMouse;

  Widget widget = XtWindowToWidget(event->xany.display, event->xany.window);
 
  /* Posting the popup puts focus on the URLBar (needed for paste) */
  XmProcessTraversal (widget, XmTRAVERSE_CURRENT);

  if (widget == NULL)
    widget = m_widget;
		
  if (!m_popup)
    {
      m_popup = new XFE_PopupMenu("popup",(XFE_Frame*)m_toplevel, widget, NULL);
      m_popup->addMenuSpec(ccp_popup_spec);
    }
  m_popup->position(event);
  m_popup->show();
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_URLBar::comboBoxButtonPressEH(Widget		/* w */,
                                  XtPointer	clientData,
                                  XEvent *	event,
                                  Boolean *	cont)
{
  XFE_URLBar *obj = (XFE_URLBar*)clientData;

  // Make sure Button3 was pressed
  if (event->type == ButtonPress && event->xbutton.button == Button3)
    {
      obj->doPopup(event);
    }
  *cont = True;
}
#endif /* URLBAR_POPUP */


//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_URLBar::bookmarkCascadeTearCB(Widget	/* cascade */,
								  XtPointer	clientData,
								  XtPointer callData)
{
	XFE_URLBar *					obj = (XFE_URLBar *) clientData;
	XfeSubmenuTearCallbackStruct *	data = 
		(XfeSubmenuTearCallbackStruct *) callData;

	obj->handleBookmarkCascadeTear(data->torn);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_URLBar::frameTitleChangedCB(Widget		frame_shell,
								XtPointer	clientData,
								XtPointer	callData)
{
	XFE_URLBar *	obj = (XFE_URLBar *) clientData;
	String			title;

	XtVaGetValues(frame_shell,XmNtitle,&title,NULL);
	
	XtVaGetValues(frame_shell,XmNtitle,&title,NULL);

	obj->handleFrameTitleChanged(title);

	// Dont free the title, cause it returns a private pointer
}
//////////////////////////////////////////////////////////////////////////
void
XFE_URLBar::handleBookmarkCascadeTear(XP_Bool torn)
{
	if (m_bookmarkButton && m_bookmarkButton->isAlive())
	{
		// While the bookmarks submenu is posted, the button cant be used
		m_bookmarkButton->setSensitive(!torn);

		// Make the torn shell's title the same as the frame
		if (torn && m_parentFrame && m_parentFrame->isAlive())
		{
			XtVaSetValues(m_bookmarkButton->getBaseWidget(),
						  XmNtornShellTitle,	m_parentFrame->getTitle(),
						  NULL);
		}
	}
}
//////////////////////////////////////////////////////////////////////////
void
XFE_URLBar::handleFrameTitleChanged(String title)
{

	if (m_bookmarkButton && m_bookmarkButton->isAlive())
	{
		Boolean torn;

		XtVaGetValues(m_bookmarkButton->getBaseWidget(),XmNtorn,&torn,NULL);

		if (torn)
		{
			XtVaSetValues(m_bookmarkButton->getBaseWidget(),
						  XmNtornShellTitle,	title,
						  NULL);
		}
	}
}
//////////////////////////////////////////////////////////////////////////
