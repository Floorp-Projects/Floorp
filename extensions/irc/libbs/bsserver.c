/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include <string.h>

#include "prtypes.h"
#include "prmem.h"
#include "prio.h"
#include "prclist.h"
#include "prlog.h"
#include "prnetdb.h"

#include "bspubtd.h"
#include "bsconfig.h"
#include "bserror.h"
#include "bsevent.h"
#include "bsapi.h"
#include "bsconnection.h"
#include "bsserver.h"
#include "bsutil.h"

PR_BEGIN_EXTERN_C

typedef struct BSConnectionEnumData  BSConnectionEnumData;
typedef struct BSConnectionLink      BSConnectionLink;

struct BSConnectionEnumData {
    PRPollDesc         *polldesc;
    BSConnectionClass  **connection;
};

struct BSConnectionLink {
    PRCList            list;
    BSConnectionClass  *connection;
};

BSServerClass *
bs_server_new (BSNetworkClass *parent, const char *hostname)
{
    BSServerClass *new_server;
    
    new_server = PR_NEW (BSServerClass);
    if (!new_server)
    {
        BS_ReportError (BSERR_OUT_OF_MEMORY);
        return PR_FALSE;
    }

    if (!hostname)
    {
        BS_ReportError (BSERR_INVALID_STATE);
        PR_Free (new_server);
        return NULL;
    }
    
    new_server->hostname = PR_Malloc ((strlen (hostname) + 1) *
                                      sizeof (bschar));
    if (!new_server->hostname)
    {
        BS_ReportError (BSERR_OUT_OF_MEMORY);
        PR_Free (new_server);
        return PR_FALSE;
    }

    strcpy (new_server->hostname, hostname);

    memset (new_server->callback_table, 0,
            BSSERVER_EVENT_COUNT);

    new_server->parent_network = parent;
    new_server->connection_count = 0;
    new_server->input_timeout = BS_INPUT_TIMEOUT_DEFAULT;
    
    PR_INIT_CLIST (&new_server->connection_list);

    return new_server;

}

void
bs_server_free (BSServerClass *server)
{
    PRCList            *head;
    BSConnectionClass  *connection;

    PR_ASSERT (server);

    while (!PR_CLIST_IS_EMPTY(&server->connection_list))
    {
        head = PR_LIST_HEAD(&server->connection_list);
	if (head == &server->connection_list)
	    break;
        connection = ((BSConnectionLink *)head)->connection;
	bs_connection_free (connection);
        PR_REMOVE_AND_INIT_LINK(head);
	server->connection_count--;
    }

    PR_ASSERT (server->connection_count == 0);

    if (server->hostname)
        PR_Free (server->hostname);

    PR_Free (server);
    
}

BSConnectionClass *
bs_server_connect (BSServerClass *server, bsint port, const bschar *bind_addr,
                   PRBool tcp)
{
    PRFileDesc          *fd;
    PRNetAddr           naconnect, nabind;
    PRIntervalTime      to;
    PRStatus            rv;
    BSConnectionClass   *connection;
    BSConnectionLink    *connection_link;

    PR_ASSERT (server);

    if (tcp)
        fd = PR_NewTCPSocket();
    else
        fd = PR_NewUDPSocket();
    if (!fd)
    {
        BS_ReportPRError (PR_GetError());
        return NULL;
    }

    if (bs_util_is_ip (server->hostname))
        rv = PR_StringToNetAddr (server->hostname, &naconnect);
    else
        rv = bs_util_resolve_host (server->hostname, &naconnect);
    if (PR_FAILURE == rv)
    {
        BS_ReportPRError (PR_GetError());
        goto error_out;
    }   
    
    if (bind_addr)
    {
        rv = PR_StringToNetAddr (bind_addr, &nabind);
        if (PR_FAILURE == rv)
        {
            BS_ReportPRError (PR_GetError());
            goto error_out;
        }

        PR_Bind (fd, &nabind);
    }

    rv = PR_InitializeNetAddr (PR_IpAddrNull, port, &naconnect);
    if (PR_FAILURE == rv)
    {
        BS_ReportPRError (PR_GetError());
        goto error_out;
    }

    to = PR_MillisecondsToInterval (BS_CONNECTION_TIMEOUT_MS);
    
    rv = PR_Connect (fd, &naconnect, to);
    if (PR_FAILURE == rv)
    {
        BS_ReportPRError (PR_GetError());
        goto error_out;
    }
            
    connection = bs_connection_new (server, fd, port);
    if (connection)
    {
        connection_link = PR_NEW (BSConnectionLink);
	if (!connection_link)
	{
	    BS_ReportError (BSERR_OUT_OF_MEMORY);
            PR_Shutdown (fd, PR_SHUTDOWN_BOTH);
            goto error_out;
	}
	connection_link->connection = connection;

        PR_APPEND_LINK ((PRCList *)connection_link, &server->connection_list);
    }
    
    server->connection_count++;

    return connection;

  error_out:
    PR_Close (fd);
    return NULL;

}

PRBool
bs_server_enumerate_connections (BSServerClass *server,
				 BSEnumerateCallback *enum_func, void *data)
{
    PRCList            *link;
    BSConnectionClass  *connection;
    bsuint		i = 0;

    PR_ASSERT (server);

    if (PR_CLIST_IS_EMPTY(&server->connection_list))
        return PR_TRUE;

    link = PR_LIST_HEAD(&server->connection_list);

    while (link != (PRCList *)server)
    {
        connection = ((BSConnectionLink *)link)->connection;
	if (!enum_func (connection, data, i++))
	    break;
        link = PR_NEXT_LINK (link);
    }

    return PR_TRUE;

}

PRBool
bs_server_connection_enumerator (void *connection,
				 void *enumdata, bsuint i)
{
    PRFileDesc            *fd;
    BSConnectionEnumData  *ed;

    fd = bs_connection_get_fd ((BSConnectionClass *)connection);
    ed = (BSConnectionEnumData *)enumdata;

    ed->polldesc[i].fd = fd;
    ed->polldesc[i].in_flags = PR_POLL_READ;
    ed->connection[i] = (BSConnectionClass *)connection;
    
    return PR_TRUE;

}

PRBool
bs_server_poll_connections (BSServerClass *server, BSQueueClass *event_queue)
{
    PRInt32               rv, i;
    BSConnectionEnumData  enumdata;
   
    enumdata.polldesc = PR_Calloc (server->connection_count, 
				   sizeof (PRPollDesc *));

    enumdata.connection = PR_Calloc (server->connection_count, 
				     sizeof (BSConnectionClass *));

    if (!enumdata.polldesc || !enumdata.connection)
    {
        BS_ReportError (BSERR_OUT_OF_MEMORY);
	return PR_FALSE;
    }

    bs_server_enumerate_connections (server, bs_server_connection_enumerator,
				     &enumdata);

    rv = PR_Poll (enumdata.polldesc, server->connection_count,
		  PR_INTERVAL_NO_WAIT);

    for (i = 0; i < server->connection_count; i++)
        if (enumdata.polldesc[i].out_flags & PR_POLL_READ)
            bs_connection_recv (enumdata.connection[i], event_queue);
    
    return PR_TRUE;

}

BSEventClass *
bs_server_route (BSServerClass *server, BSEventClass *event)
{

    if ((event->id < BSSERVER_EVENT_COUNT) &&
        (server->callback_table[event->id]))
        return server->callback_table[event->id](server, event);
    else
        return NULL;
    
}

PRBool
bs_server_set_handler (BSServerClass *server, BSServerEvent event_type,
                       BSServerEventCallback *handler)
{
    PR_ASSERT (server);

    if (event_type > BSSERVER_EVENT_COUNT)
    {
        BS_ReportError (BSERR_NO_SUCH_EVENT);
        return PR_FALSE;
    }

    server->callback_table[event_type] = handler;

    return PR_TRUE;

}

PRBool
bs_server_set_property (BSServerClass *server, BSServerProperty prop,
                        bsint type, void *v)
{
    PR_ASSERT (server);

    switch (prop)
    {
        case BSPROP_PARENT:
            if (!BS_CHECK_TYPE(type, BSTYPE_OBJECT))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }

            server->parent_network = (BSNetworkClass *)v;
            break;
            
        case BSPROP_INPUT_TIMEOUT:
            if (!BS_CHECK_TYPE(type, BSTYPE_UINT))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }

            server->input_timeout = *(int *)v;
            break;
            
        case BSPROP_LINE_BUFFER:
            if (!BS_CHECK_TYPE(type, BSTYPE_BOOLEAN))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }                

            server->linebuffer_flag = *(PRBool *)v;
            break;
            

        case BSPROP_HOSTNAME:
            if (!BS_CHECK_TYPE(type, BSTYPE_STRING))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }
            
            PR_FREEIF (server->hostname);
            server->hostname = PR_Malloc ((strlen ((bschar *)v) + 1) *
                                          sizeof (bschar));
            if (!server->hostname)
            {
                BS_ReportError (BSERR_OUT_OF_MEMORY);
                return PR_FALSE;
            }

            strcpy (server->hostname, (bschar *)v);
            break;            

        default:
            break;
    }
    
    return PR_FALSE;
}

PRBool
bs_server_set_uint_property (BSServerClass *server, BSServerProperty prop,
                             bsuint *v)
{
    return bs_server_set_property (server, prop, BSTYPE_UINT, &v);
    
}    

PRBool
bs_server_set_string_property (BSServerClass *server, BSServerProperty prop,
                               bschar *v)
{
    return bs_server_set_property (server, prop, BSTYPE_STRING, v);
    
}    

PRBool
bs_server_set_bool_property (BSServerClass *server, BSServerProperty prop,
                             PRBool *v)
{
    return bs_server_set_property (server, prop, BSTYPE_BOOLEAN, &v);
    
}

PRBool
bs_server_set_object_property (BSServerClass *server, BSServerProperty prop,
                               void *v)
{
    return bs_server_set_property (server, prop, BSTYPE_OBJECT, v);
    
}

PRBool
bs_server_get_property (BSServerClass *server, BSServerProperty prop,
                        bsint type, void *v)
{
    PR_ASSERT (server);

    switch (prop)
    {
        case BSPROP_PARENT:
            if (!BS_CHECK_TYPE (type, BSTYPE_OBJECT))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }

            v = server->parent_network;
            break;
            
        case BSPROP_INPUT_TIMEOUT:
            if (!BS_CHECK_TYPE (type, BSTYPE_UINT))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }

            *(bsuint *)v = server->input_timeout;
            break;
            
        case BSPROP_LINE_BUFFER:
            if (!BS_CHECK_TYPE (type, BSTYPE_BOOLEAN))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }                

            *(PRBool *)v = server->linebuffer_flag;
            break;
            

        case BSPROP_HOSTNAME:
            if (!BS_CHECK_TYPE (type, BSTYPE_STRING))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }

            v = server->hostname;
            break;            

        default:
            break;
            
    }
    
    return PR_FALSE;
}

PRBool
bs_server_get_uint_property (BSServerClass *server, BSServerProperty prop,
                             bsuint *v)

{
    return bs_server_get_property (server, prop, BSTYPE_UINT, v);
    
}

PRBool
bs_server_get_string_property (BSServerClass *server, BSServerProperty prop,
                               bschar **v)
{
    return bs_server_get_property (server, prop, BSTYPE_STRING, v);
    
}

PRBool
bs_server_get_bool_property (BSServerClass *server, BSServerProperty prop,
                             PRBool *v)
{
    return bs_server_get_property (server, prop, BSTYPE_BOOLEAN, v);
    
}

PRBool
bs_server_get_object_property (BSServerClass *server, BSServerProperty prop,
                               BSServerClass **v)

{
    return bs_server_get_property (server, prop, BSTYPE_OBJECT, v);
    
}

PR_END_EXTERN_C

