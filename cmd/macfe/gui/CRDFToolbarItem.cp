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

//
// Mike Pinkerton, Netscape Communications
//
// Implementations for all the things that can go on a toolbar.
//
// CRDFToolbarItem - the base class for things that go on toolbars
// CRDFPushButton - a toolbar button
// CRDFSeparator - a separator bar
// CRDFURLBar - the url bar w/ proxy icon
//

#include "CRDFToolbarItem.h"

CRDFToolbarItem :: CRDFToolbarItem ( )
{


}


CRDFToolbarItem :: ~CRDFToolbarItem ( )
{



}

#pragma mark -


CRDFPushButton :: CRDFPushButton ( )
{
	DebugStr("\pCreating a CRDFPushButton");

}


CRDFPushButton :: ~CRDFPushButton ( )
{



}

#pragma mark -


CRDFSeparator :: CRDFSeparator ( )
{
	DebugStr("\pCreating a CRDFSeparator");


}


CRDFSeparator :: ~CRDFSeparator ( )
{



}

#pragma mark -


CRDFURLBar :: CRDFURLBar ( )
{
	DebugStr("\pCreating a CRDFURLBar");


}


CRDFURLBar :: ~CRDFURLBar ( )
{



}
