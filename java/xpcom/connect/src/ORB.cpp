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

#include "ORB.h"
#include "Call.h"

ORB::ORB() {
    currentID = 1;
    for (int i = 0; i < STUBS_COUNT; i++) {
        stubs[i] = 0;
    }
}


ORB::~ORB() {
}

bcOID ORB::RegisterStub(bcIStub *stub) {
    stubs[currentID] = stub;
    return currentID++;
}

bcICall * ORB::CreateCall(bcIID *iid, bcOID *oid, bcMID mid) {
    return new Call(iid, oid, mid,this);
}

int ORB::SendReceive(bcICall *call) {
    bcIID iid;
    bcOID oid;
    bcMID mid;
    call->GetParams(&iid,&oid,&mid);
    bcIStub *stub = GetStub(&oid);
    if (stub) {
        stub->Dispatch(call);
        return 0;
    } else {
        return 1;  //nb need to think about error values
    }
}

bcIStub * ORB::GetStub(bcOID *oid) {
    return stubs[*oid];
}



