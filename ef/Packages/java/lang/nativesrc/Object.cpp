/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "java_lang_Object.h"
#include <stdio.h>
#include "Monitor.h"
#include "Exceptions.h"

extern "C" {
    
static inline JavaObject &toObject(Java_java_lang_Object &inObject)
{
    return *(Class *)(&inObject); 
}

/*
 * Class : java/lang/Object
 * Method : getClass
 * Signature : ()Ljava/lang/Class;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Class *)
Netscape_Java_java_lang_Object_getClass(Java_java_lang_Object *obj)
{
  return (Java_java_lang_Class *) (&toObject(*obj).getType());
}


/*
 * Class : java/lang/Object
 * Method : hashCode
 * Signature : ()I
 */
NS_EXPORT NS_NATIVECALL(int32)
Netscape_Java_java_lang_Object_hashCode(Java_java_lang_Object *obj)
{
    // XXX - This won't work once a relocating GC is implemented
    return reinterpret_cast<int32>(obj);
}


/*
 * Class : java/lang/Object
 * Method : clone
 * Signature : ()Ljava/lang/Object;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Object *)
Netscape_Java_java_lang_Object_clone(Java_java_lang_Object *inObj)
{
    JavaObject *newObj = ((Class *) toObject(*inObj).getClass().clone(*inObj));
  
    return (Java_java_lang_Object *) (newObj);
}


/*
 * Class : java/lang/Object
 * Method : notify
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Object_notify(Java_java_lang_Object *o)
{
	if (o == NULL) {
		sysThrowNamedException("java/lang/NullPointerException");
		return;
	}
    Monitor::notify(*o);
}


/*
 * Class : java/lang/Object
 * Method : notifyAll
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Object_notifyAll(Java_java_lang_Object *o)
{
	if (o == NULL) {
		sysThrowNamedException("java/lang/NullPointerException");
		return;
	}
    Monitor::notifyAll(*o);
}


/*
 * Class : java/lang/Object
 * Method : wait
 * Signature : (J)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Object_wait(Java_java_lang_Object *o, int64 l)
{
	if (o == NULL) {
		sysThrowNamedException("java/lang/NullPointerException");
		return;
	}
    Monitor::wait(*o,l);
}


} /* extern "C" */

