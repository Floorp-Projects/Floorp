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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _nsIMsgFilter_H_
#define _nsIMsgFilter_H_

#include "nscore.h"
#include "nsISupports.h"
#include "nsMsgFilterCore.h"

// 605db0f8-04a1-11d3-a50a-0060b0fc04b7
#define NS_IMSGFILTER_IID                         \
{ 0x605db0f8, 0x04a1, 0x11d3,                 \
    { 0xa5, 0x0a, 0x0, 0x60, 0xb0, 0xfc, 0x04, 0xb7 } }

class nsIMsgFilter : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IMSGFILTER_IID; return iid; }

	NS_IMETHOD GetFilterType(nsMsgFilterType *filterType)= 0;
	NS_IMETHOD EnableFilter(XP_Bool enable)= 0;
	NS_IMETHOD IsFilterEnabled(PRBool *enabled)= 0;
	NS_IMETHOD GetFilterName(char **name)= 0;	
	NS_IMETHOD SetFilterName(char *name)= 0;
	NS_IMETHOD GetFilterDesc(char **description)= 0;
	NS_IMETHOD SetFilterDesc(char *description)= 0;

	NS_IMETHOD AddTerm(     
		nsMsgSearchAttribute attrib,    /* attribute for this term                */
		nsMsgSearchOperator op,         /* operator e.g. opContains               */
		nsMsgSearchValue *value,        /* value e.g. "Dogbert"                   */
		PRBool BooleanAND, 	       /* TRUE if AND is the boolean operator. FALSE if OR is the boolean operators */
		char * arbitraryHeader)= 0;       /* arbitrary header specified by user. ignored unless attrib = attribOtherHeader */

	NS_IMETHOD GetNumTerms(PRInt32 *numTerms)= 0;

	NS_IMETHOD GetTerm(PRInt32 termIndex, 
		nsMsgSearchAttribute *attrib,    /* attribute for this term                */
		nsMsgSearchOperator *op,         /* operator e.g. opContains               */
		nsMsgSearchValue *value,         /* value e.g. "Dogbert"                   */
		PRBool *BooleanAnd,				/* TRUE if AND is the boolean operator. FALSE if OR is the boolean operator */
		char ** arbitraryHeader)= 0;        /* arbitrary header specified by user. ignore unless attrib = attribOtherHeader */

	NS_IMETHOD SetScope(nsMsgScopeTerm *scope)= 0;
	NS_IMETHOD GetScope(nsMsgScopeTerm **scope)= 0;

	/* if type is acChangePriority, value is a pointer to priority.
	   If type is acMoveToFolder, value is pointer to folder name.
	   Otherwise, value is ignored.
	*/
	NS_IMETHOD SetAction(nsMsgRuleActionType type, void *value)= 0;
	NS_IMETHOD GetAction(nsMsgRuleActionType *type, void **value) = 0;

};

#endif
