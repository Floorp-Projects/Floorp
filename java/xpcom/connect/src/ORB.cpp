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
#include "nsHashtable.h"

class bcOIDKey : public nsHashKey { 
protected:
    bcOID key;
public:
    bcOIDKey(bcOID oid) {
        key = oid;
    }
    virtual ~bcOIDKey() {
    }
    PRUint32 HashCode(void) const {                                               
        return (PRUint32)key;                                                      
    }                                                                             
                                                                                
    PRBool Equals(const nsHashKey *aKey) const {                                  
        return (key == ((const bcOIDKey *) aKey)->key);                          
    }  
    nsHashKey *Clone() const {                                                    
        return new bcOIDKey(key);                                                 
    }                                      
};

ORB::ORB() {
    currentID = 1;
    stubs = new nsHashtable(256,PR_TRUE);
}


ORB::~ORB() {
    delete stubs;
}

bcOID ORB::RegisterStub(bcIStub *stub) {
    bcOID oid = GenerateOID();
    stubs->Put(new bcOIDKey(oid),stub);
    return oid;
}

void ORB:: RegisterStubWithOID(bcIStub *stub, bcOID *oid) {
    stubs->Put(new bcOIDKey(*oid),stub);
    return;
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
    bcOIDKey *key = new bcOIDKey(*oid);
    void *tmp = stubs->Get(key);
    delete key;
    return (bcIStub*)tmp;
}



struct bcOIDstruct {
    PRUint16 high;
    PRUint16 low;
};
bcOID ORB::GenerateOID() {
    bcOID oid;
    bcOIDstruct oidStruct;
    oidStruct.low = currentID++;
    oidStruct.high = ((PRUint32)this);
    oid = *(bcOID*)&oidStruct;
    return oid;

}





