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

/* pmsgfilt.h - Private filter definitions */

/* Private API for filtering mail and news */

#ifndef _PMSGFILT_H
#define _PMSGFILT_H

#include "msg_filt.h"
#include "msg_opaq.h"
#include "pmsgsrch.h"

class XPStringObj;

typedef enum
{
	filterAttribNone,
	filterAttribVersion, 
	filterAttribLogging, 
	filterAttribName, 
	filterAttribEnabled, 
	filterAttribDescription, 
	filterAttribType,
	filterAttribScriptFile, 
	filterAttribAction, 
	filterAttribActionValue, 
	filterAttribCondition
}  FilterFileAttrib;


struct MSG_FilterList : public msg_OpaqueObject
{
public:
	MSG_FilterList ();
	virtual ~MSG_FilterList ();
public:
static  MSG_FilterError Open(MSG_Master *master, MSG_FilterType type, MSG_Pane *pane, MSG_FolderInfo *info, MSG_FilterList **filterList);
static  MSG_FilterError Open(MSG_Master *master, MSG_FilterType type, MSG_FilterList **filterList);
		MSG_FilterError Close();
		MSG_FilterError GetFilterCount(int32 *pCount);
		MSG_FilterError InsertFilterAt(MSG_FilterIndex filterIndex, MSG_Filter *filter);
		MSG_FilterError SetFilterAt(MSG_FilterIndex filterIndex, MSG_Filter *filter);
		MSG_FilterError GetFilterAt(MSG_FilterIndex filterIndex, MSG_Filter **filter);
		MSG_FilterError RemoveFilterAt(MSG_FilterIndex filterIndex);
		MSG_FilterError MoveFilterAt(MSG_FilterIndex filterIndex, MSG_FilterMotion);
		void			EnableLogging(XP_Bool enable) {m_loggingEnabled = enable;}
		XP_Bool			IsLoggingEnabled() {return m_loggingEnabled;}
		MSG_Master		*GetMaster() {return m_master;}
		int16			GetVersion() {return m_fileVersion;}
		MSG_FilterError	WriteIntAttr(XP_File fid, FilterFileAttrib attrib, int value);
		MSG_FilterError WriteStrAttr(XP_File fid, FilterFileAttrib attrib, const char *str);
		MSG_FilterError WriteBoolAttr(XP_File fid, FilterFileAttrib attrib, XP_Bool boolVal);
		MSG_FolderInfo  *GetFolderInfo() {return m_folderInfo;}
#ifdef DEBUG
		void Dump();
#endif
protected:
	virtual uint32		GetExpectedMagic ();
		MSG_FilterError SaveTextFilters(XP_File fid);
		// file streaming methods
		char			ReadChar(XP_File fid);
		XP_Bool			IsWhitespace(char ch);
		char			SkipWhitespace(XP_File fid);
		XP_Bool			StrToBool(const char *str);
		char			LoadAttrib(XP_File fid, FilterFileAttrib &attrib);
		const char		*GetStringForAttrib(FilterFileAttrib attrib);
		MSG_FilterError LoadValue(XP_File fid, XPStringObj &value);
		MSG_FilterError LoadTextFilters(XP_File fid);
		MSG_FilterError ParseCondition(const char *value);
		

	XPPtrArray			m_filters;
	static uint32		m_expectedMagic;
			int16		m_fileVersion;
			XP_Bool		m_loggingEnabled;
			MSG_Master	*m_master;
		MSG_FolderInfo	*m_folderInfo;
		MSG_Pane		*m_pane;
		MSG_Filter		*m_curFilter;	// filter we're filing in or out(?)
		const char		*m_filterFileName;
		XP_FileType		m_fileType;
};

struct MSG_Filter : public msg_OpaqueObject
{
	friend struct MSG_FilterList;
public:
	MSG_Filter (MSG_FilterType type, char *name);
	MSG_Filter ();
	virtual ~MSG_Filter ();

	MSG_FilterType	GetType() {return m_type;}
	void			SetType(MSG_FilterType type) {m_type = type;}
	void			SetEnabled(XP_Bool enabled) {m_enabled = enabled;}
	XP_Bool			GetEnabled() {return m_enabled;}
	MSG_FilterError	GetRule(MSG_Rule **);
	MSG_FilterError	GetName(char **name);
	MSG_FilterError	SetName(const char *name);
	MSG_FilterError GetFilterScript(char **name);
	MSG_FilterError SetFilterScript(const char *name);
	MSG_FilterError SetDescription(const char *desc);
	MSG_FilterError GetDescription(char **desc);
	void			SetFilterList(MSG_FilterList *filterList) {m_filterList = filterList;}
	XP_Bool			IsRule() 
						{return (m_type & (filterInboxRule | filterNewsRule)) != 0;}
	XP_Bool			IsScript() {return (m_type &
							(filterInboxJavaScript | filterNewsJavaScript)) != 0;}
	MSG_FilterError SaveToTextFile(XP_File fid);
	MSG_Master		*GetMaster() {return (m_filterList) ? m_filterList->GetMaster() : 0;}
	int16			GetVersion() {return (m_filterList) ? m_filterList->GetVersion() : 0;}
	MSG_FilterList	*GetFilterList() {return m_filterList;}
    void            SetDontFileMe(XP_Bool bDontFileMe) {m_dontFileMe = bDontFileMe;}
#ifdef DEBUG
	void	Dump();
#endif

protected:
	virtual uint32 GetExpectedMagic ();
	static uint32 m_expectedMagic;
	MSG_FilterType	m_type;
	XP_Bool			m_enabled;
	char			*m_filterName;
	char			*m_description;
	struct MSG_FilterList *m_filterList;	/* owning filter list */
	union
	{
		char		*m_scriptFileName;  /* if type == filterInboxJavaScript */
		MSG_Rule	*m_rule;			/* if type == filterInboxRule */
	} m_filter;
    XP_Bool         m_dontFileMe;
} ;

inline uint32 MSG_Filter::GetExpectedMagic ()
{ return m_expectedMagic; }


typedef struct MSG_RuleAction
{
        MSG_RuleActionType      m_type;
        union
        {
                MSG_PRIORITY m_priority;  /* priority to set rule to */
                char    *m_folderName;    /* Or some folder identifier, if such a thing is invented */
        } m_value;
        char	*m_originalServerPath;
} MSG_RuleAction;



struct MSG_Rule  : public msg_OpaqueObject
{
	friend struct MSG_FilterList;
public:
	MSG_Rule (MSG_Filter *);
	virtual ~MSG_Rule ();
	MSG_FilterError SaveToTextFile(XP_File fid);
	MSG_FilterError GetAction(MSG_RuleActionType *type, void **value);
	MSG_FilterError SetAction(MSG_RuleActionType type, void *value);
	MSG_FilterError AddTerm(
		MSG_SearchAttribute attrib,    /* attribute for this term                */
		MSG_SearchOperator op,         /* operator e.g. opContains               */
		MSG_SearchValue *value,
		XP_Bool booleanAND,            /* set to true if operator is AND        */
		char * arbiraryHeader);		   /* user specified arbitrary header string. ignored unless attrib = attribOtherHeader */
	MSG_FilterError GetNumTerms(int32 *numTerms);

	MSG_FilterError GetTerm(int32 termIndex,
		MSG_SearchAttribute *attrib,    /* attribute for this term                */
		MSG_SearchOperator *op,         /* operator e.g. opContains               */
		MSG_SearchValue *value,         /* value e.g. "Dogbert"                   */
		XP_Bool * booleanAND,			/* set to true if the operator is AND     */
		char ** arbitraryHeader);       /* user specified arbitrary header string. ignored unless attrib = attribOtherHeader */

	// this is for rule execution, not editing.
	MSG_SearchTermArray &GetTermList() {return m_termList;}
	MSG_Filter		*GetFilter() {return m_filter;}
static	void InitActionsTable();
static	MSG_FilterError GetActionMenuItems(
		MSG_FilterType type,
		MSG_RuleMenuItem *items,    /* array of caller-allocated structs       */
		uint16 *maxItems);    
static	char *GetActionStr(MSG_RuleActionType action);
static	const char *GetActionFilingStr(MSG_RuleActionType action);
static MSG_RuleActionType GetActionForFilingStr(const char *actionStr);
#ifdef DEBUG
		void Dump();
#endif
protected:
	MSG_FilterError		ConvertMoveToFolderValue(const char *relativePath);
		MSG_Filter		    *m_filter;		  /* owning filter */
        MSG_SearchTermArray m_termList;       /* linked list of criteria terms */
        MSG_ScopeTerm       *m_scope;         /* default for mail rules is inbox, but news rules could
have a newsgroup - LDAP would be invalid */
        MSG_RuleAction      m_action;
    
	virtual uint32 GetExpectedMagic ();
	static uint32 m_expectedMagic;
    static const char *kImapPrefix;
} ;

inline uint32 MSG_Rule::GetExpectedMagic ()
{ return m_expectedMagic; }


#endif /* _PMSGFILT_H */
