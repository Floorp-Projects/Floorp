/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
*/

#include "nspr.h"
#include "nsString.h"
#include "cmtcmn.h"

#include "nsIPSMComponent.h"
#include "nsIPSMSocketInfo.h"
#include "nsIServiceManager.h"
#include "nsPSMShimLayer.h"
#include "nsSSLIOLayer.h"

static PRDescIdentity  nsSSLIOLayerIdentity;
static PRIOMethods     nsSSLIOLayerMethods;
static nsIPSMComponent* gPSMService = nsnull;



class nsPSMSocketInfo : public nsIPSMSocketInfo
{
public:
    nsPSMSocketInfo();
    ~nsPSMSocketInfo();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIPSMSOCKETINFO

    // internal functions to psm-glue.
    nsresult SetSocketPtr(CMSocket *socketPtr);
    nsresult SetControlPtr(CMT_CONTROL *aControlPtr);
    nsresult SetFileDescPtr(PRFileDesc *aControlPtr);
    nsresult SetHostName(char *aHostName);

protected:
    CMT_CONTROL* mControl;
    CMSocket*    mSocket;
    PRFileDesc*  mFd;
    nsString     mHostName;
};


static PRStatus PR_CALLBACK
nsSSLIOLayerConnect(PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime timeout)
{
    nsresult                result;
    PRStatus                rv = PR_SUCCESS;
    CMTStatus               status = CMTFailure;
    char*                   hostName;

    /* Set the error in case of failure. */

    PR_SetError(PR_UNKNOWN_ERROR, status);

    if (!fd || !addr || !fd->secret)
        return PR_FAILURE;
    
    if (gPSMService == nsnull)
    {
        result = nsServiceManager::GetService( PSM_COMPONENT_PROGID,
                                               NS_GET_IID(nsIPSMComponent), 
                                               (nsISupports**)&gPSMService);  //FIX leak one per app run
        if (NS_FAILED(result))
            return PR_FAILURE;
    }
    
    
    // Make the socket non-blocking...
    PRSocketOptionData opt;
    opt.option = PR_SockOpt_Nonblocking;
    opt.value.non_blocking = PR_FALSE;
    rv = PR_SetSocketOption(fd->lower, &opt);
    if (PR_SUCCESS != rv) 
        return PR_FAILURE;

    char ipBuffer[PR_NETDB_BUF_SIZE];
    rv = PR_NetAddrToString(addr, (char*)&ipBuffer, PR_NETDB_BUF_SIZE);
    if (rv != PR_SUCCESS)
        return PR_FAILURE;
    
    if (addr->raw.family == PR_AF_INET6 && PR_IsNetAddrType(addr, PR_IpAddrV4Mapped)) 
    {
      /* Chop off the leading "::ffff:" */
      strcpy(ipBuffer, ipBuffer + 7);
    }


    CMSocket* cmsock = (CMSocket *)PR_Malloc(sizeof(CMSocket));
    if (!cmsock)
        return PR_FAILURE;
    
    memset(cmsock, 0, sizeof(CMSocket));
    
    CMT_CONTROL *control;
    result = gPSMService->GetControlConnection(&control);

    if (result != PR_SUCCESS)
    {
        PR_FREEIF(cmsock)
        return PR_FAILURE;
    }
    
    cmsock->fd        = fd->lower;
    cmsock->isUnix    = PR_FALSE;
   
    nsPSMSocketInfo *infoObject = (nsPSMSocketInfo *)fd->secret;
    
    infoObject->GetHostName(&hostName);
    infoObject->SetControlPtr(control);
    infoObject->SetSocketPtr(cmsock);
    
    status = CMT_OpenSSLConnection(control,
                                   cmsock,
                                   SSM_REQUEST_SSL_DATA_SSL,
                                   PR_ntohs(addr->inet.port),
                                   ipBuffer,
                                   (hostName ? hostName : ipBuffer),
                                   CM_FALSE, 
                                   nsnull);
    if (CMTSuccess == status)
    {
        // since our stuff can block, what we want to do is return PR_FAILURE,
        // but set the nspr ERROR to BLOCK.  This will put us into a select
        // q.
        PR_SetError(PR_WOULD_BLOCK_ERROR, status);
        return PR_FAILURE;
    }

    return PR_FAILURE;
}

  /* CMT_DestroyDataConnection(ctrl, sock); */
  /*   need to strip our layer, pass result to DestroyDataConnection */
  /*   which will clean up the CMT accounting of sock, then call our */
  /*   shim layer to translate back to NSPR */

static PRStatus PR_CALLBACK
nsSSLIOLayerClose(PRFileDesc *fd)
{
  nsPSMSocketInfo *infoObject    = (nsPSMSocketInfo *)fd->secret;
  PRDescIdentity          id     = PR_GetLayersIdentity(fd);
   
  if (infoObject && id == nsSSLIOLayerIdentity)
  {
    CMInt32 errorCode = PR_FAILURE;
    CMT_CONTROL* control;
    CMSocket*    socket;
    
    infoObject->GetControlPtr(&control);
    infoObject->GetSocketPtr(&socket);

    if (((PRStatus) CMT_GetSSLDataErrorCode(control, socket, &errorCode)) == PR_SUCCESS)
    {
        CMT_DestroyDataConnection(control, socket);
        NS_RELEASE(infoObject);
        fd->identity = PR_INVALID_IO_LAYER;
    }
    return (PRStatus)errorCode;
  }

  return PR_FAILURE;
}
 
static PRInt32 PR_CALLBACK
nsSSLIOLayerRead( PRFileDesc *fd, void *buf, PRInt32 amount)
{
    if (!fd)
        return PR_FAILURE;

    PRInt32 result = PR_Recv(fd, buf, amount, 0, PR_INTERVAL_MIN);
    
    if (result > 0)
        return result;
    
    if (result == -1)
    {
        PRErrorCode code = PR_GetError();

        if (code == PR_IO_TIMEOUT_ERROR )
            PR_SetError(PR_WOULD_BLOCK_ERROR, PR_WOULD_BLOCK_ERROR);
        return PR_FAILURE;
    }
    
    if (result == 0)
    {
        nsPSMSocketInfo *infoObject    = (nsPSMSocketInfo *)fd->secret;
        PRDescIdentity          id     = PR_GetLayersIdentity(fd);

        if (infoObject && id == nsSSLIOLayerIdentity)
        {
            CMInt32 errorCode = PR_FAILURE;
            
            CMT_CONTROL* control;
            CMSocket*    socket;
    
            infoObject->GetControlPtr(&control);
            infoObject->GetSocketPtr(&socket);

            CMT_GetSSLDataErrorCode(control, socket, &errorCode);

			if (errorCode == PR_IO_TIMEOUT_ERROR)
			{
				PR_SetError(PR_WOULD_BLOCK_ERROR, PR_WOULD_BLOCK_ERROR);
				return PR_FAILURE;
			}

            PR_SetError(0, 0);
            return errorCode;
        }
    }

    return result;
}

static PRInt32 PR_CALLBACK
nsSSLIOLayerWrite( PRFileDesc *fd, const void *buf, PRInt32 amount)
{
    if (!fd)
        return PR_FAILURE;

    PRInt32 result = PR_Send(fd, buf, amount, 0, PR_INTERVAL_MIN);
    
    if (result > 0)
        return result;

    if (result == -1)
    {
        PRErrorCode code = PR_GetError();

        if (code == PR_IO_TIMEOUT_ERROR )
            PR_SetError(PR_WOULD_BLOCK_ERROR, PR_WOULD_BLOCK_ERROR);
        return PR_FAILURE;
    }

    if (result == 0)
    {
        nsPSMSocketInfo *infoObject    = (nsPSMSocketInfo *)fd->secret;
        PRDescIdentity          id     = PR_GetLayersIdentity(fd);

        if (infoObject && id == nsSSLIOLayerIdentity)
        {
            CMInt32 errorCode = PR_FAILURE;
            CMT_CONTROL* control;
            CMSocket*    socket;
    
            infoObject->GetControlPtr(&control);
            infoObject->GetSocketPtr(&socket);

            CMT_GetSSLDataErrorCode(control, socket, &errorCode);
            PR_SetError(0, 0);
            return errorCode;
        }
    }
    return result;
}



nsPSMSocketInfo::nsPSMSocketInfo()
{ 
    NS_INIT_REFCNT(); 
    mControl = nsnull; 
    mSocket = nsnull;
}

nsPSMSocketInfo::~nsPSMSocketInfo()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsPSMSocketInfo, nsIPSMSocketInfo);

NS_IMETHODIMP
nsPSMSocketInfo::GetControlPtr(CMT_CONTROL * *aControlPtr)
{
    *aControlPtr = mControl;
    return NS_OK;
}

nsresult
nsPSMSocketInfo::SetControlPtr(CMT_CONTROL *aControlPtr)
{
    mControl = aControlPtr;
    return NS_OK;
}

NS_IMETHODIMP
nsPSMSocketInfo::GetSocketPtr(CMSocket * *socketPtr)
{
    *socketPtr = mSocket;
    return NS_OK;
}

nsresult
nsPSMSocketInfo::SetSocketPtr(CMSocket *socketPtr)
{
    mSocket = socketPtr;
    return NS_OK;
}

NS_IMETHODIMP
nsPSMSocketInfo::GetFileDescPtr(PRFileDesc * *aFilePtr)
{
    *aFilePtr = mFd;
    return NS_OK;
}


nsresult
nsPSMSocketInfo::SetFileDescPtr(PRFileDesc *aFilePtr)
{
    mFd = aFilePtr;
    return NS_OK;
}

NS_IMETHODIMP 
nsPSMSocketInfo::GetHostName(char * *aHostName)
{
    *aHostName = mHostName.ToNewCString();
    return NS_OK;
}


nsresult 
nsPSMSocketInfo::SetHostName(char *aHostName)
{
    mHostName.Assign(aHostName);
    return NS_OK;
}

NS_IMETHODIMP
nsPSMSocketInfo::GetPickledStatus(char * *pickledStatusString)
{
    *pickledStatusString = nsnull;

    if (mSocket && mControl)
    {
        long level;
        CMTItem pickledStatus = {0, nsnull, 0};
        unsigned char* ret    = nsnull;

        if (CMT_GetSSLSocketStatus(mControl, mSocket, &pickledStatus, &level) == PR_FAILURE)
            return nsnull;
        
        ret = (unsigned char*) PR_Malloc( (SSMSTRING_PADDED_LENGTH(pickledStatus.len) + sizeof(int)) );
        if (!ret) 
            return NS_ERROR_OUT_OF_MEMORY;

        *(int*)ret = pickledStatus.len;
        memcpy(ret+sizeof(int), pickledStatus.data, *(int*)ret);
    
        PR_FREEIF(pickledStatus.data);
    
        *pickledStatusString = (char*) ret;
    }
    return NS_OK;
}

nsresult
nsSSLIOLayerNewSocket(const char* hostName, PRFileDesc **fd, nsISupports** info)
{
    static PRBool  firstTime = PR_TRUE;
    if (firstTime)
    {
        nsSSLIOLayerIdentity        = PR_GetUniqueIdentity("Cartman layer");
        nsSSLIOLayerMethods         = *PR_GetDefaultIOMethods();

        nsSSLIOLayerMethods.connect = nsSSLIOLayerConnect;
        nsSSLIOLayerMethods.close   = nsSSLIOLayerClose;
        nsSSLIOLayerMethods.read    = nsSSLIOLayerRead;
        nsSSLIOLayerMethods.write   = nsSSLIOLayerWrite;
        firstTime                   = PR_FALSE;
    }
    

    PRFileDesc *   sock;
    PRFileDesc *   layer;
    PRStatus       rv;

    /* Get a normal NSPR socket */
    sock = PR_NewTCPSocket();  
    if (! sock) return NS_ERROR_OUT_OF_MEMORY;

    layer = PR_CreateIOLayerStub(nsSSLIOLayerIdentity, &nsSSLIOLayerMethods);
    if (! layer)
    {
        PR_Close(sock);
        return NS_ERROR_FAILURE;
    }
        
    nsPSMSocketInfo *infoObject = new nsPSMSocketInfo();
    if (!infoObject)
    {
        PR_Close(sock);
        // clean up IOLayerStub.
        return NS_ERROR_FAILURE;
    }
    
    NS_ADDREF(infoObject);
    infoObject->SetHostName((char*)hostName);
    layer->secret = (PRFilePrivate*) infoObject;
    rv = PR_PushIOLayer(sock, PR_GetLayersIdentity(sock), layer);
    
    if (rv == PR_SUCCESS)
    {
        *fd   = sock;
        *info = infoObject;
        NS_ADDREF(*info);
        return NS_OK;
    }
    
    PR_Close(sock);
    return NS_ERROR_FAILURE;
}
