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
 * $Id: jvmp_extension.h,v 1.2 2001/07/12 19:58:07 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#ifndef _JVMP_EXTENSION_H
#define _JVMP_EXTENSION_H
#ifdef __cplusplus
extern "C" {
#endif
/* XXX: this include should be more generic - different transports */ 
#include "shmtran.h"
#include "jvmp_caps.h"
/**
 * Usage scenario:
 * HA: CreatePeer(id) -> JVM: is extension inited?
 * if not yet - look in registry of registred extensions, or just 
 * ask all registred ext DLL - can they handle this type - use latest
 * version of handler, call JVMPExt_Init() of matching DLL,
 * load and run bootstrap class
 * if yes - just instanciate proper peer, using special factory
 * registred by bootstrap class in PluggableJVM.
 */

struct JVMP_Extension {
  /**
   * Init native part of this extension.
   * Argument is side on which extension is loaded, if in 
   * separate process situation (0 - not separate process case
   * 1 - host side, 2 - JVM side).
   */
  jint (JNICALL *JVMPExt_Init)(jint side);
  /**
   * Shutdown this extension. Usually means nobody wants it anymore.
   */
  jint (JNICALL *JVMPExt_Shutdown)();
  /**
   * the only function which can be called before Init 
   * pCID - contract ID, pVersion - version of handler
   */
  jint (JNICALL *JVMPExt_GetExtInfo)(const char*          *pCID, 
				     jint                 *pVersion, 
				     JVMP_ExtDescription* *desc);
  /**
   * Return classpath to find bootstrap Java class, and its name
   * Classpath is "|"-separated list of URLs, like
   * jar:file:///usr/java/ext/mozilla/moz6_ext.jar|file:///usr/java/ext/mozilla/classes|http://www.mozilla.org/Waterfall/classes
   * Name is smth like: sun.jvmp.mozilla.MozillaPeerFactory
   */
  jint (JNICALL *JVMPExt_GetBootstrapClass)(char* *bootstrapClassPath,
					    char* *bootstrapClassName);
  /** 
   * This is extension's callback to schedule request in host 
   * in different proc or STA situation.
   * Different methods executed depending 
   * on funcno in request, use macros to numerate your functions:
   * JVMP_NEW_EXT_FUNCNO(vendorID, funcno). Implementation of this callback 
   * is application/platform dependent. It can be implemented using
   * XtAddEventHandler on X, SendMessage on Win32.
   * If local == JNI_TRUE - just execute this request locally.
   */ 
  jint (JNICALL *JVMPExt_ScheduleRequest)(JVMP_ShmRequest* req, jint local);
  
  /**
   * This function reserves capabilities space for this extension.
   * Arguments are two caps bitfields - first one is set of capabilities this
   * extension is interesting, and secind describes sharing policy on this set 
   * of caps:
   * if bit set - this cap is to be shared.   
   * After this call for all decisions on caps in this range 
   * delegated to AccessControlDecider of this extension.
   * If access to capability range is shared, then Waterfall chains
   * all deciders for given capability, if anyone forbids capability setting
   * - this capability not granted.
   */
  jint (JNICALL *JVMPExt_GetCapsRange)(JVMP_SecurityCap* caps,
				       JVMP_SecurityCap* sh_mask);
};

typedef struct JVMP_Extension JVMP_Extension;

typedef _JVMP_IMPORT_OR_EXPORT jint (JNICALL *JVMP_GetExtension_t)(JVMP_Extension** ext);
typedef jint (JNICALL *JVMPExt_ExecuteRequest_t)(JVMP_ShmRequest* req);
#ifdef __cplusplus
};
#endif
#endif
