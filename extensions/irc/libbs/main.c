/*
 * Copyright (C) 1999 Robert Ginda
 * All Rights Reserved
 */

#include <stdio.h>

#include "prtypes.h"
#include "prinit.h"

#include "bsapi.h"
#include "bsnetwork.h"
#include "bsserver.h"
#include "bsconnection.h"

BSEventClass *
sample_onRawData (BSConnectionClass *connection, BSEventClass *event)
{

    fprintf (stdout, "rawdata event '%s'\n", (bschar *)event->data);

    return NULL;
    
}

int
main (int argc, char **argv)
{
    BSNetworkClass     *network;
    BSServerClass      *server;
    BSConnectionClass  *connection1, *connection2;
    bsuint             event_count;
    PRBool             lb;
    PRIntervalTime     to;

    network = bs_network_new ("EchoNet");

    server = bs_network_add_server (network, "127.0.0.1");

    connection1 = bs_server_connect (server, 7, NULL, PR_TRUE);
    bs_connection_set_handler (connection1, BSEVENT_CONNECTION_RAWDATA,
                               sample_onRawData);

    connection2 = bs_server_connect (server, 7, NULL, PR_TRUE);
    bs_connection_set_handler (connection2, BSEVENT_CONNECTION_RAWDATA,
                               sample_onRawData);

    bs_connection_get_bool_property (connection1, BSPROP_LINE_BUFFER, &lb);
    printf ("Connection 1 linebuffer flag: %i\n", lb);

    bs_connection_set_bool_property (connection1, BSPROP_LINE_BUFFER,
				     PR_FALSE);
    bs_connection_get_bool_property (connection1, BSPROP_LINE_BUFFER, &lb);
    printf ("Set connection 1 linebuffer flag: %i\n", lb);

    bs_connection_send_string (connection1, "hello\nconnection 1\n");

    bs_connection_send_string (connection2, "hello connection 2\n");

    to = PR_MillisecondsToInterval (1000);

    do
    {
        PR_Sleep (to);
        bs_network_poll_servers (network);
        bs_network_step_events (network, &event_count);
        
    } while (event_count > 0);

    bs_network_free (network);

    return PR_Cleanup();
    
}




