/* 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
 * Inc. All Rights Reserved. 
 */
#ifndef __PlugletEngine_h__
#define __PlugletEngine_h__
#include "nsplugin.h"
#include "jni.h"
#include "nsJVMManager.h"
#include "PlugletsDir.h"

class PlugletEngine : public nsIPlugin {
 public:
    NS_IMETHOD CreatePluginInstance(nsISupports *aOuter, REFNSIID aIID, 
				    const char* aPluginMIMEType,
				    void **aResult);
    NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID & iid, void * *_retval);
    NS_IMETHOD LockFactory(PRBool aLock);
    NS_IMETHOD Initialize(void);
    NS_IMETHOD Shutdown(void); 
    NS_IMETHOD GetMIMEDescription(const char* *result);
    NS_IMETHOD GetValue(nsPluginVariable variable, void *value);
    NS_DECL_ISUPPORTS
    PlugletEngine(nsISupports* aService);
    virtual ~PlugletEngine(void);
    static JNIEnv * GetJNIEnv(void);
    static jobject GetPlugletManager(void);
    static void IncObjectCount(void);
    static void DecObjectCount(void);
    static PRBool IsUnloadable(void);
    static PlugletEngine * GetEngine(void);
 private:
    static int objectCount;
    static PRInt32 lockCount;
    static PlugletsDir *dir;
    static PlugletEngine * engine;
    //nb static nsJVMManager * jvmManager;
};    

#endif /* __PlugletEngine_h__ */


