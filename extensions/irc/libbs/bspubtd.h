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

#ifndef bspubtd_h___
#define bspubtd_h___

#include "prtypes.h"
#include "prclist.h"
#include "prio.h"

PR_BEGIN_EXTERN_C

typedef PRInt32   bsint;
typedef PRUint16  bsuint;
typedef PRInt64   bsdouble;
typedef char      bschar;

typedef bsint BSHandlerID;

typedef bsuint BSNetworkProperty;
typedef bsuint BSServerProperty;
typedef bsuint BSConnectionProperty;
typedef bsuint BSEventProperty;

typedef struct BSQueueClass       BSQueueClass;
typedef struct BSEventClass       BSEventClass;
typedef struct BSNetworkClass     BSNetworkClass;
typedef struct BSServerClass      BSServerClass;
typedef struct BSConnectionClass  BSConnectionClass;

typedef BSEventClass *BSEventHookCallback (BSEventClass *event);
typedef BSEventClass *BSNetworkEventCallback (BSNetworkClass *network,
                                              BSEventClass *event);
typedef BSEventClass *BSServerEventCallback (BSServerClass *server,
                                             BSEventClass *event);
typedef BSEventClass *BSConnectionEventCallback (BSConnectionClass *connection,
                                                 BSEventClass *event);

typedef PRBool BSEnumerateCallback (void *obj, void *data, bsuint i);
typedef void *BSEventDeallocator (BSEventClass *event);


typedef enum BSType {
    BSTYPE_UINT = 1,
    BSTYPE_STRING = 2,
    BSTYPE_BOOLEAN = 4,
    BSTYPE_OBJECT = 8
} BSType;

typedef enum BSObjectType {
    BSOBJ_NETWORK,
    BSOBJ_SERVER,
    BSOBJ_CONNECTION,
    BSOBJ_INVALID_OBJECT
} BSObjectType;

struct BSEventClass {
    BSEventDeallocator  *da;
    BSObjectType        obj_type;
    BSEventClass        *previous;
    BSHandlerID         id;
    void                *dest;
    void                *data;
    
};

/** network typedefs **/
typedef enum BSNetworkEvent {
    BSEVENT_NETWORK_HOOK_CALLBACK,
    BSNETWORK_EVENT_COUNT
} BSNetworkEvent;

struct BSNetworkClass 
{
    PRCList                 server_list;
    BSNetworkEventCallback  *callback_table[BSNETWORK_EVENT_COUNT];
    bschar                  *name;
    BSQueueClass            *event_queue;
    bsuint                  events_per_step, server_count;
    
};

/** server typedefs **/
typedef enum BSServerEvent {
    BSEVENT_SERVER_HOOK_CALLBACK,
    BSEVENT_SERVER_CONNECT,
    BSEVENT_SERVER_DISCONNECT,
    BSEVENT_SERVER_RAWDATA,
    BSSERVER_EVENT_COUNT
} BSServerEvent;

struct BSServerClass 
{
    PRCList                connection_list;
    BSServerEventCallback  *callback_table[BSSERVER_EVENT_COUNT];
    BSNetworkClass         *parent_network;
    bschar                 *hostname;
    bsuint                 input_timeout, connection_count;
    PRBool                 linebuffer_flag;
};

/** connection typedefs **/
typedef enum BSConnectionEvent {
    BSEVENT_CONNECTION_HOOK_CALLBACK,
    BSEVENT_CONNECTION_CONNECT,
    BSEVENT_CONNECTION_DISCONNECT,
    BSEVENT_CONNECTION_RAWDATA,
    BSCONNECTION_EVENT_COUNT
} BSConnectionEvent;

struct BSConnectionClass 
{
    BSConnectionEventCallback  *callback_table[BSCONNECTION_EVENT_COUNT];
    BSServerClass              *parent_server;
    PRBool                     linebuffer_flag, is_connected;
    bsint                      port;
    bschar                     *linebuffer;
    PRFileDesc                 *fd;
    
};

PR_END_EXTERN_C

#endif /* bspubtd_h___ */

