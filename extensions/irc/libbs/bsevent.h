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

#ifndef bsevent_h___
#define bsevent_h___

#include "prtypes.h"

PR_BEGIN_EXTERN_C

/* constructor */
extern BSEventClass *
bs_event_new (BSObjectType obj_type, void *dest, BSHandlerID id, void *data,
              BSEventClass *previous, BSEventDeallocator *da);

/* 
 * alternate constructor.  makes a memcpy of *data, and automatically
 * frees it in bs_event_free
 */
extern BSEventClass *
bs_event_new_copy (BSObjectType obj_type, void *dest, BSHandlerID id,
		   void *data, bsuint length, BSEventClass *previous);

/* 
 * alternate constructor.  makes a copy of the null terminated string in
 * *data, and automatically frees it in bs_event_free
 */
extern BSEventClass *
bs_event_new_copyZ (BSObjectType obj_type, void *dest, BSHandlerID id,
		    bschar *data, BSEventClass *previous);

/* deallocator */
extern void
bs_event_free (BSEventClass *event);

extern PRBool
bs_event_route (BSEventClass *event, BSEventHookCallback *hook_func);

PR_END_EXTERN_C

#endif /* bsevent_h___ */
