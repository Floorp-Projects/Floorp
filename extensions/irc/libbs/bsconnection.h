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

#ifndef bsconnection_h___
#define bsconnection_h___

#include "prtypes.h"
#include "prio.h"

#include "bspubtd.h"
#include "bsqueue.h"
#include "bsevent.h"

PR_BEGIN_EXTERN_C

/* constructor, should be called only by a friend's factory */
extern BSConnectionClass *
bs_connection_new (BSServerClass *parent, PRFileDesc *fd, bsint port);

/* deallocator, should be called only by a friend's factory */
extern void
bs_connection_free (BSConnectionClass *connection);

/* restablish the connection */
extern PRBool
bs_connection_reconnect (BSConnectionClass *connection);

/* close the connection */
extern PRBool
bs_connection_disconnect (BSConnectionClass *connection);

/* send an arbitrary number of bytes */
extern PRBool
bs_connection_send_data (BSConnectionClass *connection, bschar *data,
                         bsuint length);

/* send a null-terminated string */
extern PRBool
bs_connection_send_string (BSConnectionClass *connection, const bschar *data);

PRBool /* friend function */
bs_connection_recv_data (BSConnectionClass *connection, PRInt32 timeout,
                         bschar **buf);

/*
 * recv data from the connection.  This is a friend function, non-friends
 * should subscribe to BSEVENT_SERVER_RAWDATA or BSEVENT_CONNECTION_RAWDATA.
 * The friendly factory which created the connection is responsible for
 * periodically calling this function, or the RAWDATA event will never occur.
 */
extern PRBool
bs_connection_recv (BSConnectionClass *connection, BSQueueClass *event_queue);

/* retrieve connections fd, friend function */
extern PRFileDesc *
bs_connection_get_fd (BSConnectionClass *connection);

/* route an event, friends only */
BSEventClass *
bs_connection_route (BSConnectionClass *connection, BSEventClass *event);

/* changes an event handler */
extern PRBool
bs_connection_set_handler (BSConnectionClass *connection,
                           BSConnectionEvent event_type,
                           BSConnectionEventCallback *handler);

/* property access routines */
extern PRBool
bs_connection_set_uint_property (BSConnectionClass *connection,
                                 BSConnectionProperty prop,
                                 bsuint v);

extern PRBool
bs_connection_set_string_property (BSConnectionClass *connection,
                                   BSConnectionProperty prop,
                                   bschar *v);

extern PRBool
bs_connection_set_bool_property (BSConnectionClass *connection,
                                 BSConnectionProperty prop,
                                 PRBool v);

extern PRBool
bs_connection_set_object_property (BSConnectionClass *connection,
                                   BSConnectionProperty prop,
                                   void *v);

/* property gets */
extern PRBool
bs_connection_get_uint_property (BSConnectionClass *connection,
                                 BSConnectionProperty prop,
                                 bsuint *v);

extern PRBool
bs_connection_get_string_property (BSConnectionClass *connection,
                                   BSConnectionProperty prop,
                                   bschar **v);

extern PRBool
bs_connection_get_bool_property (BSConnectionClass *connection,
                                 BSConnectionProperty prop,
                                 PRBool *v);

extern PRBool
bs_connection_get_object_property (BSConnectionClass *connection,
                                   BSConnectionProperty prop,
                                   void **v);

PR_END_EXTERN_C

#endif /* bsconnection_h___ */
