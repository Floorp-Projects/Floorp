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
#include "PlugletEngine.h"
#include "Pluglet.h"
#include "nsIServiceManager.h"
#include "prenv.h"

static NS_DEFINE_IID(kIPluginIID,NS_IPLUGIN_IID);
static NS_DEFINE_CID(kPluginCID,NS_PLUGIN_CID);
static NS_DEFINE_IID(kIServiceManagerIID, NS_ISERVICEMANAGER_IID);
static NS_DEFINE_IID(kIJVMManagerIID,NS_IJVMMANAGER_IID);
static NS_DEFINE_CID(kJVMManagerCID,NS_JVMMANAGER_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

#define PLUGIN_MIME_DESCRIPTION "*:*:Pluglet Engine"

//nsJVMManager * PlugletEngine::jvmManager = NULL;
int PlugletEngine::objectCount = 0;
PlugletsDir * PlugletEngine::dir = NULL;
PRInt32 PlugletEngine::lockCount = 0;
PlugletEngine * PlugletEngine::engine = NULL;
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
    Pluglet * pluglet = NULL;
    nsresult res = NS_ERROR_NULL_POINTER; 
    if ((res = dir->GetPluglet(aPluginMIMEType,&pluglet)) != NS_OK
	|| !pluglet) {
        return res; 
    }
    //we do not delete pluglet because we do not allocate new memory in dir->GetPluglet()
    return pluglet->CreatePluginInstance(aPluginMIMEType,aResult);
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

PlugletEngine::PlugletEngine(nsISupports* aService) {
    NS_INIT_REFCNT();
    dir = new PlugletsDir();
    nsresult res = NS_OK;
#if 0
    nsIServiceManager *sm;
    res = aService->QueryInterface(nsIServiceManager::GetIID(),(void**)&sm);
    if (NS_FAILED(res)) {
	jvmManager = NULL;
	return;
    }
    res = sm->GetService(kJVMManagerCID,kIJVMManagerIID,(nsISupports**)&jvmManager);
    //nb 
    if (NS_FAILED(res)) {
	jvmManager = NULL;
    }
    NS_RELEASE(sm);
#endif 
    engine = this;
    objectCount++;
}

PlugletEngine::~PlugletEngine(void) {
    delete dir;
    objectCount--;
}

//nb for debug only
#ifndef PATH_SEPARATOR
#define PATH_SEPARATOR ';'
#endif 

JavaVM *jvm = NULL;	

static void StartJVM() {
    JNIEnv *env = NULL;	
    jint res;
	JDK1_1InitArgs vm_args;
    char classpath[1024];
    vm_args.version = 0x00010001;
    JNI_GetDefaultJavaVMInitArgs(&vm_args);
    /* Append USER_CLASSPATH to the default system class path */
    //nb todo urgent
	
    sprintf(classpath, "%s%c%s",
            vm_args.classpath, PATH_SEPARATOR, PR_GetEnv("CLASSPATH"));
	printf("-- classpath %s\n",classpath);
    vm_args.classpath = classpath;
    /* Create the Java VM */	
    res = JNI_CreateJavaVM(&jvm, &env, &vm_args);
}

JNIEnv * PlugletEngine::GetJNIEnv(void) {
   JNIEnv * res;
#if 0
   if (!jvmManager) {
       //nb it is bad :(
       return NULL;
   }
   jvmManager->CreateProxyJNI(NULL,&res);
   //nb error handling
#endif
//#if 0
    if (!jvm) {
           printf(":) starting jvm\n");
	   StartJVM();
   }
   jvm->AttachCurrentThread(&res,NULL);
//#endif
   return res;
}

jobject PlugletEngine::GetPlugletManager(void) {
    //nb todo urgent
    return NULL;
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

// ******************************************
extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports* serviceMgr,
	     const nsCID &aClass,
	     const char *aClassName,
	     const char *aProgID,
	     nsIFactory **aFactory)
{
    if (aClass.Equals(kPluginCID)) {
	if (PlugletEngine::GetEngine()) {
	    *aFactory = PlugletEngine::GetEngine();
	    return NS_OK;
	}
	PlugletEngine * engine = new PlugletEngine(serviceMgr);
	if (!engine) 
	    return NS_ERROR_OUT_OF_MEMORY;
	engine->AddRef();
	*aFactory = engine;
	return NS_OK;
    }
    return NS_ERROR_FAILURE; 
}

extern "C" NS_EXPORT PRBool
NSCanUnload(nsISupports* serviceMgr)
{
    return (PlugletEngine::IsUnloadable());
}


extern  "C" char*  
NP_GetMIMEDescription(void)
{
    return PLUGIN_MIME_DESCRIPTION;
}









































