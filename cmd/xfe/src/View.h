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
   XFEView.h -- class definition for XFEView
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */



#ifndef _xfe_xfeview_h
#define _xfe_xfeview_h

#include "structs.h"
#include "xp_core.h"

#include "Command.h"
#include "Component.h"

class XFE_View : public XFE_Component
{
public:
  XFE_View(XFE_Component *toplevel_component, XFE_View *parent_view = NULL, MWContext *context = NULL);
  virtual ~XFE_View();

  XFE_View*	getParent();
  void		setParent(XFE_View *parent_view);
  MWContext*	getContext();
  int		getNumSubViews();

  virtual const char* getClassName();

  // called when all connections when this window are finished.
  // Really should be a notification...
  virtual void allConnectionsComplete(MWContext  *context);
  static const char *allConnectionsCompleteCallback;

  /* The view that is returned is either this view or one of it's sub views. */
  virtual XFE_View* widgetToView(Widget w);

  /* These are the methods that views will want to overide to add
     their own functionality. */

  /* this method is used by the toplevel to sensitize menu/toolbar items. */
  virtual Boolean isCommandEnabled(CommandType cmd, void *calldata = NULL,
								   XFE_CommandInfo* m = NULL);

  /* this method is used by the toplevel to dispatch a command. */
  virtual void doCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* i = NULL);

  /* used by toplevel to see which view can handle a command.  Returns true
     if we can handle it. */
  virtual Boolean handlesCommand(CommandType cmd, void *calldata = NULL, 
								 XFE_CommandInfo* i = NULL);

  /* used by toplevel to change the labels specified in menu items.  Return NULL
     if no change. */
  virtual char* commandToString(CommandType cmd, void *calldata = NULL,
								XFE_CommandInfo* i = NULL);

  /* used by toplevel to change the selection state of specified toggle menu 
     items.  This method only applies to toggle button */
  virtual Boolean isCommandSelected(CommandType cmd, void *calldata = NULL,
									XFE_CommandInfo* i = NULL);

  virtual XFE_Command* getCommand(CommandType) { return NULL; };
  virtual XFE_View*    getCommandView(XFE_Command*);

  static const char *chromeNeedsUpdating; // update all the chrome in this window
  static const char *commandNeedsUpdating; // update the status of one command -- sent in calldata
  static const char *statusNeedsUpdating; // update status to a string -- sent in calldata
  static const char *statusNeedsUpdatingMidTruncated; // update status to mid-truncated string -- sent in calldata

  virtual Pixel getFGPixel();
  virtual Pixel getBGPixel();
  virtual Pixel getTopShadowPixel();
  virtual Pixel getBottomShadowPixel();

  virtual void    setScrollbarsActive(XP_Bool b);
  virtual XP_Bool getScrollbarsActive();

	// tooltips and doc string
	virtual char *getDocString(CommandType /* cmd */) {return NULL;}
	virtual char *getTipString(CommandType /* cmd */) {return NULL;}

protected:
  MWContext *m_contextData; // the MWContext *
  virtual void addView(XFE_View *new_view);
  virtual Boolean hasSubViews();

   // list of children
  XFE_View **m_subviews;
  int m_numsubviews;
  int m_numsubviews_allocated;
  XP_Bool m_areScrollbarsActive;
private:
  XFE_View *m_parentView;
};

#endif /* _xfe_xfeview_h */
