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

#ifndef __H_URobustCreateWindow
#define __H_URobustCreateWindow
#pragma once

/*======================================================================================
	AUTHOR:			Ted Morris <tmorris@netscape.com> - 12 DEC 96

	DESCRIPTION:	Improves upon LWindow::CreateWindow() by making it much more robust
					in the face of exceptions thrown when creating a window.
					
					URobustCreateWindow::CreateWindow() should be used in all places
					where you would normally use the LWindow::CreateWindow() method.

	MODIFICATIONS:

	Date			Person			Description
	----			------			-----------
======================================================================================*/


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

class LWindow;
class LCommander;


#pragma mark -
/*====================================================================================*/
	#pragma mark CLASS DECLARATIONS
/*====================================================================================*/

class URobustCreateWindow {
					  
public:

	static LWindow	*CreateWindow(ResIDT inWindowID, LCommander *inSuperCommander);
	static void		ReadObjects(OSType inResType, ResIDT inResID, void **outFirstObject, 
								OSErr *outErr);
	
private:

	static void		*ObjectsFromStream(LStream *inStream, void **outFirstObject);

};
#endif // __H_URobustCreateWindow

