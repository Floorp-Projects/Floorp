/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is Basic Socket Library
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. Copyright (C) 1999
 * New Dimenstions Consulting, Inc. All Rights Reserved.
 *
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 */

#include "prerror.h"
#include "plstr.h"
#include "nscore.h"

#include "nsIAllocator.h"
#include "bsIConnection.h"

#include "bsapi.h"
#include "bspubtd.h"
#include "bsconnection.h"
#include "bsserver.h"
#include "bserror.h"

#define BS_INIT_CHECK if (!mInitializedFlag) \
                           return NS_ERROR_FAILURE

class bsConnection : public bsIConnection
{

public:
    bsConnection();
    virtual ~bsConnection();

    NS_DECL_ISUPPORTS

    NS_IMETHOD Init(const char *hostname);
    NS_IMETHOD Destroy();
    
    NS_IMETHOD GetIsConnected(PRBool *aIsConnected);
    NS_IMETHOD SetIsConnected(PRBool aIsConnected);

    NS_IMETHOD GetLinebufferFlag(PRBool *aLinebufferFlag);
    NS_IMETHOD SetLinebufferFlag(PRBool aLinebufferFlag);

    NS_IMETHOD GetPort(PRUint16 *aPort);
    NS_IMETHOD SetPort(PRUint16 aPort);

    NS_IMETHOD Disconnect(PRBool *rval);

    NS_IMETHOD Connect(PRUint16 port, const char *bindIP, PRBool tcpFlag,
                       PRBool *rval);

    NS_IMETHOD SendData(const char *data);

    NS_IMETHOD HasData(PRBool *_retval);

    NS_IMETHOD ReadData(PRUint32 timeout, char **aData);
    
private:
    BSConnectionClass  *connection;
    BSServerClass      *server;
    PRBool             mInitializedFlag;
    PRBool             mDataFlag;
    PRBool             mData;
    
};

/* static constructor used by factory */
extern nsresult
BS_NewConnection(bsIConnection** aConnection)
{
    NS_PRECONDITION(aConnection != nsnull, "null ptr");
    if (!aConnection)
        return NS_ERROR_NULL_POINTER;

    *aConnection = new bsConnection();
    if (!*aConnection)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aConnection);
    return NS_OK;
    
}

bsConnection::bsConnection()
{
    NS_INIT_REFCNT();
    mInitializedFlag = PR_FALSE;    
}

bsConnection::~bsConnection()
{
    if (mInitializedFlag)
        destroy();
    
}

NS_IMPL_ISUPPORTS(bsConnection, bsIConnection::GetIID());

NS_IMETHODIMP
bsConnection::Init(const char *hostname)
{
    if (mInitializedFlag)
        return NS_ERROR_FAILURE;

    server = bs_server_new (nsnull, hostname);
    if (!server)
        return NS_ERROR_OUT_OF_MEMORY;

    connection = nsnull;
    
    mInitializedFlag = PR_TRUE;
    return NS_OK;
    
}

NS_IMETHODIMP
bsConnection::Destroy()
{
    if (mInitializedFlag)
        bs_server_free (server);

    return NS_OK;
}


NS_IMETHODIMP
bsConnection::GetIsConnected(PRBool *aIsConnected)
{
    BS_INIT_CHECK;

    bs_connection_get_bool_property (connection, BSPROP_STATUS, aIsConnected);
    return NS_OK;
    
}

NS_IMETHODIMP
bsConnection::SetIsConnected(PRBool aIsConnected)
{
    BS_INIT_CHECK;

    bs_connection_set_bool_property (connection, BSPROP_STATUS, aIsConnected);
    return NS_OK;
    
}

NS_IMETHODIMP
bsConnection::GetLinebufferFlag(PRBool *aLinebufferFlag)
{
    BS_INIT_CHECK;

    bs_connection_get_bool_property (connection, BSPROP_LINE_BUFFER,
                                     aLinebufferFlag);
    return NS_OK;
    
}
    
NS_IMETHODIMP
bsConnection::SetLinebufferFlag(PRBool aLinebufferFlag)
{
    BS_INIT_CHECK;

    bs_connection_set_bool_property (connection, BSPROP_LINE_BUFFER,
                                     aLinebufferFlag);
    return NS_OK;
    
}

NS_IMETHODIMP
bsConnection::GetPort(PRUint16 *aPort)
{
    BS_INIT_CHECK;

    bs_connection_get_uint_property (connection, BSPROP_PORT, aPort);
    return NS_OK;
    
}

NS_IMETHODIMP
bsConnection::SetPort(PRUint16 aPort)
{
    BS_INIT_CHECK;

    bs_connection_set_uint_property (connection, BSPROP_PORT, aPort);
    return NS_OK;
    
}

NS_IMETHODIMP
bsConnection::Disconnect(PRBool *rval)
{
    BS_INIT_CHECK;

    if (connection != (void *)0)
        *rval = bs_connection_disconnect (connection);
    else
        *rval = PR_FALSE;
    
    return NS_OK;
    
}

NS_IMETHODIMP
bsConnection::Connect(PRUint16 port, const char *bindIP, PRBool tcpFlag,
                      PRBool *rval)
{
    BS_INIT_CHECK;

    connection = bs_server_connect(server, port, bindIP, tcpFlag);

    *rval = (connection != (void *)0) ? PR_TRUE : PR_FALSE;

    return NS_OK;
    
}

NS_IMETHODIMP
bsConnection::SendData(const char *data)
{
    BS_INIT_CHECK;

    if (!connection)
        return NS_ERROR_FAILURE;

    if (!bs_connection_send_string (connection, data))
        return NS_ERROR_FAILURE;
    
    return NS_OK;
    
}

NS_IMETHODIMP
bsConnection::HasData (PRBool *result)
{
    PRPollDesc polld;
    
    if (mDataFlag)
        return PR_TRUE;


    polld.fd = bs_connection_get_fd (connection);
    polld.in_flags = PR_POLL_READ;

    PRInt32 rv = PR_Poll (&polld, 1, PR_INTERVAL_NO_WAIT);
    if (rv < 0)
    {
        BS_ReportPRError (PR_GetError());
        return NS_ERROR_FAILURE;
    }

    *result = (polld.out_flags & PR_POLL_READ) ? PR_TRUE : PR_FALSE;

    return PR_TRUE;
    
}

NS_IMETHODIMP
bsConnection::ReadData(PRUint32 timeout, char **aData)
{
    NS_PRECONDITION (aData, "null ptr");
    bschar *data;

    bsint c = bs_connection_recv_data (connection, timeout, &data);
    
    if (c < 0)
        return NS_ERROR_FAILURE;
    else
        if (c == 0)
        {
            *aData = (char*) nsAllocator::Alloc(1);
            **aData = '\00';
        }
        else
        {
            *aData = (char*) nsAllocator::Alloc(PL_strlen(data) + 1);
            if (! *aData)
                return NS_ERROR_OUT_OF_MEMORY;
 
            PL_strcpy(*aData, data);
        }

    mDataFlag = PR_FALSE;

    return NS_OK;
    
}
