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
 * $Id: nsJavaHTMLObjectFactory.h,v 1.2 2001/07/12 20:32:09 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

#ifndef __nsJavaHTMLObjectFactory_h
#define __nsJavaHTMLObjectFactory_h

#include "nsCOMPtr.h"
#include "nsPluggableJVM.h"
#include "nsIBrowserJavaSupport.h"
#include "nsIPlugin.h"
#include "nsIPluginManager.h"
#include "nsIPluginManager2.h"
#include "nsILiveconnect.h"
#include "nsILiveConnectManager.h"
#include "nsIJavaObjectInfo.h"
#include "nsIJVMPlugin.h"
#include "nsIJVMManager.h"
#include "nsIJVMConsole.h"
#include "nsIServiceManager.h"
#include "wf_moz6_common.h"

/* this class is derived from nsIPlugin as I don't like idea 
 * to touch Mozilla internals in applet handling rigth now
 */
class nsJavaHTMLObjectFactory : public nsIPlugin,
				public nsIJVMPlugin,
				public nsIBrowserJavaSupport,
				public nsIJVMConsole
{   
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBROWSERJAVASUPPORT
  
  virtual ~nsJavaHTMLObjectFactory(void);

  // nsIFactory
  NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID & iid,
			    void **result);
  NS_IMETHOD LockFactory(PRBool aLock);
  
  // nsIPlugin
  NS_IMETHOD CreatePluginInstance(nsISupports *aOuter, REFNSIID aIID,
				  const char* aPluginMIMEType,
				  void **aResult);
  NS_IMETHOD Initialize(void);
  NS_IMETHOD Shutdown(void);
  NS_IMETHOD GetMIMEDescription(const char* *resultingDesc);
  NS_IMETHOD GetValue(nsPluginVariable variable, void *value);

  // nsIJVMPlugin
  NS_IMETHOD AddToClassPath(const char* dirPath);
  NS_IMETHOD RemoveFromClassPath(const char* dirPath);
  NS_IMETHOD GetClassPath(const char* *result);
  NS_IMETHOD GetJavaWrapper(JNIEnv* jenv, jint obj, jobject *jobj);
  NS_IMETHOD CreateSecureEnv(JNIEnv* proxyEnv, nsISecureEnv* *outSecureEnv);
  NS_IMETHOD SpendTime(PRUint32 timeMillis);
  NS_IMETHOD UnwrapJavaWrapper(JNIEnv* jenv, jobject jobj, jint* obj);
  
  // nsIJVMConsole
  NS_IMETHOD Show();
  NS_IMETHOD Hide();
  NS_IMETHOD IsVisible(PRBool *result);
  NS_IMETHOD Print(const char* msg, const char* encodingName = NULL);
  
  
  // the only way to create instance using this factory
  static NS_METHOD Create(nsISupports* outer, 
			  const nsIID& aIID, 
			  void* *aInstancePtr);
  // Get JVM used by this factory now
  // doesn't NS_ADDREF result
  NS_IMETHOD GetJVM(nsIPluggableJVM* *jvm);
  // friend proxy class
  friend class nsJavaPluginProxy;
  static nsJavaHTMLObjectFactory* instance;
 
 protected:
  nsJavaHTMLObjectFactory();
  nsPluggableJVM*                 m_jvm;
  nsCOMPtr<nsIPluginManager>      m_mgr;
  nsCOMPtr<nsIPluginManager2>     m_mgr2;
  nsCOMPtr<nsILiveconnect>        m_liveConnect;
  nsCOMPtr<nsIJVMManager>         m_jvmMgr;
  BrowserSupportWrapper*          m_wrapper;
  JNIEnv*                         m_env;
  NS_IMETHOD doGetProxyForURL(const char* url,
                              char* *target);
  NS_IMETHOD doJSCall(jint jstid, 
                      struct JSObject_CallInfo** call);
  NS_IMETHOD initLiveConnect();
  PRThread*                       m_mainThread;
  jint                            m_jsjRecursion;
};
#endif //__nsJavaHTMLObjectFactory_h








