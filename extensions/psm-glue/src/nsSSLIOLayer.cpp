/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
static PRBool  firstTime = PR_TRUE;



class nsPSMSocketInfo : public nsIPSMSocketInfo
{
public:
    nsPSMSocketInfo();
    virtual ~nsPSMSocketInfo();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPSMSOCKETINFO
    
    // internal functions to psm-glue.
    nsresult SetSocketPtr(CMSocket *socketPtr);
    nsresult SetControlPtr(CMT_CONTROL *aControlPtr);
    nsresult SetFileDescPtr(PRFileDesc *aControlPtr);
    nsresult SetHostName(const char *aHostName);
    nsresult SetProxyName(const char *aName);
    
    nsresult SetHostPort(PRInt32 aPort);
    nsresult SetProxyPort(PRInt32 aPort);
    nsresult SetPickledStatus();

    nsresult SetUseTLS(PRBool useTLS);
    nsresult GetUseTLS(PRBool *useTLS);
    
protected:
    CMT_CONTROL* mControl;
    CMSocket*    mSocket;
    PRFileDesc*  mFd;
    
    nsString     mHostName;
    PRInt32      mHostPort;
    
    nsString     mProxyName;
    PRInt32      mProxyPort;

    PRBool       mForceHandshake;
    PRBool       mUseTLS;
    
    unsigned char* mPickledStatus;
};


static PRStatus PR_CALLBACK
nsSSLIOLayerConnect(PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime timeout)
{
    nsresult                result;
    PRStatus                rv = PR_SUCCESS;
    CMTStatus               status = CMTFailure;
    
    /* Set the error in case of failure. */
    
    PR_SetError(PR_UNKNOWN_ERROR, status);
    
    if (!fd || !addr || !fd->secret || !gPSMService)
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

    
    CMT_CONTROL *control;
    result = gPSMService->GetControlConnection(&control);
    if (result != PR_SUCCESS)
        return PR_FAILURE;
    
    CMSocket* cmsock = (CMSocket *)PR_Malloc(sizeof(CMSocket));
    if (!cmsock)
        return PR_FAILURE;
    
    memset(cmsock, 0, sizeof(CMSocket));
    
    cmsock->fd        = fd->lower;
    cmsock->isUnix    = PR_FALSE;
    
    nsPSMSocketInfo *infoObject = (nsPSMSocketInfo *)fd->secret;
    
    infoObject->SetControlPtr(control);
    infoObject->SetSocketPtr(cmsock);
    
    char* proxyName;
    char* hostName;
    PRInt32 proxyPort;
    PRInt32 hostPort;
    PRBool forceHandshake;
    PRBool useTLS;
    infoObject->GetProxyName(&proxyName);
    infoObject->GetHostName(&hostName);
    infoObject->GetProxyPort(&proxyPort);
    infoObject->GetHostPort(&hostPort);
    infoObject->GetForceHandshake(&forceHandshake);
    infoObject->GetUseTLS(&useTLS);

    if (proxyName)
    {
        PRInt32 destPort;
        
        infoObject->GetProxyPort(&destPort);
        
        status = CMT_OpenSSLProxyConnection(control,
                                            cmsock,
                                            destPort,
                                            // we assume that we were called 
                                            // with the addr of the proxy host
                                            ipBuffer,
                                            proxyName);
    }
    else if (useTLS)
    {
        status = CMT_OpenTLSConnection(control,
                                       cmsock,
                                       PR_ntohs(addr->inet.port),
                                       ipBuffer,
                                       (hostName ? hostName : ipBuffer));
    }
    else
    {
        CMBool handshake = forceHandshake ? CM_TRUE : CM_FALSE;
        // Direct connection
        status = CMT_OpenSSLConnection(control,
                                       cmsock,
                                       SSM_REQUEST_SSL_DATA_SSL,
                                       PR_ntohs(addr->inet.port),
                                       ipBuffer,
                                       (hostName ? hostName : ipBuffer),
                                       handshake,
                                       nsnull);
    }
    
    if (hostName)  Recycle(hostName);
    if (proxyName) Recycle(proxyName);
    
    if (CMTSuccess == status)
    {
        PRSocketOptionData sockopt;
        sockopt.option = PR_SockOpt_Nonblocking;
        rv = PR_GetSocketOption(fd, &sockopt);
        
        if (PR_SUCCESS == rv && !sockopt.value.non_blocking) {
            // this is a nonblocking socket, so we can return success
            return PR_SUCCESS;
        }
        
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
        CMSocket*    sock;
        
        PR_Shutdown(fd, PR_SHUTDOWN_BOTH);
        
        infoObject->GetControlPtr(&control);
        infoObject->GetSocketPtr(&sock);
        infoObject->SetPickledStatus();
        
        CMT_GetSSLDataErrorCode(control, sock, &errorCode);
        CMT_DestroyDataConnection(control, sock);
        NS_RELEASE(infoObject);  // if someone is interested in us, the better have an addref.
        fd->identity = PR_INVALID_IO_LAYER;
        
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
            CMSocket*    sock;
            
            infoObject->GetControlPtr(&control);
            infoObject->GetSocketPtr(&sock);
            
            CMT_GetSSLDataErrorCode(control, sock, &errorCode);
            
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
            CMSocket*    sock;
            
            infoObject->GetControlPtr(&control);
            infoObject->GetSocketPtr(&sock);
            
            CMT_GetSSLDataErrorCode(control, sock, &errorCode);
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
    mPickledStatus = nsnull;
    mForceHandshake = PR_FALSE;
    mUseTLS = PR_FALSE;
}

nsPSMSocketInfo::~nsPSMSocketInfo()
{
    PR_FREEIF(mPickledStatus);    
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsPSMSocketInfo, nsIPSMSocketInfo);

// if the connection was via a proxy, we need to have the
// ssl layer "step up" to take an active role in the connection
NS_IMETHODIMP
nsPSMSocketInfo::ProxyStepUp()
{
    nsCAutoString hostName;
    hostName.AssignWithConversion(mHostName);
    
    return CMT_ProxyStepUp(mControl, mSocket, nsnull, hostName);
}

NS_IMETHODIMP
nsPSMSocketInfo::TLSStepUp()
{
    return CMT_TLSStepUp(mControl, mSocket, nsnull);
}

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
    if (mHostName.IsEmpty())
        *aHostName = nsnull;
    else
        *aHostName = mHostName.ToNewCString();
    return NS_OK;
}


nsresult 
nsPSMSocketInfo::SetHostName(const char *aHostName)
{
    mHostName.AssignWithConversion(aHostName);
    return NS_OK;
}

NS_IMETHODIMP 
nsPSMSocketInfo::GetHostPort(PRInt32 *aPort)
{
    *aPort = mHostPort;
    return NS_OK;
}

nsresult 
nsPSMSocketInfo::SetHostPort(PRInt32 aPort)
{
    mHostPort = aPort;
    return NS_OK;
}

NS_IMETHODIMP 
nsPSMSocketInfo::GetProxyName(char * *aName)
{
    if (mProxyName.IsEmpty())
        *aName = nsnull;
    else
        *aName = mProxyName.ToNewCString();
    return NS_OK;
}


nsresult 
nsPSMSocketInfo::SetProxyName(const char *aName)
{
    mProxyName.AssignWithConversion(aName);
    return NS_OK;
}


NS_IMETHODIMP 
nsPSMSocketInfo::GetProxyPort(PRInt32 *aPort)
{
    *aPort = mProxyPort;
    return NS_OK;
}

nsresult 
nsPSMSocketInfo::SetProxyPort(PRInt32 aPort)
{
    mProxyPort = aPort;
    return NS_OK;
}

NS_IMETHODIMP
nsPSMSocketInfo::GetForceHandshake(PRBool *forceHandshake)
{
    *forceHandshake = mForceHandshake;
    return NS_OK;
}

NS_IMETHODIMP
nsPSMSocketInfo::SetForceHandshake(PRBool forceHandshake)
{
    mForceHandshake = forceHandshake;
    return NS_OK;
}

nsresult
nsPSMSocketInfo::GetUseTLS(PRBool *aResult)
{
    *aResult = mUseTLS;
    return NS_OK;
}

nsresult 
nsPSMSocketInfo::SetUseTLS(PRBool useTLS)
{
    mUseTLS = useTLS;
    return NS_OK;
}


nsresult
nsPSMSocketInfo::SetPickledStatus()
{
    PR_FREEIF(mPickledStatus);
    
    long level;
    CMTItem pickledStatus = {0, nsnull, 0};
    unsigned char* ret    = nsnull;
    
    if (NS_SUCCEEDED(CMT_GetSSLSocketStatus(mControl, mSocket, &pickledStatus, &level)))
    {        
        ret = (unsigned char*) PR_Malloc( (SSMSTRING_PADDED_LENGTH(pickledStatus.len) + sizeof(int)) );
        if (ret) 
        {
            *(int*)ret = pickledStatus.len;
            memcpy(ret+sizeof(int), pickledStatus.data, *(int*)ret);
        }
        
        PR_FREEIF(pickledStatus.data);
        mPickledStatus = ret;
    }
    return NS_OK;
}


NS_IMETHODIMP
nsPSMSocketInfo::GetPickledStatus(char * *pickledStatusString)
{
    if (!mPickledStatus)
        SetPickledStatus();
    
    if (mPickledStatus)
    {
        PRInt32 len = *(int*)mPickledStatus;
        char *out = (char *)nsMemory::Alloc(len);
        memcpy(out, mPickledStatus, len);
        *pickledStatusString = out;
        return NS_OK;
    }
    *pickledStatusString = nsnull;
    return NS_ERROR_FAILURE;
}

nsresult
nsSSLIOLayerNewSocket( const char *host, 
                       PRInt32 port, 
                       const char *proxyHost, 
                       PRInt32 proxyPort,
                       PRFileDesc **fd, 
                       nsISupports** info,
                       PRBool useTLS)
{
    if (firstTime)
    {
        nsSSLIOLayerIdentity        = PR_GetUniqueIdentity("Cartman layer");
        nsSSLIOLayerMethods         = *PR_GetDefaultIOMethods();
        
        nsSSLIOLayerMethods.connect = nsSSLIOLayerConnect;
        nsSSLIOLayerMethods.close   = nsSSLIOLayerClose;
        nsSSLIOLayerMethods.read    = nsSSLIOLayerRead;
        nsSSLIOLayerMethods.write   = nsSSLIOLayerWrite;
        
        
        nsresult result = nsServiceManager::GetService( PSM_COMPONENT_CONTRACTID,
                                                        NS_GET_IID(nsIPSMComponent), 
                                                        (nsISupports**)&gPSMService);  
        if (NS_FAILED(result))
            return PR_FAILURE;
        
        firstTime                   = PR_FALSE;
        
    }
    
    
    PRFileDesc *   sock;
    PRFileDesc *   layer;
    PRStatus       rv;
    
    /* Get a normal NSPR socket */
    sock = PR_NewTCPSocket();  
    if (! sock) return NS_ERROR_OUT_OF_MEMORY;
    
    /* disable Nagle algorithm delay for control sockets */
    PRSocketOptionData sockopt;
    sockopt.option = PR_SockOpt_NoDelay;
    sockopt.value.no_delay = PR_TRUE;   
    rv = PR_SetSocketOption(sock, &sockopt);
    PR_ASSERT(PR_SUCCESS == rv);
    
    
    layer = PR_CreateIOLayerStub(nsSSLIOLayerIdentity, &nsSSLIOLayerMethods);
    if (! layer)
    {
        PR_Close(sock);
        return NS_ERROR_FAILURE;
    }
    
    nsPSMSocketInfo * infoObject = new nsPSMSocketInfo();
    if (!infoObject)
    {
        PR_Close(sock);
        // clean up IOLayerStub.
        PR_DELETE(layer);
        return NS_ERROR_FAILURE;
    }

    NS_ADDREF(infoObject);
    infoObject->SetHostName(host);
    infoObject->SetHostPort(port);
    infoObject->SetProxyName(proxyHost);
    infoObject->SetProxyPort(proxyPort);
    infoObject->SetUseTLS(useTLS);
    
    layer->secret = (PRFilePrivate*) infoObject;
    rv = PR_PushIOLayer(sock, PR_GetLayersIdentity(sock), layer);
    
    if (NS_FAILED(rv))
    {
        PR_Close(sock);
        NS_RELEASE(infoObject);
        PR_DELETE(layer);
        return NS_ERROR_FAILURE;
    }
    
    *fd   = sock;
    *info = infoObject;
    NS_ADDREF(*info);
    return NS_OK;
}

nsresult
nsSSLIOLayerAddToSocket( const char *host, 
                         PRInt32 port, 
                         const char *proxyHost, 
                         PRInt32 proxyPort,
                         PRFileDesc *fd, 
                         nsISupports** info,
                         PRBool useTLS)
{
    if (firstTime)
    {
        nsSSLIOLayerIdentity        = PR_GetUniqueIdentity("Cartman layer");
        nsSSLIOLayerMethods         = *PR_GetDefaultIOMethods();
        
        nsSSLIOLayerMethods.connect = nsSSLIOLayerConnect;
        nsSSLIOLayerMethods.close   = nsSSLIOLayerClose;
        nsSSLIOLayerMethods.read    = nsSSLIOLayerRead;
        nsSSLIOLayerMethods.write   = nsSSLIOLayerWrite;
        
        
        nsresult result = nsServiceManager::GetService( PSM_COMPONENT_CONTRACTID,
                                                        NS_GET_IID(nsIPSMComponent), 
                                                        (nsISupports**)&gPSMService);  
        if (NS_FAILED(result))
            return PR_FAILURE;
        
        firstTime                   = PR_FALSE;
        
    }
    
    
    PRFileDesc *   layer;
    PRStatus       rv;
    
    /* disable Nagle algorithm delay for control sockets */
    PRSocketOptionData sockopt;
    sockopt.option = PR_SockOpt_NoDelay;
    sockopt.value.no_delay = PR_TRUE;   
    rv = PR_SetSocketOption(fd, &sockopt);
    PR_ASSERT(PR_SUCCESS == rv);
    
    
    layer = PR_CreateIOLayerStub(nsSSLIOLayerIdentity, &nsSSLIOLayerMethods);
    if (! layer)
    {
        return NS_ERROR_FAILURE;
    }
    
    nsPSMSocketInfo * infoObject = new nsPSMSocketInfo();
    if (!infoObject)
    {
        // clean up IOLayerStub.
        PR_DELETE(layer);
        return NS_ERROR_FAILURE;
    }

    NS_ADDREF(infoObject);
    infoObject->SetHostName(host);
    infoObject->SetHostPort(port);
    infoObject->SetProxyName(proxyHost);
    infoObject->SetProxyPort(proxyPort);
    infoObject->SetUseTLS(useTLS);
    
    layer->secret = (PRFilePrivate*) infoObject;
    rv = PR_PushIOLayer(fd, PR_GetLayersIdentity(fd), layer);
    
    if (NS_FAILED(rv))
    {
        NS_RELEASE(infoObject);
        PR_DELETE(layer);
        return rv;
    }
    
    *info = infoObject;
    NS_ADDREF(*info);
    return NS_OK;
}



