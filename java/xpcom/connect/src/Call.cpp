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

#include "Call.h"
#include "Marshaler.h"
#include "UnMarshaler.h"

Call::Call() {
}

Call::Call(bcIID *_iid, bcOID *_oid, bcMID _mid, bcIORB *_orb):out(0),in(0) {
    iid = *_iid;
    oid = *_oid;
    mid = _mid;
    orb = _orb;
}

Call::~Call() {
    if (out) 
        delete out;
    if (in)
        delete in;
}

int Call::GetParams(bcIID *_iid, bcOID *_oid, bcMID *_mid) {
    *_iid = iid;
    *_oid = oid;
    *_mid = mid;
    return 0;
}

bcIMarshaler * Call::GetMarshaler() {
    out = new ostrstream();
    return new Marshaler(out);
}

bcIUnMarshaler * Call::GetUnMarshaler() {
    if (!out) {
        return NULL;
    }
    char  *buf = out->str();
    //    cout<<"Call::GetUnMarshaler "<<out->pcount()<<"\n";
#if 0
    cout<<"Call::GetUnMarshaler buf:\n";
    for (int i = 0; i < out->pcount(); i++) {
	cout<<"  buf["<<i<<"]"<<(unsigned)buf[i]<<"\n";
    }
#endif
    if (out->pcount()) {
        in = new istrstream(buf,out->pcount());
    }
    return new UnMarshaler(in);
}

bcIORB * Call::GetORB() {
    return orb;
}


