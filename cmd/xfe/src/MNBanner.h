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
   MNBanner.h -- class definitions the little banner thingy on MN Frames (except compose).
   Created: Chris Toshok <toshok@netscape.com>, 15-Oct-96.
 */



#ifndef _xfe_mnbanner_h
#define _xfe_mnbanner_h

#include "xp_core.h"
#include "Component.h"
#include "Button.h"
#include "ProxyIcon.h"
#include "MNView.h"
#include "ToolboxItem.h"

#include "mozilla.h"
#include "icons.h"

class XFE_Logo;

/*

  The MNBanner looks like:
   +---------------------------------------------------+
   | <icon><title><subtitle>    	  <info> | <mommy> |
   +---------------------------------------------------+

   With the title shown in bold, and the subtitle shown in the normal
   font.  The shadow around the banner is in (XmSHADOW_IN, that is.)

 */

class XFE_MNBanner : public XFE_ToolboxItem
{
public:
	XFE_MNBanner(XFE_Frame *parentFrame,
				 XFE_Toolbox * parent_toolbox,
				 char *title = NULL, char *subtitle = NULL,
				 char *info = NULL);

	void setProxyIcon(fe_icon *icon);
	void setMommyIcon(IconGroup *icons);
	void setTitle(char *title);
	void setTitleComponent(XFE_Component *title_component);
	void setSubtitle(char *subtitle);
	void setInfo(char *info);

	Widget getComponentParent();

	XFE_ProxyIcon*	getProxyIcon();
	char*			getTitle();
	char*			getSubtitle();
	char*			getInfo();
	XFE_Button*		getMommyButton();
	static const char *		twoPaneView;
        void showFolderBtn();
  	void setShowFolder(XP_Bool show);

protected:

	// Override AbstractToolbar methods
	virtual void	update() {}

private:

	XFE_CALLBACK_DECL(doMommyCommand)
	XFE_CALLBACK_DECL(showFolder)

	Widget m_form;
	Widget m_proxylabel;
	Widget m_titlelabel;
	Widget m_arrowButton;
	XFE_Component *m_titleComponent;
	Widget m_subtitlelabel;
	Widget m_infolabel;
	XFE_Button *m_mommyButton;
	XFE_Frame *m_parentFrame;

	XFE_ProxyIcon *m_proxyicon;
};

#endif /* _xfe_mnbanner_h */
