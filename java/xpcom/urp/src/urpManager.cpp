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
#include "nsCOMPtr.h"
#include "xptcall.h"
#include "nsCRT.h"
#include "urpManager.h"
#include <unistd.h>
#include "urpStub.h"
#include "urpMarshalToolkit.h"


#include "nsIModule.h"


class threadHashKey : public nsHashKey {
public:
        threadHashKey(bcTID thrdID) : threadID(thrdID) {}

        virtual PRUint32 HashCode(void) const
        {
                return PRUint32(threadID);
        }

        virtual PRBool Equals(const nsHashKey *aKey) const
        {
                return ((threadHashKey*)aKey)->threadID == threadID;
        }

        virtual nsHashKey *Clone(void) const
        {
                return new threadHashKey(threadID);
        }

private:
        bcTID threadID;
};

struct localThreadArg {
    urpManager *mgr;
    urpConnection *conn;
    PRBool isClnt;
    localThreadArg( urpManager *mgr, urpConnection *conn, PRBool ic ) {
        this->mgr = mgr;
        this->conn = conn;
	this->isClnt = ic;
    }
};

struct monitCall {
    PRMonitor *mon;
    bcICall* call;
    monitCall(PRMonitor *m, bcICall* c) {
	this->mon = m;
	this->call = c;
    }
};

void thread_start( void *arg )
{
    urpManager *manager = ((localThreadArg *)arg)->mgr;
    urpConnection *connection = ((localThreadArg *)arg)->conn;
    PRBool ic = ((localThreadArg *)arg)->isClnt;
    nsresult rv = manager->ReadMessage( connection, ic );
}

urpManager::urpManager(PRBool IsClient, bcIORB *orb, urpConnection* conn) {
    broker = orb;
    monitTable = new nsHashtable(20);
    if(IsClient) {
	connTable = nsnull;
	connection = conn;
	localThreadArg *arg = new localThreadArg( this, conn, PR_TRUE );
	PRThread *thr = PR_CreateThread( PR_USER_THREAD,
                                                  thread_start,
                                                  arg,
                                                  PR_PRIORITY_NORMAL,
                                                  PR_GLOBAL_THREAD,
                                                  PR_UNJOINABLE_THREAD,
                                                  0);
	if(thr == nsnull) {
	   printf("Error couldn't run listener\n");
	   exit(-1);
	}
    } else {
	connection = nsnull;
	connTable = new nsHashtable(20);
    }
}


urpManager::~urpManager() {
   if(connTable)
	delete connTable;
}



void urpManager::SendUrpRequest(bcOID oid, bcIID iid,
                                PRUint16 methodIndex,
                          nsIInterfaceInfo* interfaceInfo,
			  bcICall *call,
			  PRUint32 paramCount, const nsXPTMethodInfo* info) {
	printf("this is method sendUrpRequest and mid is %x\n",methodIndex);
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

	urpMarshalToolkit* mt = new urpMarshalToolkit(PR_TRUE);
	mt->WriteType(iid, message);
	mt->WriteOid(oid, message);
	bcTID thrID = (bcTID)PR_GetCurrentThread();
printf("OID is written %ld\n", thrID);
	mt->WriteThreadID(thrID, message);
	broker = call->GetORB();
	mt->WriteParams(call, paramCount, info, interfaceInfo, message, methodIndex);
	delete mt;
	if(connTable) {
	   bcTID thrID = (bcTID)PR_GetCurrentThread();
           threadHashKey thrHK(thrID);
           urpConnection* con = connTable->Get(&thrHK);
	   con->Write(message);
	} else
	   connection->Write(message);
	delete message;
}

void urpManager::TransformMethodIDAndIID() {
	printf("this is method transformMethodIDAndIID\n");
}

nsresult
urpManager::ReadReply(urpPacket* message, char header,
		    bcICall* call, PRUint32 paramCount, 
		    const nsXPTMethodInfo *info, 
		    nsIInterfaceInfo *interfaceInfo, PRUint16 methodIndex) {
	nsresult rv = NS_OK;
        printf("this is method readReply\n");
	urpMarshalToolkit* mt = new urpMarshalToolkit(PR_TRUE); 
	rv = mt->ReadParams(paramCount, info, message, interfaceInfo, methodIndex, call, broker, this);
	delete mt;
	return rv;
}

nsresult
urpManager::ReadMessage(urpConnection* conn, PRBool isClient) {
	nsresult rv = NS_OK;
	if(!isClient) {
	   bcTID thrID = (bcTID)PR_GetCurrentThread();
           threadHashKey thrHK(thrID);
           connTable->Put(&thrHK, conn);
	}
	while(conn->GetStatus() == urpSuccess) {
	   urpPacket* message = conn->Read();
	   char header = message->ReadByte();
	   bcTID tid;
	   if((header & BIG_HEADER) != 0) { // full header?
               if((header & REQUEST) != 0) // a request ?
                   rv = ReadLongRequest(header, message);
               else { // a reply
		   bcIID iid; bcOID oid; bcMID mid;

		   bcTID tid;
		   urpMarshalToolkit* mt = new urpMarshalToolkit(PR_TRUE);
		   if((header & NEWTID) != 0) { // new thread id ?
		      printf("new threadID\n");
		      tid = mt->ReadThreadID(message);
		   }
		   else
		      printf("old threadID\n");
		   delete mt;
		   threadHashKey thrHK(tid);
		   monitCall* mc = monitTable->Get(&thrHK);

  		   mc->call->GetParams(&iid, &oid, &mid);
		   nsIInterfaceInfo *interfaceInfo;
  		   nsIInterfaceInfoManager* iimgr;
  		   if( (iimgr = XPTI_GetInterfaceInfoManager()) ) {
        	     if (NS_FAILED(iimgr->GetInfoForIID(&iid, &interfaceInfo))) {
		       printf("Error in ReadMessage\n");
            	       return NS_ERROR_FAILURE;  //nb exception handling
        	     }
        	     NS_RELEASE(iimgr);
  		   } else {
		     printf("Error in ReadMessage in second place\n");
        	     return NS_ERROR_FAILURE;
  		   }

  		   nsXPTMethodInfo* info;
  		   interfaceInfo->GetMethodInfo(mid, (const nsXPTMethodInfo **)&info);
  		   PRUint32 paramCount = info->GetParamCount();
                   ReadReply(message, header, mc->call, paramCount, info, interfaceInfo, mid);
		   PR_EnterMonitor(mc->mon);
		   PR_Notify(mc->mon);
		   PR_ExitMonitor(mc->mon);
		}
           }
           else // only a short request header
               rv = ReadShortRequest(header, message);
	   delete message;
	}
	if(!isClient) {
	   bcTID thrID = (bcTID)PR_GetCurrentThread();
	   threadHashKey thrHK(thrID);
	   connTable->Remove(&thrHK);
	}
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
urpManager::SendReply(bcTID tid, bcICall* call, PRUint32 paramCount,
		   const nsXPTMethodInfo* info, 
		   nsIInterfaceInfo *interfaceInfo, PRUint16 methodIndex) {
	nsresult rv = NS_OK;
	char header = (char)BIG_HEADER;
	header |= NEWTID;
	urpPacket* message = new urpPacket();
	message->WriteByte(header);
	urpMarshalToolkit* mt = new urpMarshalToolkit(PR_FALSE);
	mt->WriteThreadID(tid, message);
	rv = mt->WriteParams(call, paramCount, info, interfaceInfo, message,
			methodIndex);
	delete mt;
	if(NS_FAILED(rv)) return rv;
	if(connTable) {
	   bcTID thrID = (bcTID)PR_GetCurrentThread();
           threadHashKey thrHK(thrID);
           urpConnection* con = (urpConnection*)connTable->Get(&thrHK);
	   con->Write(message);
	} else
	   connection->Write(message);
	delete message;
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

	urpMarshalToolkit* mt = new urpMarshalToolkit(PR_FALSE);
	if((header & NEWTYPE) != 0)
                        iid = mt->ReadType(message);

        if((header & NEWOID) != 0) // new oid?
                        oid = mt->ReadOid(message);

        if((header & NEWTID) != 0) // new thread id ?
                        tid = mt->ReadThreadID(message);

printf("method readLongRequest: tid %ld %ld\n",tid,oid);
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
		delete mt;
                return NS_ERROR_FAILURE;  //nb exception handling
            }
            NS_RELEASE(iimgr);
        } else {
	    delete mt;
            return NS_ERROR_FAILURE;
        }
char* name;
  interfaceInfo->GetName(&name);
  printf("in handleRequest interface name is %s\n",name);
        nsXPTMethodInfo* info;
        interfaceInfo->GetMethodInfo(methodId,(const nsXPTMethodInfo **)&info);
	PRUint32 paramCount = info->GetParamCount();
	bcICall *call = broker->CreateCall(&iid, &oid, methodId);
	mt->ReadParams(paramCount, info, message, interfaceInfo, methodId, call, broker, this);
	delete mt;
        //nb return value; excepion handling
        broker->SendReceive(call);
	rv = SendReply(tid, call, paramCount, info, interfaceInfo, methodId);
	return rv;
}

nsresult 
urpManager::SetCall(bcICall* call, PRMonitor *m) {
printf("method SetCall %p %p %p\n",call, m, this);
	monitCall* mc = new monitCall(m, call);
	bcTID thrID = (bcTID)PR_GetCurrentThread();
        threadHashKey thrHK(thrID);
        monitTable->Put(&thrHK, mc);
	return NS_OK;
}

nsresult
urpManager::RemoveCall() {
printf("method RemoveCall\n");
        bcTID thrID = (bcTID)PR_GetCurrentThread();
        threadHashKey thrHK(thrID);
        monitCall* mc = monitTable->Remove(&thrHK);
	delete mc;
        return NS_OK;
}
