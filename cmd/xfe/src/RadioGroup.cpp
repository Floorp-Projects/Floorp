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
   RadioGroup.cpp -- Radio groups that don't have to share the same row column parent.
   Created: Chris Toshok <toshok@netscape.com>, 28-Sep-96.
 */



#include "xp_core.h"
#include "xpassert.h"
#include "xp_str.h"
#include "xp_mem.h"
#include "xp_mcom.h"  // For XP_STRDUP(). 
#include "RadioGroup.h"

#include <Xm/ToggleB.h>
#include <Xm/ToggleBG.h>

#if DEBUG_slamm
#define D(x) x
#else
#define D(x)
#endif

XP_List *XFE_RadioGroup::groups = NULL;

XFE_RadioGroup::XFE_RadioGroup(char *name, XFE_Frame *frame)
{
  m_children = XP_ListNew();
  m_name = XP_STRDUP(name);
  m_frame = frame;
  m_incallback = False;

  registerGroup(this);
}

XFE_RadioGroup::~XFE_RadioGroup()
{
  XP_ListDestroy(m_children);
  XP_FREE(m_name);
  unregisterGroup(this);
}

char *
XFE_RadioGroup::getName()
{
  return m_name;
}

XFE_Frame *
XFE_RadioGroup::getFrame()
{
  return m_frame;
}

void
XFE_RadioGroup::addChild(Widget w)
{
  XP_ListAddObject(m_children, w);

  if (XmIsToggleButton(w) || XmIsToggleButtonGadget(w))
    {
      XtVaSetValues(w, 
                    XmNindicatorType, XmONE_OF_MANY,
                    NULL);

      XtAddCallback(w, XmNvalueChangedCallback, toggle_cb, this);
    }
}

void
XFE_RadioGroup::removeChild(Widget w)
{
  XP_ListRemoveObject(m_children, w);

  if (XmIsToggleButton(w) || XmIsToggleButtonGadget(w))
    XtRemoveCallback(w, XmNvalueChangedCallback, toggle_cb, this);
}

XFE_RadioGroup *
XFE_RadioGroup::getByName(char *name, XFE_Frame *forFrame)
{
  XP_List *cur;
  XFE_RadioGroup *new_group;

  if (!groups)
    groups = XP_ListNew();

  for (cur = groups;
       cur != NULL;
       cur = cur->next)
    {
      XFE_RadioGroup *g = (XFE_RadioGroup*)cur->object;

      if (g && 
	  !strcmp(name, g->getName()) &&
	  forFrame == g->getFrame())
	{
	  return g;
	}
    }

  new_group = new XFE_RadioGroup(name, forFrame);

  registerGroup(new_group);

  return new_group;
}

void
XFE_RadioGroup::registerGroup(XFE_RadioGroup *group)
{
  XP_ListAddObject(groups, group);
}

void
XFE_RadioGroup::unregisterGroup(XFE_RadioGroup *group)
{
  XP_ListRemoveObject(groups, group);
}

void
XFE_RadioGroup::toggle(Widget w)
{
  XP_List *cur;

  if (m_incallback)
    {
      // keep from doing all this over and over again.
      // we can't set the callCallbacks parameter to XmToggleButtonSetStatus
      // to false because we also want other entities out there to know
      // what's happening.
      return;
    }

  m_incallback = True;
  m_callbackwidget = w;

  D( printf ("XFE_RadioGroup::toggle: Untoggling all except: %s\n",
             getLabel(w)); )

  for (cur = m_children;
       cur != NULL;
       cur = cur->next)
    {
      Widget cur_w = (Widget)cur->object;

      if (cur_w && cur_w != w)
        {
          if (XmIsToggleButton(cur_w))
            {
              XmToggleButtonSetState(cur_w, False, True);
            }
          else if (XmIsToggleButtonGadget(cur_w))
            {
              XmToggleButtonGadgetSetState(cur_w, False, True);
            }
        }
    }
  m_incallback = False;
}

void
XFE_RadioGroup::toggle_cb(Widget w, XtPointer clientData, XtPointer callData)
{
  XFE_RadioGroup *obj = (XFE_RadioGroup*)clientData;
  XmToggleButtonCallbackStruct *cbs = (XmToggleButtonCallbackStruct*)callData;

  if (cbs->set)
    obj->toggle(w);
  else if (obj->m_incallback == False)
    {
      // don't know if we always want this to happen, but
      // for now, always have one toggle set (or more to the
      // point, don't let them unset a toggle.)
      if (XmIsToggleButton(w))
        {
          D( printf ("Resetting toggle button: %s\n", obj->getLabel(w)); )
          XmToggleButtonSetState(w, True, False);
        }
      else if (XmIsToggleButtonGadget(w))
        {
          D( printf ("Resetting toggle button gadget: %s\n",
                     obj->getLabel(w)); )
          XmToggleButtonGadgetSetState(w, True, False);
        }
    }
}

#ifdef DEBUG
char *
XFE_RadioGroup::getLabel(Widget w)
{
  XP_List *cur;

  for (cur = m_children;
       cur != NULL;
       cur = cur->next)
    {
      Widget cur_w = (Widget)cur->object;

      if (cur_w == w)
        {
          char *text;
          XmString label;
          XtVaGetValues(w,
                        XmNlabelString,   &label,
                        NULL);
          XmStringGetLtoR(label, XmFONTLIST_DEFAULT_TAG, &text);
          return text;
        }
    }

  return "(null)";
}
#endif
