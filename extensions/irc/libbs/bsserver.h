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

#ifndef bsserver_h___
#define bsserver_h___

#include "prtypes.h"

#include "bsnetwork.h"

PR_BEGIN_EXTERN_C

/* constructor */
extern BSServerClass *
bs_server_new (BSNetworkClass *parent, const char *hostname);

/* deallocator */
extern void 
bs_server_free (BSServerClass *server);

/* connect to the server.  Basically, a connection factory */
extern BSConnectionClass *
bs_server_connect (BSServerClass *server, bsint port, const bschar *bind_addr,
                   PRBool tcp);

/* poll all connections */
PRBool
bs_server_poll_connections (BSServerClass *server, BSQueueClass *event_queue);

/* route an event, friends only */
BSEventClass *
bs_server_route (BSServerClass *server, BSEventClass *event);

/* changes an event handler */
extern PRBool
bs_server_set_handler (BSServerClass *server,
                       BSServerEvent event_type,
                       BSServerEventCallback *handler);

/* property access routines */
extern PRBool
bs_server_set_uint_property (BSServerClass *server, BSServerProperty prop,
                             bsuint *v);

extern PRBool
bs_server_set_string_property (BSServerClass *server, BSServerProperty prop,
                               bschar *v);

extern PRBool
bs_server_set_bool_property (BSServerClass *server, BSServerProperty prop,
                             PRBool *v);

extern PRBool
bs_server_set_object_property (BSServerClass *server, BSServerProperty prop,
                               void *v);

extern PRBool
bs_server_get_uint_property (BSServerClass *server, BSServerProperty prop,
                             bsuint *v);

extern PRBool
bs_server_get_string_property (BSServerClass *server, BSServerProperty prop,
                               bschar **v);

extern PRBool
bs_server_get_bool_property (BSServerClass *server, BSServerProperty prop,
                             PRBool *v);

extern PRBool
bs_server_get_object_property (BSServerClass *server, BSServerProperty prop,
                               BSServerClass **v);

PR_END_EXTERN_C

#endif /* bsserver_h___ */
