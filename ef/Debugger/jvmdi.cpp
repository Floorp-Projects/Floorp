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

#include "JavaVM.h"
#include "Debugger.h"
#include "jvmdi.h"
#include "NativeCodeCache.h"
#include "Thread.h"

extern "C" {

static JVMDI_EventHook eventHook;

/*
 * Return via "statusPtr" the status of the thread as one of
 * JVMDI_THREAD_STATUS_*.
 * Errors: JVMDI_ERROR_INVALID_THREAD, JVMDI_ERROR_NULL_POINTER
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_GetThreadStatus(JNIEnv * /* env */, jthread thread, jint *statusPtr)
{
    if (statusPtr == NULL) {
        return JVMDI_ERROR_NULL_POINTER;
    }

	Thread::JavaThread *jt = (Thread::JavaThread *) thread;
	Thread *t = (Thread*)static_cast<int32>(jt->eetop);

    *statusPtr = t->getStatus();

    return JVMDI_ERROR_NONE;
}

/* Note: jvm.h has suspend/resume (which may be deprecated - as
	 * Thread.suspend() has been).  JVMDI version is different, as it
	 * takes precautions to attempt to avoid deadlock.  Also it
	 * cannot be depreciated.
	 */
/*
 * Suspend the specified thread.
 * Errors: JVMDI_ERROR_INVALID_THREAD, JVMDI_ERROR_THREAD_SUSPENDED,
 * JVMDI_ERROR_VM_DEAD
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_SuspendThread(JNIEnv * /*env*/, jthread thread)
{
	Thread::JavaThread *jt = (Thread::JavaThread *) thread;
	Thread *t = (Thread*)static_cast<int32>(jt->eetop);

	t->suspend();

	return JVMDI_ERROR_NONE;
}

/*
 * Resume the specified thread.
 * Errors: JVMDI_ERROR_INVALID_THREAD, JVMDI_ERROR_THREAD_NOT_SUSPENDED,
 * JVMDI_ERROR_VM_DEAD
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_ResumeThread(JNIEnv * /*env*/, jthread thread)
{
	Thread::JavaThread *jt = (Thread::JavaThread *) thread;
	Thread *t = (Thread*)static_cast<int32>(jt->eetop);

	t->resume();

	return JVMDI_ERROR_NONE;
}

/*
 * If shouldStep is true cause the thread to generate a
 * JVMDI_EVENT_SINGLE_STEP event with each byte-code executed.
 * Errors: JVMDI_ERROR_INVALID_THREAD,
 * JVMDI_ERROR_THREAD_NOT_SUSPENDED (should suspension be a requirement?),
 * JVMDI_ERROR_VM_DEAD
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_SetSingleStep(JNIEnv * /*env*/, jthread thread, jboolean shouldStep)
{
	Thread::JavaThread *jt = (Thread::JavaThread *) thread;
	
	jt->single_step = shouldStep;

	return JVMDI_ERROR_NONE;
}



/*
   *  Thread Groups
   */

/*
 * Return in 'groupsPtr' an array of top-level thread groups (parentless
 * thread groups).
 * Note: array returned via 'groupsPtr' is allocated globally and must be
 * explictly freed with DeleteGlobalRef.
 * Errors: JVMDI_ERROR_NULL_POINTER, JVMDI_ERROR_OUT_OF_MEMORY
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_GetTopThreadGroups(JNIEnv * /*env*/, jobjectArray * /*groupsPtr*/)
{
	return JVMDI_ERROR_NOT_IMPLEMENTED;
}


/*
 * Return in 'threadsPtr' an array of the threads within the specified
 * thread group.
 * Note: array returned via 'threadsPtr' is allocated globally and must be
 * explictly freed with DeleteGlobalRef.
 * Errors: JVMDI_ERROR_INVALID_THREAD_GROUP, JVMDI_ERROR_NULL_POINTER,
 * JVMDI_ERROR_OUT_OF_MEMORY
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_GetGroupsThreads(JNIEnv * /*env*/, jthreadGroup /*group*/,
					   jobjectArray * /*threadsPtr*/)
{
	return JVMDI_ERROR_NOT_IMPLEMENTED;
}

/*
 * Return in 'groupsPtr' an array of the thread groups within the
 * specified thread group.
 * Note: array returned via 'groupsPtr' is allocated globally and must be
 * explictly freed with DeleteGlobalRef.
 * Errors: JVMDI_ERROR_INVALID_THREAD_GROUP, JVMDI_ERROR_NULL_POINTER,
 * JVMDI_ERROR_OUT_OF_MEMORY
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_GetGroupsThreadGroups(JNIEnv * /*env*/, jthreadGroup /*group*/,
							jobjectArray * /*groupsPtr*/)
{
	return JVMDI_ERROR_NOT_IMPLEMENTED;
}


/*
 * Suspend all threads recursively contained in the thread group,
 * except the specified threads.
 * If except is NULL suspend all threads in group.
 * Errors: JVMDI_ERROR_INVALID_THREAD_GROUP, JVMDI_ERROR_INVALID_THREAD
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_SuspendThreadGroup(JNIEnv * /*env*/, jthreadGroup /*group*/,
						 jobjectArray /*except*/)
{
	return JVMDI_ERROR_NOT_IMPLEMENTED;
}


/*
 * Resume all threads (recursively) in the thread group, except the
 * specified threads.  If except is NULL resume all threads in group.
 * Errors: JVMDI_ERROR_INVALID_THREAD_GROUP, JVMDI_ERROR_INVALID_THREAD
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_ResumeThreadGroup(JNIEnv * /*env*/, jthreadGroup /*group*/, jobjectArray /*except*/)
{
	return JVMDI_ERROR_NOT_IMPLEMENTED;
}


/*
   *  Stack access
   */

/*
 * Return via "framePtr" the frame ID for the current stack frame
 * of this thread.  Thread must be suspended.  Only Java and
 * Java native frames are returned.  FrameIDs become invalid if
 * the frame is resumed.
 * Errors: JVMDI_ERROR_NO_MORE_FRAMES, JVMDI_ERROR_INVALID_THREAD,
 * JVMDI_ERROR_THREAD_NOT_SUSPENDED, JVMDI_ERROR_NULL_POINTER
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_GetCurrentFrame(JNIEnv * /*env*/, jthread /*thread*/, jframeID* framePtr)
{
	if (framePtr == NULL) {
        return JVMDI_ERROR_NULL_POINTER;
    }

    return JVMDI_ERROR_NONE;
}


/*
 * Return via "framePtr" the frame ID for the stack frame that called
 * the specified frame. Only Java and Java native frames are returned.
 * Errors: JVMDI_ERROR_NO_MORE_FRAMES, JVMDI_ERROR_INVALID_FRAMEID,
 * JVMDI_ERROR_NULL_POINTER
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_GetCallerFrame(JNIEnv * /* env */, jframeID /* called*/, jframeID * /*framePtr*/)
{
	// Use code from the stackwalker here
	return JVMDI_ERROR_NOT_IMPLEMENTED;
}


/*
 * Return via "classPtr" and "methodPtr" the active method in the
 * specified frame.
 * Note: class returned via 'classPtr' is allocated globally and must be
 * explictly freed with DeleteGlobalRef.
 * Errors: JVMDI_ERROR_INVALID_FRAMEID, JVMDI_ERROR_NULL_POINTER
 * JVMDI_ERROR_INVALID_FRAMEID, JVMDI_ERROR_OUT_OF_MEMORY
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_GetFrameMethod(JNIEnv * /*env*/, jframeID /*frame*/,
					 jclass * /*classPtr*/, jmethodID * /*methodPtr*/)
{
	// Use code from the stackwalker here
	return JVMDI_ERROR_NOT_IMPLEMENTED;
}


/*
 * Return via "bciPtr" the byte-code index within the active method of
 * the specified frame.  This is the index of the currently executing
 * instruction.
 * Errors: JVMDI_ERROR_INVALID_FRAMEID, JVMDI_ERROR_NATIVE_FRAME,
 * JVMDI_ERROR_NULL_POINTER
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_GetFrameBCI(JNIEnv * /*env*/, jframeID /*frame*/, jint * /*bciPtr*/)
{
	return JVMDI_ERROR_NOT_IMPLEMENTED;
}


/*
   *  Local variables
   */

/*
 * Return via "valuePtr" the value of the local variable at the
 * specificied slot.
 * Note: object returned via 'valuePtr' is allocated globally and must be
 * explictly freed with DeleteGlobalRef.
 * Errors: JVMDI_ERROR_INVALID_FRAMEID, JVMDI_ERROR_INVALID_SLOT,
 * JVMDI_ERROR_TYPE_MISMATCH, JVMDI_ERROR_NULL_POINTER,
 * JVMDI_ERROR_NATIVE_FRAME, JVMDI_ERROR_OUT_OF_MEMORY
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_GetLocalObject(JNIEnv * /*env*/, jframeID /*frame*/, jint /*slot*/,
					 jobject * /*valuePtr*/)
{
	return JVMDI_ERROR_NOT_IMPLEMENTED;
}


/*
 * Return via "valuePtr" the value of the local variable at the
 * specificied slot.
 * Errors: JVMDI_ERROR_INVALID_FRAMEID, JVMDI_ERROR_INVALID_SLOT,
 * JVMDI_ERROR_TYPE_MISMATCH, JVMDI_ERROR_NULL_POINTER,
 * JVMDI_ERROR_NATIVE_FRAME
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_GetLocalInt(JNIEnv * /*env*/, jframeID /*frame*/, jint /*slot*/,
				  jint * /*valuePtr*/)
{
	return JVMDI_ERROR_NOT_IMPLEMENTED;
}


/*
 * Return via "valuePtr" the value of the local variable at the
 * specificied slot.
 * Errors: JVMDI_ERROR_INVALID_FRAMEID, JVMDI_ERROR_INVALID_SLOT,
 * JVMDI_ERROR_TYPE_MISMATCH, JVMDI_ERROR_NULL_POINTER,
 * JVMDI_ERROR_NATIVE_FRAME
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_GetLocalLong(JNIEnv * /*env*/, jframeID /*frame*/, jint /*slot*/,
				   jlong * /*valuePtr*/)
{
	return JVMDI_ERROR_NOT_IMPLEMENTED;
}


/*
 * Return via "valuePtr" the value of the local variable at the
 * specificied slot.
 * Errors: JVMDI_ERROR_INVALID_FRAMEID, JVMDI_ERROR_INVALID_SLOT,
 * JVMDI_ERROR_TYPE_MISMATCH, JVMDI_ERROR_NULL_POINTER,
 * JVMDI_ERROR_NATIVE_FRAME
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_GetLocalFloat(JNIEnv * /*env*/, jframeID /*frame*/, jint /*slot*/,
					jfloat * /*valuePtr*/)
{
	return JVMDI_ERROR_NOT_IMPLEMENTED;
}


/*
 * Return via "valuePtr" the value of the local variable at the
 * specificied slot.
 * Errors: JVMDI_ERROR_INVALID_FRAMEID, JVMDI_ERROR_INVALID_SLOT,
 * JVMDI_ERROR_TYPE_MISMATCH, JVMDI_ERROR_NULL_POINTER,
 * JVMDI_ERROR_NATIVE_FRAME
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_GetLocalDouble(JNIEnv * /*env*/, jframeID /*frame*/, jint /*slot*/,
					 jdouble * /*valuePtr*/)
{
	return JVMDI_ERROR_NOT_IMPLEMENTED;
}


/*
 * Set the value of the local variable at the specificied slot.
 * Errors: JVMDI_ERROR_INVALID_FRAMEID, JVMDI_ERROR_INVALID_SLOT,
 * JVMDI_ERROR_TYPE_MISMATCH, JVMDI_ERROR_NATIVE_FRAME
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_SetLocalObject(JNIEnv * /*env*/, jframeID /*frame*/, jint /*slot*/, jobject /*value*/)
{
	return JVMDI_ERROR_NOT_IMPLEMENTED;
}


/*
 * Set the value of the local variable at the specificied slot.
 * Errors: JVMDI_ERROR_INVALID_FRAMEID, JVMDI_ERROR_INVALID_SLOT,
 * JVMDI_ERROR_TYPE_MISMATCH, JVMDI_ERROR_NATIVE_FRAME
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_SetLocalInt(JNIEnv * /*env*/, jframeID /*frame*/, jint /*slot*/, jint /*value*/)
{
	return JVMDI_ERROR_NOT_IMPLEMENTED;
}


/*
 * Set the value of the local variable at the specificied slot.
 * Errors: JVMDI_ERROR_INVALID_FRAMEID, JVMDI_ERROR_INVALID_SLOT,
 * JVMDI_ERROR_TYPE_MISMATCH, JVMDI_ERROR_NATIVE_FRAME
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_SetLocalLong(JNIEnv * /*env*/, jframeID /*frame*/, jint /*slot*/, jlong /*value*/)
{
	return JVMDI_ERROR_NOT_IMPLEMENTED;
}


/*
 * Set the value of the local variable at the specificied slot.
 * Errors: JVMDI_ERROR_INVALID_FRAMEID, JVMDI_ERROR_INVALID_SLOT,
 * JVMDI_ERROR_TYPE_MISMATCH, JVMDI_ERROR_NATIVE_FRAME
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_SetLocalFloat(JNIEnv * /*env*/, jframeID /*frame*/, jint /*slot*/, jfloat /*value*/)
{
	return JVMDI_ERROR_NOT_IMPLEMENTED;
}


/*
 * Set the value of the local variable at the specificied slot.
 * Errors: JVMDI_ERROR_INVALID_FRAMEID, JVMDI_ERROR_INVALID_SLOT,
 * JVMDI_ERROR_TYPE_MISMATCH, JVMDI_ERROR_NATIVE_FRAME
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_SetLocalDouble(JNIEnv * /*env*/, jframeID /*frame*/, jint /*slot*/, jdouble /*value*/)
{
	return JVMDI_ERROR_NOT_IMPLEMENTED;
}


/*
 *  Breakpoints
 */

/*
 * Set a breakpoint.  Send a JVMDI_EVENT_BREAKPOINT event when the
 * instruction at the specified byte-code index in the specified
 * method is about to be executed.
 * Errors: JVMDI_ERROR_INVALID_METHODID, JVMDI_ERROR_INVALID_CLASS,
 * JVMDI_ERROR_INVALID_BCI, JVMDI_ERROR_DUPLICATE_BREAKPOINT,
 * JVMDI_ERROR_VM_DEAD, JVMDI_ERROR_OUT_OF_MEMORY
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_SetBreakpoint(JNIEnv * /*env*/, jclass /* clazz */, 
					jmethodID method, jint /*bci*/)
{
	CacheEntry*		ce;
	ce = NativeCodeCache::getCache().lookupByRange((Uint8*) method);
	
	if (ce == NULL) {
		/* either the method is not compiled - in which case compile it */
		return JVMDI_ERROR_INVALID_METHODID;
	}

	void *code = addressFunction(ce->descriptor.method->getCode());

	if (code == NULL) {
		return JVMDI_ERROR_INVALID_METHODID;
	} // else if (code == stub) compile and then break;

	// Convert the bci to a pc offset and add it to code

	VM::debugger.setBreakPoint(code);

	return JVMDI_ERROR_NONE;
}


/*
 * Clear a breakpoint.
 * Errors: JVMDI_ERROR_INVALID_METHODID, JVMDI_ERROR_INVALID_CLASS,
 * JVMDI_ERROR_INVALID_BCI, JVMDI_ERROR_NO_SUCH_BREAKPOINT,
 * JVMDI_ERROR_VM_DEAD
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_ClearBreakpoint(JNIEnv * /*env*/, jclass /*clazz*/, jmethodID /*method*/, jint /*bci*/)
{
	return JVMDI_ERROR_NOT_IMPLEMENTED;
}


/*
 * Clear all breakpoints.
 * Errors: JVMDI_ERROR_VM_DEAD
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_ClearAllBreakpoints(JNIEnv * /*env*/)
{
	VM::debugger.clearAllBreakPoints();

	return JVMDI_ERROR_NONE;
}


/*
 * Set the routine which will handle in-coming events.
 * Passing NULL as hook unsets hook.
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_SetEventHook(JNIEnv * /*env*/, JVMDI_EventHook hook)
{
    eventHook = hook;

	return JVMDI_ERROR_NONE;
}



/*
   *  Method access
   */

/*
 * Return via "namePtr" the method's name and via "signaturePtr" the
 * method's signature.
 * Note: strings returned via 'namePtr' and 'signaturePtr' are
 * allocated globally and must be explictly freed with DeleteGlobalRef.
 * Errors: JVMDI_ERROR_INVALID_METHODID, JVMDI_ERROR_INVALID_CLASS,
 * JVMDI_ERROR_NULL_POINTER, JVMDI_ERROR_OUT_OF_MEMORY
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_GetMethodName(JNIEnv * env, jclass /*clazz*/, jmethodID method,
					jstring *namePtr, jstring *signaturePtr)
{
	CacheEntry*		ce;
	ce = NativeCodeCache::getCache().lookupByRange((Uint8*) method);

    if (namePtr == NULL || signaturePtr == NULL) {
        return JVMDI_ERROR_NULL_POINTER;
    }
    if (ce == NULL) {
        return JVMDI_ERROR_INVALID_METHODID;
    }
    
    *namePtr = (jstring) env->NewGlobalRef(
               env->NewStringUTF(ce->descriptor.method->toString())); 
    *signaturePtr = (jstring) env->NewGlobalRef(
               env->NewStringUTF(ce->descriptor.method->getSignatureString()));
														
    return JVMDI_ERROR_NONE;
}

/*
 * Return via "definingClassPtr" the class in which this method is
 * defined.
 * Note: class returned via 'definingClassPtr' is allocated globally
 * and must be explictly freed with DeleteGlobalRef.
 * Errors: JVMDI_ERROR_INVALID_METHODID, JVMDI_ERROR_INVALID_CLASS,
 * JVMDI_ERROR_NULL_POINTER, JVMDI_ERROR_OUT_OF_MEMORY
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_GetMethodDefiningClass(JNIEnv * env, jclass /* clazz */, jmethodID method,
							 jclass * definingClassPtr)
{
	CacheEntry*		ce;
	ce = NativeCodeCache::getCache().lookupByRange((Uint8*) method);

    if (definingClassPtr == NULL) {
        return JVMDI_ERROR_NULL_POINTER;
    }
    *definingClassPtr = (jclass) (env->NewGlobalRef((jobject)
                            ce->descriptor.method->getDeclaringClass()));

    return JVMDI_ERROR_NONE;
}


/*
 * Return via "isNativePtr" a boolean indicating whether the method
 * is native.
 * Errors: JVMDI_ERROR_INVALID_METHODID, JVMDI_ERROR_INVALID_CLASS,
 * JVMDI_ERROR_NULL_POINTER
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_IsMethodNative(JNIEnv * /* env */, jclass /* clazz */, jmethodID method,
					 jboolean * isNativePtr)
{
	if (isNativePtr == NULL) {
        return JVMDI_ERROR_NULL_POINTER;
    }

	CacheEntry*		ce;
	ce = NativeCodeCache::getCache().lookupByRange((Uint8*) method);

	if (ce == NULL) {
		/* either the method is not compiled - in which case compile it */
		return JVMDI_ERROR_INVALID_METHODID;
	}

    *isNativePtr = ce->descriptor.method->getMethodInfo().isNative();

	return JVMDI_ERROR_NONE;
}


/*
   *  Misc
   */

/*
 * Return via "classesPtr" all classes currently loaded in the VM.
 * Note: array returned via 'classesPtr' is allocated globally
 * and must be explictly freed with DeleteGlobalRef.
 * Errors: JVMDI_ERROR_NULL_POINTER, JVMDI_ERROR_OUT_OF_MEMORY
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_GetLoadedClasses(JNIEnv * /*env*/, jobjectArray * /*classesPtr*/)
{
	return JVMDI_ERROR_NOT_IMPLEMENTED;
}


/*
 * Return via "versionPtr" the JVMDI version number.
 * Higher 16 bits is major version number, lower 16 bit is minor
 * version number.
 * Errors: JVMDI_ERROR_NULL_POINTER
 */
JNIEXPORT JNICALL(jvmdiError)
JVMDI_GetVersionNumber(JNIEnv * /*env*/, jint *versionPtr)
{
    if (versionPtr == NULL) {
        return JVMDI_ERROR_NULL_POINTER;
    }
    *versionPtr = 0x00000003;
    return JVMDI_ERROR_NONE;
}

} // extern C
