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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef __nsMsgSearchArray_h
#define __nsMsgSearchArray_h

#include "nsVoidArray.h"

class nsMsgSearchTerm;
class nsMsgSearchScopeTerm;
struct nsMsgSearchValue;

class nsMsgSearchTermArray : public nsVoidArray
{
public:
	nsMsgSearchTerm *ElementAt(PRUint32 i) const { return (nsMsgSearchTerm*) nsVoidArray::ElementAt(i); }
};

class nsMsgSearchValueArray : public nsVoidArray
{
public:
	nsMsgSearchValue *ElementAt(PRUint32 i) const { return (nsMsgSearchValue*) nsVoidArray::ElementAt(i); }
};

class nsMsgSearchScopeTermArray : public nsVoidArray
{
public:
	nsMsgSearchScopeTerm *ElementAt(PRUint32 i) const { return (nsMsgSearchScopeTerm*) nsVoidArray::ElementAt(i); }
};

#endif

