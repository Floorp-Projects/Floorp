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

//	CProxyDragTask.h


#ifndef CProxyDragTask_H
#define CProxyDragTask_H
#pragma once

// Includes

#include "CBrowserDragTask.h"

#include "CProxyPane.h"

// Forward declarations

class LView;
class CProxyPane;
class LCaption;

// Class declaration

class CExtraFlavorAdder // to be called by AddFlavor.  Allows a window to add extra flavors.
{
	public:
		virtual void AddExtraFlavorData(DragReference inDragRef, ItemReference inItemRef) = 0;
};

class CProxyDragTask : public CBrowserDragTask
{
public:
	typedef CBrowserDragTask Inherited;
	
							CProxyDragTask(
											LView&					inProxyView,
											CProxyPane&				inProxyPane,
											LCaption&				inPageProxyCaption,
											const EventRecord&		inEventRecord,
											CExtraFlavorAdder*		inFlavorAdder = nil);
	virtual	 				~CProxyDragTask();

	virtual OSErr			DoDrag();
	virtual void			AddFlavors(DragReference inDragRef);
						
protected:
	virtual	void			DoNormalDrag();
	virtual	void			DoTranslucentDrag();
		
	virtual void			MakeDragRegion(
											DragReference			inDragRef,
											RgnHandle				inDragRegion);

	LView&					mProxyView;
	CProxyPane&				mProxyPane;
	LCaption&				mPageProxyCaption;
	CExtraFlavorAdder*		mExtraFlavorAdder;
};


#endif
