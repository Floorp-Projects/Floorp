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

#ifndef bsnetwork_h___
#define bsnetwork_h___

#include "prtypes.h"

#include "bspubtd.h"
#include "bsserver.h"
#include "bsconnection.h"

PR_BEGIN_EXTERN_C

extern BSNetworkClass *
bs_network_new (const char *name);

extern void
bs_network_free (BSNetworkClass *network);

BSServerClass *
bs_network_add_server (BSNetworkClass *network, const char *hostname);

PRBool
bs_network_poll_servers (BSNetworkClass *network);

BSEventClass *
bs_network_route (BSNetworkClass *network, BSEventClass *event);

extern void
bs_network_step_events (BSNetworkClass *network, bsuint *event_count);

extern PRBool
bs_connection_set_handler (BSConnectionClass *connection,
                           BSConnectionEvent event_type,
                           BSConnectionEventCallback *handler);

extern PRBool
bs_network_set_uint_property (BSNetworkClass *network, BSNetworkProperty prop,
                              bsuint v);

extern PRBool
bs_network_set_string_property (BSNetworkClass *network,
                                BSNetworkProperty prop, bschar *v);

extern PRBool
bs_network_set_bool_property (BSNetworkClass *network, BSNetworkProperty prop,
                              PRBool v);

extern PRBool
bs_network_set_object_property (BSNetworkClass *network,
                                BSNetworkProperty prop, void *v);

extern PRBool
bs_network_get_uint_property (BSNetworkClass *network, BSNetworkProperty prop,
                              bsuint *v);

extern PRBool
bs_network_get_string_property (BSNetworkClass *network,
                                BSNetworkProperty prop, bschar **v);

extern PRBool
bs_network_get_bool_property (BSNetworkClass *network,
                              BSNetworkProperty prop, PRBool *v);

extern PRBool
bs_network_get_object_property (BSNetworkClass *network,
                                BSNetworkProperty prop, BSNetworkClass **v);

PR_END_EXTERN_C

#endif /* bsnetwork_h__ */
