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

/*   jssrules.h --- private style sheet routines
 */
#ifndef __JSSRULES_H_
#define __JSSRULES_H_

/*
 * A JSS rule consists of a list of selectors, a list of declarations,
 * and a specificity of its selector
 */
struct _StyleRule {
	StyleTag    tag;
	unsigned    nSelectors;
	StyleTag  **selectors;  /* array of pointers to StyleTag structures */
	StyleRule  *next;
};

/*
 * Specificity is implemented as three 8-bit components: the number of tags	in
 * the selector (the least significant component), the number of classes in the
 * selector, and the number of ids in the selector (the most significant component)
 */
#define JSS_SPECIFICITY(nTags, nClasses, nIDs)\
	((uint32)(nTags) | ((uint32)(nClasses)<<8) | ((uint32)(nIDs)<<16))

/*
 * This routine creates a new rule object and adds it to the list of existing
 * rules
 *
 * Returns the new rule if successful; 0 otherwise
 */
extern StyleRule *
jss_NewRule(JSContext *mc, unsigned nSelectors, jsval *selectors);

/* Destroys a list of rules */
extern void
jss_DestroyRules(StyleRule *);

/*
 * Given a list of rules and a contextual selector (array of simple
 * selectors), looks to see if the contextual selector matches that of an
 * existing rule
 */
extern StyleRule *
jss_LookupRule(JSContext *mc, StyleRule *rules, unsigned nSelectors, jsval *selectors);

/*
 * Callback function for jss_EnumApplicableRules(). Return MOCHA_TRUE to continuing
 * iterating and MOCHA_FALSE to stop
 */
typedef JSBool (* RULECALLBACK)(StyleTag *tag, void	*data);

/*
 * This routine takes as an argument a list of rules, a stack of open HTML
 * elements, and a callback function. The callback function is called once for
 * each rule that applies
 */
extern JSBool
jss_EnumApplicableRules(JSSContext		 *jc,
						StyleRule		 *rules,
						StyleAndTagStack *styleStack,
						RULECALLBACK	  callback,
						void		     *data);

#endif /* __JSSRULES_H_ */

