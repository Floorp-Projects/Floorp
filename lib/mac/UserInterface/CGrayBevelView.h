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

//
// Note from pinkerton:
//
// This class is now obsolete in Mozilla. Use it at your own risk. Better yet, use
// CAMSavvyBevelView to get appearance manager support
//

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//		A beveled gray view that can draw raised, recessed, or no bevel
//		for any of its subviews.
//
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#pragma once

#include "CBevelView.h"
#include "UStdBevels.h"

class CGrayBevelView : public CBevelView
{
	public:
		enum { class_ID = 'GbVw' };
		
							CGrayBevelView(LStream *inStream);
		virtual				~CGrayBevelView();
			
		const SBevelColorDesc& GetBevelColors() const;

	protected:
	
		virtual	void		DrawBeveledFill(void);
		virtual	void		DrawBeveledFrame(void);
		virtual	void		DrawBeveledSub(const SSubBevel&	inDesc);

		SBevelColorDesc		mBevelColors;
};

inline const SBevelColorDesc& CGrayBevelView::GetBevelColors() const
	{	return mBevelColors;		}
	
