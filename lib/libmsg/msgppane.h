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

#ifndef _MsgProgressPane_H_
#define _MsgProgressPane_H_

#include "msgpane.h"

class MSG_ProgressPane : public MSG_Pane
{
public:
	MSG_ProgressPane(MWContext* context, MSG_Master* master, MSG_Pane *parentPane);
	virtual ~MSG_ProgressPane();
	virtual MsgERR	DoCommand(MSG_CommandType command, MSG_ViewIndex* indices,
								int32 numIndices);
	virtual MSG_FolderInfo	*GetFolder();
	virtual void CrushUpdateLevelToZero();
	virtual void StartingUpdate(MSG_NOTIFY_CODE /*code*/, MSG_ViewIndex /*where*/,
							  int32 /*num*/);
	virtual void EndingUpdate(MSG_NOTIFY_CODE /*code*/, MSG_ViewIndex /*where*/,
							  int32 /*num*/);

	virtual MSG_Pane* GetParentPane();

	virtual void OnFolderAdded (MSG_FolderInfo *, MSG_Pane *);

protected:
	MSG_Pane	*m_parentPane;
};
#endif


