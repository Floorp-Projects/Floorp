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

//	CBrowserDragTask.h


#ifndef CBrowserDragTask_H
#define CBrowserDragTask_H
#pragma once

// Includes

#include <LDragTask.h>

// Class declaration

class CBrowserDragTask : public LDragTask
{
public:
	typedef LDragTask super;
	
							CBrowserDragTask( const EventRecord& inEventRecord );
	virtual	 				~CBrowserDragTask();
						
protected:
	void					AddFlavorBookmark(ItemReference inItemRef, const char* inData = nil);
	void					AddFlavorBookmarkFile(ItemReference inItemRef);
	void					AddFlavorURL(ItemReference inItemRef);

	virtual void			AddFlavors(DragReference inDragRef);
};


#endif
