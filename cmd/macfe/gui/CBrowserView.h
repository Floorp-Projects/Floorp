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

#pragma once

#include "CHTMLView.h"
#include "CKeyUpReceiver.h"

class CBrowserViewDoDragReceiveMochaCallback;

class CBrowserView : public CHTMLView, public CKeyUpReceiver
{
	private:
		typedef CHTMLView Inherited;
	public:
	
		enum { class_ID = 'BrVw' };
		
								CBrowserView(LStream* inStream);
		virtual					~CBrowserView();
	
		virtual	void			DrawSelf();		// needed for target framer to work properly
		
		virtual void			ScrollImageBy( Int32 inLeftDelta, Int32 inTopDelta, Boolean inRefresh );
		
		virtual Boolean			DragIsAcceptable(DragReference inDragRef);
		virtual Boolean			ItemIsAcceptable(DragReference inDragRef,
													ItemReference inItemRef);
/*		
		virtual void			ReceiveDragItem(DragReference inDragRef,
												DragAttributes inDragAttrs,
												ItemReference inItemRef,
												Rect& inItemBounds);
*/	
		// Drag receive is now processed through a callback, so that javascript has a
		// chance to cancel the event.
		virtual void			DoDragReceive(DragReference	inDragRef);
		virtual void			DoDragReceiveCallback(XP_List* inRequestList);
		
		virtual void		MoveBy(
								Int32				inHorizDelta,
								Int32				inVertDelta,
								Boolean				inRefresh);
								
			// COMMANDER
		Boolean					FindCommandStatusForContextMenu(
									CommandT inCommand,
									Boolean	&outEnabled,
									Boolean	&outUsesMark,
									Char16	&outMark,
									Str255	outName);
		virtual void			FindCommandStatus(
									CommandT inCommand,
									Boolean	&outEnabled,
									Boolean	&outUsesMark,
									Char16	&outMark,
									Str255	outName);
		virtual Boolean			ObeyCommand(CommandT inCommand, void* ioParam);
		
	protected:
		virtual void		BeTarget();
		virtual void		DontBeTarget();
		
		virtual void		QueueFTPUpload(const FSSpec & spec, URL_Struct* request);

};


