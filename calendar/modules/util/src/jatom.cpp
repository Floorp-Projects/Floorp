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

// jatom.cpp
// John Sun
// 4:37 PM February 27 1998


#include "stdafx.h"
#include "jdefines.h"
#include "jatom.h"

#include <unistring.h>

//---------------------------------------------------------------------

JAtom::JAtom()
{
    m_HashCode = -1;
}

//---------------------------------------------------------------------

void
JAtom::setString(const UnicodeString & string)
{
    //m_String = string;
    m_HashCode = string.hashCode();
}

//---------------------------------------------------------------------

JAtom::JAtom(const UnicodeString & string)
{
    //m_String = string;
    m_HashCode = string.hashCode();
}
//---------------------------------------------------------------------

