/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
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
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/*
 * This file is part of the Java-vendor-neutral implementation of LiveConnect
 *
 * It contains the class definition to implement nsILiveconnect XP-COM interface.
 *
 */


#ifndef nsCLiveconnect_h___
#define nsCLiveconnect_h___

#include "nsILiveconnect.h"
#include "nsAgg.h"


/**
 * nsCLiveconnect implements nsILiveconnect interface for navigator.
 * This is used by a JVM to implement netscape.javascript.JSObject functionality.
 */
class nsCLiveconnect :public nsILiveconnect {
public:
    ////////////////////////////////////////////////////////////////////////////
    // from nsISupports and AggregatedQueryInterface:

    NS_DECL_AGGREGATED

    ////////////////////////////////////////////////////////////////////////////
    // from nsILiveconnect:

    /**
     * get member of a Native JSObject for a given name.
     *
     * @param obj        - A Native JS Object.
     * @param name       - Name of a member.
     * @param pjobj      - return parameter as a java object representing 
     *                     the member. If it is a basic data type it is converted to
     *                     a corresponding java type. If it is a NJSObject, then it is
     *                     wrapped up as java wrapper netscape.javascript.JSObject.
     */
    NS_IMETHOD	
    GetMember(JNIEnv *jEnv, jsobject obj, const jchar *name, jsize length, void* principalsArray[], 
              int numPrincipals, nsISupports *securitySupports, jobject *pjobj);

    /**
     * get member of a Native JSObject for a given index.
     *
     * @param obj        - A Native JS Object.
     * @param index      - Index of a member.
     * @param pjobj      - return parameter as a java object representing 
     *                     the member. 
     */
    NS_IMETHOD	
    GetSlot(JNIEnv *jEnv, jsobject obj, jint slot, void* principalsArray[], 
            int numPrincipals, nsISupports *securitySupports,  jobject *pjobj);

    /**
     * set member of a Native JSObject for a given name.
     *
     * @param obj        - A Native JS Object.
     * @param name       - Name of a member.
     * @param jobj       - Value to set. If this is a basic data type, it is converted
     *                     using standard JNI calls but if it is a wrapper to a JSObject
     *                     then a internal mapping is consulted to convert to a NJSObject.
     */
    NS_IMETHOD	
    SetMember(JNIEnv *jEnv, jsobject obj, const jchar *name, jsize length, jobject jobj, void* principalsArray[], 
              int numPrincipals, nsISupports *securitySupports);

    /**
     * set member of a Native JSObject for a given index.
     *
     * @param obj        - A Native JS Object.
     * @param index      - Index of a member.
     * @param jobj       - Value to set. If this is a basic data type, it is converted
     *                     using standard JNI calls but if it is a wrapper to a JSObject
     *                     then a internal mapping is consulted to convert to a NJSObject.
     */
    NS_IMETHOD	
    SetSlot(JNIEnv *jEnv, jsobject obj, jint slot, jobject jobj,  void* principalsArray[], 
            int numPrincipals, nsISupports *securitySupports);

    /**
     * remove member of a Native JSObject for a given name.
     *
     * @param obj        - A Native JS Object.
     * @param name       - Name of a member.
     */
    NS_IMETHOD	
    RemoveMember(JNIEnv *jEnv, jsobject obj, const jchar *name, jsize length,  void* principalsArray[], 
                 int numPrincipals, nsISupports *securitySupports);

    /**
     * call a method of Native JSObject. 
     *
     * @param obj        - A Native JS Object.
     * @param name       - Name of a method.
     * @param jobjArr    - Array of jobjects representing parameters of method being caled.
     * @param pjobj      - return value.
     */
    NS_IMETHOD	
    Call(JNIEnv *jEnv, jsobject obj, const jchar *name, jsize length, jobjectArray jobjArr, void* principalsArray[], 
         int numPrincipals, nsISupports *securitySupports, jobject *pjobj);

    /**
     * Evaluate a script with a Native JS Object representing scope.
     *
     * @param obj                - A Native JS Object.
     * @param pNSIPrincipaArray  - Array of principals to be used to compare privileges.
     * @param numPrincipals      - Number of principals being passed.
     * @param script             - Script to be executed.
     * @param pjobj              - return value.
     */
    NS_IMETHOD	
    Eval(JNIEnv *jEnv, jsobject obj, const jchar *script, jsize length, void* principalsArray[], 
         int numPrincipals, nsISupports *securitySupports, jobject *pjobj);

    /**
     * Get the window object for a plugin instance.
     *
     * @param pJavaObject        - Either a jobject or a pointer to a plugin instance 
     *                             representing the java object.
     * @param pjobj              - return value. This is a native js object 
     *                             representing the window object of a frame 
     *                             in which a applet/bean resides.
     */
    NS_IMETHOD	
    GetWindow(JNIEnv *jEnv, void *pJavaObject,  void* principalsArray[], 
              int numPrincipals, nsISupports *securitySupports, jsobject *pobj);

    /**
     * Get the window object for a plugin instance.
     *
     * @param jEnv       - JNIEnv on which the call is being made.
     * @param obj        - A Native JS Object.
     */
    NS_IMETHOD	
    FinalizeJSObject(JNIEnv *jEnv, jsobject obj);

    /**
     * Get the window object for a plugin instance.
     *
     * @param jEnv       - JNIEnv on which the call is being made.
     * @param obj        - A Native JS Object.
     * @param jstring    - Return value as a jstring representing a JS object.
     */
    NS_IMETHOD
    ToString(JNIEnv *jEnv, jsobject obj, jstring *pjstring);
    ////////////////////////////////////////////////////////////////////////////
    // from nsCLiveconnect:

    nsCLiveconnect(nsISupports *aOuter);
    virtual ~nsCLiveconnect(void);

protected:
    void* mJavaClient;
};

#endif // nsCLiveconnect_h___
