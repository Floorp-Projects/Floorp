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

#include "CPatternButtonPopupText.h"

class CEditView;


class CColorPopup : 	public CPatternButtonPopupText
{
	public:
		enum { class_ID = 'Cpop' };
		enum { COLOR_DISPLAY_TEXT = 0,	// need to fix "*" bug before turning on (among other things)
		 		COLOR_FRAME_BORDER = 2,
				COLOR_BOX_WIDTH = 14,
				COLOR_BOX_HEIGHT = 14,
				COLOR_HEX_DISPLAY_SIZE = 16
		 	};
		 enum {
		 		LAST_COLOR_PICKED_CHAR = '<',
		 		LAST_COLOR_PICKED_ITEM = 1,
		 		COLOR_CHIP_CHAR = '#',
		 		COLOR_PICKER_CHAR = '*',
		 		COLOR_PICKER_ITEM = 62,
		 		DEFAULT_COLOR_CHAR = '@',
		 		DEFAULT_COLOR_ITEM = 72,
		 		CURRENT_COLOR_CHAR = '.',
		 		CURRENT_COLOR_ITEM = 73
		 	};

	typedef CPatternButtonPopupText super;
	
		
	static	void*	CreateCColorPopupStream( LStream *inStream ) {return( new CColorPopup (inStream ));};
					CColorPopup( LStream *inStream );		//	¥ Constructor
	virtual void	FinishCreateSelf();
	
						// ¥ drawing
	void			DrawButtonContent(void);
	void 			DrawButtonTitle(void);
	void			DrawPopupArrow(void);
	void			DrawButtonGraphic(void);

						// ¥ hooks
	virtual void 		SetValue( Int32 inValue );
	virtual void		AdjustMenuContents();
	virtual	void		HandlePopupMenuSelect( Point inPopupLoc, Int16 inCurrentItem,
												Int16& outMenuID, Int16& outMenuItem );
	virtual Boolean		HandleNewValue(Int32 inNewValue);
	virtual Boolean		GetMenuItemRGBColor(short menuItem, RGBColor *theColor);
	virtual short		GetMenuItemFromRGBColor(RGBColor *theColor);
	
	void			InitializeCurrentColor();

protected:
	virtual void		HandleEnablingPolicy();
	CEditView			*mEditView;
	Boolean				mDoSetLastPickedPreference;
};
