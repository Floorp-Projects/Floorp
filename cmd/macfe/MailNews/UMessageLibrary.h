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
 * Copyright (C) 1997 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// UMessageLibrary.h

#pragma once
#include "msgcom.h"

//======================================
class UMessageLibrary
// Utility calls
//======================================
{
public:
	enum { invalid_command = 0xFFFFFFFF };

	static MSG_MotionType GetMotionType(CommandT inCommand);
		// Converts a FE command number to a msgcom.h motion command ID.
		// Test result with IsValidMotion()
	static Boolean IsValidMotion(MSG_MotionType cmd) { return cmd != invalid_command; }

	static MSG_CommandType GetMSGCommand(CommandT inCommand);
		// Converts a FE command number to a msgcom.h command ID.
		// Test result with IsValidCommand
	static Boolean IsValidCommand(MSG_CommandType cmd) { return cmd != invalid_command; }
	static Boolean		FindMessageLibraryCommandStatus(
							MSG_Pane*			inPane,
							MSG_ViewIndex*		inIndices,
							int32				inNumIndices,
							CommandT			inCommand,
							Boolean				&outEnabled,
							Boolean				&outUsesMark,
							Char16				&outMark,
							Str255				outName);
}; // class UMessageLibrary
