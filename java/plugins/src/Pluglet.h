/* 
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
 */
#ifndef __Pluglet_h__
#define __Pluglet_h__
#include "nsplugin.h"
#include "jni.h"
#include "PlugletView.h"

class iPlugletEngine;

class Pluglet : public nsIPluginInstance {
 public:
    NS_IMETHOD HandleEvent(nsPluginEvent* event, PRBool* handled);
    NS_IMETHOD Initialize(nsIPluginInstancePeer* peer);
    NS_IMETHOD GetPeer(nsIPluginInstancePeer* *result);
    NS_IMETHOD Start(void);
    NS_IMETHOD Stop(void);
    NS_IMETHOD Destroy(void);
    NS_IMETHOD NewStream(nsIPluginStreamListener** listener);
    NS_IMETHOD SetWindow(nsPluginWindow* window);
    NS_IMETHOD Print(nsPluginPrint* platformPrint);
    NS_IMETHOD GetValue(nsPluginInstanceVariable variable, void *value);
    NS_DECL_ISUPPORTS

    Pluglet(jobject object);
    virtual ~Pluglet(void);
 private:
    jobject jthis;
    static  jmethodID initializeMID;
    static  jmethodID startMID;
    static  jmethodID stopMID;
    static  jmethodID destroyMID;
    static  jmethodID newStreamMID;
    static  jmethodID setWindowMID;
    static  jmethodID printMID; 
    static  jmethodID getValueMID;
    nsIPluginInstancePeer *peer;
    PlugletView *view;
    nsCOMPtr<iPlugletEngine> plugletEngine;
};
#endif /* __Pluglet_h__ */
