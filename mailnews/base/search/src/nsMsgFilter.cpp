/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

// this file implements the nsMsgFilterList interface 

#include "msgCore.h"
#include "nsMsgFilterList.h"
#include "nsMsgFilter.h"
#include "nsMsgUtils.h"

static const char *kImapPrefix = "//imap:";


nsMsgFilter::nsMsgFilter()
{
	m_filterList = nsnull;
	NS_INIT_REFCNT();
}

nsMsgFilter::~nsMsgFilter()
{
}

NS_IMPL_ADDREF(nsMsgFilter)
NS_IMPL_RELEASE(nsMsgFilter)

NS_IMETHODIMP nsMsgFilter::QueryInterface(REFNSIID aIID, void** aResult)
{   
    if (aResult == NULL)  
        return NS_ERROR_NULL_POINTER;  

    if (aIID.Equals(nsIMsgFilter::GetIID()) ||
        aIID.Equals(::nsISupports::GetIID()))
	{
        *aResult = NS_STATIC_CAST(nsMsgFilter*, this);   
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}   

NS_IMETHODIMP nsMsgFilter::GetFilterType(nsMsgFilterType *filterType)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::EnableFilter(PRBool enable)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::IsFilterEnabled(PRBool *enabled)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::GetFilterName(char **name)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::SetFilterName(char *name)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::GetFilterDesc(char **description)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::SetFilterDesc(char *description)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::AddTerm(     
	nsMsgSearchAttribute attrib,    /* attribute for this term                */
	nsMsgSearchOperator op,         /* operator e.g. opContains               */
	nsMsgSearchValue *value,        /* value e.g. "Dogbert"                   */
	PRBool BooleanAND, 	       /* TRUE if AND is the boolean operator. FALSE if OR is the boolean operators */
	char * arbitraryHeader)       /* arbitrary header specified by user. ignored unless attrib = attribOtherHeader */
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::GetNumTerms(PRInt32 *numTerms)
{
	return NS_OK;
}


NS_IMETHODIMP nsMsgFilter::GetTerm(PRInt32 termIndex, 
	nsMsgSearchAttribute *attrib,    /* attribute for this term                */
	nsMsgSearchOperator *op,         /* operator e.g. opContains               */
	nsMsgSearchValue *value,         /* value e.g. "Dogbert"                   */
	PRBool *BooleanAnd,				/* TRUE if AND is the boolean operator. FALSE if OR is the boolean operator */
	char ** arbitraryHeader)        /* arbitrary header specified by user. ignore unless attrib = attribOtherHeader */
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::SetScope(nsMsgScopeTerm *scope)
{
	return NS_OK;
}

NS_IMETHODIMP nsMsgFilter::GetScope(nsMsgScopeTerm **scope)
{
	return NS_OK;
}


/* if type is acChangePriority, value is a pointer to priority.
   If type is acMoveToFolder, value is pointer to folder name.
   Otherwise, value is ignored.
*/
NS_IMETHODIMP nsMsgFilter::SetAction(nsMsgRuleActionType type, void *value)
{
	return NS_OK;
}
NS_IMETHODIMP nsMsgFilter::GetAction(nsMsgRuleActionType *type, void **value) 
{
	return NS_OK;
}



void nsMsgFilter::SetFilterList(nsMsgFilterList *filterList)
{
	m_filterList = filterList;
}

nsresult		nsMsgFilter::GetName(nsString2 *name)
{
	if (!name)
		return NS_ERROR_NULL_POINTER;
	*name = m_filterName;
	return NS_OK;
}

nsresult		nsMsgFilter::SetName(nsString2 *name)
{
	if (!name)
		return NS_ERROR_NULL_POINTER;
	m_filterName = *name;
	return NS_OK;
}

nsresult		nsMsgFilter::SetDescription(nsString2 *desc)
{
	if (!desc)
		return NS_ERROR_NULL_POINTER;
	m_description = *desc;
	return NS_OK;
}

nsresult		nsMsgFilter::GetDescription(nsString2 *desc)
{
	if (!desc)
		return NS_ERROR_NULL_POINTER;
	*desc = m_description;
	return NS_OK;
}

void			nsMsgFilter::SetFilterScript(nsString2 *fileName) 
{
	m_scriptFileName = *fileName;
}

nsresult		nsMsgFilter::SaveToTextFile(nsIOFileStream *stream);

nsresult nsMsgFilter::ConvertMoveToFolderValue(nsString2 &relativePath)
{

	m_action.m_folderName = relativePath;
	return NS_OK;
	// set m_action.m_value.m_folderName
}

nsresult nsMsgFilter::SaveToTextFile(nsIOFileStream *stream)
{
	nsresult err = m_filterList->WriteStrAttr(nsMsgFilterAttribName, m_filterName);
	err = m_filterList->WriteBoolAttr(nsMsgFilterAttribEnabled, m_enabled);
	err = m_filterList->WriteStrAttr(nsMsgFilterAttribDescription, m_description);
	err = m_filterList->WriteIntAttr(nsMsgFilterAttribType, m_type);
	if (IsScript())
		err = m_filterList->WriteStrAttr(nsMsgFilterAttribScriptFile, m_scriptFileName);
	else
		err = SaveRule();
	return err;
}

nsresult nsMsgFilter::SaveRule()
{
	nsresult err = NS_OK;
	char			*relativePath = nsnull;
	nsMsgFilterList	*filterList = GetFilterList();
	nsString2	actionFilingStr(eOneByte);

	GetActionFilingStr(m_action.m_type, actionFilingStr);

	err = filterList->WriteStrAttr(nsMsgFilterAttribAction, actionFilingStr);
	if (!NS_SUCCEEDED(err))
		return err;
	switch(m_action.m_type)
	{
	case nsMsgFilterActionMoveToFolder:
		{
		nsString2 imapTargetString(kImapPrefix, eOneByte);
		imapTargetString += m_action.m_folderName;
		err = filterList->WriteStrAttr(nsMsgFilterAttribActionValue, imapTargetString);
		}
		break;
	case nsMsgFilterActionChangePriority:
		{
			nsString2 priority(eOneByte);
			NS_MsgGetUntranslatedPriorityName (m_action.m_priority, &priority);
			err = filterList->WriteStrAttr(nsMsgFilterAttribActionValue, priority);
		}
		break;
	default:
		break;
	}
	// and here the fun begins - file out term list...
	int searchIndex;
	nsString2  condition(eOneByte);
	for (searchIndex = 0; searchIndex < m_termList.Count() && NS_SUCCEEDED(err);
			searchIndex++)
	{
		nsString2	stream(eOneByte);

		nsMsgSearchTerm * term = (nsMsgSearchTerm *) m_termList.ElementAt(searchIndex);
		if (term == NULL)
			continue;
		
		if (condition.Length() > 1)
			condition += ' ';

		if (term->m_booleanOp == nsMsgSearchBooleanOR)
			condition += "OR (";
		else
			condition += "AND (";

		nsresult searchError = term->EnStreamNew(stream);
		if (!NS_SUCCEEDED(searchError))
		{
			err = searchError;
			break;
		}
		
		condition += stream;
		condition += ')';
	}
	if (NS_SUCCEEDED(err))
		err = filterList->WriteStrAttr(nsMsgFilterAttribCondition, condition);
	return err;
}

// for each action, this table encodes the filterTypes that support the action.
struct RuleActionsTableEntry
{
	nsMsgRuleActionType	action;
	nsMsgFilterType		supportedTypes;
	PRInt32				xp_strIndex;
	const char			*actionFilingStr;	/* used for filing out filters, don't translate! */
};

// Because HP_UX native C++ compiler can't initialize static objects with ints,
//  we can't initialize this structure directly, so we have to do it in two phases.
static struct RuleActionsTableEntry ruleActionsTable[] =
{
	{ nsMsgFilterActionMoveToFolder,	nsMsgFilterInbox,	0, /*XP_FILTER_MOVE_TO_FOLDER*/		"Move to folder" },
	{ nsMsgFilterActionChangePriority,	nsMsgFilterInbox,	0, /*XP_FILTER_CHANGE_PRIORITY*/	"Change priority"},
	{ nsMsgFilterActionDelete,			nsMsgFilterAll,		0, /*XP_FILTER_DELETE */			"Delete"},
	{ nsMsgFilterActionMarkRead,		nsMsgFilterAll,		0, /*XP_FILTER_MARK_READ */			"Mark read"},
	{ nsMsgFilterActionKillThread,		nsMsgFilterAll,		0, /*XP_FILTER_KILL_THREAD */		"Ignore thread"},
	{ nsMsgFilterActionWatchThread,		nsMsgFilterAll,		0, /*XP_FILTER_WATCH_THREAD */		"Watch thread"}
};

#ifdef FILTER_UI
/*static */char *MSG_Rule::GetActionStr(MSG_RuleActionType action)
{
	int	numActions = sizeof(ruleActionsTable) / sizeof(ruleActionsTable[0]);

	for (int i = 0; i < numActions; i++)
	{
		if (action == ruleActionsTable[i].action)
			return XP_GetString(ruleActionsTable[i].xp_strIndex);
	}
	return "";
}
#endif
/*static */nsresult nsMsgFilter::GetActionFilingStr(nsMsgRuleActionType action, nsString2 &actionStr)
{
	int	numActions = sizeof(ruleActionsTable) / sizeof(ruleActionsTable[0]);

	for (int i = 0; i < numActions; i++)
	{
		if (action == ruleActionsTable[i].action)
		{
			actionStr = ruleActionsTable[i].actionFilingStr;
			return NS_OK;
		}
	}
	return NS_ERROR_INVALID_ARG;
}


nsMsgRuleActionType nsMsgFilter::GetActionForFilingStr(nsString2 &actionStr)
{
	int	numActions = sizeof(ruleActionsTable) / sizeof(ruleActionsTable[0]);

	for (int i = 0; i < numActions; i++)
	{
		if (actionStr.Equals(ruleActionsTable[i].actionFilingStr))
			return ruleActionsTable[i].action;
	}
	return nsMsgFilterActionNone;
}

void nsMsgFilter::Dump()
{
	printf("filter %s type = %c desc = %s\n", m_filterName.GetBuffer(), m_type + '0', m_description.GetBuffer());
}

