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
#include "urpManager.h"
#include <unistd.h>

#include "nsIModule.h"

#include "bcIXPCOMStubsAndProxies.h"
#include "bcXPCOMStubsAndProxiesCID.h"

static NS_DEFINE_CID(kXPCOMStubsAndProxies,BC_XPCOMSTUBSANDPROXIES_CID);

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

urpManager::urpManager(urpTransport* trans) {
    broker = NULL;
    connection = NULL;
    transport = trans;
    nsresult r;
}

urpManager::urpManager(urpTransport* trans, bcIORB *orb) {
    broker = orb;
    connection = NULL;
    transport = trans;
}


urpManager::~urpManager() {
}



void urpManager::SendUrpRequest(bcOID oid, bcIID iid,
                                PRUint16 methodIndex,
                          nsIInterfaceInfo* interfaceInfo,
			  bcICall *call, nsXPTCVariant* params,
			  PRUint32 paramCount, const nsXPTMethodInfo* info) {
	printf("this is method sendUrpRequest and mid is %x\n",methodIndex);
	connection = transport->GetConnection();
	long size = 0;
	long messagesCount = 0;
	urpPacket* message = new urpPacket();

	char header = 0x0;
	char bigHeader = 0x0;
	char synchron = 0x1;
	char mustReply = 0x1;

	if(1) { //there should be checking on whether oid is the new one
	   header |= NEWOID;
	   bigHeader = 0x1;
	}
	   
	if(1) { //there should be checking on whether type is the new one
	   header |= NEWTYPE;
	   bigHeader = 0x1;
	}

	if(1) { //there should be checking on whether threadid is the new one
	   header |= NEWTID;
	   bigHeader = 0x1;
	}

	if(bigHeader) {
	   header |= BIG_HEADER;
	   header |= 0x80;
	   header |= REQUEST;
	   header |= 0;

	   if(methodIndex > 255)
		header |= LONGMETHODID;

	   message->WriteByte(header);

	   if(methodIndex > 255)
		message->WriteShort(methodIndex);
	   else
		message->WriteByte((char)methodIndex);
	}
printf("header is written\n");

	WriteType(iid, message);
printf("Type is written\n");
	WriteOid(oid, message);
printf("OID is written\n");
	WriteThreadID(1000, message);
	broker = call->GetORB();
	WriteParams(params, paramCount, info, interfaceInfo, message, methodIndex);
printf("params are written\n");
	connection->Write(message);
	delete message;
printf("package is written\n");
}

void urpManager::TransformMethodIDAndIID() {
	printf("this is method transformMethodIDAndIID\n");
}

void 
urpManager::WriteType(bcIID iid, urpPacket* message) {

	short cache_index;
printf("IID %s\n",iid.ToString());

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

	   if(!found)
		message->WriteString(iid.ToString(), strlen(iid.ToString()));
	}
}

bcIID
urpManager::ReadType(urpPacket* message) {

	char byte = (char)message->ReadByte();
	char typeClassValue = byte & 0xff;

	short cache_index = message->ReadShort();
	
	int& size = 0;
	nsIID iid;
	char* name = message->ReadString(size);
	iid.Parse(name);
	return iid;
}

void 
urpManager::WriteOid(bcOID oid, urpPacket* message) {
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
urpManager::ReadOid(urpPacket* message) {
	int& size = 0;

	char* str = message->ReadString(size);
	short cache_index = message->ReadShort();
	return (bcOID)atol(str);
}

void
urpManager::WriteThreadID(bcTID tid, urpPacket* message) {
	short cache_index;
	int found = 0;

	char realTID[5] = {0x54, 0x14, 0x78, 0x71, 0x27};

	if(0) { // here should be checking on whether cache is used
	   printf("cache is used\n");
	   cache_index = 0x0;
	}
	else
	   cache_index = (short)0xffff;

	message->WriteOctetStream(realTID, 5);
	message->WriteShort(cache_index);
}   

bcTID
urpManager::ReadThreadID(urpPacket* message) {
	int& size = 0;

	char* array = message->ReadOctetStream(size);
	short cache_index = message->ReadShort();
	return (bcTID)array[0];
}

nsresult
urpManager::MarshalElement(void *data, nsXPTParamInfo * param, uint8 type, uint8 ind,
			nsIInterfaceInfo* interfaceInfo, urpPacket* message,
			PRUint16 methodIndex, const nsXPTMethodInfo *info,
			nsXPTCVariant* params) {
   	nsresult r = NS_OK;
	switch(type) {
                case nsXPTType::T_IID    :
printf("Marshalim T_IID\n");
                  data = *(char**)data;
		  message->WriteString((char*)data, strlen((char*)data));
                  break;
                case nsXPTType::T_I8  :
                  message->WriteByte(*(char*)data);
                  break;
                case nsXPTType::T_I16 :
                  message->WriteShort(*(short*)data);
                  break;
                case nsXPTType::T_I32 :
                  message->WriteInt(*(int*)data);
                  break;
                case nsXPTType::T_I64 :
                  message->WriteLong(*(long*)data);
                  break;
                case nsXPTType::T_U8  :
                  message->WriteByte(*(char*)data);
                  break;
                case nsXPTType::T_U16 :
                  message->WriteShort(*(short*)data);
                  break;
                case nsXPTType::T_U32 :
                  message->WriteInt(*(int*)data);
                  break;
                case nsXPTType::T_U64 :
                  message->WriteLong(*(long*)data);
                  break;
                case nsXPTType::T_FLOAT  :
                  message->WriteFloat(*(float*)data);
                  break;
                case nsXPTType::T_DOUBLE :
                  message->WriteDouble(*(double*)data);
                  break;
		case nsXPTType::T_CHAR_STR  :
                case nsXPTType::T_WCHAR_STR :
                  {
                   data = *(char **)data;
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
                   message->WriteString((char*)data,length);
                   break;
                  }
		case nsXPTType::T_INTERFACE :
        	case nsXPTType::T_INTERFACE_IS :
            	{
                  nsIID *iid;
                  if (type == nsXPTType::T_INTERFACE) {
                      if(NS_FAILED(r = interfaceInfo->GetIIDForParam(methodIndex, param, &iid))) {
                        	return r;
                      }
                  } else {
                      uint8 argnum;
                      if (NS_FAILED(r = interfaceInfo->GetInterfaceIsArgNumberForParam(methodIndex, param, &argnum))) {
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
                  bcOID oid = 0;
                  if (*(char**)data != NULL) {
                    NS_WITH_SERVICE(bcIXPCOMStubsAndProxies, xpcomStubsAndProxies, kXPCOMStubsAndProxies, &r);
                    if (NS_FAILED(r)) {
printf("NS_WITH_SERVICE(bcXPC in Marshal failed\n");
                        return r;
                    }
                    xpcomStubsAndProxies->GetOID(*(nsISupports**)data, broker,&oid);
                  }
                  WriteOid(oid, message);
		  bcIID biid = *(iid);
                  WriteType(biid, message);
                  break;
              }
		case nsXPTType::T_PSTRING_SIZE_IS:
        case nsXPTType::T_PWSTRING_SIZE_IS:
        case nsXPTType::T_ARRAY:             //nb array of interfaces [to do]
            {
                PRUint32 arraySize;
                if (!GetArraySizeFromParam(interfaceInfo,info, *param,methodIndex,
                                          ind,params,GET_LENGTH, &arraySize)) {
                    return r;
                }
                if (type == nsXPTType::T_ARRAY) {
                    nsXPTType datumType;
                    if(NS_FAILED(interfaceInfo->GetTypeForParam(methodIndex, param, 1,&datumType))) {
                        return r;
                    }
printf("arraySize %d %d\n",arraySize,(int)&arraySize);
                    message->WriteInt(arraySize);
                    PRInt16 elemSize = GetSimpleSize(datumType);
                    char *current = *(char**)data;
                    for (unsigned int i = 0; i < arraySize; i++, current+=elemSize)
 {
                        r = MarshalElement(current,param,datumType.TagPart(),0,
			interfaceInfo, message, methodIndex, info, params);
			if(NS_FAILED(r)) return r;
                    }
                } else {
                    size_t length = 0;
                    if (type == nsXPTType::T_PWSTRING_SIZE_IS) {
                        length = arraySize * sizeof(PRUnichar);
                    } else {
                        length = arraySize;
                    }
                    message->WriteString((char*)data, length);
                }
                break;
            }
        default:
            return r;
    }
    return r;
}

nsresult
urpManager::WriteParams(nsXPTCVariant* params, PRUint32 paramCount, const nsXPTMethodInfo *info, nsIInterfaceInfo* interfaceInfo, urpPacket* message, PRUint16 methodIndex) {
	int i;
	nsresult rv = NS_OK;
	for(i=0;i<paramCount;i++) {
	    short cache_index;
	    nsXPTParamInfo param = info->GetParam(i);
	    PRBool isOut = param.IsOut();
	    if ((transport->IsClient() && !param.IsIn()) ||
		(transport->IsServer() && !param.IsOut())) {
		continue;
	    }
	    nsXPTCVariant *value = & params[i];
            void *data;
            data = (isOut) ? value->val.p : value;
printf("before Marshal %d %d %d %d\n",methodIndex,paramCount,i,param.GetType().TagPart());
	    rv = MarshalElement(data, &param,param.GetType().TagPart(), i, interfaceInfo, message, methodIndex, info, params);
	    if(NS_FAILED(rv)) return rv;


/*
            if(0) //here should be checking on whether class is simple or not
               printf("class is simple\n");
            else {
               if(0) { // here should be checking on whether cache is used
                  printf("cache is used\n");
                  cache_index = 0x0;
               }
               else
                  cache_index = (short)0xffff;

               message->writeByte((char)typeClass | (found ? 0x0 : 0x80));
               message->writeShort(cache_index);

               if(!found)
                    message->writeString("com.sun.star.uno.XNamingService", 31);
            }
*/
	}
	return rv;
}

nsresult
urpManager::UnMarshal(void *data, nsXPTParamInfo * param, uint8 type,
                        nsIInterfaceInfo* interfaceInfo, urpPacket* message,
                        PRUint16 methodIndex, bcIAllocator * allocator) {
	nsresult r = NS_OK;
	switch(type) {
                case nsXPTType::T_IID    :
		{
printf("Unmarsahalim T_IID\n");
/*
                  *(char**)data = (char*)new nsIID();
                  data = *(char**)data;
*/
		  int& size = 0;
		  *(char**)data = message->ReadString(size);
                  break;
		}
                case nsXPTType::T_I8  :
                  *(char*)data = message->ReadByte();
                  break;
                case nsXPTType::T_I16 :
                  *(short*)data = message->ReadShort();
                  break;
                case nsXPTType::T_I32 :
                  *(int*)data = message->ReadInt();
                  break;
                case nsXPTType::T_I64 :
                  *(long*)data = message->ReadLong();
                  break;
                case nsXPTType::T_U8  :
                  *(char*)data = message->ReadByte();
                  break;
                case nsXPTType::T_U16 :
                  *(short*)data = message->ReadShort();
                  break;
                case nsXPTType::T_U32 :
                  *(int*)data = message->ReadInt();
                  break;
                case nsXPTType::T_U64 :
                  *(long*)data = message->ReadLong();
                  break;
                case nsXPTType::T_FLOAT  :
                  *(float*)data = message->ReadFloat();
                  break;
                case nsXPTType::T_DOUBLE :
                  *(double*)data = message->ReadDouble();
                  break;
		case nsXPTType::T_PSTRING_SIZE_IS:
        	case nsXPTType::T_PWSTRING_SIZE_IS:
                case nsXPTType::T_CHAR_STR  :
                case nsXPTType::T_WCHAR_STR :
		{
                  int& size = 0;
                  *(char**)data = message->ReadString(size);
                  break;
		}
		case nsXPTType::T_INTERFACE :
        	case nsXPTType::T_INTERFACE_IS :
		{
                    bcOID oid = ReadOid(message);
                    nsIID iid = ReadType(message);
                    nsISupports *proxy = NULL;
                    if (oid != 0) {
                        NS_WITH_SERVICE(bcIXPCOMStubsAndProxies, xpcomStubsAndProxies,
                                        kXPCOMStubsAndProxies, &r);
                        if (NS_FAILED(r)) {
printf("NS_WITH_SERVICE(bcXPCOMStubsAndProxies, xp failed\n");
                            return r;
                        }
                        xpcomStubsAndProxies->GetProxy(oid, iid, broker,&proxy);
                    }
                    *(nsISupports**)data = proxy;
                    break;
		}
        case nsXPTType::T_ARRAY:
            {
                nsXPTType datumType;
                if(NS_FAILED(interfaceInfo->GetTypeForParam(methodIndex, param, 1,&datumType))) {
                    return r;
                }
                PRUint32 arraySize;
                PRInt16 elemSize = GetSimpleSize(datumType);
                arraySize = message->ReadInt();
printf("arraySize is %d\n",arraySize);

                char * current;
                *(char**)data = current = (char *) allocator->Alloc(elemSize*arraySize);
                //nb what about arraySize=0?
                for (unsigned int i = 0; i < arraySize; i++, current+=elemSize) {
                    r = UnMarshal(current, param, datumType.TagPart(), interfaceInfo, message, methodIndex, allocator);
		    if(NS_FAILED(r)) return r;
                }
                break;
            }
        default:
            return r;
        }
	return r;
}

nsresult
urpManager::ReadParams(nsXPTCVariant* params, PRUint32 paramCount, const nsXPTMethodInfo *info, urpPacket* message, nsIInterfaceInfo *interfaceInfo, PRUint16 methodIndex) {
	bcIAllocator * allocator = new urpAllocator(nsAllocator::GetGlobalAllocator());
	int i;
	nsresult rv = NS_OK;
	for(i=0;i<paramCount;i++) {
	    short cache_index;
	    nsXPTParamInfo param = info->GetParam(i);
	    PRBool isOut = param.IsOut();
	    nsXPTCMiniVariant tmpValue = params[i]; //we need to set value for client side
            nsXPTCMiniVariant * value;
            value = &tmpValue;
	    if (transport->IsServer() && param.IsOut()) {
	        value->val.p = allocator->Alloc(sizeof(nsXPTCMiniVariant)); // sizeof(nsXPTCMiniVariant) is good
                params[i].Init(*value,param.GetType(),0);
                params[i].ptr = params[i].val.p = value->val.p;
                params[i].flags |= nsXPTCVariant::PTR_IS_DATA;
	    }

	    if ((transport->IsServer() && !param.IsIn()) ||
		(transport->IsClient() && !param.IsOut())) {
		continue;
	    }

            void *data;
            data = (isOut) ? value->val.p : value;
printf("before UnMarshal %d %d %d %d\n",methodIndex,paramCount,i,param.GetType().TagPart());
	    rv = UnMarshal(data, &param, param.GetType().TagPart(), interfaceInfo, 
		message, methodIndex, allocator);
	    if(NS_FAILED(rv)) return rv;

	     params[i].Init(*value,param.GetType(),0);

/*
            if(0) //here should be checking on whether class is simple or not
               printf("class is simple\n");
            else {
               if(0) { // here should be checking on whether cache is used
                  printf("cache is used\n");
                  cache_index = 0x0;
               }
               else
                  cache_index = (short)0xffff;

               message->writeByte((char)typeClass | (found ? 0x0 : 0x80));
               message->writeShort(cache_index);

               if(!found)
                    message->writeString("com.sun.star.uno.XNamingService", 31);
            }
*/
	}
	free(allocator);
	return rv;
}

nsresult
urpManager::ReadReply(nsXPTCVariant* params, PRUint32 paramCount, const nsXPTMethodInfo *info, nsIInterfaceInfo *interfaceInfo, PRUint16 methodIndex) {
	nsresult rv = NS_OK;
        printf("this is method readReply\n");
	urpPacket* message = connection->Read();
	char header = message->ReadByte();
printf("header in reply is %x %x %x\n",header, BIG_HEADER, header & BIG_HEADER);
	if((header & BIG_HEADER) != 0) { // full header?
                        if((header & REQUEST) != 0) // a request ?
			   printf("long request\n");
			else // reply
			   rv = ParseReply(message, header, params, paramCount,
						info, interfaceInfo, methodIndex);
	}
	else
	   printf("short request header\n");
	delete connection;
//	transport->close();
	return rv;
}

nsresult
urpManager::ParseReply(urpPacket* message, char header,
		    nsXPTCVariant* params, PRUint32 paramCount, 
		    const nsXPTMethodInfo *info, 
		    nsIInterfaceInfo *interfaceInfo, PRUint16 methodIndex) {
	nsresult rv = NS_OK;
	if((header & NEWTID) != 0) { // new thread id ?
		printf("new threadID\n");
		bcTID tid = ReadThreadID(message);
	}
	else
		printf("old threadID\n");
	rv = ReadParams(params, paramCount, info, message, interfaceInfo, methodIndex);
	return rv;
}

nsresult
urpManager::HandleRequest(urpConnection* conn) {
printf("method handleRequest\n");
	connection = conn;
	urpPacket* message = connection->Read();
	nsresult rv = NS_OK;
	char header = message->ReadByte();
	bcTID tid;
	if((header & BIG_HEADER) != 0) { // full header?
            if((header & REQUEST) != 0) // a request ?
                rv = ReadLongRequest(header, message);
            else // a reply
                printf("Error: here should be request\n");
        }
        else // only a short request header
            rv = ReadShortRequest(header, message);
	delete message;
	return rv;
}

nsresult
urpManager::ReadShortRequest(char header, urpPacket* message) {
	nsresult rv = NS_OK;
	bcTID tid = 0;
	printf("null implementation of readShortRequest\n");
	return rv;
}

nsresult
urpManager::SendReply(bcTID tid, nsXPTCVariant *params, PRUint32 paramCount,
		   const nsXPTMethodInfo* info, 
		   nsIInterfaceInfo *interfaceInfo, PRUint16 methodIndex) {
	nsresult rv = NS_OK;
	char header = (char)BIG_HEADER;
	header |= NEWTID;
	urpPacket* message = new urpPacket();
	message->WriteByte(header);
	WriteThreadID(tid, message);
	rv = WriteParams(params, paramCount, info, interfaceInfo, message,
			methodIndex);
	if(NS_FAILED(rv)) return rv;
	connection->Write(message);
	delete message;
	delete connection;
	return rv;
}
	

nsresult
urpManager::ReadLongRequest(char header, urpPacket* message) {
	bcIID iid;
	bcOID oid;
	bcTID tid;
	PRUint16 methodId;
	nsresult rv = NS_OK;
	if((header & LONGMETHODID) != 0) // usigned short ?
                        methodId = message->ReadShort();
                else
                        methodId = message->ReadByte();

	if((header & NEWTYPE) != 0)
                        iid = ReadType(message);

        if((header & NEWOID) != 0) // new oid?
                        oid = ReadOid(message);

        if((header & NEWTID) != 0) // new thread id ?
                        tid = ReadThreadID(message);

printf("method readLongRequest: tid %ld\n",tid);
	char ignore_cache = ((header & IGNORECACHE) != 0); // do not use cache for this request?

        char mustReply;

        if((header & MOREFLAGS) != 0) {// is there an extended flags byte?
                        char exFlags = message->ReadByte();

                        mustReply   = (exFlags & MUSTREPLY) != 0;
        }
        else {
                        mustReply = 0x1;
        }

	nsIInterfaceInfo *interfaceInfo;
        nsIInterfaceInfoManager* iimgr;
        if( (iimgr = XPTI_GetInterfaceInfoManager()) ) {
            if (NS_FAILED(iimgr->GetInfoForIID(&iid, &interfaceInfo))) {
printf("here oblomalis\n");
                return NS_ERROR_FAILURE;  //nb exception handling
            }
            NS_RELEASE(iimgr);
        } else {
printf("zdesia oblom\n");
            return NS_ERROR_FAILURE;
        }
char* name;
  interfaceInfo->GetName(&name);
  printf("in handleRequest interface name is %s\n",name);
        nsXPTCVariant *params;
        nsXPTMethodInfo* info;
        interfaceInfo->GetMethodInfo(methodId,(const nsXPTMethodInfo **)&info);
	PRUint32 paramCount = info->GetParamCount();
	if (paramCount > 0) {
            params = (nsXPTCVariant *)  PR_Malloc(sizeof(nsXPTCVariant)*paramCount);
        }
	ReadParams(params, paramCount, info, message, interfaceInfo, methodId);
nsXPTCMiniVariant* tempVari = (nsXPTCMiniVariant *)  PR_Malloc(sizeof(nsXPTCMiniVariant)*paramCount);
	for(int ii =0; ii<paramCount;ii++)
	    tempVari[ii] = params[ii];
        //nb return value; excepion handling
//        XPTC_InvokeByIndex(object, methodId, paramCount, params);
	bcICall *call = broker->CreateCall(&iid, &oid, methodId);
        bcIMarshaler *marshaler = call->GetMarshaler();
        bcXPCOMMarshalToolkit * mt = new bcXPCOMMarshalToolkit(methodId, interfaceInfo, tempVari,broker);
        mt->Marshal(marshaler);
        broker->SendReceive(call);
        bcIUnMarshaler * unmarshaler = call->GetUnMarshaler();
        mt->UnMarshal(unmarshaler);
        delete call; delete marshaler; delete unmarshaler; delete mt;
	rv = SendReply(tid, params, paramCount, info, interfaceInfo, methodId);
	PR_Free(tempVari);
	PR_Free(params);
	return rv;
}

nsresult urpManager::GetArraySizeFromParam( nsIInterfaceInfo *_interfaceInfo,
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


PRInt16 urpManager::GetSimpleSize(uint8 type) {
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

