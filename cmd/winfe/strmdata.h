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
/* ** */

#ifndef __StreamData_H
//	Avoid include redundancy
//
#define __StreamData_H

//	Purpose:	Allow a mechanism to decide of what type a
//					automated content type converter data object is.
//	Comments:	No source file.
//	Revision History:
//		01-06-95	created GAB
//

//	Required Includes
//

//	Constants
//

//	Structures
//
struct CStreamData	{
	enum	{
		m_DDE = 0,
		m_OLE
	};
	int m_iType;
	
	int GetType() { return m_iType; }
	CStreamData(int iType) : m_iType(iType) { }
};

//	Global variables
//

//	Macros
//

//	Function declarations
//

#endif
