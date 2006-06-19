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
#ifndef __PlugletEngine_h__
#define __PlugletEngine_h__
#include "nsplugin.h"
#include "nsIPluginManager.h"
#include "nsIObserver.h"
#include "PlugletsDir.h"
#include "iPlugletEngine.h"
#include "nsCOMPtr.h"

#ifndef OJI_DISABLE
#include "nsJVMManager.h"
#include "PlugletSecurityContext.h"
#endif /* OJI_DISABLE */

class PlugletEngine : public nsIObserver, public iPlugletEngine, public nsIPlugin {
 public:
    PlugletEngine();
    virtual ~PlugletEngine(void);
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    NS_DECL_IPLUGLETENGINE
    NS_DECL_NSIPLUGIN
    NS_DECL_NSIFACTORY

    static PlugletEngine *_NewInstance();

 private:
    static int objectCount;
    static PRInt32 lockCount;
    static PlugletsDir *dir;
    static PlugletEngine * engine;
    static nsCOMPtr<nsIPluginManager>  pluginManager;
    static jobject plugletManager;
#ifndef OJI_DISABLE
    static nsJVMManager * jvmManager;
    static PlugletSecurityContext *securityContext;
#else /* OJI_DISABLE */
    static JavaVM *jvm;
    static void StartJVM(void);
#endif /* OJI_DISABLE */
};    

#endif /* __PlugletEngine_h__ */





