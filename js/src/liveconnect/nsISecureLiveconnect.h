/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
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
/*
 * This file is part of the Java-vendor-neutral implementation of LiveConnect
 *
 * It contains the public XP-COM based interface for java to javascript communication.
 * This interface allows scripts to be evaluated in a secure fashion.
 *
 */

#ifndef nsISecureLiveconnect_h___
#define nsISecureLiveconnect_h___

#include "nsISupports.h"
#include "nsIFactory.h"
#include "jni.h"

typedef jint jsobject;

class nsISecureLiveconnect : public nsISupports {
public:
    /**
     * Evaluate a script with a Native JS Object representing scope.
     *
     * @param jEnv               - JNIEnv pointer
     * @param obj                - A Native JS Object.
     * @param script             - JavaScript to be executed.
     * @param pNSIPrincipaArray  - Array of principals to be used to compare privileges.
     * @param numPrincipals      - Number of principals being passed.
     * @param context            - Security context.
     * @param pjobj              - return value.
     */
    NS_IMETHOD	
    Eval(JNIEnv *jEnv, jsobject obj, const jchar *script, jsize length, void **pNSIPrincipaArray, 
         int numPrincipals, void *pNSISecurityContext, jobject *pjobj) = 0;
};


#define NS_ISECURELIVECONNECT_IID                          \
{ /* 68190910-3318-11d2-97f0-00805f8a28d0 */         \
    0x68190910,                                      \
    0x3318,                                          \
    0x11d2,                                          \
    {0x97, 0xf0, 0x00, 0x80, 0x5f, 0x8a, 0x28, 0xd0} \
}


#endif // nsILiveconnect_h___
