/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * $Id: nsPluggableJVM.h,v 1.1 2001/05/09 18:51:36 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

#ifndef __nsPluggableJVM_h__
#define __nsPluggableJVM_h__
#include "nsIPluggableJVM.h"
#include "nsIBrowserJavaSupport.h"
#include "nsCOMPtr.h"
#include "nspr.h"
// Waterfall interface
#include "jvmp.h"

#define LIBJVMP "jvmp"
#define LIBEXT  "wf_moz6"

class nsPluggableJVM : public nsIPluggableJVM {
 public:
  NS_DECL_ISUPPORTS

  NS_DECL_NSIPLUGGABLEJVM
  
  nsPluggableJVM();

  virtual ~nsPluggableJVM();

  NS_IMETHOD GetWFCtx(JVMP_RuntimeContext* *rt_ctx, 
		      JVMP_CallingContext* *c_ctx);
  NS_IMETHOD DeleteGlobalRef(jobject jobj);
  NS_IMETHOD DeleteLocalRef(jobject jobj);  
 protected:
  // pointer to WF plugin context
  JVMP_RuntimeContext* m_jvmp_context;
  // Waterfall DLL handle
  PRLibrary*           m_dll;
  // this field contains  JVMP_CallingContext only for main NS thread
  // other threads should use JVMP_GetCallingContext()
  JVMP_CallingContext*               m_ctx;
  // descriptor provided by Waterfall library for proper Mozilla extension
  jint                               m_extID;
  // browser Java support field
  nsCOMPtr<nsIBrowserJavaSupport>    m_support;
  // just load Waterfall and set up m_jvmp_context correctly
  NS_METHOD            loadPluginDLL(const char* plugin_home);
  // start up JVM and set up m_ctx correctly - call from main thread
  NS_METHOD            initJVM(JVMP_CallingContext* *penv, 
			       JavaVMInitArgs*       vm_args);
};

#endif /* __nsPluggableJVM_h__ */
