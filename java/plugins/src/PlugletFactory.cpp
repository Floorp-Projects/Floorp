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
#include "nsIServiceManager.h"

#include "PlugletFactory.h"
#include "iPlugletEngine.h"
#include "PlugletLoader.h"
#include "Pluglet.h"
#include "plstr.h"
#include "PlugletLog.h"

#include "nsServiceManagerUtils.h"

jmethodID PlugletFactory::createPlugletMID = nsnull;
jmethodID PlugletFactory::initializeMID = nsnull;  
jmethodID PlugletFactory::shutdownMID = nsnull;


nsresult PlugletFactory::CreatePluginInstance(const char* aPluginMIMEType, void **aResult) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletFactory::CreatePluginInstance\n"));
    if(!aResult
       || Initialize() != NS_OK) {
	return NS_ERROR_FAILURE;
    }
    JNIEnv *env = nsnull;
    nsresult rv = NS_ERROR_FAILURE;

    nsCOMPtr<iPlugletEngine> plugletEngine = 
	do_GetService(PLUGLETENGINE_ContractID, &rv);
    rv = plugletEngine->GetJNIEnv(&env);

    if (NS_FAILED(rv)) {
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	       ("Pluglet::Initialize: plugletEngine->GetJNIEnv failed\n"));
	return rv;
    }
    jstring jstr = env->NewStringUTF(aPluginMIMEType);
    jobject obj = env->CallObjectMethod(jthis,createPlugletMID, jstr);
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        return NS_ERROR_FAILURE;
    }
    if (!obj) {
	return NS_ERROR_FAILURE;
    }
    nsISupports * instance = new Pluglet(obj);
    NS_ADDREF(instance);
    *aResult = instance;
    return NS_OK;
}

nsresult PlugletFactory::Initialize(void) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletFactory::Initialize\n"));
    JNIEnv *env = nsnull;
    nsresult rv;
    nsCOMPtr<iPlugletEngine> plugletEngine = 
	do_GetService(PLUGLETENGINE_ContractID, &rv);
    rv = plugletEngine->GetJNIEnv(&env);

    if (NS_FAILED(rv)) {
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	       ("Pluglet::Initialize: plugletEngine->GetJNIEnv failed\n"));
	return rv;
    }

    if (!initializeMID) { 
	jclass clazz = env->FindClass("org/mozilla/pluglet/PlugletFactory");
	if(!clazz) {
            env->ExceptionDescribe();
	    return NS_ERROR_FAILURE;
	}
	createPlugletMID = env->GetMethodID(clazz,"createPluglet","(Ljava/lang/String;)Lorg/mozilla/pluglet/Pluglet;");
	if (!createPlugletMID) {
            env->ExceptionDescribe(); 
	    return NS_ERROR_FAILURE;
	}
	shutdownMID = env->GetMethodID(clazz,"shutdown","()V");
	if (!shutdownMID) {
            env->ExceptionDescribe(); 
	    return NS_ERROR_FAILURE;
	}   
	initializeMID = env->GetMethodID(clazz,"initialize",
					 "(Ljava/lang/String;Lorg/mozilla/pluglet/mozilla/PlugletManager;)V");
	if (!initializeMID) {
            env->ExceptionDescribe();
	    return NS_ERROR_FAILURE;
	}
	
    }
    if (!jthis) {
	jthis = PlugletLoader::GetPluglet(path);
	if (!jthis) {
	    return NS_ERROR_FAILURE;
	}
	jobject plugletEngineObj = nsnull;
	rv = plugletEngine->GetPlugletManager((void **) &plugletEngineObj);
	if (NS_FAILED(rv)) {
	    return rv;
	}
	jstring jpath = env->NewStringUTF(path);
	env->CallVoidMethod(jthis,initializeMID, jpath, plugletEngineObj);
	env->ReleaseStringUTFChars(jpath, path);
	if (env->ExceptionOccurred()) {
	    env->ExceptionDescribe();
	    return NS_ERROR_FAILURE;
	}
    }
    return NS_OK;
}

nsresult PlugletFactory::Shutdown(void) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletFactory::Shutdown\n"));
    
    if(!jthis) {
	return NS_ERROR_FAILURE;
    }
    JNIEnv *env = nsnull;
    nsresult rv;
    nsCOMPtr<iPlugletEngine> plugletEngine = 
	do_GetService(PLUGLETENGINE_ContractID, &rv);
    rv = plugletEngine->GetJNIEnv(&env);

    if (NS_FAILED(rv)) {
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	       ("Pluglet::Initialize: plugletEngine->GetJNIEnv failed\n"));
	return rv;
    }
    env->CallVoidMethod(jthis,shutdownMID);
    if (env->ExceptionOccurred()) {
	env->ExceptionDescribe();
	return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

nsresult PlugletFactory::GetMIMEDescription(const char* *result) {
    if(!result) {
	return NS_ERROR_FAILURE;
    }
    *result = mimeDescription;
    return (*result) ? NS_OK : NS_ERROR_FAILURE;
}

PlugletFactory::PlugletFactory(const char *inMimeDescription, const char *inPath) : jthis(nsnull), path(PL_strdup(inPath)), mimeDescription(PL_strdup(inMimeDescription))
{
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	   ("PlugletFactory::PlugletFactory: mimeDescription: %s, path: %s\n", 
	    mimeDescription, path));

}
 
PlugletFactory::~PlugletFactory(void) {
    
    if (this->path) {
        PL_strfree(this->path);
	this->path = nsnull;
    }
    if (this->mimeDescription) {
        PL_strfree(this->mimeDescription);
	this->mimeDescription = nsnull;
    }
    JNIEnv *env = nsnull;
    nsresult rv;
    nsCOMPtr<iPlugletEngine> plugletEngine = 
	do_GetService(PLUGLETENGINE_ContractID, &rv);
    if (NS_SUCCEEDED(rv)) {
	rv = plugletEngine->GetJNIEnv(&env);
	
	if (NS_FAILED(rv)) {
	    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
		   ("PlugletFactory::~PlugletFactory: plugletEngine->GetJNIEnv failed\n"));
	    return;
	}
	
	if (env != nsnull) {
	    env->DeleteGlobalRef(jthis);
	}
    }
}

PlugletFactory * PlugletFactory::Load(const char * path) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletFactory::Load\n"));
    char * mime = PlugletLoader::GetMIMEDescription(path);
    PlugletFactory * result = nsnull;
    if (mime) {
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	       ("PlugletFactory::Load: About to create PlugletFactory instance\n"));
	result = new PlugletFactory(mime,path);
	PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	       ("PlugletFactory::Load: successfully created PlugletFactory instance\n"));

	//delete[] mime;  //nb we have a strange exception here 
    }
    return result;
}

/*
  1 if good
  0 if not good
 */
int PlugletFactory::Compare(const char *mimeType) {
    /* mimedDescription mimeTypes:extensions:description
       mimeTypes mimeType;mimeType;...
       extensions extension;extension;...
    */
    if (!mimeType) {
        return 0;
    }
    char * terminator = strchr(mimeDescription,':'); // terminator have to be not equal to nsnull;
    char *p1 = mimeDescription;
    char *p2 = strchr(p1,';');
    while ( p1 != nsnull && p1 < terminator ) {
        size_t n = sizeof(char) * ( ( (p2 == nsnull || p2 > terminator) ? terminator : p2) - p1 );
        if (PL_strncasecmp(p1,mimeType,n) == 0) {
            return 1;
        }
        p1 = p2 ;
        if (p2 != nsnull) {
            p2 = strchr(++p2,';');
            p1++;
        }
    }
    return 0;
}


