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
#include "java_lang_Thread.h"
#include "Thread.h"

extern "C" {
/*
 * Class : java/lang/Thread
 * Method : currentThread
 * Signature : ()Ljava/lang/Thread;
 */
NS_EXPORT NS_NATIVECALL(Java_java_lang_Thread *)
Netscape_Java_java_lang_Thread_currentThread()
{
	return (Java_java_lang_Thread *)Thread::currentThread();
}


/*
 * Class : java/lang/Thread
 * Method : yield
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void) 
Netscape_Java_java_lang_Thread_yield()
{
	Thread::yield();
}


/*
 * Class : java/lang/Thread
 * Method : sleep
 * Signature : (J)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Thread_sleep(int64 l)
{
	Thread::sleep(l);
}


/*
 * Class : java/lang/Thread
 * Method : start
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Thread_start(Java_java_lang_Thread *t)
{
	Thread* thr = (Thread*)static_cast<int32>(t->eetop);
	if (thr == NULL) {
		//thr = new(*Thread::pool) Thread((JavaObject*)t);
		thr = new Thread((JavaObject*)t);
		t->eetop = (int64)thr;
	}
	thr->start();
}


/*
 * Class : java/lang/Thread
 * Method : isInterrupted
 * Signature : (Z)Z
 */
NS_EXPORT NS_NATIVECALL(uint32) /* bool */ 
Netscape_Java_java_lang_Thread_isInterrupted(Java_java_lang_Thread *t, uint32 f)
{
	Thread* thr = (Thread*)static_cast<int32>(t->eetop);
	if (thr == NULL) {
		//thr = new(*Thread::pool) Thread((JavaObject*)t);
		thr = new Thread((JavaObject*)t);
		t->eetop = (int64)thr;
	}
	return thr->isInterrupted(f);
}


/*
 * Class : java/lang/Thread
 * Method : isAlive
 * Signature : ()Z
 */
NS_EXPORT NS_NATIVECALL(uint32) /* bool */ 
Netscape_Java_java_lang_Thread_isAlive(Java_java_lang_Thread *t)
{
  	Thread* thr = (Thread*)static_cast<int32>(t->eetop);
	if (thr == NULL) {
		//thr = new(*Thread::pool) Thread((JavaObject*)t);
		thr = new Thread((JavaObject*)t);
		t->eetop = (int64)thr;
	}
	return thr->isAlive();
}


/*
 * Class : java/lang/Thread
 * Method : countStackFrames
 * Signature : ()I
 */
NS_EXPORT NS_NATIVECALL(int32)
Netscape_Java_java_lang_Thread_countStackFrames(Java_java_lang_Thread *t)
{
  	Thread* thr = (Thread*)static_cast<int32>(t->eetop);
	if (thr == NULL) {
		//thr = new(*Thread::pool) Thread((JavaObject*)t);
		thr = new Thread((JavaObject*)t);
		t->eetop = (int64)thr;
	}
	return thr->countStackFrames();
}


/*
 * Class : java/lang/Thread
 * Method : setPriority0
 * Signature : (I)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Thread_setPriority0(Java_java_lang_Thread *t, int32 p)
{
  	Thread* thr = (Thread*)static_cast<int32>(t->eetop);
	if (thr == NULL) {
		//thr = new(*Thread::pool) Thread((JavaObject*)t);
		thr = new Thread((JavaObject*)t);
		t->eetop = (int64)thr;
	}
	thr->setPriority(p);
}


/*
 * Class : java/lang/Thread
 * Method : stop0
 * Signature : (Ljava/lang/Object;)V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Thread_stop0(Java_java_lang_Thread *t, Java_java_lang_Object *exc)
{
  	Thread* thr = (Thread*)static_cast<int32>(t->eetop);
	if (thr == NULL) {
		//thr = new(*Thread::pool) Thread((JavaObject*)t);
		thr = new Thread((JavaObject*)t);
		t->eetop = (int64)thr;
	}
	thr->stop((JavaObject*)exc);
}


/*
 * Class : java/lang/Thread
 * Method : suspend0
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Thread_suspend0(Java_java_lang_Thread *t)
{
  	Thread* thr = (Thread*)static_cast<int32>(t->eetop);
	if (thr == NULL) {
		//thr = new(*Thread::pool) Thread((JavaObject*)t);
		thr = new Thread((JavaObject*)t);
		t->eetop = (int64)thr;
	}
	thr->suspend();
}


/*
 * Class : java/lang/Thread
 * Method : resume0
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Thread_resume0(Java_java_lang_Thread *t)
{
  	Thread* thr = (Thread*)static_cast<int32>(t->eetop);
	if (thr == NULL) {
		//thr = new(*Thread::pool) Thread((JavaObject*)t);
		thr = new Thread((JavaObject*)t);
		t->eetop = (int64)thr;
	}
	thr->resume();
}


/*
 * Class : java/lang/Thread
 * Method : interrupt0
 * Signature : ()V
 */
NS_EXPORT NS_NATIVECALL(void)
Netscape_Java_java_lang_Thread_interrupt0(Java_java_lang_Thread *t)
{
  	Thread* thr = (Thread*)static_cast<int32>(t->eetop);
	if (thr == NULL) {
		//thr = new(*Thread::pool) Thread((JavaObject*)t);
		thr = new Thread((JavaObject*)t);
		t->eetop = (int64)thr;
	}
	thr->interrupt();
}


} /* extern "C" */

