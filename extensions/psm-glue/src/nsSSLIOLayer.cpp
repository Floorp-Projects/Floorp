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

#include "cmtcmn.h"

#include "nsIPSMComponent.h"
#include "nsIServiceManager.h"
#include "nsPSMShimLayer.h"
#include "nsSSLIOLayer.h"

/* TODO: check for failures */
static PRDescIdentity  nsSSLIOLayerIdentity;
static PRIOMethods     nsSSLIOLayerMethods;

typedef struct nsSSLIOLayerSecretData
{
  PCMT_CONTROL  control;
  CMSocket     *cmsock;
} nsSSLIOLayerSecretData;

static nsIPSMComponent* psm = nsnull;

static PRStatus PR_CALLBACK
nsSSLIOLayerConnect(PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime timeout)
{
    nsSSLIOLayerSecretData *secret;
    nsresult                result;
    PRStatus                rv = PR_SUCCESS;
    CMTStatus               status;

    const char*             hostName;

    /* Set the error in case of failure. */

    PR_SetError(PR_UNKNOWN_ERROR, status);

    if (!fd || !addr)
        return PR_FAILURE;

    secret = (nsSSLIOLayerSecretData *)PR_Malloc(sizeof(nsSSLIOLayerSecretData));
    if (!secret) return PR_FAILURE;

    memset(secret, 0, sizeof(nsSSLIOLayerSecretData));
    
    /* TODO: this should be allocated from cmshim layer */
    secret->cmsock = (CMSocket *)PR_Malloc(sizeof(CMSocket));
    if (!secret->cmsock)
    {
        PR_Free(secret);
        return PR_FAILURE;
    }   
    memset(secret->cmsock, 0, sizeof(CMSocket));

    if (psm == nsnull)
    {
        result = nsServiceManager::GetService( PSM_COMPONENT_PROGID,
                                               NS_GET_IID(nsIPSMComponent), 
                                               (nsISupports**)&psm);
        if (NS_FAILED(result))
        {
            rv=PR_FAILURE;
            goto fail;
        }
    }
   
    result = psm->GetControlConnection(&secret->control);

    if (result != PR_SUCCESS)
    {
        rv = PR_FAILURE;
        goto fail;
    }
    
    secret->cmsock->fd        = fd->lower;
    secret->cmsock->isUnix    = PR_FALSE;
    
    /* TODO: XXX fix this RSN */
    {
        PRSocketOptionData opt;

        // Make the socket non-blocking...
        opt.option = PR_SockOpt_Nonblocking;
        opt.value.non_blocking = PR_FALSE;
        rv = PR_SetSocketOption(fd->lower, &opt);
        if (PR_SUCCESS != rv) 
        {
            goto fail;
        }
    }

    char ipBuffer[PR_NETDB_BUF_SIZE];
    rv = PR_NetAddrToString(addr, (char*)&ipBuffer, PR_NETDB_BUF_SIZE);

    if (rv != PR_SUCCESS)
    {
        goto fail;
    }

    if (addr->raw.family == PR_AF_INET6 && PR_IsNetAddrType(addr, PR_IpAddrV4Mapped)) {
      /* Chop off the leading "::ffff:" */
      strcpy(ipBuffer, ipBuffer + 7);
    }

    if (fd->secret)  // how do we know that this is a necko nsSocketTransportFDPrivate??
    {
        hostName = (const char*)fd->secret;
    }
    else
    {
        // no hostname, use ip address.
        hostName = ipBuffer;
    }
    

    fd->secret = (PRFilePrivate *)secret;

    status = CMT_OpenSSLConnection(secret->control,
                                   secret->cmsock,
                                   SSM_REQUEST_SSL_DATA_SSL,
                                   PR_ntohs(addr->inet.port),
                                   ipBuffer,
                                   (char*)hostName,
                                   CM_TRUE, 
                                   nsnull);
    if (CMTSuccess == status)
    {
        // since our stuff can block, what we want to do is return PR_FAILURE,
        // but set the nspr ERROR to BLOCK.  This will put us into a select
        // q.
        PR_SetError(PR_WOULD_BLOCK_ERROR, status);
        return PR_FAILURE;
    }

fail:
    fd->secret = nsnull;
    PR_FREEIF(secret->cmsock)
    
    secret->cmsock = nsnull;
    PR_FREEIF(secret);
    
    secret = nsnull;
    return rv;
}

  /* CMT_DestroyDataConnection(ctrl, sock); */
  /*   need to strip our layer, pass result to DestroyDataConnection */
  /*   which will clean up the CMT accounting of sock, then call our */
  /*   shim layer to translate back to NSPR */

static PRStatus PR_CALLBACK
nsSSLIOLayerClose(PRFileDesc *fd)
{
  nsSSLIOLayerSecretData *secret = (nsSSLIOLayerSecretData *)fd->secret;
  PRDescIdentity          id     = PR_GetLayersIdentity(fd);

  if (secret && id == nsSSLIOLayerIdentity)
  {
    CMInt32 errorCode = PR_FAILURE;

    if (((PRStatus) CMT_GetSSLDataErrorCode(secret->control, secret->cmsock, &errorCode)) == PR_SUCCESS)
    {
        CMT_DestroyDataConnection(secret->control, secret->cmsock);

        PR_Free(secret);

        fd->secret = NULL;
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
        nsSSLIOLayerSecretData *secret = (nsSSLIOLayerSecretData *)fd->secret;
        PRDescIdentity          id     = PR_GetLayersIdentity(fd);

        if (secret && id == nsSSLIOLayerIdentity)
        {
            CMInt32 errorCode = PR_FAILURE;

            CMT_GetSSLDataErrorCode(secret->control, secret->cmsock, &errorCode);

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
        nsSSLIOLayerSecretData *secret = (nsSSLIOLayerSecretData *)fd->secret;
        PRDescIdentity          id     = PR_GetLayersIdentity(fd);

        if (secret && id == nsSSLIOLayerIdentity)
        {
            CMInt32 errorCode = PR_FAILURE;

            CMT_GetSSLDataErrorCode(secret->control, secret->cmsock, &errorCode);
            PR_SetError(0, 0);
            return errorCode;
        }
    }
    

    return result;
}

nsresult
nsSSLIOLayerNewSocket(const char* hostName, PRFileDesc **fd, nsISupports **securityInfo)
{
    static PRBool  firstTime = PR_TRUE;

    *fd = nsnull;
    *securityInfo = nsnull;

    PRFileDesc *   layer;
    PRFileDesc *   sock;
    PRStatus       rv;

    /* Get a normal NSPR socket */
    sock = PR_NewTCPSocket();  PR_ASSERT(NULL != sock);

    if (! sock) return NS_ERROR_FAILURE;


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
  
    layer = PR_CreateIOLayerStub(nsSSLIOLayerIdentity, &nsSSLIOLayerMethods);

    if (layer)
    {
        layer->secret = (PRFilePrivate*)hostName;
        rv = PR_PushIOLayer(sock, PR_GetLayersIdentity(sock), layer);
    }

    if(PR_SUCCESS != rv)
        return NS_ERROR_FAILURE;
    *fd = sock;

    return NS_OK;
}
