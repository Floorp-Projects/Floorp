/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsIScriptable_h___
#define nsIScriptable_h___

#include "nsIProperties.h"
#include "nsISupportsArray.h"

#define NS_ISCRIPTABLE_IID                           \
{ /* db941bf0-dc17-11d2-9311-00e09805570f */         \
    0xdb941bf0,                                      \
    0xdc17,                                          \
    0x11d2,                                          \
    {0x93, 0x11, 0x00, 0xe0, 0x98, 0x05, 0x57, 0x0f} \
}

class nsIScriptable : public nsIProperties {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_ISCRIPTABLE_IID; return iid; }
    
    NS_IMETHOD Call(const char* command,
                    nsISupportsArray* arguments,
                    nsISupports* *result) = 0;
};

// Returns an implementation of an nsIScriptable that accesses attributes
// and invokes methods of the interface specified by aIID on the specified object.
extern nsresult
NS_NewIScriptable(REFNSIID aIID, nsISupports* object, nsIScriptable* *result);

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIScriptable_h___ */
