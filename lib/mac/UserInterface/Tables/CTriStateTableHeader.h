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

// This class implements a tree header that can operate in one of three states, in 
// the following order:
//
// - no sort (natural order)
// - sort forwards
// - sort backwards
//
// When it reaches the end, it cycles back to the start (no sort). 

#pragma once

#include "LTableViewHeader.h"

class CTriStateTableHeader : public LTableViewHeader
{
public:

					CTriStateTableHeader(LStream* inStream);
	virtual			~CTriStateTableHeader();

protected:

	bool CycleSortDirection ( ColumnIndexT & ioColumn ) ;

}; // class CTriStateTableHeader