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
 * Sergey Lunegov <lsv@sparc.spb.su>
 */

#include "prmem.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIAllocator.h"
#include "nsCOMPtr.h"
#include "xptcall.h"
#include "nsCRT.h"
#include "urpMarshalToolkit.h"
#include <unistd.h>
#include "urpStub.h"


#include "nsIModule.h"


class urpAllocator : public bcIAllocator { //nb make is smarter. It should deallocate allocated memory.
public:
    urpAllocator(nsIAllocator *_allocator) {
        allocator = _allocator;
    }
    virtual ~urpAllocator() {}
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


urpMarshalToolkit::urpMarshalToolkit(PRBool isclnt) {
	isClient = isclnt;
}


urpMarshalToolkit::~urpMarshalToolkit() {
}

nsresult
urpMarshalToolkit::WriteElement(bcIUnMarshaler *um, nsXPTParamInfo * param, uint8 type, uint8 ind,
			nsIInterfaceInfo* interfaceInfo, urpPacket* message,
			PRUint16 methodIndex, const nsXPTMethodInfo *info,
			bcIAllocator * allocator) {
	void* data = allocator->Alloc(sizeof(void*));
	nsID *id;
	char* help;
   	nsresult r = NS_OK;
	switch(type) {
                case nsXPTType::T_IID    :
		  id = new nsID();
		  um->ReadSimple(id,XPTType2bcXPType(type));
		  help = ((nsID)(*id)).ToString();
		  printf("nsIID %s\n", help);
		  message->WriteString(help, strlen(help));
		  delete id;
		  PR_Free(help);
                  break;
                case nsXPTType::T_I8  :
		  um->ReadSimple(data,XPTType2bcXPType(type));
                  message->WriteByte(*(char*)data);
                  break;
                case nsXPTType::T_I16 :
		  um->ReadSimple(data,XPTType2bcXPType(type));
                  message->WriteShort(*(short*)data);
                  break;
                case nsXPTType::T_I32 :
		  um->ReadSimple(data,XPTType2bcXPType(type));
                  message->WriteInt(*(int*)data);
                  break;
                case nsXPTType::T_I64 :
		  um->ReadSimple(data,XPTType2bcXPType(type));
                  message->WriteLong(*(long*)data);
                  break;
                case nsXPTType::T_U8  :
		  um->ReadSimple(data,XPTType2bcXPType(type));
                  message->WriteByte(*(char*)data);
                  break;
                case nsXPTType::T_U16 :
		  um->ReadSimple(data,XPTType2bcXPType(type));
                  message->WriteShort(*(short*)data);
                  break;
                case nsXPTType::T_U32 :
		  um->ReadSimple(data,XPTType2bcXPType(type));
                  message->WriteInt(*(int*)data);
                  break;
                case nsXPTType::T_U64 :
		  um->ReadSimple(data,XPTType2bcXPType(type));
                  message->WriteLong(*(long*)data);
                  break;
                case nsXPTType::T_FLOAT  :
		  um->ReadSimple(data,XPTType2bcXPType(type));
                  message->WriteFloat(*(float*)data);
                  break;
                case nsXPTType::T_DOUBLE :
		  um->ReadSimple(data,XPTType2bcXPType(type));
                  message->WriteDouble(*(double*)data);
                  break;
		case nsXPTType::T_CHAR   :
        	case nsXPTType::T_WCHAR  :
		  um->ReadSimple(data,XPTType2bcXPType(type));
		  message->WriteByte(*(char*)data);
		  break;
		case nsXPTType::T_CHAR_STR  :
                case nsXPTType::T_WCHAR_STR :
                  {
		   size_t size;
            	   um->ReadString(data,&size,allocator);
            	   {
                    char *str = *(char**)data;
                    for (int i = 0; i < size && type == nsXPTType::T_WCHAR_STR; i++) {
                    	 char c = str[i];
                    }
            	   }
                   data = *(char **)data;
                   size_t length = 0;
		   if(data != nsnull) {
                      if (type == nsXPTType::T_WCHAR_STR) {
                       length = nsCRT::strlen((const PRUnichar*)data);
                       length *= 2;
                       length +=2;
                       for (int i = 0; i < length && type == nsXPTType::T_WCHAR_STR; i++) {
                        char c = ((char*)data)[i];
                       }
                      } else {
                       length = nsCRT::strlen((const char*)data);
                       length+=1;
                      }
		   }
                   message->WriteString((char*)data,length);
                   break;
                  }
		case nsXPTType::T_INTERFACE :
        	case nsXPTType::T_INTERFACE_IS :
            	{
                  nsIID iid;
                  bcOID oid;
		  um->ReadSimple(&oid,XPTType2bcXPType(type));
		  um->ReadSimple(&iid,bc_T_IID);
                  WriteOid(oid, message);
                  WriteType(iid, message);
                  break;
                }
		case nsXPTType::T_PSTRING_SIZE_IS:
                case nsXPTType::T_PWSTRING_SIZE_IS:
                case nsXPTType::T_ARRAY:             //nb array of interfaces [to do]
            {
		size_t size;
		nsXPTType datumType;
		if(type != nsXPTType::T_ARRAY) {
                   um->ReadString(data,&size,allocator);
                   {
                       char *str = *(char**)data;
                       for (int i = 0; i < size && type == nsXPTType::T_WCHAR_STR; i++) {
                           char c = str[i];
                       }
		   }
                } else {
                   if(NS_FAILED(interfaceInfo->GetTypeForParam(methodIndex, param, 1,&datumType))) {
		    allocator->Free(data);
                    return NS_ERROR_FAILURE;
                   }
                   PRUint32 arraySize;
                   PRInt16 elemSize = GetSimpleSize(datumType);
                   um->ReadSimple(&arraySize,bc_T_U32);
                   message->WriteInt(arraySize);
		   for (unsigned int i = 0; i < arraySize; i++) {
			r = WriteElement(um,param,datumType.TagPart(),0,
                        interfaceInfo, message, methodIndex, info, allocator);
			if(NS_FAILED(r)) {
			   allocator->Free(data);
			   return r;
			}
		   }
		}
                if (type != nsXPTType::T_ARRAY) {
                    size_t length = 0;
                    if (type == nsXPTType::T_PWSTRING_SIZE_IS) {
                        length = size * sizeof(PRUnichar);
                    } else {
                        length = size;
                    }
                    message->WriteString((char*)data, length);
                }
                break;
            }
        default:
	    allocator->Free(data);
            return r;
    }
    allocator->Free(data);
    return r;
}

nsresult
urpMarshalToolkit::WriteParams(bcICall *call, PRUint32 paramCount, const nsXPTMethodInfo *info, nsIInterfaceInfo* interfaceInfo, urpPacket* message, PRUint16 methodIndex) {
	int i;
	nsresult rv = NS_OK;
	bcIAllocator * allocator = new urpAllocator(nsAllocator::GetGlobalAllocator());
	bcIUnMarshaler *um = call->GetUnMarshaler();
	if(!isClient) {
	   nsresult result;
	   um->ReadSimple(&result, bc_T_U32);
	   if(!NS_SUCCEEDED(result)) {
	      printf("Returned result is error on server side\n");
	   }
	   message->WriteInt(result);
	}
	for(i=0;i<paramCount;i++) {
	    short cache_index;
	    nsXPTParamInfo param = info->GetParam(i);
	    PRBool isOut = param.IsOut();
	    if ((isClient && !param.IsIn()) ||
		(!isClient && !param.IsOut())) {
		continue;
	    }
	    rv = WriteElement(um, &param,param.GetType().TagPart(), i, interfaceInfo, message, methodIndex, info, allocator);
	    if(NS_FAILED(rv)) {
		delete allocator;
		delete um;
		return rv;
	    }


	}
	delete allocator;
	delete um;
	return rv;
}

nsresult
urpMarshalToolkit::ReadElement(nsXPTParamInfo * param, uint8 type,
                        nsIInterfaceInfo* interfaceInfo, urpPacket* message,
                        PRUint16 methodIndex, bcIAllocator* allocator,
			bcIMarshaler* m, bcIORB *broker, urpManager* man,
			urpConnection* conn) {
	void* data = allocator->Alloc(sizeof(void*));
	nsresult r = NS_OK;
	char* str;
	nsID id;
	switch(type) {
                case nsXPTType::T_IID    :
		{
		  int size;
		  str = message->ReadString(size);
		  id.Parse((char*)str);
		  m->WriteSimple((char*)&id, XPTType2bcXPType(type));
		  PR_Free(str);
                  break;
		}
                case nsXPTType::T_I8  :
                  *(char*)data = message->ReadByte();
		  m->WriteSimple(data, XPTType2bcXPType(type));
                  break;
                case nsXPTType::T_I16 :
                  *(short*)data = message->ReadShort();
		  m->WriteSimple(data, XPTType2bcXPType(type));
                  break;
                case nsXPTType::T_I32 :
                  *(int*)data = message->ReadInt();
		  m->WriteSimple(data, XPTType2bcXPType(type));
                  break;
                case nsXPTType::T_I64 :
                  *(long*)data = message->ReadLong();
		  m->WriteSimple(data, XPTType2bcXPType(type));
                  break;
                case nsXPTType::T_U8  :
                  *(char*)data = message->ReadByte();
		  m->WriteSimple(data, XPTType2bcXPType(type));
                  break;
                case nsXPTType::T_U16 :
                  *(short*)data = message->ReadShort();
		  m->WriteSimple(data, XPTType2bcXPType(type));
                  break;
                case nsXPTType::T_U32 :
                  *(int*)data = message->ReadInt();
		  m->WriteSimple(data, XPTType2bcXPType(type));
                  break;
                case nsXPTType::T_U64 :
                  *(long*)data = message->ReadLong();
		  m->WriteSimple(data, XPTType2bcXPType(type));
                  break;
                case nsXPTType::T_FLOAT  :
                  *(float*)data = message->ReadFloat();
		  m->WriteSimple(data, XPTType2bcXPType(type));
                  break;
                case nsXPTType::T_DOUBLE :
                  *(double*)data = message->ReadDouble();
		  m->WriteSimple(data, XPTType2bcXPType(type));
                  break;
                case nsXPTType::T_CHAR_STR  :
                case nsXPTType::T_WCHAR_STR :
		{
                  int size;
/*
                  *(char**)data = message->ReadString(size);
		  data = *(char **)data;
*/
		  str = message->ReadString(size);
		  data = str;
                  size_t length = 0;
                  if (type == nsXPTType::T_WCHAR_STR) {
                    length = nsCRT::strlen((const PRUnichar*)data);
                    length *= 2;
                    length +=2;
                    for (int i = 0; i < length && type == nsXPTType::T_WCHAR_STR; i++) {
                        char c = ((char*)data)[i];
                    }
                  } else {
                    length = nsCRT::strlen((const char*)data);
                    length+=1;
                  }
                  m->WriteString(data,length);
		  PR_Free(str);
                  break;
		}
		case nsXPTType::T_INTERFACE :
        	case nsXPTType::T_INTERFACE_IS :
		{
                    bcOID oid = ReadOid(message);
                    nsIID iid = ReadType(message);
                    nsISupports *proxy = NULL;
                    if (oid != 0) {
			   urpStub* stub = new urpStub(man, conn);
			   broker->RegisterStubWithOID(stub, &oid);
                    }
                    m->WriteSimple(&oid, XPTType2bcXPType(type));
                    m->WriteSimple(&iid,bc_T_IID);
                    break;
		}
		case nsXPTType::T_PSTRING_SIZE_IS:
        	case nsXPTType::T_PWSTRING_SIZE_IS:
        case nsXPTType::T_ARRAY:
            {
                nsXPTType datumType;
                if(NS_FAILED(interfaceInfo->GetTypeForParam(methodIndex, param, 1,&datumType))) {
		    allocator->Free(data);
                    return r;
                }
                PRUint32 arraySize;
                PRInt16 elemSize = GetSimpleSize(datumType);
                arraySize = message->ReadInt();

		if(type == nsXPTType::T_ARRAY) {
		   m->WriteSimple(&arraySize,bc_T_U32);
		   char *current = *(char**)data;
		   for (int i = 0; i < arraySize; i++) {
			ReadElement(param,datumType.TagPart(),interfaceInfo, message, methodIndex, allocator, m, broker, man, conn);
		   }
		} else {
		   size_t length = 0;
                    if (type == nsXPTType::T_PWSTRING_SIZE_IS) {
                        length = arraySize * sizeof(PRUnichar);
                    } else {
                        length = arraySize;
                    }
                    m->WriteString(data, length);
                }

                break;
            }
        default:
	    allocator->Free(data);
            return r;
        }
	allocator->Free(data);
	return r;
}

nsresult
urpMarshalToolkit::ReadParams(PRUint32 paramCount, const nsXPTMethodInfo *info, urpPacket* message, nsIInterfaceInfo *interfaceInfo, PRUint16 methodIndex, bcICall* call, bcIORB *orb, urpManager* man, urpConnection* conn) {
	bcIAllocator * allocator = new urpAllocator(nsAllocator::GetGlobalAllocator());
	bcIMarshaler* m = call->GetMarshaler();
	int i;
	nsresult rv = NS_OK;
	if(isClient) {
	   nsresult result = message->ReadInt();
	   m->WriteSimple(&result, bc_T_U32);
	   if (!NS_SUCCEEDED(result)) {
		printf("Returned result is error on client side\n");
	   }
	}
	for(i=0;i<paramCount;i++) {
	    short cache_index;
	    nsXPTParamInfo param = info->GetParam(i);
	    PRBool isOut = param.IsOut();

	    if ((!isClient && !param.IsIn()) ||
		(isClient && !param.IsOut())) {
		continue;
	    }

	    rv = ReadElement(&param, param.GetType().TagPart(), interfaceInfo, 
		message, methodIndex, allocator, m, orb, man, conn);
	    if(NS_FAILED(rv)) {
		delete allocator;
		delete m;
		return rv;
	    }


	}
	delete allocator;
	delete m;
	return rv;
}

PRInt16 urpMarshalToolkit::GetSimpleSize(uint8 type) {
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

bcXPType urpMarshalToolkit::XPTType2bcXPType(uint8 type) {
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
void 
urpMarshalToolkit::WriteType(bcIID iid, urpPacket* message) {

	short cache_index;

	char typeClass = INTERFACE;
	int found = 0;
	
	if(0) //here should be checking on whether class is simple or not
	   printf("class is simple\n");
	else {
	   if(0) { // here should be checking on whether cache is used
	      printf("cache is used\n");
	      cache_index = 0x0;
	   }
	   else
	      cache_index = (short)0xffff;
	   
	   message->WriteByte((char)typeClass | (found ? 0x0 : 0x80));
printf("write type is %x\n",(char)typeClass | (found ? 0x0 : 0x80));
	   message->WriteShort(cache_index);
printf("write type is %x\n",cache_index);

	   if(!found) {
		char* iidStr = iid.ToString();
		message->WriteString(iidStr, strlen(iidStr));
		PR_Free(iidStr);
	   }
	}
}

bcIID
urpMarshalToolkit::ReadType(urpPacket* message) {

	char byte = (char)message->ReadByte();
	char typeClassValue = byte & 0xff;

	short cache_index = message->ReadShort();
	
	int size;
	nsIID iid;
	char* name = message->ReadString(size);
	iid.Parse(name);
	PR_Free(name);
	return iid;
}

void 
urpMarshalToolkit::WriteOid(bcOID oid, urpPacket* message) {
	short cache_index;
	int found = 0;

	if(0) { // here should be checking on whether cache is used
	   printf("cache is used\n");
	   cache_index = 0x0;
	}
	else
	   cache_index = (short)0xffff;

	char* str = (char*)calloc(100, sizeof(char));
	sprintf(str,"%ld",oid);
	message->WriteString(str, strlen(str));
	free(str);
	message->WriteShort(cache_index);
}

bcOID
urpMarshalToolkit::ReadOid(urpPacket* message) {
	int size;
	bcOID result;

	char* str = message->ReadString(size);
	short cache_index = message->ReadShort();
	result = (bcOID)atol(str);
	PR_Free(str);
	return result;
}

void
urpMarshalToolkit::WriteThreadID(bcTID tid, urpPacket* message) {
	short cache_index;
	int found = 0;

	char realTID[4]; /* this is hack: I know that bcTID is PRUint32
			    so it is represented by four bytes */
	realTID[0] = tid>>24 & 0xff;
	realTID[1] = (tid>>16) & 0xff;
	realTID[2] = (tid>>8) & 0xff;
	realTID[3] = tid & 0xff;

	if(0) { // here should be checking on whether cache is used
	   printf("cache is used\n");
	   cache_index = 0x0;
	}
	else
	   cache_index = (short)0xffff;

	message->WriteOctetStream(realTID, 4);
	message->WriteShort(cache_index);
}   

bcTID
urpMarshalToolkit::ReadThreadID(urpPacket* message) {
	int size = 0;
	bcTID result;
	char* array = message->ReadOctetStream(size);
	short cache_index = message->ReadShort();
	result = ((unsigned char)array[0]<<24) + ((unsigned char)array[1]<<16) + ((unsigned char)array[2]<<8) + ((unsigned char)array[3]&0xff);
	PR_Free(array);
	return result;
}
