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
   HTMLView.h -- class definition for the HTML view class
   Created: Spence Murray <spence@netscape.com>, 23-Oct-96.
 */



#ifndef _xfe_htmlview_h
#define _xfe_htmlview_h

#include "structs.h"
#include "mozilla.h"
#include "xfe.h"

#include "PopupMenu.h"
#include "Menu.h"
#include "View.h"
#include "Command.h"

#define DO_NOT_PASS_LAYER_N_EVENT 1

class XFE_HTMLView : public XFE_View
{
public:
  XFE_HTMLView(XFE_Component *toplevel_component, Widget parent, XFE_View *parent_view, MWContext *context);

  virtual ~XFE_HTMLView();

  virtual int getURL(URL_Struct *url, Boolean skip_get_url);

  // this gets called by our toplevel to let us do some things
  // after it's been realized, but before we're on the screen.
  XFE_CALLBACK_DECL(beforeToplevelShow)

  // this gets called when links attribute is changed
  XFE_CALLBACK_DECL(ChangeLinksAttribute)

  // this gets called when any default color is changed
  XFE_CALLBACK_DECL(ChangeDefaultColors)

  // this gets called when default font is changed
  XFE_CALLBACK_DECL(ChangeDefaultFont)

  virtual Boolean isCommandEnabled(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual void doCommand(CommandType, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual Boolean handlesCommand(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual char* commandToString(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* i = NULL);
  virtual XP_Bool isCommandSelected(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);

  virtual void    setScrollbarsActive(XP_Bool b);

  // interrogation of HTMLView's contents.
  void translateToDocCoords(int x, int y, int *doc_x, int *doc_y);

  URL_Struct *URLAtPosition(int x, int y, CL_Layer *layer=NULL);
  URL_Struct *imageURLAtPosition(int x, int y, CL_Layer *layer=NULL);
  LO_Element *layoutElementAtPosition(int x, int y, CL_Layer *layer=NULL);

  void doPopup(MWContext *context, CL_Layer *layer,
	       CL_Event *layer_event);

	URL_Struct *URLUnderMouse();
	URL_Struct *ImageUnderMouse();
	URL_Struct *BackgroundUnderMouse();

#if DO_NOT_PASS_LAYER_N_EVENT
  virtual void tipCB(MWContext *context, int x, int y, char* alt_text,
					 XtPointer cb_data);
#else
  virtual void tipCB(MWContext *context, CL_Layer *layer,
					 CL_Event *layer_event, XtPointer cb_data);
#endif
	XFE_PopupMenu* getPopupMenu();
	void           setPopupMenu(XFE_PopupMenu*);

  static const char *newURLLoading; // called whenever a new page is loading -- the url needs to change
  static const char *spacebarAtPageBottom; // called when space is hit at the bottom of the page.
  static const char *popupNeedsShowing; // called so that containing views can override the htmlview's popup.

protected:
  XFE_PopupMenu *m_popup;

private:
  Widget m_scrollerForm;

  static MenuSpec separator_spec[];
  static MenuSpec openLinkNew_spec[];
  static MenuSpec openFrameNew_spec[];
  static MenuSpec openLinkEdit_spec[];
  static MenuSpec go_spec[];
  static MenuSpec showImage_spec[];
  static MenuSpec stopLoading_spec[];
  static MenuSpec page_details_spec[];
  static MenuSpec openImage_spec[];
  static MenuSpec addLinkBookmark_spec[];
  static MenuSpec addFrameBookmark_spec[];
  static MenuSpec addBookmark_spec[];
  static MenuSpec sendPage_spec[];
  static MenuSpec saveLink_spec[];
  static MenuSpec saveImage_spec[];
  static MenuSpec saveBGImage_spec[];
  static MenuSpec copy_spec[];
  static MenuSpec copyLink_spec[];
  static MenuSpec copyImage_spec[];

  URL_Struct *m_urlUnderMouse;
  URL_Struct *m_imageUnderMouse;
  URL_Struct *m_backgroundUnderMouse;

  void makeScroller(Widget parent);
  void findLayerForPopupMenu(Widget widget, XEvent *event);
  void reload(Widget, XEvent *event, XP_Bool onlyReloadFrame = FALSE);
  void openFileAction (String *av, Cardinal *ac);

  // This gets called by our toplevel when the encoding has
  // changed for this window.
  XFE_CALLBACK_DECL(DocEncoding)
};

extern "C" void fe_HTMLViewDoPopup (MWContext *context, 
				    CL_Layer *layer,
				    CL_Event *layer_event);

#endif /* _xfe_htmlview_h */

