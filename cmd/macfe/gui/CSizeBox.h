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

#ifndef __H_CSizeBox
#define __H_CSizeBox
#pragma once

/*======================================================================================

	DESCRIPTION:	Implements a class for placing a resize box anywhere within a window
					and allowing it to be resized.
					
					The CSizeBox.rsrc resource file should also be included.
					
	MODIFICATIONS:

	Date			Person			Description
	----			------			-----------
======================================================================================*/


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include <LButton.h>


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

class CSizeBox : public LButton {

#if !defined(__MWERKS__) || (__MWERKS__ >= 0x2000)
	typedef LButton inherited;
#endif
				  
public:

						enum { class_ID = 'SiZe' };

						CSizeBox(LStream *inStream) :
								 LButton(inStream) {
						}

protected:
	
	virtual void		ClickSelf(const SMouseDownEvent &inMouseDown);
	virtual void		ActivateSelf(void);
	virtual void		DeactivateSelf(void);
	virtual void		DrawSelf(void);
};


#endif // __H_CSizeBox

