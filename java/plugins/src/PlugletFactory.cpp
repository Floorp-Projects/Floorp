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
#include "PlugletFactory.h"
#include "PlugletEngine.h"
#include "PlugletLoader.h"
#include "Pluglet.h"
#include "plstr.h"
#include "PlugletLog.h"

jmethodID PlugletFactory::createPlugletMID = NULL;
jmethodID PlugletFactory::initializeMID = NULL;  
jmethodID PlugletFactory::shutdownMID = NULL;


nsresult PlugletFactory::CreatePluginInstance(const char* aPluginMIMEType, void **aResult) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletFactory::CreatePluginInstance\n"));
    if(!aResult
       || Initialize() != NS_OK) {
	return NS_ERROR_FAILURE;
    }
    JNIEnv *env = PlugletEngine::GetJNIEnv();
    if(!env) {
	return NS_ERROR_FAILURE;
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
    JNIEnv *env = PlugletEngine::GetJNIEnv();
    if(!env) {
        return NS_ERROR_FAILURE;
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
	initializeMID = env->GetMethodID(clazz,"initialize","(Lorg/mozilla/pluglet/mozilla/PlugletManager;)V");
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
	env->CallVoidMethod(jthis,initializeMID,PlugletEngine::GetPlugletManager());
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
    JNIEnv *env = PlugletEngine::GetJNIEnv();
    if(!env) {
	return NS_ERROR_FAILURE;
    }
    env->CallVoidMethod(jthis,shutdownMID);
    if (env->ExceptionOccurred()) {
	env->ExceptionDescribe();
	return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

nsresult PlugletFactory::GetMIMEDescription(const char* *result) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletFactory::GetMimeDescription\n"));
    if(!result) {
	return NS_ERROR_FAILURE;
    }
    *result = mimeDescription;
    return (*result) ? NS_OK : NS_ERROR_FAILURE;
}

PlugletFactory::PlugletFactory(const char *mimeDescription, const char *path) {
    jthis = NULL;
    this->path = new char[strlen(path)+1];
    strcpy(this->path,path);
    this->mimeDescription = new char[strlen(mimeDescription)+1];
    strcpy(this->mimeDescription,mimeDescription);
    
}
 
PlugletFactory::~PlugletFactory(void) {
    if (path != NULL) {
        delete[] path;
    }
    if (mimeDescription != NULL) {
        delete[] mimeDescription;
    }
    JNIEnv *env = PlugletEngine::GetJNIEnv();
    if (env != NULL) {
        env->DeleteGlobalRef(jthis);
    }
}

PlugletFactory * PlugletFactory::Load(const char * path) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletFactory::Load\n"));
    char * mime = PlugletLoader::GetMIMEDescription(path);
    PlugletFactory * result = NULL;
    if (mime) {
	result = new PlugletFactory(mime,path);
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
    char * terminator = strchr(mimeDescription,':'); // terminator have to be not equal to NULL;
    char *p1 = mimeDescription;
    char *p2 = strchr(p1,';');
    while ( p1 != NULL && p1 < terminator ) {
        size_t n = sizeof(char) * ( ( (p2 == NULL || p2 > terminator) ? terminator : p2) - p1 );
        if (PL_strncasecmp(p1,mimeType,n) == 0) {
            return 1;
        }
        p1 = p2 ;
        if (p2 != NULL) {
            p2 = strchr(++p2,';');
            p1++;
        }
    }
    return 0;
}


