/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; -*-  
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*******************************************************************************
                            S P O R T   M O D E L    
                              _____
                         ____/_____\____
                        /__o____o____o__\        __
                        \_______________/       (@@)/
                         /\_____|_____/\       x~[]~
             ~~~~~~~~~~~/~~~~~~~|~~~~~~~\~~~~~~~~/\~~~~~~~~~

    Advanced Technology Garbage Collector
    Copyright (c) 1997 Netscape Communications, Inc. All rights reserved.
    Recovered by: Warren Harris
*******************************************************************************/

#include "smstack.h"

#define SM_SEG_END(seg) \
    ((SMObject**)((char*)(seg) + (SM_STACK_SEG_PAGES * SM_PAGE_SIZE)))

SMObject*
sm_StackUnderflow(SMStack* stack)
{
    SMStackSegment* prev = NULL;
    SMStackSegment* next;
    if (stack->segment == NULL)
        return NULL;
    prev = stack->segment->header.prev;
    if (prev == NULL)
        return NULL;

    /* Introduce a lag in the segments deallocated. That way we don't 
       thrash as we push and pop across segment boundaries. */
    next = stack->segment->header.next;
    if (next) {
        sm_DestroyCluster(&sm_PageMgr, (SMPage*)next, SM_STACK_SEG_PAGES);
        stack->segment->header.next = NULL;
    }

    stack->segment = prev;
    stack->min = &prev->element;
    stack->max = SM_SEG_END(prev);
    stack->top = stack->max;
    return SM_STACK_POP0(stack);
}

PRBool
sm_StackOverflow(SMStack* stack, SMObject* obj)
{
    SMStackSegment* next = NULL;
    if (stack->segment) {
        next = stack->segment->header.next;
    }
    if (next == NULL) {
        next = (SMStackSegment*)sm_NewCluster(&sm_PageMgr, SM_STACK_SEG_PAGES);
        if (next == NULL) {
            stack->overflowCount++;
            return PR_FALSE;
        }
        next->header.prev = stack->segment;
        next->header.next = NULL;
    }
    stack->segment = next;
    stack->min = &next->element;
    stack->top = &next->element;
    stack->max = SM_SEG_END(next);
    SM_STACK_PUSH0(stack, obj);
    return PR_TRUE;
}

PRUword
sm_StackSize(SMStack* stack)
{
    PRUword size = 0;
    SMStackSegment* seg = stack->segment;
    if (seg) {
        while (seg->header.next != NULL) {
            seg = seg->header.next;
        }
        do {
            SMStackSegment* prev = seg->header.prev;
            size += (SM_STACK_SEG_PAGES * SM_PAGE_SIZE);
            seg = prev;
        } while (seg);
    }
    return size;
}

void
sm_DestroyStack(SMStack* stack)
{
    SMStackSegment* seg = stack->segment;
    if (seg) {
        while (seg->header.next != NULL) {
            seg = seg->header.next;
        }
        do {
            SMStackSegment* prev = seg->header.prev;
            sm_DestroyCluster(&sm_PageMgr, (SMPage*)seg, SM_STACK_SEG_PAGES);
            seg = prev;
        } while (seg);
    }
    SM_INIT_STACK(stack);
}

/******************************************************************************/
