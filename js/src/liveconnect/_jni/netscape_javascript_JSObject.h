/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Declarations of private (internal) functions/data/types for
 * JavaScript <==> Java communication.
 *
 */

#include <jni.h>
/* Header for class netscape_javascript_JSObject */

#ifndef _Included_netscape_javascript_JSObject
#define _Included_netscape_javascript_JSObject
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     netscape_javascript_JSObject
 * Method:    initClass
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_netscape_javascript_JSObject_initClass
  (JNIEnv *, jclass);

/*
 * Class:     netscape_javascript_JSObject
 * Method:    getMember
 * Signature: (Ljava/lang/String;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_netscape_javascript_JSObject_getMember
  (JNIEnv *, jobject, jstring);

/*
 * Class:     netscape_javascript_JSObject
 * Method:    getSlot
 * Signature: (I)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_netscape_javascript_JSObject_getSlot
  (JNIEnv *, jobject, jint);

/*
 * Class:     netscape_javascript_JSObject
 * Method:    setMember
 * Signature: (Ljava/lang/String;Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_netscape_javascript_JSObject_setMember
  (JNIEnv *, jobject, jstring, jobject);

/*
 * Class:     netscape_javascript_JSObject
 * Method:    setSlot
 * Signature: (ILjava/lang/Object;)V
 */
JNIEXPORT void JNICALL Java_netscape_javascript_JSObject_setSlot
  (JNIEnv *, jobject, jint, jobject);

/*
 * Class:     netscape_javascript_JSObject
 * Method:    removeMember
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_netscape_javascript_JSObject_removeMember
  (JNIEnv *, jobject, jstring);

/*
 * Class:     netscape_javascript_JSObject
 * Method:    call
 * Signature: (Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_netscape_javascript_JSObject_call
  (JNIEnv *, jobject, jstring, jobjectArray);

/*
 * Class:     netscape_javascript_JSObject
 * Method:    eval
 * Signature: (Ljava/lang/String;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_netscape_javascript_JSObject_eval
  (JNIEnv *, jobject, jstring);

/*
 * Class:     netscape_javascript_JSObject
 * Method:    toString
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_netscape_javascript_JSObject_toString
  (JNIEnv *, jobject);

/*
 * Class:     netscape_javascript_JSObject
 * Method:    getWindow
 * Signature: (Ljava/applet/Applet;)Lnetscape/javascript/JSObject;
 */
JNIEXPORT jobject JNICALL Java_netscape_javascript_JSObject_getWindow
  (JNIEnv *, jclass, jobject);

/*
 * Class:     netscape_javascript_JSObject
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_netscape_javascript_JSObject_finalize
  (JNIEnv *, jobject);

/*
 * Class:     netscape_javascript_JSObject
 * Method:    equals
 * Signature: (Ljava/lang/Object;)Z
 */
JNIEXPORT jboolean JNICALL Java_netscape_javascript_JSObject_equals
  (JNIEnv *, jobject, jobject);

#ifdef __cplusplus
}
#endif
#endif
