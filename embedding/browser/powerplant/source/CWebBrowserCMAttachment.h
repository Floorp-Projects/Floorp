/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 */

#ifndef __CWebBrowserCMAttachment__
#define __CWebBrowserCMAttachment__

#include "LCMAttachment.h"

//*****************************************************************************
// class CWebBrowserCMAttachment
//
// This override is needed because LCMAttachment intercepts mouse clicks, tracks
// the click, and handles it if the click is on a context menu. We don't want so
// complete a solution. In our case, mozilla handles the click, determines that
// it's a context menu click and then calls nsIContextMenuListener::OnShowContextMenu.
// This override allows DoContextMenuClick to be called after the click.
//
//*****************************************************************************   

class	CWebBrowserCMAttachment : public LCMAttachment {
public:

	enum { class_ID = FOUR_CHAR_CODE('WBCM') };
				
								CWebBrowserCMAttachment();
								CWebBrowserCMAttachment(LCommander*	inTarget,
										                PaneIDT		inTargetPaneID = PaneIDT_Unspecified);
								CWebBrowserCMAttachment(LStream* inStream);
	
	virtual						~CWebBrowserCMAttachment();
	
	// LAttachment
	virtual	void				ExecuteSelf(MessageT		inMessage,
										    void*			ioParam);
										    
	// CWebBrowserCMAttachment
	virtual void                SetCommandList(SInt16 mcmdResID);
	virtual void                DoContextMenuClick(const EventRecord&	inMacEvent);


};

#endif // __CWebBrowserCMAttachment__
