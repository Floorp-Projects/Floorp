/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Author:
 *   Adam Lock <adamlock@netscape.com>
 *
 * Contributor(s): 
 */
#ifndef PROPERTYLIST_H
#define PROPERTYLIST_H


// Property is a name,variant pair held by the browser. In IE, properties
// offer a primitive way for DHTML elements to talk back and forth with
// the host app.

struct Property
{
  CComBSTR szName;
  CComVariant vValue;
};

// A list of properties
typedef std::vector<Property> PropertyList;


// DEVNOTE: These operators are required since the unpatched VC++ 5.0
//          generates code even for unreferenced template methods in
//          the file <vector>  and will give compiler errors without
//          them. Service Pack 1 and above fixes this problem

int operator <(const Property&, const Property&);
int operator ==(const Property&, const Property&); 


#endif