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

#include "xp.h"
#include "libstyle.h"
#include "jsapi.h"
#include "xp_mcom.h"
#include "jsspriv.h"
#include "jssrules.h"

/*
 * Helper routine to determine whether the array of simple selectors matches
 * the selectors for the specified rule
 */
static JSBool
jss_IsSelectorEqual(JSContext *mc,
					StyleRule *rule,
					unsigned   nSelectors,
					jsval	  *selectors)
{
	unsigned	i;

	if (rule->nSelectors == nSelectors) {
		for (i = 0; i < rule->nSelectors; i++) {
			/* We expect each simple selector to be a JavaScript object */
			XP_ASSERT(JSVAL_IS_OBJECT(selectors[i]));
	
			/* The simple selectors match if they point to the same StyleTag structure */
			if (rule->selectors[i] != JS_GetPrivate(mc, (JSObject *)selectors[i]))
				return JS_FALSE;
		}

		return JS_TRUE;
	}
	
	return JS_FALSE;
}

/*
 * Given a list of rules and a contextual selector (array of simple
 * selectors), looks to see if the contextual selector matches an
 * existing rule
 *
 * Returns the rule if successful, and 0 otherwise
 */
StyleRule *
jss_LookupRule(JSContext *mc,
			   StyleRule *rules,
			   unsigned   nSelectors,
			   jsval     *selectors)
{
	while (rules) {
		if (jss_IsSelectorEqual(mc, rules, nSelectors, selectors))
			return rules;

		rules = rules->next;
	}

	return 0;
}

/* This routine creates and returns a new rule */
StyleRule *
jss_NewRule(JSContext *mc, unsigned argc, jsval *argv)
{
	StyleRule *rule;
	unsigned   i;

	/* Create a new rule */
	rule = (StyleRule *)XP_CALLOC(1, sizeof(StyleRule));
	if (!rule) {
		JS_ReportOutOfMemory(mc);
		return 0;
	}

	/* Initialize the list of selectors. While we're at it compute the specificity */
	rule->nSelectors = argc;
	rule->selectors = XP_CALLOC((size_t)rule->nSelectors, sizeof(StyleTag *));
	rule->tag.specificity = 0;

	for (i = 0; i < rule->nSelectors; i++) {
		/* We expect each simple selector to be a JavaScript object */
		XP_ASSERT(JSVAL_IS_OBJECT(argv[i]));

		rule->selectors[i] = JS_GetPrivate(mc, (JSObject *)argv[i]);
		if (!rule->selectors[i]) {
			XP_ASSERT(FALSE);
			XP_FREE(rule);
			return 0;
		}

		/* The specificity of the rule is just the sum of the specificity of each selector */
		rule->tag.specificity += rule->selectors[i]->specificity;
	}

	return rule;
}

/* Destroys a list of rules */
void
jss_DestroyRules(StyleRule *rules)
{
	StyleRule *next;

	while (rules) {
		next = rules->next;

		/* Destroy any properties associated with the tag structure */
		XP_ASSERT(rules->tag.name == 0);
		XP_ASSERT(rules->tag.rules == 0);
		jss_DestroyProperties(rules->tag.properties);

		/* Destroy the array of selectors */
		if (rules->selectors)
			XP_FREE(rules->selectors);

		XP_FREE(rules);
		rules = next;
	}
}

/*
 * Helper function to determine if a simple selector applies. index
 * represents where in the tag stack the search should begin
 *
 * Returns JS_TRUE if selector applies. In this case index will be set
 * to the tag stack entry where the match occured. Returns JS_FALSE if
 * the selector doesn't apply
 */
static  JSBool
jss_SelectorApplies(JSSContext 		 *jc,
					StyleTag		 *jsstag,
					StyleAndTagStack *styleStack,
					int32			 *index)
{
	TagStruct	*tag;

	for (tag = STYLESTACK_GetTagByIndex(styleStack, *index);
		 tag;
		 tag = STYLESTACK_GetTagByIndex(styleStack, ++(*index))) {

		/* We determine if the selector applies by comparing StyleTag pointers */
		if (tag->id && jc->ids && jc->ids->table) {
			if ((StyleTag *)PR_HashTableLookup(jc->ids->table, tag->id) == jsstag)
				return JS_TRUE;
		}

		if (tag->class_name && jc->classes && jc->classes->table) {
			StyleObject *classes = (StyleObject *)PR_HashTableLookup(jc->classes->table, tag->class_name);

			if (classes && classes->table) {
				/* Check against all elements of the class, e.g. classes.punk.all */
				if ((StyleTag *)PR_HashTableLookup(classes->table, "all") == jsstag)
					return JS_TRUE;

				/* Now check for the specified tag, e.g. classes.punk.H1 */
				XP_ASSERT(tag->name);
				if (tag->name && ((StyleTag *)PR_HashTableLookup(classes->table, tag->name) == jsstag))
					return JS_TRUE;
			}
		}

		XP_ASSERT(tag->name);
		if (tag->name && jc->tags && jc->tags->table) {
			if ((StyleTag *)PR_HashTableLookup(jc->tags->table, tag->name) == jsstag)
				return JS_TRUE;
		}
	}

	/* The selector didn't match against any element of the tag stack */
	return JS_FALSE;
}

/*
 * Helper function to determine if a contextual selector applies. Returns
 * JS_TRUE if the selector applies and JS_FALSE otherwise
 */
static JSBool
jss_RuleApplies(JSSContext *jc, StyleRule *rule, StyleAndTagStack *styleStack)
{
	int		i;
	int32	index = 0;

	/*
	 * Starting with the right-most selector, check each selector to see
	 * if it applies
	 */
	for (i = (int)rule->nSelectors - 1; i >= 0; i--) {
		if (!jss_SelectorApplies(jc, rule->selectors[i], styleStack, &index))
			return JS_FALSE;

		index++;  /* continue with the next HTML element */
	}

	return JS_TRUE;
}

/*
 * This routine takes as an argument a list of rules, a stack of open HTML
 * elements, and a callback function. The callback function is called once for
 * each rule that applies
 */
JSBool
jss_EnumApplicableRules(JSSContext		 *jc,
						StyleRule 		 *rules,
						StyleAndTagStack *styleStack,
						RULECALLBACK	  callback,
						void			 *data)
{
	while (rules) {
		if (jss_RuleApplies(jc, rules, styleStack))
			callback(&rules->tag, data);

		rules = rules->next;
	}

	return JS_TRUE;
}
