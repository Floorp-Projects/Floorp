/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Sergey Lunegov <lsv@sparc.spb.su>
 */

#ifndef __urpIConnectComponent_h__
#define __urpIConnectComponent_h_
#include "nsISupports.h"
#include "urpTransport.h"

/*b7f595cb-dd93-4d70-868b-9f9ae35e6181*/
#define URP_CONNECTCOMPONENT_IID \
   { 0xb7f595cb, 0xdd93, 0x4d70, \
    {0x86, 0x8b, 0x9f, 0x9a, 0xe3, 0x5e, 0x61, 0x81}}

class urpIConnectComponent : public nsISupports {
 public:
    NS_DEFINE_STATIC_IID_ACCESSOR(URP_CONNECTCOMPONENT_IID) 
    NS_IMETHOD GetCompMan(char* cntStr, nsISupports** man) = 0;
    NS_IMETHOD GetTransport(char* cntStr, urpTransport** trans) = 0;
};
#endif
