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

#include "prtypes.h"
#include "prmem.h"
#include "prlog.h"

#include "bsapi.h"
#include "bsconfig.h"
#include "bspubtd.h"
#include "bserror.h"
#include "bsserver.h"
#include "bsconnection.h"

PR_BEGIN_EXTERN_C

typedef struct BSServerLink BSServerLink;

struct BSServerLink {
    PRCList        list;
    BSServerClass  *server;
};

typedef struct BSServerList BSServerList;

BSNetworkClass *
bs_network_new (const char *name)
{
    BSNetworkClass *new_network;

    new_network = PR_NEW (BSNetworkClass);
    if (!new_network)
        return PR_FALSE;

    new_network->name = PR_Malloc ((strlen (name) + 1) *
                                   sizeof (bschar));
    if (!new_network->name)
    {
        BS_ReportError (BSERR_OUT_OF_MEMORY);
        return PR_FALSE;
    }

    strcpy (new_network->name, name);

    new_network->event_queue = bs_queue_new();
    if (!new_network->event_queue)
    {
        BS_ReportError (BSERR_OUT_OF_MEMORY);
        return NULL;
    }

    new_network->events_per_step = BS_EVENTS_PER_STEP_DEFAULT;
    new_network->server_count = 0;
    
    PR_INIT_CLIST (&new_network->server_list);
    
    return new_network;

}
    
void
bs_network_free (BSNetworkClass *network)
{
    PRCList        *head;
    BSServerClass  *server;

    PR_ASSERT (network);

    while (bs_queue_size (network->event_queue) > 0)
        bs_queue_pop (network->event_queue, NULL);
            
    bs_queue_free (network->event_queue);

    while (!PR_CLIST_IS_EMPTY(&network->server_list))
    {
        head = PR_LIST_HEAD(&network->server_list);
	if (head == &network->server_list)
	    break;	
        server = ((BSServerLink *)head)->server;
	bs_server_free (server);
        PR_REMOVE_AND_INIT_LINK(head);
	network->server_count--;
    }

    PR_ASSERT (network->server_count == 0);

    PR_FREEIF (server->hostname);
    PR_Free (server);
    
}

BSServerClass *
bs_network_add_server (BSNetworkClass *network, const char *hostname)
{
    BSServerLink *server_link;

    server_link = PR_NEW(BSServerLink);
    if (!server_link)
        return NULL;
    
    server_link->server = bs_server_new (network, hostname);

    if (!server_link->server)
    {
        BS_ReportError (BSERR_OUT_OF_MEMORY);
        return NULL;
    }

    PR_APPEND_LINK ((PRCList *)server_link, &network->server_list);

    network->server_count++;

    return server_link->server;
    
}

PRBool
bs_network_enumerate_servers (BSNetworkClass *network,
			      BSEnumerateCallback *enum_func, void *data)
{
    PRCList        *link;
    BSServerClass  *server;
    bsuint	    i = 0;

    PR_ASSERT (network);

    if (PR_CLIST_IS_EMPTY(&network->server_list))
        return PR_TRUE;

    link = PR_LIST_HEAD(&network->server_list);

    while (link != (PRCList *)network)
    {
        server = ((BSServerLink *)link)->server;
	if (!enum_func (server, data, i++))
	    break;
        link = PR_NEXT_LINK (link);
    }

    return PR_TRUE;

}

PRBool
bs_network_server_enumerator (void *server, void *event_queue, 
			      bsuint i)
{
    
    bs_server_poll_connections ((BSServerClass *)server,
                                (BSQueueClass *)event_queue);
    return PR_TRUE;

}

PRBool
bs_network_poll_servers (BSNetworkClass *network)
{

    bs_network_enumerate_servers (network, bs_network_server_enumerator,
				  network->event_queue);

    return PR_TRUE;

}

BSEventClass *
bs_network_route (BSNetworkClass *network, BSEventClass *event)
{

    if ((event->id < BSNETWORK_EVENT_COUNT) &&
        (network->callback_table[event->id]))
        return network->callback_table[event->id](network, event);
    else
        return NULL;
    
}                               

void
bs_network_step_events (BSNetworkClass *network, bsuint *event_count)
{
    bsuint i;
    BSEventClass *event;

    if (event_count)
        *event_count = 0;
    
    for (i = 0; (i < network->events_per_step) &&
	   (bs_queue_size (network->event_queue) > 0); i++)
    {
	event = bs_queue_pop (network->event_queue, NULL);
	bs_event_route (event, NULL);
        if (event_count)
            (*event_count)++;
    }

}

PRBool
bs_network_set_handler (BSNetworkClass *network, BSNetworkEvent event_type,
                        BSNetworkEventCallback *handler)
{
    PR_ASSERT (network);

    network->callback_table[event_type] = handler;

    return PR_TRUE;

}
    
PRBool
bs_network_set_property (BSNetworkClass *network, BSNetworkProperty prop,
                         bsint type, void *v)
{
    PR_ASSERT (network);

    switch (prop)
    {
	    case BSPROP_NAME:
            if (!BS_CHECK_TYPE (type, BSTYPE_STRING))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }
			
			BS_ReportError (BSERR_READ_ONLY);
			return PR_FALSE;
			break;

        case BSPROP_EVENTS_PER_STEP:
            if (!BS_CHECK_TYPE (type, BSTYPE_UINT))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }

            network->events_per_step = *(bsint *)v;
            break;

        default:
            break;

    }

    return PR_TRUE;
    
}

PRBool
bs_network_set_uint_property (BSNetworkClass *network, BSNetworkProperty prop,
                              bsuint v)
{
    return bs_network_set_property (network, prop, BSTYPE_UINT, &v);
    
}    

PRBool
bs_network_set_string_property (BSNetworkClass *network,
                                BSNetworkProperty prop, bschar *v)
{
    return bs_network_set_property (network, prop, BSTYPE_STRING, v);
    
}    

PRBool
bs_network_set_bool_property (BSNetworkClass *network, BSNetworkProperty prop,
                              PRBool v)
{
    return bs_network_set_property (network, prop, BSTYPE_BOOLEAN, &v);
    
}

PRBool
bs_network_set_object_property (BSNetworkClass *network,
                                BSNetworkProperty prop, void *v)
{
    return bs_network_set_property (network, prop, BSTYPE_OBJECT, v);
    
}

PRBool
bs_network_get_property (BSNetworkClass *network, BSServerProperty prop,
                         bsint type, void **v)
{
    PR_ASSERT (network);

    switch (prop)
	{

	    case BSPROP_NAME:
            if (!BS_CHECK_TYPE (type, BSTYPE_STRING))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }

            *(bschar **)v = network->name;
            break;
			
        case BSPROP_EVENTS_PER_STEP:
            if (!BS_CHECK_TYPE (type, BSTYPE_UINT))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }

            *(bsint *)v = network->events_per_step;
            break;

        default:
            break;
            
    }

    return PR_TRUE;
    
}

PRBool
bs_network_get_uint_property (BSNetworkClass *network, BSNetworkProperty prop,
                              bsuint *v)

{
    return bs_network_set_property (network, prop, BSTYPE_UINT, v);
    
}

PRBool
bs_network_get_string_property (BSNetworkClass *network,
                                BSNetworkProperty prop, bschar **v)
{
    return bs_network_set_property (network, prop, BSTYPE_STRING, v);
    
}

PRBool
bs_network_get_bool_property (BSNetworkClass *network, BSNetworkProperty prop,
                             PRBool *v)
{
    return bs_network_set_property (network, prop, BSTYPE_BOOLEAN, v);
    
}

PRBool
bs_network_get_object_property (BSNetworkClass *network,
                                BSNetworkProperty prop, BSNetworkClass **v)

{
    return bs_network_set_property (network, prop, BSTYPE_OBJECT, v);
    
}

PR_END_EXTERN_C
