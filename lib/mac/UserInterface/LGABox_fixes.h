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


// LGABox_fixes.h

#pragma once

/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include <LGABox.h>


#pragma mark -
/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/


#pragma mark -
/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/


#pragma mark -
/*====================================================================================*/
	#pragma mark EXTERNAL FUNCTION PROTOTYPES
/*====================================================================================*/


#pragma mark -
/*====================================================================================*/
	#pragma mark CLASS DECLARATIONS
/*====================================================================================*/

class LGABox_fixes : public LGABox {

#if !defined(__MWERKS__) || (__MWERKS__ >= 0x2000)
	typedef LGABox inherited;
#endif

public:

	enum { class_ID = 'Gbo_' };
							LGABox_fixes(LStream *inStream) :
										 LGABox(inStream) {
								
								SetRefreshAllWhenResized(false);
							}

	virtual	void			ResizeFrameBy(Int16 inWidthDelta, Int16 inHeightDelta,
										  Boolean inRefresh);
	virtual	void			RefreshBoxBorder(void);
	virtual	void			RefreshBoxTitle(void);
										  
protected:

	virtual	void			Disable(void);
	virtual	void			Enable(void);
};
					  
