// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// The contents of this file are subject to the Netscape Public
// License Version 1.1 (the "License"); you may not use this file
// except in compliance with the License. You may obtain a copy of
// the License at http://www.mozilla.org/NPL/
//
// Software distributed under the License is distributed on an "AS
// IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
// implied. See the License for the specific language governing
// rights and limitations under the License.
//
// The Original Code is the JavaScript 2 Prototype.
//
// The Initial Developer of the Original Code is Netscape
// Communications Corporation.  Portions created by Netscape are
// Copyright (C) 1998 Netscape Communications Corporation. All
// Rights Reserved.

#include "world.h"
namespace JS = JavaScript;


// Return an existing StringAtom corresponding to the String s if there is
// one; if not, create a new StringAtom with String s and return that StringAtom.
JS::StringAtom &JS::StringAtomTable::operator[](const String &s)
{
	HT::Reference r(ht, s);
	if (r)
		return *r;
	else
		return ht.insert(r, s);
}
