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
/*
 * This file is part of the Java-vendor-neutral implementation of LiveConnect
 *
 * It contains the public XP-COM based interface for java to javascript communication.
 *
 */

#ifndef nsILiveconnect_h___
#define nsILiveconnect_h___

#include "nsISupports.h"
#include "nsIFactory.h"
#include "jni.h"

#define NS_ILIVECONNECT_IID                          \
{ /* 68190910-3318-11d2-97f0-00805f8a28d0 */         \
    0x68190910,                                      \
    0x3318,                                          \
    0x11d2,                                          \
    {0x97, 0xf0, 0x00, 0x80, 0x5f, 0x8a, 0x28, 0xd0} \
}

#define NS_CLIVECONNECT_CID                          \
{ /* b8f0cef0-3931-11d2-97f0-00805f8a28d0 */         \
    0xb8f0cef0,                                      \
    0x3931,                                          \
    0x11d2,                                          \
    {0x97, 0xf0, 0x00, 0x80, 0x5f, 0x8a, 0x28, 0xd0} \
}

typedef jint jsobject;

class nsILiveconnect : public nsISupports {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_ILIVECONNECT_IID)
	NS_DEFINE_STATIC_CID_ACCESSOR(NS_CLIVECONNECT_CID)
	
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
    GetMember(JNIEnv *jEnv, jsobject jsobj, const jchar *name, jsize length, void* principalsArray[], 
                     int numPrincipals, void *pNSISecurityContext, jobject *pjobj) = 0;

    /**
     * get member of a Native JSObject for a given index.
     *
     * @param obj        - A Native JS Object.
     * @param slot      - Index of a member.
     * @param pjobj      - return parameter as a java object representing 
     *                     the member. 
     */
    NS_IMETHOD
    GetSlot(JNIEnv *jEnv, jsobject jsobj, jint slot, void* principalsArray[], 
                     int numPrincipals, void *pNSISecurityContext, jobject *pjobj) = 0;

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
    SetMember(JNIEnv *jEnv, jsobject jsobj, const jchar* name, jsize length, jobject jobj, void* principalsArray[], 
                     int numPrincipals, void *pNSISecurityContext) = 0;

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
    SetSlot(JNIEnv *jEnv, jsobject jsobj, jint slot, jobject jobj, void* principalsArray[], 
                     int numPrincipals, void *pNSISecurityContext) = 0;

    /**
     * remove member of a Native JSObject for a given name.
     *
     * @param obj        - A Native JS Object.
     * @param name       - Name of a member.
     */
    NS_IMETHOD
    RemoveMember(JNIEnv *jEnv, jsobject jsobj, const jchar* name, jsize length,  void* principalsArray[], 
                     int numPrincipals, void *pNSISecurityContext) = 0;

    /**
     * call a method of Native JSObject. 
     *
     * @param obj        - A Native JS Object.
     * @param name       - Name of a method.
     * @param jobjArr    - Array of jobjects representing parameters of method being caled.
     * @param pjobj      - return value.
     */
    NS_IMETHOD
    Call(JNIEnv *jEnv, jsobject jsobj, const jchar* name, jsize length, jobjectArray jobjArr,  void* principalsArray[], 
                     int numPrincipals, void *pNSISecurityContext, jobject *pjobj) = 0;

    /**
     * Evaluate a script with a Native JS Object representing scope.
     *
     * @param obj                - A Native JS Object.
     * @param principalsArray    - Array of principals to be used to compare privileges.
     * @param numPrincipals      - Number of principals being passed.
     * @param script             - Script to be executed.
     * @param pjobj              - return value.
     */
    NS_IMETHOD	
    Eval(JNIEnv *jEnv, jsobject obj, const jchar *script, jsize length, void* principalsArray[], 
         int numPrincipals, void *pNSISecurityContext, jobject *pjobj) = 0;

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
    GetWindow(JNIEnv *jEnv, void *pJavaObject, void* principalsArray[], 
                     int numPrincipals, void *pNSISecurityContext, jsobject *pobj) = 0;

    /**
     * Get the window object for a plugin instance.
     *
     * @param jEnv       - JNIEnv on which the call is being made.
     * @param obj        - A Native JS Object.
     */
    NS_IMETHOD
    FinalizeJSObject(JNIEnv *jEnv, jsobject jsobj) = 0;

    /**
     * Get the window object for a plugin instance.
     *
     * @param jEnv       - JNIEnv on which the call is being made.
     * @param obj        - A Native JS Object.
     * @param jstring    - Return value as a jstring representing a JS object.
     */
    NS_IMETHOD
    ToString(JNIEnv *jEnv, jsobject obj, jstring *pjstring) = 0;
};

#endif // nsILiveconnect_h___
