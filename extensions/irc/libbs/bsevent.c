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
#include "bspubtd.h"
#include "bserror.h"
#include "bsevent.h"
#include "bsnetwork.h"
#include "bsserver.h"
#include "bsconnection.h"

PR_BEGIN_EXTERN_C

BSEventClass *
bs_event_new (BSObjectType  obj_type, void *dest, BSHandlerID id, void *data,
              BSEventClass *previous, BSEventDeallocator *da)
{
    BSEventClass *new_event;

    new_event = PR_NEW (BSEventClass);
    if (!new_event)
    {
        BS_ReportError (BSERR_OUT_OF_MEMORY);
        return NULL;
    }

    new_event->obj_type = obj_type;
    new_event->dest = dest;
    new_event->id = id;
    new_event->data = data;
    new_event->previous = previous;
    new_event->da = da;
    
    return new_event;
    
}

void *
bs_event_default_deallocator (BSEventClass *event)
{
    PR_ASSERT (event);

    PR_FREEIF (event->data);

    return NULL;

}

BSEventClass *
bs_event_new_copy (BSObjectType obj_type, void *dest, BSHandlerID id,
		   void *data, bsuint length, BSEventClass *previous)
{
    BSEventClass *rv;
    char *event_data;

    event_data = PR_Malloc (length);
    if (!event_data)
    {
        BS_ReportError (BSERR_OUT_OF_MEMORY);
	return NULL;
    }
    
    memcpy (event_data, data, length);

    rv = bs_event_new (obj_type, dest, id, event_data, previous,
		       bs_event_default_deallocator);
    if (!rv)
        return NULL;

    return rv;

}
    
BSEventClass *
bs_event_new_copyZ (BSObjectType obj_type, void *dest, BSHandlerID id,
		    bschar *data, BSEventClass *previous)
{

    return bs_event_new_copy (obj_type, dest, id, data, strlen (data) + 1,
			      previous);

}

void
bs_event_free (BSEventClass *event)
{
    PR_ASSERT (event);

    if (event->previous)
        bs_event_free (event->previous);

    if (event->da)
        event->da (event);

    PR_Free (event);
    
}

PRBool
bs_event_route (BSEventClass *event, BSEventHookCallback *hook_func)
{
    BSEventClass *next_event;
    PR_ASSERT (event);

    if ((hook_func) && (!hook_func (event)))
        return PR_TRUE;

    switch (event->obj_type)
    {
        case BSOBJ_NETWORK:
            next_event = bs_network_route ((BSNetworkClass *)event->dest,
                                           event);
            break;
            
        case BSOBJ_SERVER:
            next_event = bs_server_route ((BSServerClass *)event->dest,
                                          event);
            break;
            
        case BSOBJ_CONNECTION:
	    next_event = bs_connection_route ((BSConnectionClass *)event->dest,
                                              event);
            break;

        default:
            BS_ReportError (BSERR_INVALID_STATE);
            return PR_FALSE;
            break;
    }

    if (next_event)
    {
        next_event->previous = event;
        bs_event_route (next_event, hook_func);
    }
    
    return PR_TRUE;
    
}

PRBool
bs_event_set_property (BSEventClass *event, BSEventProperty prop, bsint type,
		       void *v)
{
    PR_ASSERT (event);
    
    switch (prop)
    {
        
        case BSPROP_OBJECT_TYPE:
            if (!BS_CHECK_TYPE (type, BSTYPE_UINT))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }
            
            if ((BSObjectType)v >= BSOBJ_INVALID_OBJECT)
            {
                BS_ReportError (BSERR_INVALID_STATE);
                return PR_FALSE;
            }

            event->obj_type = *(BSObjectType *)v;
            break;

        case BSPROP_DESTOBJECT:
            if (!BS_CHECK_TYPE (type, BSTYPE_OBJECT))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }

            event->dest = (void *)v;
            break;

        case BSPROP_HANDLER_ID:
            if (!BS_CHECK_TYPE (type, BSTYPE_UINT))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }

            event->id = *(BSHandlerID *)v;
            break;

        case BSPROP_DATA:
            BS_ReportError (BSERR_READ_ONLY);
            return PR_FALSE;
            break;

        default:
            BS_ReportError (BSERR_NO_SUCH_PARAM);
            return PR_FALSE;
            break;
            
    }
    
    return PR_FALSE;
}

PRBool
bs_event_set_uint_property (BSEventClass *event, BSEventProperty prop,
			    bsuint v)
{
    return bs_event_set_property (event, prop, BSTYPE_UINT, &v);
    
}    

PRBool
bs_event_set_string_property (BSEventClass *event, BSEventProperty prop,
			      char *v)
{
    return bs_event_set_property (event, prop, BSTYPE_STRING, v);
    
}    

PRBool
bs_event_set_bool_property (BSEventClass *event, BSEventProperty prop,
			    PRBool v)
{
    return bs_event_set_property (event, prop, BSTYPE_BOOLEAN, &v);
    
}

PRBool
bs_event_set_object_property (BSEventClass *event, BSEventProperty prop,
			      void *v)
{
    return bs_event_set_property (event, prop, BSTYPE_OBJECT, v);
    
}

PRBool
bs_event_get_property (BSEventClass *event, BSEventProperty prop, bsint type,
		       void *v)
{
    PR_ASSERT (event);

    switch (prop)
    {
        case BSPROP_OBJECT_TYPE:
            if (!BS_CHECK_TYPE (type, BSTYPE_UINT))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }
            
            *(BSObjectType *)v = event->obj_type;
            break;

        case BSPROP_DESTOBJECT:
            if (!BS_CHECK_TYPE (type, BSTYPE_OBJECT))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }

            *(void **)v = event->dest;
            break;

        case BSPROP_HANDLER_ID:
            if (!BS_CHECK_TYPE (type, BSTYPE_UINT))
            {
                BS_ReportError (BSERR_PROPERTY_MISMATCH);
                return PR_FALSE;
            }

            *(BSHandlerID *)v = event->id;
            break;

        case BSPROP_DATA:
            *(void **)v = event->data;
            break;

        default:
            BS_ReportError (BSERR_NO_SUCH_PARAM);
            return PR_FALSE;
            break;
            
    }
    
    return PR_TRUE;

}

PRBool
bs_event_get_uint_property (BSEventClass *event, BSEventProperty prop,
			    bsuint *v)

{
    return bs_event_set_property (event, prop, BSTYPE_UINT, v);
    
}

PRBool
bs_event_get_string_property (BSEventClass *event, BSEventProperty prop,
			      bschar **v)
{
    return bs_event_set_property (event, prop, BSTYPE_STRING, v);
    
}

PRBool
bs_event_get_bool_property (BSEventClass *event, BSEventProperty prop,
			    PRBool *v)
{
    return bs_event_set_property (event, prop, BSTYPE_BOOLEAN, v);
    
}

PRBool
bs_event_get_object_property (BSEventClass *event, BSEventProperty prop,
			      void **v)

{
    return bs_event_set_property (event, prop, BSTYPE_OBJECT, v);
    
}

PR_END_EXTERN_C
