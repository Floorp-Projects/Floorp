 /*
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
////////////////////////////////////////////////////////////////////////////////////////
//
// 	exception_str.r
// 
//	This file contains the strings used by our exception system. Each
//	string has a corresponding code associated with it in the file "exception_codes.h".
//	These two files should always be in synch.
//
//	These strings are used in an alert where the default text is :
//	"Your last command could not be completed because ^0.  Error number ^1".
//	The strings are substituted for the first argument ( ^0 ).
//
//////////////////////////////////////////////////////////////////////////
//
//	When		Who			What
//	-----		----		-----
//
//	1/6/97		piro		Genesis
//
//////////////////////////////////////////////////////////////////////////

#include "Types.r"
#include "SysTypes.r"
#include "exception_codes.h"

// ¥Êgeneric exceptions
resource 'STR#' ( genericExStringsID, "Generic exceptions" ) {{
	"of an unknown error",			// 1
	"a missing resource"			// 2
}};

// ¥Êbrowser exceptions
resource 'STR#' ( browserExStringsID, "Browser exceptions" ) {{
	"of an unknown browser error" 	// 1
}};

// ¥Êmail exceptions
resource 'STR#' ( mailExStringsID, "Mail exceptions" ) {{
	"of an unknown mail error", 		// 1
	"the preference is missing"			// 2
}};
