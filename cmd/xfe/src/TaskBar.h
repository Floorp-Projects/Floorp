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
   TaskBar.h -- class definition for taskbar.
   Created: Stephen Lamm <slamm@netscape.com>, 19-Nov-96.
 */



#ifndef _xfe_taskbar_h
#define _xfe_taskbar_h

#include "xp_core.h"
#include "Chrome.h"
#include "Command.h"
#include "Component.h"
#include "IconGroup.h"
#include "icons.h"

#include <Xm/Xm.h>

class XFE_Frame;

typedef struct TaskBarSpec 
{
    const char *	taskBarButtonName;
    EChromeTag		tag;
    IconGroup *		icons;
} TaskBarSpec;


class XFE_TaskBar : public XFE_Component
{
public:
	
	XFE_TaskBar(Widget			parent,
				XFE_Frame *		parent_frame,
				XP_Bool			is_floating);
	
	virtual ~XFE_TaskBar();
	
	XFE_CALLBACK_DECL(doCommandNotice)
		
	void		setFloatingTitle			(const char * title);
	void		updateFloatingAppearance	();

	Cardinal	numEnabledButtons			();

private:
  
	static TaskBarSpec	m_floatingSpec[];
	static TaskBarSpec	m_dockedSpec[];
	
	XP_Bool				m_isFloating;
	XFE_Frame *			m_parentFrame;

#ifdef MOZ_MAIL_NEWS
	XP_Bool				m_biffNoticeInstalled;
	
	// these two needed to update the biff icon
	XFE_CALLBACK_DECL(updateBiffStateNotice)
#endif

	void setIconGroupForCommand(CommandType cmd, IconGroup *icons);    
	
	Widget		createTaskBarButton			(TaskBarSpec *spec);
	void		createButtons				(TaskBarSpec * spec);

	void		createFloatingWidgets		(Widget parent);
	void		createDockedWidgets			(Widget parent);

    // update the icon appearance
    XFE_CALLBACK_DECL(updateIconAppearance)

	XFE_CALLBACK_DECL(updateFloatingBusyState)
};

#endif /* _xfe_taskbar_h */
