/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "PlugletEngine.h"
#include "Pluglet.h"
#include "nsIServiceManager.h"
#include "prenv.h"
#include "PlugletManager.h"
#include "nsIGenericFactory.h"
#include "nsIModule.h"

#ifndef OJI_DISABLE
#include "ProxyJNI.h"
static NS_DEFINE_CID(kJVMManagerCID,NS_JVMMANAGER_CID);
PlugletSecurityContext *PlugletEngine::securityContext = NULL;
nsJVMManager * PlugletEngine::jvmManager = NULL;
#endif /* OJI_DISABLE */


static NS_DEFINE_CID(kIPluginIID,NS_IPLUGIN_IID);
static NS_DEFINE_CID(kPluginCID,NS_PLUGIN_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_CID(kPluginManagerCID, NS_PLUGINMANAGER_CID);


#define PLUGLETENGINE_PROGID \
"component://netscape/blackwood/pluglet-engine"

#define  PLUGLETENGINE_CID  \
{ /* C1E694F3-9BE1-11d3-837C-0004AC56C49E */ \
  0xc1e694f3, 0x9be1, 0x11d3, { 0x83, 0x7c, 0x0, 0x4, 0xac, 0x56, 0xc4, 0x9e } \
} 

NS_GENERIC_FACTORY_CONSTRUCTOR(PlugletEngine)

static  nsModuleComponentInfo components[] = 
{
    {
        "Pluglet Engine",
        PLUGLETENGINE_CID,
        PLUGLETENGINE_PROGID,
        PlugletEngineConstructor
    }
};

NS_IMPL_NSGETMODULE("PlugletEngineModule",components);


int PlugletEngine::objectCount = 0;
PlugletsDir * PlugletEngine::dir = NULL;
PRInt32 PlugletEngine::lockCount = 0;
PlugletEngine * PlugletEngine::engine = NULL;
nsCOMPtr<nsIPluginManager> PlugletEngine::pluginManager;
jobject PlugletEngine::plugletManager = NULL;

#define PLUGIN_MIME_DESCRIPTION "*:*:Pluglet Engine"

NS_IMPL_ISUPPORTS(PlugletEngine,kIPluginIID);
NS_METHOD PlugletEngine::Initialize(void) {
    //nb ???
    return NS_OK;
}

NS_METHOD PlugletEngine::Shutdown(void) {
    //nb ???
    return NS_OK;
}

NS_METHOD PlugletEngine::CreateInstance(nsISupports *aOuter,
			 REFNSIID aIID,	void **aResult) {
    return NS_ERROR_FAILURE; //nb to do
}
 
NS_METHOD PlugletEngine::CreatePluginInstance(nsISupports *aOuter, REFNSIID aIID, 
				const char* aPluginMIMEType, void **aResult) {
    if (!aResult) {
	return NS_ERROR_FAILURE; 
    }
    PlugletFactory * plugletFactory = NULL;
    nsresult res = NS_ERROR_NULL_POINTER; 
    if ((res = dir->GetPlugletFactory(aPluginMIMEType,&plugletFactory)) != NS_OK
	|| !plugletFactory) {
        return res; 
    }
    //we do not delete pluglet because we do not allocate new memory in dir->GetPluglet()
    return plugletFactory->CreatePluginInstance(aPluginMIMEType,aResult);
}

NS_METHOD PlugletEngine::GetMIMEDescription(const char* *result) {
    if (!result) {
	return NS_ERROR_FAILURE;
    }
    *result = PLUGIN_MIME_DESCRIPTION;
    return NS_OK;
}

NS_METHOD PlugletEngine::GetValue(nsPluginVariable variable, void *value) {
    //nb ????
    return NS_OK;
}

NS_METHOD PlugletEngine::LockFactory(PRBool aLock) {
    if(aLock) {
        PR_AtomicIncrement(&lockCount); 
    } else {
        PR_AtomicDecrement(&lockCount);
    }
    return NS_OK;
}

char *ToString(jobject obj,JNIEnv *env) {
	static jmethodID toStringID = NULL;
	if (!toStringID) {
	    jclass clazz = env->FindClass("java/lang/Object");
	    toStringID = env->GetMethodID(clazz,"toString","()Ljava/lang/String;");
	}
	jstring jstr = (jstring) env->CallObjectMethod(obj,toStringID);
	const char * str = NULL;
	str = env->GetStringUTFChars(jstr,NULL);
	char * res = new char[strlen(str)];
	strcpy(res,str);
	env->ReleaseStringUTFChars(jstr,str);
	return res;
}

PlugletEngine::PlugletEngine() {
    NS_INIT_REFCNT();
    dir = new PlugletsDir();
    engine = this;
    objectCount++;
}

PlugletEngine::~PlugletEngine(void) {
    delete dir;
    objectCount--;
}


#ifdef OJI_DISABLE

#ifdef XP_PC
#define PATH_SEPARATOR ';'
#else /* XP_PC */
#define PATH_SEPARATOR ':'
#endif /* XP_PC */

JavaVM *PlugletEngine::jvm = NULL;	

void PlugletEngine::StartJVM(void) {
    JNIEnv *env = NULL;	
    jint res;
    JDK1_1InitArgs vm_args;
    char classpath[1024];
    JNI_GetDefaultJavaVMInitArgs(&vm_args);
    vm_args.version = 0x00010001;
    /* Append USER_CLASSPATH to the default system class path */
    sprintf(classpath, "%s%c%s",
            vm_args.classpath, PATH_SEPARATOR, PR_GetEnv("CLASSPATH"));
	printf("-- classpath %s\n",classpath);
    char **props = new char*[2];
    props[0]="java.compiler=NONE";
    props[1]=0;
    vm_args.properties = props;
    vm_args.classpath = classpath;
    /* Create the Java VM */	
    res = JNI_CreateJavaVM(&jvm, &env, &vm_args);
    if(res < 0 ) {
        printf("--JNI_CreateJavaVM failed \n");
    } else {
        printf("--PlugletEngine::StartJVM() jvm was started \n");
    }
}
#endif /* OJI_DISABLE */

JNIEnv * PlugletEngine::GetJNIEnv(void) {
   JNIEnv * res;
#ifndef OJI_DISABLE
   nsresult result;
   if (!jvmManager) {
       NS_WITH_SERVICE(nsIJVMManager, _jvmManager, kJVMManagerCID, &result);
       if (NS_SUCCEEDED(result)) {
           jvmManager = (nsJVMManager*)((nsIJVMManager*)_jvmManager);
       }
   }
   if (!jvmManager) {
       return NULL;
   }
   jvmManager->CreateProxyJNI(NULL,&res);
   if (!securityContext) {
       securityContext = new PlugletSecurityContext();
   }
   SetSecurityContext(res,securityContext);
#else  /* OJI_DISABLE */
    if (!jvm) {
           printf(":) starting jvm\n");
	   StartJVM();
   }
   jvm->AttachCurrentThread(&res,NULL);
   printf("--PluglgetEngine::GetJNIEnv after jvm->Attach \n");
#endif /* OJI_DISABLE */
   return res;
}

jobject PlugletEngine::GetPlugletManager(void) {
    if (!pluginManager) {
        nsresult  res;
        NS_WITH_SERVICE(nsIPluginManager,_pluginManager,kPluginManagerCID,&res);
        if (NS_SUCCEEDED(res)) { 
            pluginManager = _pluginManager;
        }
    }
    if (!pluginManager) {
        return NULL;
    }
    if (!plugletManager) {
        plugletManager = PlugletManager::GetJObject(pluginManager.get());
    }
    return plugletManager;
}

PlugletEngine * PlugletEngine::GetEngine(void) {
    return engine;
}
void PlugletEngine::IncObjectCount(void) {
    objectCount++;
}

void PlugletEngine::DecObjectCount(void) {
    objectCount--;
}

PRBool PlugletEngine::IsUnloadable(void) {
  return (lockCount == 0 
	  && objectCount == 0);
}

