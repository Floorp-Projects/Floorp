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

// Author: John R. McMullen

#include <LAttachment.h>
#include <QuickDraw.h>

class LCommander;
class LPane;
//======================================
class CTargetFramer : public LAttachment
// Attach this to a pane in a tab group to show which pane has focus.  The pane, sadly,
// must also call ExecuteAttachments in overrides of BeTarget(), DontBeTarget() and DrawSelf()
// in order to make this work.  See CBrowserView/CHTMLView, CStandardFlexTable for examples.
//======================================
{
private:
	typedef LAttachment Inherited;
public:
	enum { class_ID			= 'TrgF'
	,	msg_BecomingTarget	= 'TYea'
	,	msg_ResigningTarget = 'TNay'
	,	msg_DrawSelfDone	= 'DSDn' };
								CTargetFramer();
								CTargetFramer(LStream* inStream);
	virtual						~CTargetFramer();
	virtual void 				SetOwnerHost(LAttachable* inAttachable);
	virtual void 				ExecuteSelf(MessageT inMessage, void*);
protected:
	void						InvertBorder();
	void						CalcBorderRegion(RgnHandle ioBorderRgn);
// Data
protected:
	LCommander*			mCommander;
	LPane*				mPane;
	Boolean				mDrawWithHilite;
}; // class CTargetFramer