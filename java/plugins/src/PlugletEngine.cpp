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
#include "nsServiceManagerUtils.h"
#include "prenv.h"
#include "PlugletManager.h"
#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "PlugletLog.h"

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

PRLogModuleInfo* PlugletLog::log = NULL;

#define PLUGLETENGINE_ContractID \
 "@mozilla.org/blackwood/pluglet-engine;1"

#define  PLUGLETENGINE_CID  \
{ /* C1E694F3-9BE1-11d3-837C-0004AC56C49E */ \
  0xc1e694f3, 0x9be1, 0x11d3, { 0x83, 0x7c, 0x0, 0x4, 0xac, 0x56, 0xc4, 0x9e } \
} 




#ifdef XP_PC

NS_GENERIC_FACTORY_CONSTRUCTOR(PlugletEngine)

#else  //XP_PC

#include <dlfcn.h>

/* 
 * CheckForXTSymbol
 * return value
 * 1 - AWT will not complain
 * 0 - AWT will not work
 *
 */

int
CheckForXTSymbol(void) 
{
    Dl_info dlinfo;
    void *v;
    v = dlsym(RTLD_DEFAULT, "vendorShellWidgetClass");
    if (v != NULL && dladdr(v, &dlinfo)) {
        if (strstr(dlinfo.dli_fname, "libXt.so") != NULL) {
            fprintf(stderr, "\nRuntime link error - it appears that "
                    "libXt got loaded before libXm,\n"
                    "which is not allowed for pluglets\n");
            return 0;
        }
    }
    return 1;
    
}

static NS_IMETHODIMP                                                            
PlugletEngineConstructor(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{                                                                               
    nsresult rv;                                                                
    PlugletEngine * inst;
    if ( !CheckForXTSymbol() ) {
        rv = NS_ERROR_FAILURE;
        return rv;
    }
    if (NULL == aResult) {                                                      
        rv = NS_ERROR_NULL_POINTER;                                             
        return rv;                                                              
    }                                                                           
    *aResult = NULL;                                                            
    if (NULL != aOuter) {                                                       
        rv = NS_ERROR_NO_AGGREGATION;                                           
        return rv;                                                              
    }                                                                           
    NS_NEWXPCOM(inst, PlugletEngine);                                          
    if (NULL == inst) {                                                         
      rv = NS_ERROR_OUT_OF_MEMORY;                                            
      return rv;                                                              
    }                                                                           
    NS_ADDREF(inst);                                                            
    rv = inst->QueryInterface(aIID, aResult);                                   
    NS_RELEASE(inst);                                                           
    return rv;                                                                  
}                                                                               

#endif //XP_PC


static  nsModuleComponentInfo components[] = 
{
    {
        "Pluglet Engine",
        PLUGLETENGINE_CID,
        PLUGLETENGINE_ContractID,
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

NS_IMPL_ISUPPORTS1(PlugletEngine,nsIPlugin);
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
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletEngine::CreatePluginInstance\n"));
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
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletEngine::GetMimeDescription\n"));
    if (!result) {
	return NS_ERROR_FAILURE;
    }
    *result = PLUGIN_MIME_DESCRIPTION;
    return NS_OK;
}

NS_METHOD PlugletEngine::GetValue(nsPluginVariable variable, void *value) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletEngine::GetValue; stub\n"));
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
    PlugletLog::log = PR_NewLogModule("pluglets");
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

void PlugletEngine::StartJVM() {
    JNIEnv *env = NULL;
    jint res;
    jsize jvmCount;
    JNI_GetCreatedJavaVMs(&jvm, 1, &jvmCount);
    if (jvmCount) {
        PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
               ("PlugletEngine::StartJVM we got already started JVM\n"));
        return;
    }
    char classpath[1024]="";
    JavaVMInitArgs vm_args;
    JavaVMOption options[2];
    char * classpathEnv = PR_GetEnv("CLASSPATH");
    if (classpathEnv != NULL) {
        sprintf(classpath, "-Djava.class.path=%s",classpathEnv);
        PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
               ("PlugletEngine::StartJVM about to create JVM classpath=%s\n",classpath));
    }
    options[0].optionString = classpath;
    options[1].optionString=""; //-Djava.compiler=NONE";
    vm_args.version = 0x00010002;
    vm_args.options = options;
    vm_args.nOptions = 2;
    vm_args.ignoreUnrecognized = JNI_TRUE;
    /* Create the Java VM */
    res = JNI_CreateJavaVM(&jvm, (void**)&env, &vm_args);
    printf("--bcJavaGlobal::StartJVM jvm started res %d\n",res);
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
           ("PlugletEngine::StartJVM after CreateJavaVM res = %d\n",res));
}

#endif /* OJI_DISABLE */

JNIEnv * PlugletEngine::GetJNIEnv(void) {
   JNIEnv * res = NULL;
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
   if (NS_FAILED(jvmManager->CreateProxyJNI(NULL,&res))) {
       return NULL;
   }
   if (!securityContext) {
       securityContext = new PlugletSecurityContext();
   }
   SetSecurityContext(res,securityContext);
#else  /* OJI_DISABLE */
   if (!jvm) {
       PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
              ("PlugletEngine::GetJNIEnv going to start our own jvm \n"));
       StartJVM();
   }
   if (jvm) {
       jvm->AttachCurrentThread((void**)&res,NULL);
   }
#endif /* OJI_DISABLE */
   return res;
}

jobject PlugletEngine::GetPlugletManager(void) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
           ("PlugletEngine::GetPlugletManager\n"));
    //Changed by John Sublet NS_WITH_SERVICE deprecated currently is
    //problematic: lxr.mozilla.org indicates version of
    //do_GetService that allows the res to be included in the
    //do_GetService call but that wouldn't work: FIXME
    
    //NS_WITH_SERVICE(nsIPluginManager,_pluginManager,kPluginManagerCID,&res);
    nsCOMPtr<nsIPluginManager> _pluginManager (do_GetService(kPluginManagerCID));
    
    
    // Changed by John Sublet : FIXME this assumes _pluginManager will be properly set to NULL
    if (_pluginManager) {
        pluginManager = _pluginManager;
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

