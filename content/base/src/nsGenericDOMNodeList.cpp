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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsGenericDOMNodeList.h"
#include "nsGenericElement.h"

nsGenericDOMNodeList::nsGenericDOMNodeList() 
{
  NS_INIT_REFCNT();
}

nsGenericDOMNodeList::~nsGenericDOMNodeList()
{
}


NS_IMPL_ADDREF(nsGenericDOMNodeList)
NS_IMPL_RELEASE(nsGenericDOMNodeList)


// XPConnect interface list for nsGenericDOMNodeList
NS_CLASSINFO_MAP_BEGIN(NodeList)
  NS_CLASSINFO_MAP_ENTRY(nsIDOMNodeList)
NS_CLASSINFO_MAP_END


// QueryInterface implementation for nsGenericDOMNodeList
NS_INTERFACE_MAP_BEGIN(nsGenericDOMNodeList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNodeList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(NodeList)
NS_INTERFACE_MAP_END


