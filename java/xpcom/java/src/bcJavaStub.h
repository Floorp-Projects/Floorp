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

#ifndef __bcJavaStub_h
#define __bcJavaStub_h
#include "bcIStub.h"
#include "jni.h"
#include "xptinfo.h"

class bcJavaStub : public bcIStub {
 public:
    bcJavaStub(jobject obj);
    virtual ~bcJavaStub();
    virtual void Dispatch(bcICall *call) ;
private:
    jobject object;
    static jclass objectClass;
    static jclass utilitiesClass;
    static jmethodID callMethodByIndexMID;
    void Init();
    PRBool HasRetval(PRUint32 paramCount, nsXPTMethodInfo *info);
};

#endif
