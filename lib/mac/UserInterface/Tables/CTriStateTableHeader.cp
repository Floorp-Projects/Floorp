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

// This class implements a tree header that can operate in one of three states, in 
// the following order:
//
// - no sort (natural order)
// - sort forwards
// - sort backwards
//
// When it reaches the end, it cycles back to the start (no sort). 

#include "CTriStateTableHeader.h"

CTriStateTableHeader::CTriStateTableHeader ( LStream* inStream )
:	LTableViewHeader(inStream)
{
}

CTriStateTableHeader :: ~CTriStateTableHeader ( )
{
}

//
// CycleSortDirection
//
// Called when the user clicks (not drags) in a column header.
//
bool
CTriStateTableHeader :: CycleSortDirection ( ColumnIndexT & ioColumn ) 
{
	if ( ioColumn == mSortedColumn ) {
		if ( mSortedBackwards ) {
			ioColumn = LArray::index_Bad;
			mSortedBackwards = true;		// so inversion below will set it to false
		}
		return !mSortedBackwards;
	}
	
	// a new column was clicked, just return current sort direction
	return mSortedBackwards;

} // CycleSortDirection