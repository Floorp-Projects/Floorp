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
#include "bcXPCOMMarshalToolkit.h"
#include "nsIServiceManager.h"
#include "bcORB.h"
#include "bcXPCOMStubsAndProxies.h"

static NS_DEFINE_CID(kORBCIID,BC_ORB_CID);
static NS_DEFINE_CID(kXPCOMStubsAndProxies,BC_XPCOMSTUBSANDPROXIES_CID);

bcXPCOMMarshalToolkit::bcXPCOMMarshalToolkit(PRUint16 _methodIndex, nsIInterfaceInfo *_interfaceInfo, 
					 nsXPTCMiniVariant* _params) {
    callSide = onClient;
    methodIndex = _methodIndex;
    interfaceInfo = _interfaceInfo;
    interfaceInfo->GetMethodInfo(methodIndex,(const nsXPTMethodInfo**) &info);  // These do *not* make copies ***explicit bending of XPCOM rules***
    PRUint32 paramCount = info->GetParamCount();
    if (paramCount > 0) {
        params = (nsXPTCVariant*)malloc(sizeof(nsXPTCVariant) * paramCount);
        if (params == nsnull) {
            return;
        }
        for (unsigned int i = 0; i < paramCount; i++) {
            (params)[i].Init(_params[i], info->GetParam(i).GetType());
            if (info->GetParam(i).IsOut()) {
                params[i].flags |= nsXPTCVariant::PTR_IS_DATA;
                params[i].ptr = params[i].val.p = _params[i].val.p;
            }
        }
    }
}

bcXPCOMMarshalToolkit::bcXPCOMMarshalToolkit(PRUint16 _methodIndex, nsIInterfaceInfo *_interfaceInfo, 
                                             nsXPTCVariant* _params) {
    callSide = onServer;
    methodIndex = _methodIndex;
    interfaceInfo = _interfaceInfo;
    interfaceInfo->GetMethodInfo(methodIndex,(const nsXPTMethodInfo **)&info);  // These do *not* make copies ***explicit bending of XPCOM rules***
    params = _params;
}

bcXPCOMMarshalToolkit::~bcXPCOMMarshalToolkit() {
    //nb
}

class xpAllocator : public bcIAllocator { //nb make is smarter. It should deallocate allocated memory.
public:
    xpAllocator(nsIAllocator *_allocator) {
        allocator = _allocator;
    }
    virtual ~xpAllocator() {}
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


nsresult bcXPCOMMarshalToolkit::Marshal(bcIMarshaler *m) {
    //printf("--bcXPCOMMarshalToolkit::Marshal\n");
    nsresult r = NS_OK;
    PRUint32 paramCount = info->GetParamCount();
    for (unsigned int i = 0; (i < paramCount) && NS_SUCCEEDED(r); i++) {
        nsXPTParamInfo param = info->GetParam(i);
        PRBool isOut = param.IsOut();
        if ((callSide == onClient  && !param.IsIn())
            || (callSide == onServer && !param.IsOut())) {
            continue;
        }
        nsXPTCVariant *value = & params[i];
        void *data;
        data = (isOut) ? value->val.p : value;
        r = MarshalElement(m,data,&param,param.GetType().TagPart(),i);
    }
    return r;
}

nsresult bcXPCOMMarshalToolkit::UnMarshal(bcIUnMarshaler *um) {
    bcIAllocator * allocator = new xpAllocator(nsAllocator::GetGlobalAllocator());
    PRUint32 paramCount = info->GetParamCount();
    for (unsigned int i = 0; i < paramCount; i++) {
        nsXPTParamInfo param = info->GetParam(i);
        PRBool isOut = param.IsOut();
        nsXPTCMiniVariant tmpValue = params[i]; //we need to set value for client side
        nsXPTCMiniVariant * value;
        value = &tmpValue;
        nsXPTType type = param.GetType();
        
        if (callSide == onServer
            && param.IsOut()) { //we need to allocate memory for out parametr
            value->val.p = allocator->Alloc(sizeof(nsXPTCMiniVariant)); // sizeof(nsXPTCMiniVariant) is good
            params[i].Init(*value,type);
            params[i].ptr = params[i].val.p = value->val.p;
            params[i].flags |= nsXPTCVariant::PTR_IS_DATA;
        }
        
        if ( (callSide == onServer && !param.IsIn() 
              || (callSide == onClient && !param.IsOut()))){ 
            continue;
        }
        void *data = (isOut) ? value->val.p : value;
        UnMarshalElement(data, um, &param, param.GetType().TagPart(),allocator);
        params[i].Init(*value,type);
    }
    return NS_OK;
}

nsresult bcXPCOMMarshalToolkit::GetArraySizeFromParam( nsIInterfaceInfo *_interfaceInfo, 
                                                       const nsXPTMethodInfo* method,
                                                       const nsXPTParamInfo& param,
                                                       uint16 _methodIndex,
                                                       uint8 paramIndex,
                                                       nsXPTCVariant* nativeParams,
                                                       SizeMode mode,
                                                       PRUint32* result) {
    //code borrowed from mozilla/js/src/xpconnect/src/xpcwrappedjsclass.cpp
    uint8 argnum;
    nsresult rv;
    if(mode == GET_SIZE) {
        rv = _interfaceInfo->GetSizeIsArgNumberForParam(_methodIndex, &param, 0, &argnum);
    } else {
        rv = _interfaceInfo->GetLengthIsArgNumberForParam(_methodIndex, &param, 0, &argnum);
    }
    if(NS_FAILED(rv)) {
        return PR_FALSE;
    }
    const nsXPTParamInfo& arg_param = method->GetParam(argnum);
    const nsXPTType& arg_type = arg_param.GetType();
    
    // XXX require PRUint32 here - need to require in compiler too!
    if(arg_type.IsPointer() || arg_type.TagPart() != nsXPTType::T_U32)
        return PR_FALSE;
    
    if(arg_param.IsOut())
        *result = *(PRUint32*)nativeParams[argnum].val.p;
    else
        *result = nativeParams[argnum].val.u32;
    
    return PR_TRUE;
}

bcXPType bcXPCOMMarshalToolkit::XPTType2bcXPType(uint8 type) {
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

nsresult bcXPCOMMarshalToolkit::MarshalElement(bcIMarshaler *m, void *data, nsXPTParamInfo * param, 
                                               uint8 type, uint8 ind) {
    //printf("--bcXPCOMMarshalToolkit::MarshalElement ind=%d\n",ind);
    nsresult r = NS_OK;
    switch(type) {
        case nsXPTType::T_IID    :
            data = *(char**)data;
        case nsXPTType::T_I8  :
        case nsXPTType::T_I16 :
        case nsXPTType::T_I32 :
        case nsXPTType::T_I64 :             
        case nsXPTType::T_U8  :             
        case nsXPTType::T_U16 :             
        case nsXPTType::T_U32 :             
        case nsXPTType::T_U64 :             
        case nsXPTType::T_FLOAT  :           
        case nsXPTType::T_DOUBLE :         
        case nsXPTType::T_BOOL   :         
        case nsXPTType::T_CHAR   :         
        case nsXPTType::T_WCHAR  : 
            m->WriteSimple(data, XPTType2bcXPType(type));
            break;
        case nsXPTType::T_CHAR_STR  :
        case nsXPTType::T_WCHAR_STR :      
            data = *(char **)data;
            m->WriteString(data,strlen((char*)data)+1);
            break;
        case nsXPTType::T_INTERFACE :      	    
        case nsXPTType::T_INTERFACE_IS :
            {
                nsIID *iid;
                if (type == nsXPTType::T_INTERFACE) {
                    if(NS_FAILED(r = interfaceInfo->
                                 GetIIDForParam(methodIndex, param, &iid))) {
                        return r;
                    }
                } else {
                    uint8 argnum;
                    if (NS_FAILED(r = interfaceInfo->GetInterfaceIsArgNumberForParam(methodIndex,
                                                                                     param, &argnum))) {
                        return r;
                    }
                    const nsXPTParamInfo& arg_param = info->GetParam(argnum);
                    const nsXPTType& arg_type = arg_param.GetType();
                    if(arg_type.IsPointer() &&
                       arg_type.TagPart() == nsXPTType::T_IID) {
                        if(arg_param.IsOut())
                            iid =*((nsID**)params[argnum].val.p);
                        else
                            iid = (nsID*)params[argnum].val.p;
                    }
                }
                //printf("--[c++]XPCOMMarshallToolkit INTERFACE iid=%s\n",iid->ToString());
                bcOID oid = 0;
                if (data != NULL) {
                    NS_WITH_SERVICE(bcORB, _orb, kORBCIID, &r);
                    if (NS_FAILED(r)) {
                        return r; //nb am I sure about that?
                    }
                    
                    NS_WITH_SERVICE(bcXPCOMStubsAndProxies, xpcomStubsAndProxies, kXPCOMStubsAndProxies, &r);
                    if (NS_FAILED(r)) {
                        return r;
                    }

                    bcIORB *orb;
                    _orb->GetORB(&orb);
                    bcIStub *stub = NULL;
                    xpcomStubsAndProxies->GetStub(*(nsISupports**)data, &stub);
                    oid = orb->RegisterStub(stub);
                }
                m->WriteSimple(&oid, XPTType2bcXPType(type));
                m->WriteSimple(iid,bc_T_IID);
                break;
            }
        case nsXPTType::T_PSTRING_SIZE_IS: 
        case nsXPTType::T_PWSTRING_SIZE_IS:
        case nsXPTType::T_ARRAY:
            //nb array of interfaces [to do]
            {
                PRUint32 arraySize;
                if (!GetArraySizeFromParam(interfaceInfo,info, *param,methodIndex,
                                          ind,params,GET_LENGTH, &arraySize)) { 
                    return NS_ERROR_FAILURE;
                }
                if (type == nsXPTType::T_ARRAY) {
                    nsXPTType datumType;
                    if(NS_FAILED(interfaceInfo->GetTypeForParam(methodIndex, param, 1,&datumType))) {
                        return NS_ERROR_FAILURE;
                    }
                    m->WriteSimple(&arraySize,bc_T_U32);
                    PRInt16 elemSize = GetSimpleSize(type);
                    char *current = *(char**)data;
                    for (unsigned int i = 0; i < arraySize; i++, current+=elemSize) {
                        MarshalElement(m,current,param,datumType.TagPart(),0);
                    }
                } else {
                    m->WriteString(data, arraySize);
                }
                break;
            }
        default:
            return NS_ERROR_FAILURE;
    }
    return r;
}

nsresult 
bcXPCOMMarshalToolkit::UnMarshalElement(void *data, bcIUnMarshaler *um, nsXPTParamInfo * param, uint8 type, bcIAllocator * allocator) {
    nsresult r = NS_OK;
    switch(type) {
        case nsXPTType::T_IID    :
            allocator->Free(*(char**)data);
            *(char**)data = (char*)new nsIID(); //nb memory leak. how are we going to release it
            data = *(char**)data;
        case nsXPTType::T_I8  :
        case nsXPTType::T_I16 :
        case nsXPTType::T_I32 :     
        case nsXPTType::T_I64 :             
        case nsXPTType::T_U8  :             
        case nsXPTType::T_U16 :             
        case nsXPTType::T_U32 :             
        case nsXPTType::T_U64 :             
        case nsXPTType::T_FLOAT  :           
        case nsXPTType::T_DOUBLE :         
        case nsXPTType::T_BOOL   :         
        case nsXPTType::T_CHAR   :         
        case nsXPTType::T_WCHAR  : 
            um->ReadSimple(data,XPTType2bcXPType(type));
            break;
        case nsXPTType::T_PSTRING_SIZE_IS: 
        case nsXPTType::T_PWSTRING_SIZE_IS:
        case nsXPTType::T_CHAR_STR  :
        case nsXPTType::T_WCHAR_STR :      
            size_t size;
            um->ReadString(data,&size,allocator);
            break;
        case nsXPTType::T_INTERFACE :      	    
        case nsXPTType::T_INTERFACE_IS :      	    
                {
                    printf("--[c++] we have an interface\n");
                    bcOID oid;
                    um->ReadSimple(&oid,XPTType2bcXPType(type));
                    printf("%d oid",(int) oid);
                    nsIID iid;
                    um->ReadSimple(&iid,bc_T_IID);
                    nsISupports *proxy = NULL;
                    if (oid != 0) {
                        NS_WITH_SERVICE(bcORB, _orb, kORBCIID, &r);
                        if (NS_FAILED(r)) {
                            return r; //nb am I sure about that?
                        }
                        NS_WITH_SERVICE(bcXPCOMStubsAndProxies, xpcomStubsAndProxies,
                                        kXPCOMStubsAndProxies, &r);
                        if (NS_FAILED(r)) {
                            return r;
                        }
                        bcIORB *orb;
                        _orb->GetORB(&orb);
                        xpcomStubsAndProxies->GetProxy(oid, iid, orb,&proxy);
                    }
                    *(nsISupports**)data = proxy;
                    break;
                }
        case nsXPTType::T_ARRAY:
            {
                nsXPTType datumType;
                if(NS_FAILED(interfaceInfo->GetTypeForParam(methodIndex, param, 1,&datumType))) {
                    return NS_ERROR_FAILURE;
                }
                PRUint32 arraySize;
                PRInt16 elemSize = GetSimpleSize(type);
                um->ReadSimple(&arraySize,bc_T_U32);
                
                char * current;
                *(char**)data = current = (char *) allocator->Alloc(elemSize*arraySize); 
                //nb what about arraySize=0?
                for (unsigned int i = 0; i < arraySize; i++, current+=elemSize) {
                    UnMarshalElement(current, um, param, datumType.TagPart(), allocator);
                }
                break;
            }
        default:
            return NS_ERROR_FAILURE;
    }
    return r;
}

PRInt16 bcXPCOMMarshalToolkit::GetSimpleSize(uint8 type) {
    PRInt16 size = -1;
    switch(type) {
    case nsXPTType::T_I8:
    case nsXPTType::T_U8:
        size = sizeof(PRInt8);
        break;
    case nsXPTType::T_I16:
    case nsXPTType::T_U16:
        size = sizeof(PRInt16);
        break;
    case nsXPTType::T_I32:
    case nsXPTType::T_U32:
        size = sizeof(PRInt32);
        break;
    case nsXPTType::T_I64:
    case nsXPTType::T_U64:
        size = sizeof(PRInt64);
            break;
    case nsXPTType::T_FLOAT:
        size = sizeof(float);
        break;
    case nsXPTType::T_DOUBLE:
        size = sizeof(double);
        break;
    case nsXPTType::T_BOOL:
        size = sizeof(PRBool);
        break;
    case nsXPTType::T_CHAR:
        size = sizeof(char);
        break;
    case nsXPTType::T_WCHAR:
        size = sizeof(PRUnichar);
        break;
    default:
        size = sizeof(void*);
    }
    return size;
}


