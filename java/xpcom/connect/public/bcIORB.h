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
 * Igor Kushnirskiy <idk@eng.sun.com>
 */

#ifndef __bcIORB_h
#define __bcIORB_h
#include "bcICall.h"
#include "bcDefs.h"
#include "bcIStub.h"


class bcIORB {
public:
    virtual bcOID RegisterStub(bcIStub *stub) = 0;
    virtual void RegisterStubWithOID(bcIStub *stub, bcOID *oid) = 0;
    virtual bcICall * CreateCall(bcIID *, bcOID *, bcMID) = 0;
    virtual int SendReceive(bcICall *) = 0;
    //virtual IThread * GetThread(TID) = 0;
};

#endif
