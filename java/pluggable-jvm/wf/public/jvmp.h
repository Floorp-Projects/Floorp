/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is The Waterfall Java Plugin Module
 *  
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 * 
 * $Id: jvmp.h,v 1.2 2001/07/12 19:58:06 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#ifndef _JVMP_H
#define _JVMP_H
/* temporary include to fix subtle problem with includes on WIn32 with VC6.0 */ 
#include <jni_md.h>
#include <jni.h>

#ifdef _JVMP_IMPLEMENTATION
#define _JVMP_IMPORT_OR_EXPORT JNIEXPORT
#else
#define _JVMP_IMPORT_OR_EXPORT JNIIMPORT
#endif

#include "jvmp_md.h"
#include "jvmp_drawing.h"
#include "jvmp_threading.h"
#include "jvmp_security.h"
#include "jvmp_vendor.h"
#include "jvmp_extension.h"
#include "jvmp_event.h"
#include "jvmp_errors.h"

#ifdef __cplusplus
extern "C" {
#endif
  
struct JVMP_RuntimeContext {
  /**
   * JVM assostiated with this JVMP object - in some implementations 
   * may be zero, so you shouldn't rely on this value heavily.
   */
  JavaVM* jvm;
  /**
   * Returns description of this plugin, including: 
   * underlying JVM version, underlying JVM vendor, vendor-dependent data,
   * JVMP version - for details see jvmp_vendor.h. You shouldn't do memory
   * management on returned value - it's statically allocated.
   */
  jint (JNICALL *JVMP_GetDescription)(JVMP_PluginDescription* *pdesc);
  /**
   * Does all dirty work on loading and starting Java VM
   * with correct arguments. args can be used to pass JVM-specific arguments,
   * usually NULL is OK.
   * Argument allow_reuse, if JNI_TRUE allow to use 
   * already running JVM for plugin purposes. May be dangerous,
   * if running JVM is not the same that plugin is compiled for.
   * Also plugin system classes must be in classpath of this JVM, as current
   * JVM specification doesn't allow change classpath after running JVM.
   * As additional effect, sets jvm field in JVMP_PluginContext to 
   * currently running JVM.
   */
  jint (JNICALL *JVMP_GetRunningJVM)(JVMP_CallingContext* *pctx,
				     void*                 args, 
				     jint                  allow_reuse);
  /**
   * Send generic event directly to PluggableJVM. Can be used to 
   * extend Waterfall API without changing ABI, like ioctl() syscall on Unix.
   * Exact time of call execution depends on "priority"
   * and current state of event queue. At this point I would like to freeze 
   * jvmp.h, and all further API extensions should be done using those 
   * two calls.
   * Every WF implementation can have its own set of commands, but suggetsed 
   * minimum can be found in jvmp_event.h, see JVMP_CMD_*.
   * Note - returned value, if != 0 is result of event handling,
   * so for all JVMP_Send* calls you should test returned value == JNI_FALSE,
   * and avoid returning 0 from custom event handlers, like extension,
   * as such return values means that call failed.
   */
  jint (JNICALL *JVMP_SendSysEvent)(JVMP_CallingContext* ctx,
				    jint                 event, 
				    jlong                data, 
				    jint                 priority);
  /**
   * Async system call. Exact time of call execution depends on "priority"
   * and current state of event queue.
   */
  jint (JNICALL *JVMP_PostSysEvent)(JVMP_CallingContext* ctx,
				    jint                 event, 
				    jlong                data, 
				    jint                 priority);  
  /**
   * Should stop and unload Java VM - not well implemented in JDK1.3 yet.
   * Doesn't stop JVM, if there are unregistered extensions, so unregister 
   * it first.
   */
  jint (JNICALL *JVMP_StopJVM)(JVMP_CallingContext* ctx);
  /**
   * Registers native window handle (OS dependent, see jvmp_drawing.h) in
   * AWT, if succeed you can use PluggableJVM getFrameWithId() method with
   * returned id (>0) to obtain this frame on Java side
   */
  jint (JNICALL *JVMP_RegisterWindow)(JVMP_CallingContext*      ctx, 
				      JVMP_DrawingSurfaceInfo*  win,
				      jint                     *pID);
  /**
   * Unregisters native window, ID is invalid after this call.
   */
  jint (JNICALL *JVMP_UnregisterWindow)(JVMP_CallingContext* ctx, 
					jint                 ID);
  /**
   * Thread synchronization primitives
   */
  /**
   * Registers native monitor object (OS dependent, see jvmp_threading.h) in JVM
   * if succeed you can use PluggableJVM.getSynchroObjectWithID() 
   * to obtain SynchroObject, calling _wait(), _notify(),
   * _notifyAll() on this object will perform those operation on monitor you 
   * passed. It leads to obvious granularity problems, but all operations
   * must be pretty fast, and it works reasonable.
   * In the future, JVM could provide hooks, so it will be possible to use
   * JVMP_MonitorInfo to construct java.lang.Object with passed locks and 
   * monitors, and so Java code shouldn't care about source of synchroobject.
   * Now it would require too much JDK work, so using easiest solution.
   */
  jint (JNICALL *JVMP_RegisterMonitorObject)(JVMP_CallingContext* ctx, 
					     JVMP_MonitorInfo*    monitor, 
					     jint                *pID);
  /** 
   * Unregister monitor and destroy assotiated objects.
   */
  jint (JNICALL *JVMP_UnregisterMonitorObject)(JVMP_CallingContext* ctx, 
					       jint                 ID);
 
  /**
   * Attaches current thread to JVM. Creates java.lang.Thread for given thread,
   * and register this thread in PluggableJVM, so you can reference it using 
   * method getThreadWithId().
   * All synchrooperations must happen only after JVMP_AttachThread call
   * otherwise error is returned. Really, JVMP_GetRunningJVM and JVMP_GetCallingContext
   * call JVMP_AttachCurrentThread, so usually you shouldn't care.
   */
  jint (JNICALL *JVMP_AttachCurrentThread)(JVMP_CallingContext*  ctx, 
					   jint                 *pID);
  /**
   * Detaches current thread from JVM. 
   * This function must be called by native thread before 
   * finishing all JVMP operations, as there's no generic way to register 
   * listener on thread exit - pthread_cleanup_push() is a stupid macros working
   * in one block only. 
   * Also this call frees memory consumed by ctx, so ctx is invalid after this call.
   */
  jint (JNICALL *JVMP_DetachCurrentThread)(JVMP_CallingContext* ctx);
  
  /**
   * Gets calling context for further JVMP_* operation - also can be obtained from
   * GetRunningJVM call. 
   */
  jint (JNICALL *JVMP_GetCallingContext)(JVMP_CallingContext* *pctx);
  /**
   * Extensible event passing(invocation) API.
   */
  /**
   * Create peer (Java object) as event target.
   * vendorID  - is unique for application number,
   * version should be desired version, or 0 if doesn't matter
   * Usually created object is just a wrapper, able to mimic 
   * more specific HA-dependent objects. Way of object negotiation 
   * is HA-specific.
   * ID returned back can be considered as opaque handle
   * for further sync/async events and destroy.
   */
  jint (JNICALL *JVMP_CreatePeer)(JVMP_CallingContext* ctx,
				  const char*          cid,
				  jint                 version,
				  jint                 *target);
  /**
   * Send an synchronous event to Java peer with some data.
   * Waterfall protocol doesn't specify protocol of event passing,
   * and format of passed data, it just provides transport.
   */
  jint (JNICALL *JVMP_SendEvent)(JVMP_CallingContext* ctx,
				 jint                 target,
				 jint                 event,
				 jlong                data,
				 jint                 priority);
  /**
   * Send an asynchronous event to Java peer with some data.
   * Waterfall protocol doesn't specify protocol of event passing,
   * lifetime and format of passed data, it just provides transport.
   */
  jint (JNICALL *JVMP_PostEvent)(JVMP_CallingContext* ctx,
				 jint                 target,
				 jint                 event,
				 jlong                data,
				 jint                 priority);
  /** 
   * Destroy Java peer with given ID.
   * All pending events will be lost (XXX: or not?).
   */
  jint (JNICALL *JVMP_DestroyPeer)(JVMP_CallingContext* ctx,
				   jint                 target);
  
  /**
   * Register Waterfall extension DLL (see jvmp_extension.h).
   * If succeed, after this call, you can use JVMP_CreatePeer
   * with vendorID provided by this extension DLL. 
   * Maybe Unicode string is better, but not sure yet.
   * "data" is arbitrary assotiated with this extension, passed
   * to the start() method of extension.
   */
  jint (JNICALL *JVMP_RegisterExtension)(JVMP_CallingContext* ctx,
					 const char*          extPath,
					 jint                *pID,
					 jlong                data);
  /**
   * Send/post  event to extension-wide bootstrap class - see 
   * JVMPExt_GetBootstrapClass() in jvmp_extension.h.   
   * Could be useful, if ones wish to cooperate with whole extension,
   * not only any given peer. "target" is extension ID returned 
   * by JVMP_RegisterExtension.
   */
  jint (JNICALL *JVMP_SendExtensionEvent)(JVMP_CallingContext* ctx,
					  jint                 target,
					  jint                 event,
					  jlong                data,
					  jint                 priority);
  jint (JNICALL *JVMP_PostExtensionEvent)(JVMP_CallingContext* ctx,
					  jint                 target,
					  jint                 event,
					  jlong                data,
					  jint                 priority);
  /**
   * Unregister Waterfall extension DLL. Really it does  
   * nothing yet. XXX: lifetime
   */
  jint (JNICALL *JVMP_UnregisterExtension)(JVMP_CallingContext* ctx,
					   jint                 ID);
  /**
   * Capabilities handling. To perform priveledged actions, 
   * application must have capability to do it. Those calls intended
   * to change capabilities of current calling context.
   * It's up to HA extension and JVMP to permit/forbid capability changing,
   * using provided pricipal. 
   * ctx and principal much like username/password pair.
   */
  /**
   * Tries to enable caps, authenticating with principals.
   * On success ctx updated with asked caps, otherwise nothing happens
   * and JNI_FALSE returned. Arbitrary set of byte arrays can be used as
   * principal.
   */
  jint (JNICALL *JVMP_EnableCapabilities)(JVMP_CallingContext* ctx,
					  JVMP_SecurityCap*    caps,
					  jint                 num_principals,
					  jint*                principals_len,
					  jbyte*               *principals);
  /**
   * Drop some capabilities. Some system caps (like JVMP_CAP_SYS_PARITY)
   * cannot be dropped.
   */
  jint (JNICALL *JVMP_DisableCapabilities)(JVMP_CallingContext* ctx,
					   JVMP_SecurityCap* caps);
  /**
   * those methods are useful, if some application needs thread mapping
   * "1 -> 1" (not "many -> 1", provided by JVMP_Send* methods). 
   * Generally using of those calls isn't recommended,
   * as it could introduce complicated threading issues in situations,
   * where you don't have direct native->Java threads mapping, like 
   * "green" threads, or remote calls.
   **/
  jint (JNICALL *JVMP_DirectPeerCall)(JVMP_CallingContext* ctx,
				      jint                 target,
				      jint                 arg1,
				      jlong                arg2);
  jint (JNICALL *JVMP_DirectExtCall)(JVMP_CallingContext*  ctx,
				      jint                 target,
				      jint                 arg1,
				      jlong                arg2);
  jint (JNICALL *JVMP_DirectJVMCall)(JVMP_CallingContext*  ctx,
				     jint                 arg1,
				     jlong                arg2);
  /* buf must be big enough to keep  JVMP_MAXERRORLENGTH symbols */
  jint (JNICALL *JVMP_GetLastErrorString)(JVMP_CallingContext*  ctx,
					  char*                 buf);
  /**
   * to keep ABI fixed  - if adding new methods, decrement number in []
   **/
  void*     reserved[10]; 
};

typedef struct JVMP_RuntimeContext JVMP_RuntimeContext;

typedef _JVMP_IMPORT_OR_EXPORT jint (JNICALL *JVMP_GetPlugin_t)
	 (JVMP_RuntimeContext** cx);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* _JVMP_H */
