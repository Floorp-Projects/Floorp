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
 * $Id: nsAppletHTMLObject.h,v 1.1 2001/05/09 18:51:35 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

#ifndef __nsAppletHTMLObject_h
#define __nsAppletHTMLObject_h
#include "nsCOMPtr.h"
#include "nsIPluginInstance.h"
#include "nsplugindefs.h"
#include "nsJavaHTMLObjectFactory.h"
#include "nsIJavaObjectInfo.h"
#include "nsIPluginTagInfo2.h"
#include "nsIPluginManager.h"
#include "nsIPluginManager2.h"
#include "nsIPluginInstancePeer.h"
#include "nsIPluginInstancePeer2.h"
#include "nsIJVMPluginInstance.h"
//#include "nsIPluginStreamListener.h"
class nsJavaHTMLObject;

/** 
 *  this class is just wrapper forwarding functionality from
 *  older plugin based API to newer OJI2 based API
 */
class nsAppletHTMLObject : public nsIPluginInstance, 
			   public nsIWFInstanceWrapper,
			   public nsIJVMPluginInstance
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWFINSTANCEWRAPPER
      
  nsAppletHTMLObject(nsJavaHTMLObjectFactory *factory);
  virtual ~nsAppletHTMLObject(void);

  // from nsIPluginInstance
  NS_IMETHOD Initialize(nsIPluginInstancePeer *peer);
  NS_IMETHOD GetPeer(nsIPluginInstancePeer* *resultingPeer);
  NS_IMETHOD Start(void);
  NS_IMETHOD Stop(void);
  NS_IMETHOD Destroy(void);
  NS_IMETHOD SetWindow(nsPluginWindow* window);
  NS_IMETHOD NewStream(nsIPluginStreamListener** listener);
  NS_IMETHOD Print(nsPluginPrint* platformPrint);
  NS_IMETHOD GetValue(nsPluginInstanceVariable variable, void *value);
  NS_IMETHOD HandleEvent(nsPluginEvent* event, PRBool* handled);

  // from nsIJVMPluginInstance
  NS_IMETHOD GetJavaObject(jobject *result);
  NS_IMETHOD GetText(const char* *result);

  // friend proxy class
  friend class nsJavaPluginInstanceProxy;

 protected:
  nsJavaHTMLObject*        m_jobject;
  nsJavaHTMLObjectFactory* m_factory;
  jint                     m_id;
  nsIPluggableJVM*         m_jvm;
  JNIEnv*                  m_env; /* cached JNIEnv of main Mozilla thread */

  nsJavaObjectType mapType(nsPluginTagType pluginType);
  // methods that can be called from main thread to do real job
  NS_IMETHOD doShowStatus(const char* status);
  NS_IMETHOD doShowDocument(const char* url, 
			    const char* target);
  NS_IMETHOD doGetJSThread(jint *jstid);

  // kind of caching
  nsIPluginInstancePeer*            m_peer;
  nsIPluginInstancePeer2*           m_peer2;
  nsCOMPtr<nsIPluginManager>        m_mgr;
  // workaround for incorrect lifecycle of plugin peer
  PRBool                            m_dead;
  PRThread*                         m_mainThread;
};

#endif


