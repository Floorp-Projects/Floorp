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

#pragma once

#include "CBevelView.h"
#include "UStdBevels.h"

//
// CAMSavvyBevelView
//
// A view that bevels itself and its sub views by using appearanace manager
// routines to draw as a window header. This is a concrete implementation
// of the abstract CBevelView class.
//
class CAMSavvyBevelView : public CBevelView
{
	public:
		enum { class_ID = 'BvAM' };
		
							CAMSavvyBevelView(LStream *inStream);
		virtual				~CAMSavvyBevelView();
			
	protected:
	
		virtual	void		DrawBeveledFill(void);
		virtual	void		DrawBeveledFrame(void);
		virtual	void		DrawBeveledSub(const SSubBevel&	inDesc);

}; // class CAMSavvyBevelView
