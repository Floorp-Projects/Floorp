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
#include "xptcall.h"
#include "nsCRT.h"
#include "urpStub.h"

#include "urpManager.h"

urpStub::urpStub() {
  transport = new urpConnector();
  PRStatus status = transport->Open("socket,host=shiva,port=2009");
  if(status != NS_OK) {
     fprintf(stderr,"Error in opening of remote server on client side\n");
     exit(-1);
  }
  manager = new urpManager(transport);
}


urpStub::~urpStub() {
  if(transport) {
     transport->Close();
     delete transport;
  }
  if(manager) 
     delete manager;
}

void urpStub::Dispatch(bcICall *call) {
    
  printf("this is method Dispatch of urpStub\n");
  bcIID iid; bcOID oid; bcMID mid;
  call->GetParams(&iid, &oid, &mid);
  nsIInterfaceInfo *interfaceInfo;
  nsIInterfaceInfoManager* iimgr;
  if( (iimgr = XPTI_GetInterfaceInfoManager()) ) {
        if (NS_FAILED(iimgr->GetInfoForIID(&iid, &interfaceInfo))) {
            return;  //nb exception handling
        }
        NS_RELEASE(iimgr);
  } else {
        return;
  }
  char* name;
  interfaceInfo->GetName(&name);
  printf("real interface name is %s\n",name);

  nsXPTMethodInfo* info;
  interfaceInfo->GetMethodInfo(mid, (const nsXPTMethodInfo **)&info);
  PRUint32 paramCount = info->GetParamCount();
printf("ThreadID is written %d\n",paramCount);
  nsXPTCVariant* params;
  bcXPCOMMarshalToolkit* mt = NULL;
  if (paramCount > 0) {
      params = (nsXPTCVariant*)malloc(sizeof(nsXPTCVariant) * paramCount);
      mt = new bcXPCOMMarshalToolkit(mid, interfaceInfo, params, call->GetORB());
      bcIUnMarshaler * um = call->GetUnMarshaler();
      mt->UnMarshal(um);
      if (params == nsnull) {
          return;
      }
  }
  
  manager->SendUrpRequest(oid, iid, mid, interfaceInfo, call, params, paramCount,
		 info);
  manager->ReadReply(params, paramCount, info, interfaceInfo, mid);
  if (mt != NULL) { //nb to do what about nsresult ?
      bcIMarshaler * m = call->GetMarshaler();
      mt->Marshal(m);
  } 
}


