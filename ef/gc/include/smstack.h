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

#ifndef __SMSTACK__
#define __SMSTACK__

#include "smobj.h"

SM_BEGIN_EXTERN_C

/******************************************************************************/

typedef struct SMStackSegmentHeader {
    struct SMStackSegment*      prev;
    struct SMStackSegment*      next;
} SMStackSegmentHeader;

typedef struct SMStackSegment {
    SMStackSegmentHeader        header;
    SMObject*                   element;
    /* multiple elements follow */
} SMStackSegment;

#define SM_STACK_SEG_PAGES      1

typedef struct SMStack {
    SMStackSegment*             segment;
    SMObject**                  top;
    SMObject**                  min;
    SMObject**                  max;
    PRUword                     overflowCount;
} SMStack;

#define SM_INIT_STACK(stack)                 \
    ((stack)->segment = NULL,                \
     (stack)->top = NULL,                    \
     (stack)->min = NULL,                    \
     (stack)->max = NULL,                    \
     (stack)->overflowCount = 0)             \

#define SM_STACK_POP0(stack)                 \
    (*(--(stack)->top))                      \

#define SM_STACK_PUSH0(stack, obj)           \
    ((void)(*((stack)->top)++ = (obj)))      \

#define SM_STACK_POP(stack)                  \
    (((stack)->top > (stack)->min)           \
     ? SM_STACK_POP0(stack)                  \
     : SM_STACK_UNDERFLOW(stack))            \

#define SM_STACK_PUSH(stack, obj)            \
    (((stack)->top < (stack)->max)           \
     ? (SM_STACK_PUSH0(stack, obj), PR_TRUE) \
     : SM_STACK_OVERFLOW(stack, obj))        \

#define SM_STACK_UNDERFLOW(stack)       sm_StackUnderflow(stack)

#define SM_STACK_OVERFLOW(stack, obj)   sm_StackOverflow(stack, obj)

#define SM_STACK_IS_EMPTY(stack)               \
    ((stack)->segment == NULL                  \
     || ((stack)->segment->header.prev == NULL \
         && (stack)->top == (stack)->min))     \

extern SMObject*
sm_StackUnderflow(SMStack* stack);

extern PRBool
sm_StackOverflow(SMStack* stack, SMObject* obj);

extern PRUword
sm_StackSize(SMStack* stack);

extern void
sm_DestroyStack(SMStack* stack);

/******************************************************************************/

SM_END_EXTERN_C

#endif /* __SMSTACK__ */
/******************************************************************************/
