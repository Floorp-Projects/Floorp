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

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CDragBarDragTask.h
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#pragma once

#include <LDragTask.h>

class CDragBar;

class CDragBarDragTask :
			public LDragTask
{
	public:
								CDragBarDragTask(
									CDragBar*				inBar,
									const EventRecord& 		inEventRecord);
									
		virtual					~CDragBarDragTask();

		virtual OSErr			DoDrag(void);

		CDragBar*				GetTrackingBar(void);


	protected:
	
		virtual	void			DoNormalDrag(void);
		virtual	void			DoTranslucentDrag(void);
		
		virtual void			AddFlavors(
									DragReference			inDragRef);

		virtual void			MakeDragRegion(
									DragReference			inDragRef,
									RgnHandle				inDragRegion);


		CDragBar*				mBar;
};

inline CDragBar* CDragBarDragTask::GetTrackingBar(void)
	{	return mBar;	}
