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
 * Igor Kushnirskiy <idk@eng.sun.com>
 */

#include "nsIAllocator.h"
#include "nsCOMPtr.h"
#include "bcJavaMarshalToolkit.h"
#include "bcIIDJava.h"
#include "bcJavaStubsAndProxies.h"
#include "nsIServiceManager.h"

jclass bcJavaMarshalToolkit::objectClass = NULL;
jclass bcJavaMarshalToolkit::booleanClass = NULL;
jmethodID bcJavaMarshalToolkit::booleanInitMID = NULL;
jmethodID bcJavaMarshalToolkit::booleanValueMID = NULL;

jclass bcJavaMarshalToolkit::characterClass = NULL;
jmethodID bcJavaMarshalToolkit::characterInitMID = NULL;
jmethodID bcJavaMarshalToolkit::characterValueMID = NULL;

jclass bcJavaMarshalToolkit::byteClass = NULL;
jmethodID bcJavaMarshalToolkit::byteInitMID = NULL;
jmethodID bcJavaMarshalToolkit::byteValueMID = NULL;

jclass bcJavaMarshalToolkit::shortClass = NULL;
jmethodID bcJavaMarshalToolkit::shortInitMID = NULL;
jmethodID bcJavaMarshalToolkit::shortValueMID = NULL;

jclass bcJavaMarshalToolkit::integerClass = NULL;
jmethodID bcJavaMarshalToolkit::integerInitMID = NULL;
jmethodID bcJavaMarshalToolkit::integerValueMID = NULL;

jclass bcJavaMarshalToolkit::longClass = NULL;
jmethodID bcJavaMarshalToolkit::longInitMID = NULL;
jmethodID bcJavaMarshalToolkit::longValueMID = NULL;

jclass bcJavaMarshalToolkit::floatClass = NULL;
jmethodID bcJavaMarshalToolkit::floatInitMID = NULL;
jmethodID bcJavaMarshalToolkit::floatValueMID = NULL;

jclass bcJavaMarshalToolkit::doubleClass = NULL;
jmethodID bcJavaMarshalToolkit::doubleInitMID = NULL;
jmethodID bcJavaMarshalToolkit::doubleValueMID = NULL;

jclass bcJavaMarshalToolkit::stringClass = NULL;


static NS_DEFINE_CID(kJavaStubsAndProxies,BC_JAVASTUBSANDPROXIES_CID);

bcJavaMarshalToolkit::bcJavaMarshalToolkit(PRUint16 _methodIndex,
				       nsIInterfaceInfo *_interfaceInfo, jobjectArray _args, JNIEnv *_env, int  isOnServer, bcIORB *_orb) {
    env = _env;
    callSide = (isOnServer) ? onServer : onClient;
    methodIndex = _methodIndex;
    interfaceInfo = _interfaceInfo;
    interfaceInfo->GetMethodInfo(methodIndex,(const nsXPTMethodInfo **)&info);  // These do *not* make copies ***explicit bending of XPCOM rules***
    args = _args;
    if(!objectClass) {
        InitializeStatic();
        if(!objectClass) {
            //nb ? we do not have java classes. What could we do?
        }
    }
    orb = _orb;
}

bcJavaMarshalToolkit::~bcJavaMarshalToolkit() {
}

nsresult bcJavaMarshalToolkit::Marshal(bcIMarshaler *m) {
    //nb todo    
    return NS_OK;
}


class javaAllocator : public bcIAllocator {
public:
    javaAllocator(nsIAllocator *_allocator) {
        allocator = _allocator;
    }
    virtual ~javaAllocator() {}
    virtual void * Alloc(size_t size) {
        return allocator->Alloc(size);
    }
    virtual void Free(void *ptr) {
        allocator->Free(ptr);
    }
    virtual void * Realloc(void* ptr, size_t size) {
        return allocator->Realloc(ptr,size);
    }
private:
    nsCOMPtr<nsIAllocator> allocator;
};

nsresult bcJavaMarshalToolkit::UnMarshal(bcIUnMarshaler *um) {
    printf("--nsresult bcJavaMarshalToolkit::UnMarshal\n");
    bcIAllocator * allocator = new javaAllocator(nsAllocator::GetGlobalAllocator());
    PRUint32 paramCount = info->GetParamCount();
    jobject value;
    void * data = allocator->Alloc(sizeof(nsXPTCMiniVariant)); // sizeof(nsXPTCMiniVariant) is ok
    for (unsigned int i = 0; i < paramCount; i++) {
        nsXPTParamInfo param = info->GetParam(i);
        PRBool isOut = param.IsOut();
        nsXPTType type = param.GetType();
        if ( (callSide == onServer && !param.IsIn() 
              || (callSide == onClient && !param.IsOut()))){
            if (callSide == onServer
                && isOut) { //we need to allocate memory for out parametr
                value = Native2Java(NULL,XPTType2bcXPType(type.TagPart()),1);
                env->SetObjectArrayElement(args,i,value);
            }
            continue;
        }
        switch(type.TagPart()) {
	    case nsXPTType::T_IID :
	    case nsXPTType::T_I8  :
	    case nsXPTType::T_U8  :             
	    case nsXPTType::T_I16 :
	    case nsXPTType::T_U16 :             
	    case nsXPTType::T_I32 :     
	    case nsXPTType::T_U32 :             
	    case nsXPTType::T_I64 :             
	    case nsXPTType::T_U64 :             
	    case nsXPTType::T_FLOAT  :           
	    case nsXPTType::T_DOUBLE :         
	    case nsXPTType::T_BOOL   :         
	    case nsXPTType::T_CHAR   :         
	    case nsXPTType::T_WCHAR  : 
            um->ReadSimple(data,XPTType2bcXPType(type.TagPart()));
            value = Native2Java(data,XPTType2bcXPType(type.TagPart()),isOut);
            break;
	    case nsXPTType::T_PSTRING_SIZE_IS: 
	    case nsXPTType::T_PWSTRING_SIZE_IS:
	    case nsXPTType::T_CHAR_STR  :
	    case nsXPTType::T_WCHAR_STR :      
            size_t size;
            um->ReadString(data,&size,allocator);
            //nb to do
            break;
	    case nsXPTType::T_INTERFACE :      	    
	    case nsXPTType::T_INTERFACE_IS :      	    
            {   
                printf("--[c++] we have an interface\n");
                bcOID oid;
                um->ReadSimple(&oid,XPTType2bcXPType(type.TagPart()));
                printf("%d oid", oid);
                nsIID iid;
                um->ReadSimple(&iid,bc_T_IID);
                void * p[2];
                p[0]=&oid; p[1]= &iid;
                data = p;
                value = Native2Java(data,bc_T_INTERFACE,isOut);                
                break;
            }
 	    case nsXPTType::T_ARRAY:
            {
                nsXPTType datumType;
                if(NS_FAILED(interfaceInfo->GetTypeForParam(methodIndex, &param, 1,&datumType))) {
                    return NS_ERROR_FAILURE;
                }
                //nb to do array
                break;
            }
	    default:
            return NS_ERROR_FAILURE;
        }

        env->SetObjectArrayElement(args,i,value);
    }
    return NS_OK;
}


bcXPType bcJavaMarshalToolkit::XPTType2bcXPType(uint8 type) {
    switch(type) {
	case nsXPTType::T_I8  :
	    return bc_T_I8;
	case nsXPTType::T_U8  :             
	    return bc_T_U8;
	case nsXPTType::T_I16 :
	    return bc_T_I16;
	case nsXPTType::T_U16 :             
	    return bc_T_U16;
	case nsXPTType::T_I32 :
	    return bc_T_I32;
	case nsXPTType::T_U32 :             
	    return bc_T_U32;
	case nsXPTType::T_I64 :             
	    return bc_T_I64;
	case nsXPTType::T_U64 :             
	    return bc_T_U64;
	case nsXPTType::T_FLOAT  :           
	    return bc_T_FLOAT;
	case nsXPTType::T_DOUBLE :         
	    return bc_T_DOUBLE;
	case nsXPTType::T_BOOL   :         
	    return bc_T_BOOL;
	case nsXPTType::T_CHAR   :         
	    return bc_T_CHAR;
	case nsXPTType::T_WCHAR  : 
	    return bc_T_WCHAR;
	case nsXPTType::T_IID  :
	    return bc_T_IID;
	case nsXPTType::T_CHAR_STR  :
	case nsXPTType::T_PSTRING_SIZE_IS: 
	    return bc_T_CHAR_STR;
	case nsXPTType::T_WCHAR_STR :   
	case nsXPTType::T_PWSTRING_SIZE_IS:
	    return bc_T_WCHAR_STR;
	case nsXPTType::T_INTERFACE :      	    
	case nsXPTType::T_INTERFACE_IS :    
	    return bc_T_INTERFACE;
	case nsXPTType::T_ARRAY:
	    return bc_T_ARRAY;
	default:
	    return bc_T_UNDEFINED;
	    
    }
}



//if p == 0 and isOut than return one element array
jobject bcJavaMarshalToolkit::Native2Java(void *p, bcXPType type, int isOut) {
    //nb we shoud care about endianes. should we?
    printf("--[c++]bcJavaMarshalToolkit::Native2Java \n");
    jobject res = NULL;
    if (!p 
        && !isOut) {
        printf("--[c++]bcJavaMarshalToolkit::Native2Java !p && !isOut\n");
        return res;
    }
    switch (type) {
    case  bc_T_I8:
    case  bc_T_U8:
        if (isOut) {
            res = env->NewByteArray(1);
            if (p) {
                env->SetByteArrayRegion((jbyteArray)res,0,1,(jbyte*)p);
            }
        } else {
        res = env->NewObject(byteClass,byteInitMID,*(jbyte*)p);
        }
        break;
    case bc_T_I16:
    case bc_T_U16:
        if (isOut) {
            res = env->NewShortArray(1);
            if (p) {
                env->SetShortArrayRegion((jshortArray)res,0,1,(jshort*)p);
            }
        } else {
            res = env->NewObject(shortClass,shortInitMID,*(jshort*)p);
        }
        break;
    case bc_T_I32:
    case bc_T_U32:
        if (isOut) {
            res = env->NewIntArray(1);
            if (p) {
                env->SetIntArrayRegion((jintArray)res,0,1,(jint*)p);
            } 
        } else {
            res = env->NewObject(integerClass,integerInitMID,*(jint*)p);
            printf("--bcJavaMarshalToolkit::Native2Java we'v got i32\n");
        }
        break;
    case bc_T_I64:
    case bc_T_U64:
        if (isOut) {
            res = env->NewLongArray(1);
            if (p) {
                env->SetLongArrayRegion((jlongArray)res,0,1,(jlong*)p);
            } 
        } else {
            res = env->NewObject(longClass,longInitMID,*(jlong*)p);
        }
        break;
    case bc_T_FLOAT:
        if (isOut) {
            res = env->NewFloatArray(1);
            if (p) {
                env->SetFloatArrayRegion((jfloatArray)res,0,1,(jfloat*)p);
            }
        } else {
            res = env->NewObject(floatClass,floatInitMID,*(jfloat*)p);
        }
        break;
    case bc_T_DOUBLE:
        if (isOut) {
            res = env->NewDoubleArray(1);
            if (p) {
                env->SetDoubleArrayRegion((jdoubleArray)res,0,1,(jdouble*)p);
            }
        } else {
            res = env->NewObject(doubleClass,doubleInitMID,*(jdouble*)p);
        }
        break;
    case bc_T_BOOL:
        if (isOut) {
            res = env->NewBooleanArray(1);
            if (p) {
                env->SetBooleanArrayRegion((jbooleanArray)res,0,1,(jboolean*)p);
            }
        } else {
            res = env->NewObject(booleanClass,booleanInitMID,*(jboolean*)p);
        }
        break;
    case bc_T_CHAR:
    case bc_T_WCHAR:
        if (isOut) {
            res = env->NewCharArray(1);
            if (p) {
                env->SetCharArrayRegion((jcharArray)res,0,1,(jchar*)p);
            }
        } else {
            res = env->NewObject(characterClass,characterInitMID,*(jchar*)p);
        }
        break;
    case bc_T_CHAR_STR:
    case bc_T_WCHAR_STR:                        //nb not sure about this
        {
            jstring  str = NULL;
            if (p) {
                str = env->NewStringUTF((const char*)p);                
            }
            if (isOut) {
                res = env->NewObjectArray(1,stringClass,NULL);
                if (str) {
                    env->SetObjectArrayElement((jobjectArray)res,0,str);
            }
            } else {
                res = str;
            }
            break;
        }
    case bc_T_IID: 
        {
            jobject iid = NULL;
            if (p) {
                iid = bcIIDJava::GetObject((nsIID*)p);
            }
            if (isOut) {
                res = env->NewObjectArray(1,bcIIDJava::GetClass(),NULL);
                env->SetObjectArrayElement((jobjectArray)res,0,iid);
            } else {
                res = iid;
            }
            break;
        }
    case bc_T_INTERFACE:
        {
            printf("--[c++]bcJavaMarshalToolkit::... we have an Interfaces \n");
            jobject obj = NULL;
            nsresult r;
            nsIID *iid;
            bcOID *oid;
            jobject proxy;
            if (p) {
                oid = (bcOID*)((void**)p)[0];
                iid = (bcIID*)((void**)p)[1];
                NS_WITH_SERVICE(bcJavaStubsAndProxies, javaStubsAndProxies, kJavaStubsAndProxies, &r);
                if (NS_FAILED(r)) {
                    return NULL;
                }
                javaStubsAndProxies->GetProxy(*oid, *iid, orb, &proxy);

            }
            if (isOut) {  //nb to do
                /*
                  res = env->NewObjectArray(1,bcIIDJava::GetClass(),NULL);
                  env->SetObjectArrayElement((jobjectArray)res,0,proxy);
                */
            } else {
                res = proxy;
            }
            break;

        }
    default:
        ;
    }
    return res;
}
void bcJavaMarshalToolkit::InitializeStatic() {
    jclass clazz;
    if (!(clazz = env->FindClass("java/lang/Object"))
        || !(objectClass = (jclass) env->NewGlobalRef(clazz))) {
        return;  
    }
    
    if (!(clazz = env->FindClass("java/lang/Boolean"))
        || !(booleanClass = (jclass) env->NewGlobalRef(clazz))) {
        DeInitializeStatic();
        return;  
    }

    if (!(clazz = env->FindClass("java/lang/Character"))
        || !(characterClass = (jclass) env->NewGlobalRef(clazz))) {
        DeInitializeStatic();
        return;  
    }
    if (!(clazz = env->FindClass("java/lang/Byte"))
        || !(byteClass = (jclass) env->NewGlobalRef(clazz))) {
        DeInitializeStatic();
        return;  
    }
    if (!(clazz = env->FindClass("java/lang/Short"))
        || !(shortClass = (jclass) env->NewGlobalRef(clazz))) {
        DeInitializeStatic();
        return;  
    }
    if (!(clazz = env->FindClass("java/lang/Integer"))
        || !(integerClass = (jclass) env->NewGlobalRef(clazz))) {
        DeInitializeStatic();
        return;  
    }
    if (!(clazz = env->FindClass("java/lang/Long"))
        || !(longClass = (jclass) env->NewGlobalRef(clazz))) {
        DeInitializeStatic();
        return;  
    }
    if (!(clazz = env->FindClass("java/lang/Float"))
        || !(floatClass = (jclass) env->NewGlobalRef(clazz))) {
        DeInitializeStatic();
        return;  
    }
    if (!(clazz = env->FindClass("java/lang/Double"))
        || !(doubleClass = (jclass) env->NewGlobalRef(clazz))) {
        DeInitializeStatic();
        return;  
    } 

    if (!(clazz = env->FindClass("java/lang/String"))
        || !(stringClass = (jclass) env->NewGlobalRef(clazz))) {
        DeInitializeStatic();
        return;  
    }
    
    if (!(booleanInitMID = env->GetMethodID(booleanClass,"<init>","(Z)V"))) {
        DeInitializeStatic();
        return;
    }
    if (!(booleanValueMID = env->GetMethodID(booleanClass,"booleanValue","()Z"))) {
        DeInitializeStatic();
        return;
    }

    if (!(characterInitMID = env->GetMethodID(characterClass,"<init>","(C)V"))) {
        DeInitializeStatic();
        return;
    }
    if (!(characterValueMID = env->GetMethodID(characterClass,"charValue","()C"))) {
        DeInitializeStatic();
        return;
    }

    if (!(byteInitMID = env->GetMethodID(byteClass,"<init>","(B)V"))) {
        DeInitializeStatic();
        return;
    }
    if (!(byteValueMID = env->GetMethodID(byteClass,"byteValue","()B"))) {
        DeInitializeStatic();
        return;
    }
    if (!(shortInitMID = env->GetMethodID(shortClass,"<init>","(S)V"))) {
        DeInitializeStatic();
        return;
    }
    if (!(shortValueMID = env->GetMethodID(shortClass,"shortValue","()S"))) {
        DeInitializeStatic();
        return;
    }

    if (!(integerInitMID = env->GetMethodID(integerClass,"<init>","(I)V"))) {
        DeInitializeStatic();
        return;
    }
    if (!(integerValueMID = env->GetMethodID(integerClass,"intValue","()I"))) {
        DeInitializeStatic();
        return;
    }

    if (!(longInitMID = env->GetMethodID(longClass,"<init>","(J)V"))) {
        DeInitializeStatic();
        return;
    }
    if (!(longValueMID = env->GetMethodID(longClass,"longValue","()J"))) {
        DeInitializeStatic();
        return;
    }
    
    if (!(floatInitMID = env->GetMethodID(floatClass,"<init>","(F)V"))) {
        DeInitializeStatic();
        return;
    }
    if (!(floatValueMID = env->GetMethodID(floatClass,"floatValue","()F"))) {
        DeInitializeStatic();
        return;
    }

    if (!(doubleInitMID = env->GetMethodID(doubleClass,"<init>","(D)V"))) {
        DeInitializeStatic();
        return;
    }
    if (!(doubleValueMID = env->GetMethodID(doubleClass,"doubleValue","()D"))) {
        DeInitializeStatic();
        return;
    }

    
        
}

void bcJavaMarshalToolkit::DeInitializeStatic() {     //nb need to do

}

