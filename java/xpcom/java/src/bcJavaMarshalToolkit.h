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

#ifndef _bcJavaMarshalToolkit_h
#define _bcJavaMarshalToolkit_h

#include "nscore.h"
#include "xptcall.h"
#include "nsISupports.h"
#include "jni.h"
#include "bcIMarshaler.h"
#include "bcIUnMarshaler.h"
#include "bcIORB.h"

class bcJavaMarshalToolkit {
public: 
    bcJavaMarshalToolkit(PRUint16 methodIndex,
                        nsIInterfaceInfo *interfaceInfo, jobjectArray args, 
                         JNIEnv *env, int isOnServer, bcIORB *orb) ;
    virtual ~bcJavaMarshalToolkit();
    nsresult Marshal(bcIMarshaler *);
    nsresult Marshal(bcIMarshaler *, jobject);
    nsresult UnMarshal(bcIUnMarshaler *);
    nsresult UnMarshal(bcIUnMarshaler *, jobject*);
private:

    jobject retV;

    enum ArrayModifier { none, arrayElement, array}; 
    enum { unDefined, onServer, onClient } callSide; 
    JNIEnv *env;
    PRUint16 methodIndex;
    nsXPTMethodInfo *info;
    nsIInterfaceInfo * interfaceInfo;
    bcIORB *orb;
    jobjectArray args;
    
    static jclass objectClass;
    static jclass objectArrayClass;
    static jclass booleanClass;
    static jclass booleanArrayClass;
    static jmethodID booleanInitMID;
    static jmethodID booleanValueMID;
    static jclass charClass;
    static jclass charArrayClass;
    static jmethodID charInitMID;
    static jmethodID charValueMID;
    static jclass byteClass;
    static jclass byteArrayClass;
    static jmethodID byteInitMID;
    static jmethodID byteValueMID;
    static jclass shortClass;
    static jclass shortArrayClass;
    static jmethodID shortInitMID;
    static jmethodID shortValueMID;
    static jclass intClass;
    static jclass intArrayClass;
    static jmethodID intInitMID;
    static jmethodID intValueMID;
    static jclass longClass;
    static jclass longArrayClass;
    static jmethodID longInitMID;
    static jmethodID longValueMID;
    static jclass floatClass;
    static jclass floatArrayClass;
    static jmethodID floatInitMID;
    static jmethodID floatValueMID;
    static jclass doubleClass;
    static jclass doubleArrayClass;
    static jmethodID doubleInitMID;
    static jmethodID doubleValueMID;

    static jclass stringClass;
    static jclass stringArrayClass;

    static jclass iidClass;
    static jclass iidArrayClass;

    static jmethodID getClassMID;

    void InitializeStatic();
    void DeInitializeStatic();
    bcXPType XPTType2bcXPType(uint8 type);
    //    nsresult MarshalElement(bcIMarshaler *m, jobject array,  nsXPTParamInfo * param, bcXPType type, 
    //                      uint8 ind, ArrayModifier modifier = none);

    nsresult 
    MarshalElement(bcIMarshaler *m, jobject value, PRBool isOut,  nsXPTParamInfo * param, 
                   bcXPType type, uint8 ind, ArrayModifier modifier = none);
    nsresult 
    UnMarshalElement(jobject *value, uint8 ind, bcIUnMarshaler *um, PRBool isOut , nsXPTParamInfo * param, 
                     bcXPType type, bcIAllocator *allocator, ArrayModifier modifier = none);


};

#endif


