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
   View.cpp -- implementation file for View.
   Created: Chris Toshok <toshok@netscape.com>, 17-Sep-96.
 */



#include "View.h"
#include "ViewGlue.h"
#include "xfe.h"
#include "xp_mem.h"
#include "xpassert.h"
#include "xfe2_extern.h"
#include "Xfe/Xfe.h"

const char * XFE_View::chromeNeedsUpdating = "XFE_View::chromeNeedsUpdating";
const char * XFE_View::commandNeedsUpdating = "XFE_View::commandNeedsUpdating";
const char * XFE_View::statusNeedsUpdating = "XFE_View::statusNeedsUpdating";
const char * XFE_View::statusNeedsUpdatingMidTruncated = "XFE_View::statusNeedsUpdatingMidTruncated";
const char *XFE_View::allConnectionsCompleteCallback = "XFE_View::allConnectionsCompleteCallback";

static char myClassName[] = "XFE_View::className";

XFE_View::XFE_View(XFE_Component *toplevel_component,
		   XFE_View *parent_view, MWContext *context) : XFE_Component(toplevel_component)
{
  m_parentView = parent_view;
  m_subviews = NULL;
  m_numsubviews = 0;
  m_numsubviews_allocated = 0 ;
  m_contextData = context;

  /* View can contains other view as its sub-views*/
  m_numsubviews = 0;
  m_numsubviews_allocated = 1;

  m_subviews = (XFE_View**)XP_CALLOC(m_numsubviews_allocated, sizeof(XFE_View*));

  setScrollbarsActive(TRUE);
}

XFE_View::~XFE_View()
{

  if (m_numsubviews)
    {
      int i;

      for (i = 0; i < m_numsubviews; i ++)
	delete m_subviews[i];
    }

  XP_FREE(m_subviews);
}

const char* 
XFE_View::getClassName()
{
	return myClassName;
}

void
XFE_View::allConnectionsComplete(MWContext  *context)
{
	/* Tao_27apr98
	 * Notify whoever interested in "allConnectionsComplete" event
	 */
	notifyInterested(XFE_View::allConnectionsCompleteCallback, (void*) context);
	notifyInterested(XFE_View::chromeNeedsUpdating);
}

XFE_View *
XFE_View::getParent()
{
  return m_parentView;
}

MWContext *
XFE_View::getContext()
{
  return m_contextData;
}

int
XFE_View::getNumSubViews()
{
  return m_numsubviews;
}

void
XFE_View::setParent(XFE_View *parent_view)
{
  m_parentView = parent_view;
}

void
XFE_View::addView(XFE_View *new_view)
{
  if (m_numsubviews == m_numsubviews_allocated)
        {
          m_numsubviews_allocated *= 2;
 
          m_subviews = (XFE_View**)XP_REALLOC(m_subviews,
                                           sizeof(XFE_View*) * m_numsubviews_allocated);
        }
 
  m_subviews[ m_numsubviews ++ ] = new_view;
}

Boolean
XFE_View::hasSubViews() 
{
  return fe_GetFocusGridOfContext (getContext()) ? True : False;
}

XFE_View *
XFE_View::widgetToView(Widget w)
{
  int i;

  for (i = 0; i < m_numsubviews; i ++)
    if (m_subviews[i]->isWidgetInside(w))
      return m_subviews[i]->widgetToView(w);

  if (isWidgetInside(w))
    return this;
  else
    return NULL; /* not anywhere in this view heirarchy. */
}

Boolean
XFE_View::isCommandEnabled(CommandType cmd,
						   void *calldata, XFE_CommandInfo* info)
{
  if ( cmd == xfeCmdStopLoading
       && m_contextData )
    {
      return fe_IsContextStoppable(m_contextData);
    }
  else if (cmd == xfeCmdAboutMozilla
           || cmd == xfeCmdSearchAddress
           || cmd == xfeCmdOpenCustomUrl
           )
	  {
		  return !XP_IsContextBusy(m_contextData);
	  }
  else
    {
      for (int i = 0; i < m_numsubviews; i ++)
	{
	  if (m_subviews[i]->handlesCommand(cmd, calldata, info))
	    return m_subviews[i]->isCommandEnabled(cmd, calldata, info);
	}

      return False;
    }
}

void
XFE_View::doCommand(CommandType cmd, void *calldata, XFE_CommandInfo* info)
{
  if ( cmd == xfeCmdStopLoading
       && m_contextData )
    {
      XP_InterruptContext(m_contextData);
    }
  else if ( cmd == xfeCmdAboutMozilla )
    {
      fe_about_cb(NULL, m_contextData, NULL);
    }
#ifdef MOZ_MAIL_NEWS
  else if ( cmd == xfeCmdSearchAddress)
    {
	  fe_showLdapSearch(XfeAncestorFindApplicationShell(getToplevel()->getBaseWidget()),
						/* Tao: we might need to check if this returns a 
						 * non-NULL frame
						 */
						ViewGlue_getFrame(m_contextData),
						(Chrome*)NULL);
	
    }
#endif
  else
    {
      for (int i = 0; i < m_numsubviews; i ++)
	{
	  if (m_subviews[i]->handlesCommand(cmd, calldata, info))
	    {
	      m_subviews[i]->doCommand(cmd, calldata, info);
	      return;
	    }
	}

      XBell(XtDisplay(m_widget), 0);
    }
}

Boolean
XFE_View::handlesCommand(CommandType cmd,
						 void *calldata, XFE_CommandInfo* info)
{
  if (cmd == xfeCmdStopLoading)
    {
      return True;
    }
  else if (cmd == xfeCmdAboutMozilla
           || cmd == xfeCmdSearchAddress
           || cmd == xfeCmdOpenCustomUrl
           )
    {
      return True;
    }
  else
    {
      for (int i = 0; i < m_numsubviews; i ++)
	{
	  if (m_subviews[i]->handlesCommand(cmd, calldata, info))
	    return True;
	}
      
      return False;
    }
}

char *
XFE_View::commandToString(CommandType cmd,
						  void *calldata, XFE_CommandInfo* info)
{
  for (int i = 0; i < m_numsubviews; i ++)
    {
      if (m_subviews[i]->handlesCommand(cmd, calldata, info))
		  return m_subviews[i]->commandToString(cmd, calldata, info);
    }
  
  return NULL;
}

Boolean
XFE_View::isCommandSelected(CommandType cmd,
							void *calldata, XFE_CommandInfo* info)
{
  for (int i = 0; i < m_numsubviews; i ++)
    {
      if (m_subviews[i]->handlesCommand(cmd, calldata, info))
        return m_subviews[i]->isCommandSelected(cmd, calldata, info);
    }
  return False;
}

Pixel
XFE_View::getFGPixel()
{
  return CONTEXT_DATA(m_contextData)->fg_pixel;
}

Pixel
XFE_View::getBGPixel()
{
  return CONTEXT_DATA(m_contextData)->default_bg_pixel;
}

Pixel
XFE_View::getTopShadowPixel()
{
  return CONTEXT_DATA(m_contextData)->top_shadow_pixel;
}

Pixel
XFE_View::getBottomShadowPixel()
{
  return CONTEXT_DATA(m_contextData)->bottom_shadow_pixel;
}

void
XFE_View::setScrollbarsActive(XP_Bool b)
{
  m_areScrollbarsActive = b;
  if (m_contextData) {
	CONTEXT_DATA(m_contextData)->are_scrollbars_active = b;
  }
}

XP_Bool
XFE_View::getScrollbarsActive()
{
  return m_areScrollbarsActive;
}

XFE_View*
XFE_View::getCommandView(XFE_Command* command)
{
	int i;
	CommandType cmd_id = command->getId();

	for (i = 0; i < m_numsubviews; i ++) {
		if (m_subviews[i]->getCommand(cmd_id) == command)
			return m_subviews[i];
	}
	
	if (getCommand(cmd_id) == command)
		return this;
	else
		return NULL;
}
