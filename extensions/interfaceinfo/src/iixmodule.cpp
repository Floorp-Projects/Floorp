/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* Module level methods. */

#include "iixprivate.h"

/* Module implementation for the interface info extras library. */

// {B404670D-1EB1-4af8-86E5-FB3E9C4ECC4B}
#define NS_GENERIC_INTERFACE_INFO_SET_CID  \
{ 0xb404670d, 0x1eb1, 0x4af8, \
    { 0x86, 0xe5, 0xfb, 0x3e, 0x9c, 0x4e, 0xcc, 0x4b } }


// {ED150A6A-4592-4e2c-82F8-70C8D65F74D2}
#define NS_SCRIPTABLE_INTERFACE_INFO_CID  \
{ 0xed150a6a, 0x4592, 0x4e2c, \
    { 0x82, 0xf8, 0x70, 0xc8, 0xd6, 0x5f, 0x74, 0xd2 } }


NS_GENERIC_FACTORY_CONSTRUCTOR(nsGenericInterfaceInfoSet)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScriptableInterfaceInfo)

static const nsModuleComponentInfo components[] = {
  {nsnull, 
   NS_GENERIC_INTERFACE_INFO_SET_CID, 
   NS_GENERIC_INTERFACE_INFO_SET_CONTRACTID, 
   nsGenericInterfaceInfoSetConstructor},
  {nsnull, 
   NS_SCRIPTABLE_INTERFACE_INFO_CID, 
   NS_SCRIPTABLE_INTERFACE_INFO_CONTRACTID, 
   nsScriptableInterfaceInfoConstructor}
};

NS_IMPL_NSGETMODULE(iiextras, components)
