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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef __nsMsgSearchArray_h
#define __nsMsgSearchArray_h

#include "nsVoidArray.h"

class nsMsgResultElement;
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

class nsMsgResultArray : public nsVoidArray
{
public:
	nsMsgResultElement *ElementAt(PRUint32 i) const { return (nsMsgResultElement *) nsVoidArray::ElementAt(i); }
};

#endif

