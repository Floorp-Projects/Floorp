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

struct sendThreadArg {
    urpManager* man;
    char header;
    urpPacket* message;
    bcIID iid;
    bcOID oid;
    bcTID tid;
    PRUint16 methodId;
    urpConnection* connection;
    sendThreadArg(urpManager* m, char header, urpPacket* message, bcIID iid,
    		  bcOID oid, bcTID tid, PRUint16 methodId,
		  urpConnection* conn) {
	this->man = m;
	this->header = header;
	this->message = message;
	this->iid = iid;
	this->oid = oid;
	this->tid = tid;
	this->methodId = methodId;
	this->connection = conn;
    }
};

struct monitCall {
    PRMonitor *mon;
    bcICall* call;
    urpPacket* mess;
    char header;
    bcIID iid;
    bcOID oid;
    bcTID tid;
    bcMID mid;
    int request;
    monitCall(PRMonitor *m, bcICall* c, urpPacket* mes, char h) {
	this->mon = m;
	this->call = c;
	this->mess = mes;
	this->header = h;
	this->request = 0;
    }
};

void thread_start( void *arg )
{
    urpManager *manager = ((localThreadArg *)arg)->mgr;
    urpConnection *connection = ((localThreadArg *)arg)->conn;
    PRBool ic = ((localThreadArg *)arg)->isClnt;
    nsresult rv = manager->ReadMessage( connection, ic );
}

void send_thread_start (void * arg) {
    urpManager *manager = ((sendThreadArg *)arg)->man;
    char header = ((sendThreadArg *)arg)->header;
    urpPacket* mes = ((sendThreadArg *)arg)->message;
    bcIID iid = ((sendThreadArg *)arg)->iid;
    bcOID oid = ((sendThreadArg *)arg)->oid;
    bcTID tid = ((sendThreadArg *)arg)->tid;
    PRUint16 methodId = ((sendThreadArg *)arg)->methodId;
    urpConnection* conn = ((sendThreadArg *)arg)->connection;
    nsresult rv = manager->ReadLongRequest(header, mes, iid, oid,
						tid, methodId, conn);
//printf("just run test\n");
}

urpManager::urpManager(PRBool IsClient, bcIORB *orb, urpConnection* conn) {
    broker = orb;
    monitTable = new nsHashtable(20);
    if(IsClient) {
	threadTable = nsnull;
	localThreadArg *arg = new localThreadArg( this, conn, PR_TRUE );
	PRThread *thr = PR_CreateThread( PR_USER_THREAD,
                                                  thread_start,
                                                  arg,
                                                  PR_PRIORITY_NORMAL,
                                                  PR_LOCAL_THREAD,
                                                  PR_UNJOINABLE_THREAD,
                                                  0);
	if(thr == nsnull) {
	   printf("Error couldn't run listener\n");
	   exit(-1);
	}
    } else
	threadTable = new nsHashtable(20);
}


urpManager::~urpManager() {
   if(monitTable)
	delete monitTable;
}



void urpManager::SendUrpRequest(bcOID oid, bcIID iid,
                                PRUint16 methodIndex,
                          nsIInterfaceInfo* interfaceInfo,
			  bcICall *call,
			  PRUint32 paramCount, const nsXPTMethodInfo* info,
			  urpConnection* connection) {
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
	bcTID thrID;
	bcTID* thr;
	if(threadTable) {
	   thrID = (bcTID)PR_GetCurrentThread();
           threadHashKey thrHK(thrID);
	   thr = (bcTID*)threadTable->Get(&thrHK);
	   if(thr)
		thrID = *(bcTID*)thr;
	   else {
		printf("Error with threads in SendUrpRequest\n");
		exit(-1);
	   }
	} else
	   thrID = (bcTID)PR_GetCurrentThread();
printf("OID is written %ld %ld\n", oid, thrID);
	mt->WriteThreadID(thrID, message);
	broker = call->GetORB();
	mt->WriteParams(call, paramCount, info, interfaceInfo, message, methodIndex);
	delete mt;
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
		    nsIInterfaceInfo *interfaceInfo, PRUint16 methodIndex,
		    urpConnection* conn) {
	nsresult rv = NS_OK;
        printf("this is method readReply\n");
	urpMarshalToolkit* mt = new urpMarshalToolkit(PR_TRUE); 
	rv = mt->ReadParams(paramCount, info, message, interfaceInfo, methodIndex, call, broker, this, conn);
	delete mt;
	return rv;
}

nsresult
urpManager::ReadMessage(urpConnection* conn, PRBool isClient) {
	nsresult rv = NS_OK;
	PRBool inserted = PR_FALSE;
	bcTID tid;
	PRThread *thr;
	sendThreadArg *arg = (sendThreadArg *)PR_Malloc(sizeof(sendThreadArg));
	while(conn->GetStatus() == urpSuccess) {
	   urpPacket* message = conn->Read();
	   char header = message->ReadByte();
	   if((header & BIG_HEADER) != 0) { // full header?
               if((header & REQUEST) != 0) { // a request ?
		   bcIID iid;
        	   bcOID oid;
        	   PRUint16 methodId;
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

		   delete mt;

		   threadHashKey thrHK(tid);
                   monitCall* mc = (monitCall*)monitTable->Get(&thrHK);
		   if(mc != nsnull) {
		      mc->mess = message;
		      mc->header = header;
		      mc->iid = iid;
		      mc->oid = oid;
		      mc->tid = tid;
		      mc->mid = methodId;
		      mc->request = 1;
		      PR_EnterMonitor(mc->mon);
                      PR_Notify(mc->mon);
                      PR_ExitMonitor(mc->mon);
		   } else {
		      arg->man = this;
		      arg->header = header;
		      arg->message = message;
		      arg->iid = iid;
		      arg->oid = oid;
		      arg->tid = tid;
		      arg->methodId = methodId;
		      arg->connection = conn;
/*
		      sendThreadArg *arg = new sendThreadArg( this, header, 
					message, iid, oid, tid, methodId, conn);
*/
        	      thr = PR_CreateThread( PR_USER_THREAD,
                                                  send_thread_start,
                                                  arg,
                                                  PR_PRIORITY_NORMAL,
                                                  PR_LOCAL_THREAD,
                                                  PR_UNJOINABLE_THREAD,
                                                  0);
//ReadLongRequest(header, message, iid, oid, tid, methodId, conn);
		   }
	       } else { // a reply
		   bcIID iid; bcOID oid; bcMID mid;

		   urpMarshalToolkit* mt = new urpMarshalToolkit(PR_TRUE);
		   if((header & NEWTID) != 0) { // new thread id ?
		      printf("new threadID\n");
		      tid = mt->ReadThreadID(message);
		   }
		   else
		      printf("old threadID\n");
		   delete mt;
		   threadHashKey thrHK(tid);
		   monitCall* mc = (monitCall*)monitTable->Get(&thrHK);
		   mc->mess = message;
		   mc->header = header;
		   mc->request = 0;
/*
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
                   ReadReply(message, header, mc->call, paramCount, 
			info, interfaceInfo, mid, conn);
*/
		   PR_EnterMonitor(mc->mon);
		   PR_Notify(mc->mon);
		   PR_ExitMonitor(mc->mon);
		}
           }
           else { // only a short request header
//               rv = ReadShortRequest(header, message);
	       break;
	   }
	}
	PR_Free(arg);
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
		   nsIInterfaceInfo *interfaceInfo, PRUint16 methodIndex,
		   urpConnection* connection) {
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
	connection->Write(message);
	delete message;
	return rv;
}
	

nsresult
urpManager::ReadLongRequest(char header, urpPacket* message,
				bcIID iid, bcOID oid, bcTID tid,
				PRUint16 methodId, urpConnection* conn) {
	nsresult rv = NS_OK;

	if(threadTable != nsnull) {
	   bcTID thrID = (bcTID)PR_GetCurrentThread();
           threadHashKey thrHK(thrID);
	   bcTID* clientTID = (bcTID*)threadTable->Get(&thrHK);
	   if(clientTID) thrID = *clientTID;
	   if(clientTID) {
	      if(thrID != tid) {
		 printf("Error: threadIDs are not equal in ReadLongRequest\n");
		 exit(-1);
	      }
	   } else 
	      threadTable->Put(&thrHK, &tid);
	}

	urpMarshalToolkit* mt = new urpMarshalToolkit(PR_FALSE);

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
	mt->ReadParams(paramCount, info, message, interfaceInfo, methodId, call, broker, this, conn);
	delete mt;
	delete message;
        //nb return value; excepion handling
        broker->SendReceive(call);
	rv = SendReply(tid, call, paramCount, info, interfaceInfo, 
			methodId, conn);
//	delete call;
	NS_RELEASE(interfaceInfo);
	return rv;
}

nsresult 
urpManager::SetCall(bcICall* call, PRMonitor *m, bcTID thrID) {
	monitCall* mc = new monitCall(m, call, nsnull, 0);
printf("method SetCall %p %p %p %ld\n",call, m, this, thrID);
        threadHashKey thrHK(thrID);
        monitTable->Put(&thrHK, mc);
	return NS_OK;
}

nsresult
urpManager::RemoveCall(forReply* fR, bcTID thrID) {
printf("method RemoveCall\n");
        threadHashKey thrHK(thrID);
        monitCall* mc = (monitCall*)monitTable->Get(&thrHK);
	fR->mess= mc->mess;
	fR->header = mc->header;
	if(!mc->request) {
	   mc = (monitCall*)monitTable->Remove(&thrHK);
	   delete mc;
	} else {
	   fR->iid = mc->iid;
	   fR->oid = mc->oid;
	   fR->tid = mc->tid;
	   fR->methodId = mc->mid;
	   fR->request = mc->request;
	}
        return NS_OK;
}

bcTID
urpManager::GetThread() {
  bcTID thrID;
  bcTID* thr;
  if(threadTable) {
     thrID = (bcTID)PR_GetCurrentThread();
     threadHashKey thrHK(thrID);
     thr = (bcTID*)threadTable->Get(&thrHK);
     if(thr)
        thrID = *(bcTID*)thr;
     else {
        printf("Error with threads in SendUrpRequest\n");
        exit(-1);
     }
  } else
     thrID = (bcTID)PR_GetCurrentThread();
  return thrID;
}
