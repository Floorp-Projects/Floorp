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

#ifndef __H_CGAIconPopup
#define __H_CGAIconPopup
#pragma once

/*======================================================================================
	AUTHOR:			Ted Morris - 07 NOV 96

	DESCRIPTION:	Implements a class for drawing a popup with associated 16x16 'cicn'
					icons for each item.
					
	MODIFICATIONS:

	Date			Person			Description
	----			------			-----------
======================================================================================*/


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include <LGAPopup.h>


/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CLASS DECLARATIONS
/*====================================================================================*/

#pragma mark -

class CGAIconPopup : public LGAPopup {
				  
public:

						enum { class_ID = 'Gipu' };
						
						enum { cMenuIconWidth = 16, cMenuIconHeight = 16, cMenuIconTitleMargin = 4,
							   cMenuIconType = 'cicn' };

						CGAIconPopup(LStream *inStream) :
									 LGAPopup(inStream) {
						}
						
	Int16				GetTitleIconID(void);
	virtual	void		RefreshMenu(void);
	
protected:

	virtual	void		CalcTitleRect(Rect &outRect);
	virtual	void		CalcLabelRect(Rect &outRect);
	virtual	void		DrawPopupTitle(void);
};


#endif // __H_CGAIconPopup
