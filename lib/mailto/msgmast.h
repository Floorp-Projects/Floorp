/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _MsgMast_H_
#define _MsgMast_H_

#include "errcode.h"
#include "msgprnot.h"

class MSG_Prefs;
class MSG_Pane;
class msg_HostTable;

typedef void * pw_ptr;

class MSG_Master : public MSG_PrefsNotify {
public:
	MSG_Master(MSG_Prefs* prefs);
	virtual ~MSG_Master();

	MSG_Prefs* GetPrefs() {return m_prefs;}

	void SetFirstPane(MSG_Pane* pane);
	MSG_Pane* GetFirstPane();

	MSG_Pane *FindPaneOfType(MSG_FolderInfo *id, MSG_PaneType type) ;
	MSG_Pane *FindPaneOfType(const char *url, MSG_PaneType type) ;
	
	MSG_Pane *FindFirstPaneOfType(MSG_PaneType type);
	MSG_Pane *FindNextPaneOfType(MSG_Pane *startHere, MSG_PaneType type);

	MSG_NewsHost* FindHost(const char* name, XP_Bool isSecure,
						   int32 port); // If port is negative, then we will
										// look for a ":portnum" as part of the
										// name.  Otherwise, there had better
										// not be a colon in the name.
	MSG_NewsHost* GetDefaultNewsHost();

	void NotifyPrefsChange(NotifyCode code);

protected:

private:
	MSG_Prefs* m_prefs;
	MSG_Pane* m_firstpane;
};


#endif /* _MsgMast_H_ */
