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

#ifndef __ORB_h
#define __ORB_h
#include  "bcIORB.h"

#define STUBS_COUNT (1000)
class ORB : public bcIORB {
public:
    ORB();
    virtual ~ORB();
    virtual bcOID RegisterStub(bcIStub *stub);
    virtual bcICall * CreateCall(bcIID *, bcOID *, bcMID);
    virtual int SendReceive(bcICall *);
private:
    bcIStub * GetStub(bcOID *);
    bcIStub * stubs[STUBS_COUNT]; //nb :) it's jast for now. (Mon Mar 13 16:53:03 PST 2000)
    int currentID;
};
#endif
