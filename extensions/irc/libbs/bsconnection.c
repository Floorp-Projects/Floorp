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

#include "string.h"

#include "prtypes.h"
#include "prerror.h"
#include "prmem.h"
#include "prio.h"
#include "prlog.h"

#include "bspubtd.h"
#include "bsutil.h"
#include "bsconfig.h"
#include "bserror.h"
#include "bsevent.h"
#include "bsqueue.h"
#include "bsapi.h"
#include "bsserver.h"
#include "bsconnection.h"

PR_BEGIN_EXTERN_C

BSConnectionClass *
bs_connection_new (BSServerClass *parent, PRFileDesc *fd, bsint port)
{
    BSConnectionClass *new_connection;
    
    new_connection = PR_NEW (BSConnectionClass);
    if (!new_connection)
    {
        BS_ReportError (BSERR_OUT_OF_MEMORY);
        return PR_FALSE;
    }

    memset (new_connection->callback_table, 0, BSCONNECTION_EVENT_COUNT);
    
    new_connection->parent_server = parent;
    new_connection->linebuffer = NULL;
    new_connection->port = port;
    new_connection->fd = fd;
    new_connection->linebuffer_flag = BS_LINEBUFFER_FLAG_DEFAULT;
    new_connection->is_connected = PR_TRUE;
    
    return new_connection;
}

void
bs_connection_free (BSConnectionClass *connection)
{
    PR_ASSERT (connection);

     /* partially received line will be lost */
    PR_FREEIF (connection->linebuffer);
    
    bs_connection_disconnect (connection);
    
    PR_Free (connection);
    
}

PRBool
bs_connection_disconnect (BSConnectionClass *connection)
{
    PRStatus rv;
    
    PR_ASSERT (connection);

    if (connection->is_connected)
    {
        PR_FREEIF (connection->linebuffer);

        rv = PR_Shutdown (connection->fd, PR_SHUTDOWN_BOTH);
        if (PR_FAILURE == rv)
            BS_ReportPRError (PR_GetError());
            
        rv = PR_Close (connection->fd);
        if (PR_FAILURE == rv)
            BS_ReportPRError (PR_GetError());
    }

    connection->is_connected = PR_FALSE;
    
    return PR_FALSE;
} 

PRBool /* public method */
bs_connection_send_data (BSConnectionClass *connection, bschar *data,
                         bsuint length)
{
    PRIntervalTime to;
    PRInt32 count;
    
    PR_ASSERT (connection);

    to = PR_MillisecondsToInterval (BS_SEND_TIMEOUT_MS);

    count = PR_Send (connection->fd, data, length, 0, to);

    if (count == -1)
    {
        BS_ReportPRError (PR_GetError());
        return PR_FALSE;
    }

    return PR_TRUE;
    
}

PRBool /* public method */
bs_connection_send_string (BSConnectionClass *connection, const bschar *data)
{
    bsuint length;

    PR_ASSERT (connection);

    length =  strlen (data);

    return bs_connection_send_data (connection, data, length);
    
}   

PRInt32 /* friend function */
bs_connection_recv_data (BSConnectionClass *connection, PRInt32 timeout,
                         bschar **buf)
{
    PRIntervalTime  to;
    PRInt32         count;
    PRErrorCode     err;
    static bschar   recv_buf[BSMAX_SERVER_RECV];

    PR_ASSERT (connection);    
	*buf = recv_buf;

    to = PR_MillisecondsToInterval (timeout);

    count = PR_Recv(connection->fd, recv_buf, BSMAX_SERVER_RECV, 0, to);
    
    switch (count)
    {
        case -1: /* recv error */
            err = PR_GetError();
            switch (err)
            {
                case PR_IO_TIMEOUT_ERROR:
                    buf[0] = '\00';
                    count = 0;
                    break;

                default:
                    BS_ReportPRError (err);
                    goto err_out;
                    break;
            }
            break;
            
        case 0: /* connection closed */
            count = -1;
            goto err_out;
            
        default: /* byte count */
            ((char *)(*buf))[count] = 0;
            
    }

    return count;

  err_out:
    *buf = NULL;
    return count;

}    

PRBool /* friend function */
bs_connection_recv (BSConnectionClass *connection, BSQueueClass *event_queue)
{
    bschar *buf, *lastline = NULL;
    bschar **lineary;
    int    i, lines;    
    BSEventClass    *event;
    PRBool completeline_flag;
    PRInt32 count;
    
    PR_ASSERT (connection);
    
    count = bs_connection_recv_data (connection, BS_RECV_TIMEOUT_MS, &buf);
    
    if (count == 0)
    {
        event = bs_event_new (BSOBJ_CONNECTION, connection,
                              BSEVENT_CONNECTION_DISCONNECT, NULL,
                              NULL, NULL);
        if (!event)
            return PR_FALSE;
        
        if (0 == bs_queue_add (event_queue, event, 0))
        {
            BS_ReportError (BSERR_OUT_OF_MEMORY);
            return PR_FALSE;
            
        }
        return PR_FALSE;
    }
    if (count < 0)
    {
        BS_ReportError (BSERR_UNDEFINED_ERROR);
        return PR_FALSE;
    }

    if (connection->linebuffer_flag)
    {
        completeline_flag = (buf[count - 1] == '\n') ? PR_TRUE :
            PR_FALSE;

        lineary = bs_util_delimbuffer_to_array (buf, &lines, '\n');
        
        for (i = 0; i < lines - 1; i++)
        { 
            event = bs_event_new_copyZ (BSOBJ_CONNECTION, connection,
                                        BSEVENT_CONNECTION_RAWDATA,
                                        lineary[i], NULL);
            if (!event)
                return PR_FALSE;

            if (0 == bs_queue_add (event_queue, event, 0))
            {
                BS_ReportError (BSERR_OUT_OF_MEMORY);
                return PR_FALSE;
            }
        }

        buf = lastline = bs_util_linebuffer (lineary[lines - 1],
											 &connection->linebuffer,
											 completeline_flag);

        PR_Free (lineary);
    }
            
    if (buf)
    {
        connection->linebuffer = NULL;
                
        event = bs_event_new_copyZ (BSOBJ_CONNECTION, connection,
                                    BSEVENT_CONNECTION_RAWDATA,
                                    buf, NULL);
        if (!event)
            return PR_FALSE;

        if (0 == bs_queue_add (event_queue, event, 0))
        {
            BS_ReportError (BSERR_OUT_OF_MEMORY);
            return PR_FALSE;
        }

    }

	PR_FREEIF (lastline);
    return PR_TRUE;

}
    
PRFileDesc * /* friend function */
bs_connection_get_fd (BSConnectionClass *connection)
{
    PR_ASSERT (connection);

    if (!connection->is_connected)
        return NULL;
    
    return connection->fd;
    
}

BSEventClass *
bs_connection_route (BSConnectionClass *connection, BSEventClass *event)
{

    PR_ASSERT (connection);

    if ((event->id < BSCONNECTION_EVENT_COUNT) &&
        (connection->callback_table[event->id]))
        return connection->callback_table[event->id](connection, event);
    else
        return NULL;
    
}                               

PRBool
bs_connection_set_handler (BSConnectionClass *connection,
                           BSConnectionEvent event_type,
                           BSConnectionEventCallback *handler)
{
    PR_ASSERT (connection);

    if (event_type > BSCONNECTION_EVENT_COUNT)
    {
        BS_ReportError (BSERR_NO_SUCH_EVENT);
        return PR_FALSE;
    }

    connection->callback_table[event_type] = handler;

    return PR_TRUE;
}

PRBool
bs_connection_set_property (BSConnectionClass *connection,
			    BSConnectionProperty prop,
                            bsint type, void *v)
{
    PR_ASSERT (connection);

    switch (prop)
    {
        case BSPROP_PARENT:
            if (!BS_CHECK_TYPE (type, BSTYPE_OBJECT))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }

            connection->parent_server = (BSServerClass *)v;
            break;
            
        case BSPROP_LINE_BUFFER:
            if (!BS_CHECK_TYPE (type, BSTYPE_BOOLEAN))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }                

            connection->linebuffer_flag = *(PRBool *)v;
            break;

        case BSPROP_STATUS:
            if (!BS_CHECK_TYPE (type, BSTYPE_BOOLEAN))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }

            BS_ReportError (BSERR_READ_ONLY);
            return PR_FALSE;
            break;

        case BSPROP_PORT:
            if (!BS_CHECK_TYPE (type, BSTYPE_UINT))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }                

            if (connection->is_connected)
            {
                BS_ReportError (BSERR_READ_ONLY);
                return PR_FALSE;
            }
            
            connection->port = *(bsuint *)v;
            break;

        default:
            BS_ReportError (BSERR_NO_SUCH_PARAM);
            return PR_FALSE;
            break;
            
    }
    
    return PR_FALSE;
}

PRBool
bs_connection_set_uint_property (BSConnectionClass *connection,
                                 BSConnectionProperty prop,
                                 bsuint v)
{
    return bs_connection_set_property (connection, prop, BSTYPE_UINT, &v);
    
}    

PRBool
bs_connection_set_string_property (BSConnectionClass *connection,
                                   BSConnectionProperty prop,
                                   char *v)
{
    return bs_connection_set_property (connection, prop, BSTYPE_STRING, v);
    
}    

PRBool
bs_connection_set_bool_property (BSConnectionClass *connection,
                                 BSConnectionProperty prop,
                                 PRBool v)
{
    return bs_connection_set_property (connection, prop, BSTYPE_BOOLEAN, &v);
    
}

PRBool
bs_connection_set_object_property (BSConnectionClass *connection,
                                   BSConnectionProperty prop,
                                   void *v)
{
    return bs_connection_set_property (connection, prop, BSTYPE_OBJECT, v);
    
}

PRBool
bs_connection_get_property (BSConnectionClass *connection,
                            BSConnectionProperty prop,
                            bsint type, void *v)
{
    PR_ASSERT (connection);

    switch (prop)
    {
        case BSPROP_PARENT:
            if (!BS_CHECK_TYPE (type, BSTYPE_OBJECT))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }

            v = connection->parent_server;
            break;
            
        case BSPROP_LINE_BUFFER:
            if (!BS_CHECK_TYPE (type, BSTYPE_BOOLEAN))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }                

            *(PRBool *)v = connection->linebuffer_flag;
            break;

        case BSPROP_PORT:
            if (!BS_CHECK_TYPE (type, BSTYPE_UINT))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }                

            *(int *)v = connection->port;
            break;

        case BSPROP_STATUS:
            if (!BS_CHECK_TYPE (type, BSTYPE_BOOLEAN))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }                

            *(PRBool *)v = connection->is_connected;
            break;
            
        default:
            BS_ReportError (BSERR_NO_SUCH_PARAM);
            return PR_FALSE;
            break;
            
    }
    
    return PR_TRUE;
    
}

PRBool
bs_connection_get_uint_property (BSConnectionClass *connection,
                                 BSConnectionProperty prop,
                                 bsuint *v)

{
    return bs_connection_get_property (connection, prop, BSTYPE_UINT, v);
    
}

PRBool
bs_connection_get_string_property (BSConnectionClass *connection,
                                   BSConnectionProperty prop,
                                   bschar **v)
{
    return bs_connection_get_property (connection, prop, BSTYPE_STRING, v);
    
}

PRBool
bs_connection_get_bool_property (BSConnectionClass *connection,
                                 BSConnectionProperty prop,
                                 PRBool *v)
{
    return bs_connection_get_property (connection, prop, BSTYPE_BOOLEAN, v);
    
}

PRBool
bs_connection_get_object_property (BSConnectionClass *connection,
                                   BSConnectionProperty prop,
                                   void **v)

{
    return bs_connection_get_property (connection, prop, BSTYPE_OBJECT, v);
    
}

PR_END_EXTERN_C
