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

#ifndef _MsgLPane_H_
#define _MsgLPane_H_

#include "msgpane.h"

typedef struct MSG_LinedPaneStack MSG_LinedPaneStack;

class MSG_LinedPane : public MSG_Pane {
public:
	MSG_LinedPane(MWContext* context, MSG_Master* master);
	virtual ~MSG_LinedPane();

	virtual XP_Bool IsLinePane();
	virtual void ToggleExpansion(MSG_ViewIndex line, int32* numchanged) = 0;
	virtual int32 ExpansionDelta(MSG_ViewIndex line) = 0;
	virtual int32 GetNumLines() = 0;
	virtual void StartingUpdate(MSG_NOTIFY_CODE code, MSG_ViewIndex where,
								int32 num);
	virtual void EndingUpdate(MSG_NOTIFY_CODE code, MSG_ViewIndex where,
							  int32 num);
	virtual void OnFolderChanged(MSG_FolderInfo *);
	virtual XP_Bool IsValidIndex(MSG_ViewIndex index);
protected:
	virtual void FEStart() {		// Starting a call from the FE.
		m_fecount++;
	}

	virtual void FEEnd() {		// Finished a call from the FE.
		m_fecount--;
		XP_ASSERT(m_fecount >= 0);
	}

	virtual void DisableUpdate(); // Don't actually do anything when calling
								  // StartingUpdate() and EndingUpdate().
	virtual void EnableUpdate();  // OK; StartingUpdate() and EndingUpdate()
								  // can do things again.

	MSG_LinedPaneStack* m_stack;// Used only when DEBUG is on; helps check
								// that calls to StartingUpdate and
								// EndingUpdate match up correctly.

	// m_numstack is now in the base class because we use it to know if we're nested
	int m_maxstack;


	int m_fecount;				// How many calls we've had to FEStart()
								// without a matching call to FEEnd().

	int m_disablecount;			// How many calls we've had to DisableUpdate()
								// without a matching call to EnableUpdate().

};  

#endif /* _MsgLPane_H_ */
