/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*
	Burg.h
*/

// Burg needs these
#include <stdio.h>
#include <stdlib.h>

// Stuff that we use for burg
#include "CodeGenerator.h"
#include "Primitives.h"

typedef Primitive* NODEPTR_TYPE;

#define OP_LABEL(p)					p->getOperation()
#define STATE_LABEL(p)				p->getBurgState()
#define SET_STATE_LABEL(p, state)	p->setBurgState(state)
#define LEFT_CHILD(p)				CodeGenerator::getExpressionLeftChild(p)
#define RIGHT_CHILD(p)				CodeGenerator::getExpressionRightChild(p)	
#define PANIC						printf

extern short *burm_nts[];
extern char *burm_string[];
extern char * burm_opname[];
extern char burm_arity[];
extern short burm_cost[][4];
extern char *burm_ntname[];

// burgs exported functions

int burm_rule(int state, int goalnt);
int burm_state(int op, int l, int r);

int burm_label(NODEPTR_TYPE n);
NODEPTR_TYPE * burm_kids(NODEPTR_TYPE p, int rulenumber, NODEPTR_TYPE *kids);
NODEPTR_TYPE burm_child(NODEPTR_TYPE p, int index);
int burm_op_label(NODEPTR_TYPE p);
int burm_state_label(NODEPTR_TYPE p);

