/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*   jsspriv.h --- private style sheet routines
 */
#ifndef __JSSPRIV_H_
#define __JSSPRIV_H_

#include "stystruc.h"
#include "stystack.h"

/*
 * Have the style sheet engine retrieve the list of style properties for
 * the current tag (tag at the top of the tag stack) and stores them into
 * the current style
 */
extern XP_Bool
jss_GetStyleForTopTag(StyleAndTagStack *styleStack);

typedef struct JSSContext {
	StyleObject *tags;
	StyleObject	*classes;
	StyleObject	*ids;
} JSSContext;

typedef struct _StyleProperty StyleProperty;

struct _StyleProperty {
	StyleProperty *next;
	char	   	  *name;
	PRWord		   tag;  /* one of the jsval tags */
	union {
		char   	 *strVal;
		jsint	  nVal;
		jsdouble  dVal;
		JSBool	  bVal;
	} u;
};

/* Destroys a list of properties */
extern void
jss_DestroyProperties(StyleProperty *);

typedef struct _StyleRule StyleRule;

typedef struct _StyleTag {
	char           *name;
	StyleProperty  *properties;
	uint32			specificity;
	StyleRule	   *rules;  /* list of contextual selectors */
} StyleTag;

/* Creates a new StyleTag structure */
extern StyleTag *
jss_NewTag(char *name);

/* Destroys a StyleTag structure */
extern void
jss_DestroyTag(StyleTag *);

#endif /* __JSSPRIV_H_ */

