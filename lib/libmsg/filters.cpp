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
#include "msg.h"
#include "msg_filt.h" /* public API */
#include "pmsgfilt.h" /* private API */
#include "pmsgsrch.h" /* private Search API */
#include "msgimap.h"
#include "msgprefs.h"
#include "xpgetstr.h"
#include "msgstrob.h"
#include "imaphost.h"

extern "C" 
{
	extern int XP_FILTER_MOVE_TO_FOLDER;
	extern int XP_FILTER_CHANGE_PRIORITY;
	extern int XP_FILTER_DELETE;
	extern int XP_FILTER_MARK_READ;
	extern int XP_FILTER_KILL_THREAD;
	extern int XP_FILTER_WATCH_THREAD;
}

const int16 kFileVersion = 6;
const int16 kFileVersionOldMoveTarget = 5;
#if defined(XP_WIN) || defined(XP_OS2)
const int16 kFileVersionAbsPath = 3;
#endif
const int16 kFileVersionOldStream = 2;

// This will return a zero length string ("") instead of NULL on
// empty strings. If this is a problem, we will need a way to
// distinguish memory allocation failures from empty strings.
static int XP_FileReadAllocStr(char **str, XP_File fid)
{
	int status;
	int length;

	if ((status = XP_FileRead(&length, sizeof(length), fid)) < 0)
		return status;
	*str = (char *) XP_ALLOC(length + 1);
	if (*str != NULL)
	{
		if (length > 0)
			status = XP_FileRead(*str, length, fid);
		(*str)[length] = '\0';
	}
	return status;
}

// ------------ FilterList methods ------------------
MSG_FilterError MSG_OpenFilterList(MSG_Master *master, MSG_FilterType type, MSG_FilterList **filterList)
{
	return MSG_FilterList::Open(master, type, NULL, NULL, filterList);
}

MSG_FilterError MSG_OpenFolderFilterList(MSG_Pane *pane, MSG_FolderInfo *folder, MSG_FilterType type, MSG_FilterList **filterList)
{
	return MSG_FilterList::Open(pane->GetMaster(), type, pane, folder, filterList);
}

MSG_FilterError MSG_OpenFolderFilterListFromMaster(MSG_Master *master, MSG_FolderInfo *folder, MSG_FilterType type, MSG_FilterList **filterList)
{
	return MSG_FilterList::Open(master, type, NULL, folder, filterList);
}

MSG_FilterError MSG_FilterList::Open(MSG_Master *master, MSG_FilterType type, MSG_FilterList **filterList)
{
	return MSG_FilterList::Open(master, type, NULL, NULL, filterList);
}
// static method to open a filter list from persistent storage
MSG_FilterError MSG_FilterList::Open(MSG_Master *master, MSG_FilterType type, MSG_Pane *pane, MSG_FolderInfo *folderInfo, MSG_FilterList **filterList)
{
	MSG_FilterError	err = FilterError_Success;
	XP_File			fid;
	XP_StatStruct	outStat;
	MSG_FilterList	*newFilterList;

	if (type != filterInbox
		&& type != filterNews)
		return FilterError_InvalidFilterType;

	if (NULL == filterList)
		return FilterError_NullPointer;

	newFilterList = new MSG_FilterList;
	if (newFilterList == NULL)
		return FilterError_OutOfMemory;

	newFilterList->m_master = master;

	// hack up support for news filters by checking the current folder of the pane and massaging input params.
	if (pane != NULL && folderInfo == NULL)
	{
		folderInfo = pane->GetFolder();
		if (folderInfo)
		{
			if (folderInfo->IsNews())
				type = filterNews;
		}
	}

	newFilterList->m_folderInfo = folderInfo;
	newFilterList->m_pane = pane;

	*filterList = newFilterList;
	const char *upgradeIMAPFiltersDestFileName = 0;

	if (type == filterNews)
	{
		MSG_FolderInfoNews *newsFolder = folderInfo->GetNewsFolderInfo();
		if (newsFolder)
			newFilterList->m_filterFileName = newsFolder->GetXPRuleFileName();
		newFilterList->m_fileType = xpNewsSort;
	}
	else
	{
		MSG_IMAPFolderInfoMail *imapMailFolder = (folderInfo) ? folderInfo->GetIMAPFolderInfoMail() : (MSG_IMAPFolderInfoMail *)NULL;

		newFilterList->m_filterFileName = "";
		newFilterList->m_fileType = xpMailSort;
		if (imapMailFolder)
		{
			MSG_IMAPHost *defaultHost = imapMailFolder->GetMaster()->GetIMAPHostTable()->GetDefaultHost();
			if (imapMailFolder->GetIMAPHost() == defaultHost)
			{
				XP_Bool defaultHostFiltersExist = !XP_Stat(imapMailFolder->GetIMAPHost()->GetHostName(), &outStat, newFilterList->m_fileType);
				if (!defaultHostFiltersExist)
					upgradeIMAPFiltersDestFileName = imapMailFolder->GetIMAPHost()->GetHostName();
			}

			if (!upgradeIMAPFiltersDestFileName)
				newFilterList->m_filterFileName = imapMailFolder->GetIMAPHost()->GetHostName();
		}

	}

	if (XP_Stat(newFilterList->m_filterFileName, &outStat, newFilterList->m_fileType))
	{
		// file must not exist - no rules, we're done.
		return FilterError_Success;
	}
	fid = XP_FileOpen(newFilterList->m_filterFileName, newFilterList->m_fileType, XP_FILE_READ_BIN);
	if (fid) 
	{
		err = newFilterList->LoadTextFilters(fid);
		XP_FileClose(fid);
		// if the file version changed, save it out right away.
		if (newFilterList->GetVersion() != kFileVersion || upgradeIMAPFiltersDestFileName)
		{
			if (upgradeIMAPFiltersDestFileName)
				newFilterList->m_filterFileName = upgradeIMAPFiltersDestFileName;
			newFilterList->Close();
		}
	}
	else
	{
		err = FilterError_FileError;
	}

	return err;
}

extern "C" MSG_FolderInfo *MSG_GetFolderInfoForFilterList(MSG_FilterList *filterList)
{
	return filterList ? filterList->GetFolderInfo() : (MSG_FolderInfo *)NULL;
}

typedef struct
{
	FilterFileAttrib	attrib;
	const char			*attribName;
} FilterFileAttribEntry;

FilterFileAttribEntry FilterFileAttribTable[] =
{
	{filterAttribNone,			""},
	{filterAttribVersion,		"version"},
	{filterAttribLogging,		"logging"},
	{filterAttribName,			"name"},
	{filterAttribEnabled,		"enabled"},
	{filterAttribDescription,	"description"},
	{filterAttribType,			"type"},
	{filterAttribScriptFile,	"scriptName"},
	{filterAttribAction,		"action"},
	{filterAttribActionValue,	"actionValue"},
	{filterAttribCondition,		"condition"}
};

// If we want to buffer file IO, wrap it in here.
char MSG_FilterList::ReadChar(XP_File fid)
{
	char	newChar;
	int status = XP_FileRead(&newChar, sizeof(newChar), fid);
	return (status > 0) ? newChar : (char) -1;
}

XP_Bool MSG_FilterList::IsWhitespace(char ch)
{
	return (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t');
}

char MSG_FilterList::SkipWhitespace(XP_File fid)
{
	char ch;
	do
	{
		ch = ReadChar(fid);
	} while (IsWhitespace(ch));
	return ch;
}

XP_Bool MSG_FilterList::StrToBool(const char *str)
{
	return !XP_STRCASECMP(str, "yes");
}

char MSG_FilterList::LoadAttrib(XP_File fid, FilterFileAttrib &attrib)
{
	char	attribStr[100];
	char	curChar;
	
	curChar = SkipWhitespace(fid);
	int i;
	for (i = 0; i + 1 < sizeof(attribStr); )
	{
		if (curChar == (char) -1 || IsWhitespace(curChar) || curChar == '=')
			break;
		attribStr[i++] = curChar;
		curChar = ReadChar(fid);
	}
	attribStr[i] = '\0';
	for (int tableIndex = 0; tableIndex < sizeof(FilterFileAttribTable) / sizeof(FilterFileAttribTable[0]); tableIndex++)
	{
		if (!XP_STRCASECMP(attribStr, FilterFileAttribTable[tableIndex].attribName))
		{
			attrib = FilterFileAttribTable[tableIndex].attrib;
			break;
		}
	}
	return curChar;
}

const char *MSG_FilterList::GetStringForAttrib(FilterFileAttrib attrib)
{
	for (int tableIndex = 0; tableIndex < sizeof(FilterFileAttribTable) / sizeof(FilterFileAttribTable[0]); tableIndex++)
	{
		if (attrib == FilterFileAttribTable[tableIndex].attrib)
			return FilterFileAttribTable[tableIndex].attribName;
	}
	return NULL;
}

MSG_FilterError MSG_FilterList::LoadValue(XP_File fid, XPStringObj &value)
{
	char	valueStr[100];
	char	curChar;
	value = "";
	curChar = SkipWhitespace(fid);
	if (curChar != '"')
	{
		XP_ASSERT(FALSE);
		return FilterError_FileError;
	}
	curChar = ReadChar(fid);
	for (int i = 0; i + 2 < sizeof(valueStr); )
	{
		if (curChar == '\\')
		{
			char nextChar = ReadChar(fid);
			if (nextChar == '"')
				curChar = '"';
			else if (nextChar == '\\')	// replace "\\" with "\"
			{
				curChar = ReadChar(fid);
			}
			else
			{
				valueStr[i++] = curChar;
				curChar = nextChar;
			}
		}
		else
		{
			if (curChar == (char) -1 || curChar == '"' || curChar == '\n' || curChar == '\r')
			{
				valueStr[i] = '\0';
				if (XP_STRLEN(value))
				    value += (const char *)valueStr;
				else
				    value = (const char *) valueStr;
				break;
			}
		}
		valueStr[i++] = curChar;
		curChar = ReadChar(fid);
		if (i + 2 >= sizeof(valueStr))
		{
			valueStr[i] = '\0';
			if (XP_STRLEN(value))
				value += (const char *)valueStr;
			else
				value = (const char *) valueStr;
			i = 0;
		}
	}
	return FilterError_Success;
}

MSG_FilterError MSG_FilterList::LoadTextFilters(XP_File fid)
{
	MSG_FilterError	err = FilterError_Success;
	FilterFileAttrib attrib;

	// We'd really like to move lot's of these into the objects that they refer to.
	XP_FileSeek(fid, 0, SEEK_SET);
	do 
	{
		XPStringObj	value;
		char curChar = LoadAttrib(fid, attrib);
		if (attrib == filterAttribNone)
			break;
		err = LoadValue(fid, value);
		if (err != FilterError_Success)
			break;
		switch(attrib)
		{
		case filterAttribNone:
			break;
		case filterAttribVersion:
			m_fileVersion = XP_ATOI(value);
			break;
		case filterAttribLogging:
			m_loggingEnabled = StrToBool(value);
			break;
		case filterAttribName:
		{
			MSG_Filter *filter = new MSG_Filter;
			if (filter == NULL)
			{
				err = FilterError_OutOfMemory;
				break;
			}
			filter->SetFilterList(this);
			filter->SetName(value);
			m_curFilter = filter;
			m_filters.Add(filter);
		}
			break;
		case filterAttribEnabled:
			if (m_curFilter)
				m_curFilter->SetEnabled(StrToBool(value));
			break;
		case filterAttribDescription:
			if (m_curFilter)
				m_curFilter->SetDescription(value);
			break;
		case filterAttribType:
			if (m_curFilter)
			{
				m_curFilter->SetType((MSG_FilterType) XP_ATOI(value));
				if (!m_curFilter->IsScript())
				{
					m_curFilter->m_filter.m_rule = new MSG_Rule(m_curFilter);
					if (m_curFilter->m_filter.m_rule == NULL)
						err = FilterError_OutOfMemory;
				}
			}
			break;
		case filterAttribScriptFile:
			if (m_curFilter)
				m_curFilter->SetFilterScript(value);
			break;
		case filterAttribAction:
			m_curFilter->m_filter.m_rule->m_action.m_type = MSG_Rule::GetActionForFilingStr(value);
			break;
		case filterAttribActionValue:
			if (m_curFilter->m_filter.m_rule->m_action.m_type == acMoveToFolder)
				err = m_curFilter->m_filter.m_rule->ConvertMoveToFolderValue((const char *) value);
			else if (m_curFilter->m_filter.m_rule->m_action.m_type == acChangePriority)
				m_curFilter->m_filter.m_rule->SetAction(m_curFilter->m_filter.m_rule->m_action.m_type, (void *) (int32) MSG_GetPriorityFromString(value));
			break;
		case filterAttribCondition:
			err = ParseCondition(value);
			break;
		}
	} while (attrib != filterAttribNone);
	return err;
}

// parse condition like "(subject, contains, fred) AND (body, isn't, "foo)")"
// values with close parens will be quoted.
// what about values with close parens and quotes? e.g., (body, isn't, "foo")")
// I guess interior quotes will need to be escaped - ("foo\")")
// which will get written out as (\"foo\\")\") and read in as ("foo\")"
MSG_FilterError MSG_FilterList::ParseCondition(const char *value)
{
	XP_Bool	done = FALSE;
	MSG_FilterError	err = FilterError_Success;
	const char *curPtr = value;
	while (!done)
	{
		// insert code to save the boolean operator if there is one for this search term....
		const char *openParen = XP_STRCHR(curPtr, '(');
		const char *orTermPos = XP_STRCHR(curPtr, 'O');		// determine if an "OR" appears before the openParen...
		XP_Bool ANDTerm = TRUE;
		if (orTermPos && orTermPos < openParen) // make sure OR term falls before the '('
			ANDTerm = FALSE;

		char *termDup = NULL;
		if (openParen)
		{
			XP_Bool foundEndTerm = FALSE;
			XP_Bool inQuote = FALSE;
			for (curPtr = openParen +1; *curPtr; curPtr++)
			{
				if (*curPtr == '\\' && *(curPtr + 1) == '"')
					curPtr++;
				else if (*curPtr == ')' && !inQuote)
				{
					foundEndTerm = TRUE;
					break;
				}
				else if (*curPtr == '"')
					inQuote = !inQuote;
			}
			if (foundEndTerm)
			{
				int termLen = curPtr - openParen - 1;
				termDup = (char *) XP_ALLOC(termLen + 1);
				if (termDup)
					XP_STRNCPY_SAFE(termDup, openParen + 1, termLen + 1);
				else
				{
					err = FilterError_OutOfMemory;
					break;
				}
			}
		}
		else
			break;
		if (termDup)
		{
			MSG_SearchTerm	*newTerm = new MSG_SearchTerm;
			if (newTerm)
				newTerm->m_booleanOp = ANDTerm ? MSG_SearchBooleanAND : MSG_SearchBooleanOR;
			if (newTerm->DeStreamNew(termDup, XP_STRLEN(termDup)) == SearchError_Success)
				m_curFilter->m_filter.m_rule->GetTermList().Add(newTerm);
			FREEIF(termDup);
		}
		else
			break;
	}
	return err;
}

MSG_FilterError MSG_FilterList::WriteIntAttr(XP_File fid, FilterFileAttrib attrib, int value)
{
	const char *attribStr = GetStringForAttrib(attrib);
	if (attribStr)
		XP_FilePrintf(fid, "%s=\"%d\"%s", attribStr, value, LINEBREAK);
	return FilterError_Success;
}

MSG_FilterError MSG_FilterList::WriteStrAttr(XP_File fid, FilterFileAttrib attrib, const char *str)
{
	char *escapedStr = NULL;
	if (XP_STRCHR(str, '"'))
		escapedStr = MSG_SearchTerm::EscapeQuotesInStr(str);

	const char *attribStr = GetStringForAttrib(attrib);
	if (attribStr)
		XP_FilePrintf(fid, "%s=\"%s\"%s", attribStr, (escapedStr) ? escapedStr : str, LINEBREAK);
	FREEIF(escapedStr);
	return FilterError_Success;
}

MSG_FilterError MSG_FilterList::WriteBoolAttr(XP_File fid, FilterFileAttrib attrib, XP_Bool boolVal)
{
	return WriteStrAttr(fid, attrib, (boolVal) ? "yes" : "no");
}

MSG_FilterError MSG_FilterList::SaveTextFilters(XP_File fid)
{
	MSG_FilterError	err = FilterError_Success;
	const char *attribStr;
	int32			filterCount = m_filters.GetSize();

	attribStr = GetStringForAttrib(filterAttribVersion);
	err = WriteIntAttr(fid, filterAttribVersion, kFileVersion);
	err = WriteBoolAttr(fid, filterAttribLogging, m_loggingEnabled);
	for (int i = 0; i < filterCount; i ++)
	{
		MSG_Filter *filter;
		if (GetFilterAt(i, &filter) == FilterError_Success && filter != NULL)
		{
			filter->SetFilterList(this);
			if ((err = filter->SaveToTextFile(fid)) != FilterError_Success)
				break;
		}
		else
			break;
	}
	return err;
}

MSG_FilterList::MSG_FilterList() : msg_OpaqueObject (m_expectedMagic)
{
	m_loggingEnabled = FALSE;
	m_master = NULL;
	m_curFilter = NULL;
}

MSG_FilterList::~MSG_FilterList()
{
	for (int32 i = 0; i < m_filters.GetSize(); i++)
	{
		MSG_Filter *filter;
		if (GetFilterAt(i, &filter) == FilterError_Success)
			delete filter;
	}
}

uint32 MSG_FilterList::GetExpectedMagic ()
{
	return m_expectedMagic; 
}

// What should we do about file errors here? If we blow away the
// filter list even when we have an error, we can't let the user
// correct the problem. But if we don't blow it away, we'll have
// memory leaks.
MSG_FilterError MSG_CloseFilterList(MSG_FilterList *filterList)
{
	MSG_FilterError err;

	if (filterList == NULL)
		return FilterError_NullPointer;

	err = filterList->Close();
	if (err == FilterError_Success)
		delete filterList;
	return err;
}

MSG_FilterError MSG_CancelFilterList(MSG_FilterList *filterList)
{
	if (filterList == NULL)
		return FilterError_NullPointer;

	delete filterList;
	return FilterError_Success;
}

MSG_FilterError	MSG_SaveFilterList(MSG_FilterList *filterList)
{
	if (filterList == NULL)
		return FilterError_NullPointer;

	return filterList->Close();
}

MSG_FilterError MSG_FilterList::Close()
{
	MSG_FilterError err = FilterError_FileError;
	XP_File			fid;
	XP_FileType		retType;
	const char		*finalName = m_filterFileName;
	char			*tmpName = (finalName) ? FE_GetTempFileFor(NULL, finalName, m_fileType, &retType) : (char *)NULL;


	if (!tmpName || !finalName) 
		return FilterError_OutOfMemory;
	fid = XP_FileOpen(tmpName, xpTemporary,
								 XP_FILE_TRUNCATE_BIN);
	if (fid) 
	{
		err = SaveTextFilters(fid);
		XP_FileClose(fid);
		if (err == FilterError_Success)
		{
			int status = XP_FileRename(tmpName, xpTemporary, finalName, m_fileType);
			XP_ASSERT(status >= 0);
		}
	}

	FREEIF(tmpName);
	return err;
}

MSG_FilterError MSG_GetFilterCount(MSG_FilterList *filterList, int32 *pCount)
{
	return filterList->GetFilterCount(pCount);
}


MSG_FilterError MSG_FilterList::GetFilterCount(int32 *pCount)
{
	if (pCount == NULL)
		return FilterError_NullPointer;
	*pCount = m_filters.GetSize();
	return FilterError_Success;
}

MSG_FilterError MSG_GetFilterAt(MSG_FilterList *filterList, MSG_FilterIndex filterIndex, MSG_Filter **filter)
{
	return filterList->GetFilterAt(filterIndex, filter);
}

MSG_FilterError MSG_FilterList::GetFilterAt(MSG_FilterIndex filterIndex, MSG_Filter **filter)
{
	if (!m_filters.IsValidIndex(filterIndex))
		return FilterError_InvalidIndex;
	if (filter == NULL)
		return FilterError_NullPointer;
	*filter = (MSG_Filter *) m_filters.GetAt(filterIndex);
	return FilterError_Success;
}

MSG_FilterError MSG_SetFilterAt(MSG_FilterList *filterList, MSG_FilterIndex filterIndex,
								MSG_Filter *filter)
{
	return filterList->SetFilterAt(filterIndex, filter);
}

MSG_FilterError MSG_FilterList::SetFilterAt(MSG_FilterIndex filterIndex, MSG_Filter *filter)
{
	m_filters.SetAtGrow(filterIndex, filter);
	return FilterError_Success;
}

MSG_FilterError MSG_RemoveFilterAt(MSG_FilterList *filterList, MSG_FilterIndex filterIndex)
{
	return filterList->RemoveFilterAt(filterIndex);
}

MSG_FilterError MSG_FilterList::RemoveFilterAt(MSG_FilterIndex filterIndex)
{
	m_filters.RemoveAt(filterIndex, 1);
	return FilterError_Success;
}

MSG_FilterError MSG_InsertFilterAt(MSG_FilterList *filterList, 
							MSG_FilterIndex filterIndex, MSG_Filter *filter)
{
	return filterList->InsertFilterAt(filterIndex, filter);
}


MSG_FilterError MSG_FilterList::InsertFilterAt(MSG_FilterIndex filterIndex, MSG_Filter *filter)
{
	m_filters.InsertAt(filterIndex, filter);
	return FilterError_Success;
}


MSG_FilterError MSG_MoveFilterAt(MSG_FilterList *filterList, 
								 MSG_FilterIndex filterIndex, MSG_FilterMotion motion)
{
	return filterList->MoveFilterAt(filterIndex, motion);
}

// Attempt to move the filter at index filterIndex in the specified direction.
// If motion not possible in that direction, we still return success.
// We could return an error if the FE's want to beep or something.
MSG_FilterError MSG_FilterList::MoveFilterAt(MSG_FilterIndex filterIndex, 
										 MSG_FilterMotion motion)
{
	MSG_Filter	*tempFilter;

	if (!m_filters.IsValidIndex(filterIndex))
		return FilterError_InvalidIndex;

	tempFilter = (MSG_Filter *) m_filters.GetAt(filterIndex);
	if (motion == filterUp)
	{
		if (filterIndex == 0)
			return FilterError_Success;
		m_filters.SetAt(filterIndex, m_filters.GetAt(filterIndex - 1));
		m_filters.SetAt(filterIndex - 1, tempFilter);
	}
	else if (motion == filterDown)
	{
		if (filterIndex + 1 > (MSG_FilterIndex) (m_filters.GetSize() - 1))
			return FilterError_Success;
		m_filters.SetAt(filterIndex, m_filters.GetAt(filterIndex + 1));
		m_filters.SetAt(filterIndex + 1, tempFilter);
	}
	else
	{
		return FilterError_InvalidMotion;
	}
	return FilterError_Success;
}

MSG_FilterError MSG_EnableLogging(MSG_FilterList *filterList, XP_Bool enable)
{
	if (filterList == NULL)
		return FilterError_NullPointer;
	filterList->EnableLogging(enable);
	return FilterError_Success;
}

XP_Bool			MSG_IsLoggingEnabled(MSG_FilterList *filterList)
{
	if (filterList == NULL)
		return FALSE;
	return filterList->IsLoggingEnabled();
}

#ifdef DEBUG
void MSG_FilterList::Dump()
{
	XP_Trace("%d filters\n", m_filters.GetSize());

	for (int32 i = 0; i < m_filters.GetSize(); i++)
	{
		MSG_Filter *filter;
		if (GetFilterAt(i, &filter) == FilterError_Success)
			filter->Dump();
	}

}
#endif

// ------------ End FilterList methods ------------------

// ------------ MSG_Filter methods ----------------------
MSG_Filter::MSG_Filter (MSG_FilterType type, char *name) : msg_OpaqueObject (m_expectedMagic)
{
	m_filterName = XP_STRDUP(name);
	m_type = type;
	m_description = NULL;
	m_enabled = FALSE;
	if (IsRule())
		m_filter.m_rule = new MSG_Rule(this);
	else
		m_filter.m_rule = NULL;
	m_dontFileMe = FALSE;
}

MSG_Filter::MSG_Filter()  : msg_OpaqueObject (m_expectedMagic)
{
	m_filterName = NULL;
	m_description = NULL;
	m_dontFileMe = FALSE;
}

MSG_Filter::~MSG_Filter () 
{
	FREEIF(m_filterName);
	FREEIF(m_description);
	if (IsRule())
		delete m_filter.m_rule;
}

MSG_FilterError MSG_CreateFilter (MSG_FilterType type,	char *name,	MSG_Filter **result)
{
	if (NULL == result  || NULL == name)
		return FilterError_NullPointer;

	*result = new MSG_Filter(type, name);
	if (*result == NULL)
		return FilterError_OutOfMemory;
	return FilterError_Success;
}			

MSG_FilterError MSG_DestroyFilter(MSG_Filter *filter)
{
	if (NULL == filter)
		return FilterError_NullPointer;
	delete filter;
	return FilterError_Success;
}

MSG_FilterError MSG_Filter::SaveToTextFile(XP_File fid)
{
	MSG_FilterError	err;

	err = m_filterList->WriteStrAttr(fid, filterAttribName, m_filterName);
	err = m_filterList->WriteBoolAttr(fid, filterAttribEnabled, m_enabled);
	err = m_filterList->WriteStrAttr(fid, filterAttribDescription, m_description);
	err = m_filterList->WriteIntAttr(fid, filterAttribType, m_type);
	if (IsScript())
		err = m_filterList->WriteStrAttr(fid, filterAttribScriptFile, m_filter.m_scriptFileName);
	else
		err = m_filter.m_rule->SaveToTextFile(fid);
	return err;
}

MSG_FilterError MSG_GetFilterType(MSG_Filter *filter, MSG_FilterType *filterType)
{
	if (NULL == filterType || NULL == filter)
		return FilterError_NullPointer;
	*filterType = filter->GetType();
	return FilterError_Success;
}

MSG_FilterError MSG_EnableFilter(MSG_Filter *filter, XP_Bool enable)
{
	if (NULL == filter)
		return FilterError_NullPointer;
	filter->SetEnabled(enable);
	return FilterError_Success;
}

MSG_FilterError MSG_IsFilterEnabled(MSG_Filter *filter, XP_Bool *enabled)
{
	if (NULL == filter || NULL == enabled)
		return FilterError_NullPointer;

	*enabled = filter->GetEnabled();
	return FilterError_Success;
}

MSG_FilterError MSG_GetFilterRule(MSG_Filter *filter, MSG_Rule ** result)
{
	if (NULL == filter || NULL == result)
		return FilterError_NullPointer;
	return filter->GetRule(result);
}

MSG_FilterError MSG_Filter::GetRule(MSG_Rule **result)
{
	if (!(m_type & (filterInboxRule | filterNewsRule)))
		return FilterError_NotRule;
	*result = m_filter.m_rule;
	return FilterError_Success;
}

MSG_FilterError MSG_GetFilterName(MSG_Filter *filter, char **name)
{	
	return filter->GetName(name);
}

MSG_FilterError MSG_Filter::GetName(char **name)
{
	*name = m_filterName;
	return FilterError_Success;
}

MSG_FilterError MSG_GetFilterScript(MSG_Filter *filter, char **name)
{
	if (NULL == filter || NULL == name)
		return FilterError_NullPointer;
	return filter->GetFilterScript(name);
}

MSG_FilterError MSG_Filter::GetFilterScript(char **name)
{
	if (!(m_type & (filterInboxJavaScript | filterNewsJavaScript)))
		return FilterError_NotScript;
	*name = m_filter.m_scriptFileName;
	return FilterError_Success;
}

MSG_FilterError MSG_SetFilterScript(MSG_Filter *filter, const char *name)
{
	if (NULL == filter || NULL == name)
		return FilterError_NullPointer;

	return filter->SetFilterScript(name);
}

MSG_FilterError MSG_Filter::SetFilterScript(const char *name)
{
	m_filter.m_scriptFileName = XP_STRDUP(name);
	return FilterError_Success;
}

MSG_FilterError MSG_SetFilterDesc(MSG_Filter *filter, const char *desc)
{
	if (NULL == filter)
		return FilterError_NullPointer;
	return filter->SetDescription(desc);
}

MSG_FilterError MSG_SetFilterName(MSG_Filter *filter, const char *name)
{
	if (NULL == filter)
		return FilterError_NullPointer;
	return filter->SetName(name);
}

MSG_FilterError MSG_Filter::SetDescription(const char *desc)
{
	FREEIF(m_description);
	m_description = XP_STRDUP(desc);
	return FilterError_Success;
}

MSG_FilterError MSG_Filter::SetName(const char *name)
{
	FREEIF(m_filterName);
	m_filterName = XP_STRDUP(name);
	return FilterError_Success;
}

MSG_FilterError MSG_GetFilterDesc(MSG_Filter *filter, char **desc)
{	
	return filter->GetDescription(desc);
}

MSG_FilterError MSG_Filter::GetDescription(char **desc)
{
	*desc = m_description;
	return FilterError_Success;
}

#ifdef DEBUG
void MSG_Filter::Dump()
{
	XP_Trace("filter %s type %d enabled %s\n", m_filterName, m_type, 
		m_enabled ? "TRUE" : "FALSE");
	XP_Trace("desc = %s\n", m_description);
	if (IsRule())
	{
		m_filter.m_rule->Dump();
	}
	else
	{
		XP_Trace("script file = %s\n", m_filter.m_scriptFileName);
	}

}
#endif

// ------------ End MSG_Filter methods ------------------
MSG_Rule::MSG_Rule(MSG_Filter *filter) : msg_OpaqueObject(m_expectedMagic)
{
	m_scope = NULL;
	m_action.m_type = acNone;
	m_action.m_originalServerPath = NULL;
	m_filter = filter;
}

const char *MSG_Rule::kImapPrefix = "//imap:";

MSG_Rule::~MSG_Rule()
{
	for (int i = 0; i < m_termList.GetSize(); i++)
	{
		MSG_SearchTerm * term = (MSG_SearchTerm *) m_termList.GetAt(i);
		if (term == NULL)
			continue;
		delete term;
	}
	if (m_scope != NULL)
		delete m_scope;
	if (acMoveToFolder == m_action.m_type)
	{
		FREEIF(m_action.m_value.m_folderName);
		FREEIF(m_action.m_originalServerPath);
	}
}
MSG_FilterError MSG_Rule::SaveToTextFile(XP_File fid)
{
	MSG_FilterError	err = FilterError_Success;
	MSG_Master		*master = GetFilter()->GetMaster();
	char			*relativePath = NULL;
	const char		*folderDirectory;
	MSG_FilterList	*filterList = m_filter->GetFilterList();

	err = filterList->WriteStrAttr(fid, filterAttribAction, GetActionFilingStr(m_action.m_type));
	if (err != FilterError_Success)
		return err;
	switch(m_action.m_type)
	{
	case acMoveToFolder:
		folderDirectory = master->GetPrefs()->GetFolderDirectory();
		// if path starts off with folder directory, strip it off.
		if (!XP_STRNCASECMP(m_action.m_value.m_folderName, folderDirectory, XP_STRLEN(folderDirectory)))
		{
			relativePath = m_action.m_value.m_folderName + XP_STRLEN(folderDirectory);
			if (*relativePath == '/')
				relativePath++;
			err = filterList->WriteStrAttr(fid, filterAttribActionValue, relativePath);
		}
		else
		{
			if (!m_action.m_originalServerPath && m_action.m_value.m_folderName)
			{
				// attempt to convert full path to server path
				MSG_IMAPFolderInfoMail *imapMailFolder = (filterList->GetFolderInfo()) ? filterList->GetFolderInfo()->GetIMAPFolderInfoMail() : (MSG_IMAPFolderInfoMail *)NULL;
				MSG_IMAPFolderInfoContainer *imapContainer = (imapMailFolder) ? imapMailFolder->GetIMAPContainer() : GetFilter()->GetMaster()->GetImapMailFolderTree();
				if (imapContainer)
				{
					MSG_IMAPFolderInfoMail *imapFolder = (MSG_IMAPFolderInfoMail *) imapContainer->FindMailPathname(m_action.m_value.m_folderName);
					if (imapFolder)
						m_action.m_originalServerPath = XP_STRDUP(imapFolder->GetOnlineName());
				}
				
				if (!m_action.m_originalServerPath)
					m_action.m_originalServerPath = XP_STRDUP("");	// emergency fallback
			}
			
			if (m_action.m_originalServerPath)
			{
				char *imapTargetString = (char *)XP_ALLOC(XP_STRLEN(kImapPrefix) + XP_STRLEN(m_action.m_originalServerPath) + 1);
				if (imapTargetString)
				{
					XP_STRCPY(imapTargetString, kImapPrefix);
					XP_STRCAT(imapTargetString, m_action.m_originalServerPath);
					err = filterList->WriteStrAttr(fid, filterAttribActionValue, imapTargetString);
					XP_FREE(imapTargetString);
				}
			}
		}
		break;
	case acChangePriority:
		{
			char priority[50];
			MSG_GetUntranslatedPriorityName (m_action.m_value.m_priority, priority, sizeof(priority));
			err = filterList->WriteStrAttr(fid, filterAttribActionValue, priority);
		}
		break;
	default:
		break;
	}
	// and here we begin - file out term list...
	int searchIndex;
	XPStringObj  condition = " ";   // do not inser the open paren....
	for (searchIndex = 0; searchIndex < m_termList.GetSize() && err == FilterError_Success;
			searchIndex++)
	{
		char	*stream;
		int16		length;
		MSG_SearchError	searchError;

		MSG_SearchTerm * term = (MSG_SearchTerm *) m_termList.GetAt(searchIndex);
		if (term == NULL)
			continue;
		
		if (XP_STRLEN(condition) > 1)
			condition += ' ';

		if (term->m_booleanOp == MSG_SearchBooleanOR)
			condition += "OR (";
		else
			condition += "AND (";

		searchError = term->EnStreamNew(&stream, &length);
		if (searchError != SearchError_Success)
		{
			err = FilterError_SearchError;
			break;
		}
		
		condition += stream;
		condition += ')';
		XP_FREE(stream);
	}
	if (err == FilterError_Success)
		err = filterList->WriteStrAttr(fid, filterAttribCondition, condition);
	return err;
}



MSG_FilterError MSG_Rule::ConvertMoveToFolderValue(const char *relativePath)
{
	MSG_Master		*master = GetFilter()->GetMaster();
	const char		*folderDirectory;
	MSG_FilterList	*filterList = m_filter->GetFilterList();
	if (relativePath)
	{
		folderDirectory = master->GetPrefs()->GetFolderDirectory();
		// ### dmb - check to see if the start of the dest folder name is the
		// mail directory. If not, add the directory name back on. In general,
		// the mail directory won't be there, so this is upgrade code.
		
		// km add check to make sure we don't prepend the mail dir to an imap path
		const char *imapDirectory = master->GetPrefs()->GetIMAPFolderDirectory();
		
		if (GetFilter()->GetVersion() <= kFileVersionOldMoveTarget)
		{
			if (XP_STRNCASECMP(relativePath, folderDirectory, XP_STRLEN(folderDirectory)) &&
				XP_STRNCASECMP(relativePath, imapDirectory, XP_STRLEN(imapDirectory)) )
			{
				m_action.m_value.m_folderName = PR_smprintf("%s/%s", master->GetPrefs()->GetFolderDirectory(), relativePath);
			}
			else
			{
				m_action.m_originalServerPath = NULL;
				m_action.m_value.m_folderName = XP_STRDUP(relativePath);
				
				// upgrade: save m_originalServerPath
				MSG_IMAPFolderInfoMail *imapMailFolder = (filterList->GetFolderInfo()) ? filterList->GetFolderInfo()->GetIMAPFolderInfoMail() : (MSG_IMAPFolderInfoMail *)NULL;
				MSG_IMAPFolderInfoContainer *imapContainer = (imapMailFolder) ? imapMailFolder->GetIMAPContainer() : GetFilter()->GetMaster()->GetImapMailFolderTree();
				if (imapContainer)
				{
					MSG_IMAPFolderInfoMail *imapFolder = (MSG_IMAPFolderInfoMail *) imapContainer->FindMailPathname(relativePath);
					if (imapFolder)
						m_action.m_originalServerPath = XP_STRDUP(imapFolder->GetOnlineName());
				}
				
				if (!m_action.m_originalServerPath)
				{
					// did the user switch servers??
					FREEIF(m_action.m_value.m_folderName);
					m_action.m_value.m_folderName = XP_STRDUP("");
				}
			}
		}
		else
		{
			if (XP_STRNCMP(MSG_Rule::kImapPrefix, relativePath, XP_STRLEN(MSG_Rule::kImapPrefix)))
				m_action.m_value.m_folderName = PR_smprintf("%s/%s", master->GetPrefs()->GetFolderDirectory(), relativePath);
			else
			{
				m_action.m_originalServerPath = XP_STRDUP(relativePath + XP_STRLEN(MSG_Rule::kImapPrefix));
				// convert the server path to the local full path
				MSG_IMAPFolderInfoMail *imapMailFolder = (filterList->GetFolderInfo()) ? filterList->GetFolderInfo()->GetIMAPFolderInfoMail() : (MSG_IMAPFolderInfoMail *)NULL;
				MSG_IMAPFolderInfoContainer *imapContainer = (imapMailFolder) ? imapMailFolder->GetIMAPContainer() : GetFilter()->GetMaster()->GetImapMailFolderTree();

				MSG_IMAPFolderInfoMail *imapFolder = NULL;
				if (imapContainer)
					imapFolder = GetFilter()->GetMaster()->FindImapMailFolder(imapContainer->GetHostName(), relativePath + XP_STRLEN(MSG_Rule::kImapPrefix), NULL, FALSE);
				if (imapFolder)
					m_action.m_value.m_folderName = XP_STRDUP(imapFolder->GetPathname());
				else
				{
					// did the user switch servers??
					// we'll still save this filter, the filter code in the mail parser will handle this case
					m_action.m_value.m_folderName = XP_STRDUP("");
				}
			}
		}
#if defined(XP_WIN) || defined(XP_OS2)
		if (GetFilter()->GetVersion() <= kFileVersionAbsPath)
		{
			XP_File folderFile = XP_FileOpen(m_action.m_value.m_folderName, xpMailFolder, XP_FILE_READ_BIN);
			char *mailPart = m_action.m_value.m_folderName;
			char *newFolderName = NULL;
			while (!folderFile)	// dest got messed up - try to fix by finding "mail" part of path
			{					// preprending mail directory, and seeing if the folder exists.
				if (newFolderName)
					XP_FREE(newFolderName);
				mailPart = strcasestr(mailPart + 1, "mail");
				if (mailPart)
				{
					newFolderName = PR_smprintf("%s/%s", folderDirectory, mailPart + 5);
					folderFile = XP_FileOpen(newFolderName, xpMailFolder, XP_FILE_READ_BIN);
				}
				else
				{
					newFolderName = XP_STRDUP(INBOX_FOLDER_NAME);
#ifdef DEBUG_bienvenu
					FE_Alert(NULL, "Couldn't find destination for move rule");
#endif
					break;
				}
			}
			if (newFolderName)
			{
				XP_FREE(m_action.m_value.m_folderName);
				m_action.m_value.m_folderName = newFolderName;
			}

			if (folderFile)
				XP_FileClose(folderFile);
		}
#endif
	}
	return FilterError_Success;
}

MSG_FilterError MSG_RuleAddTerm(MSG_Rule *rule,     
	MSG_SearchAttribute attrib,    /* attribute for this term                */
    MSG_SearchOperator op,         /* operator e.g. opContains               */
    MSG_SearchValue *value,       /* value e.g. "Fred"                   */
	XP_Bool booleanAND,			  /* set to true of boolean operator is AND */
	char * arbitraryHeader)		  /* the "Other..." header the user manually provided the FE.
								     ignored by BE unless attrib is attribOtherHeader */
{
	if (NULL == rule)
		return FilterError_NullPointer;

	return rule->AddTerm(attrib, op, value, booleanAND, arbitraryHeader);
}

MSG_FilterError MSG_Rule::AddTerm(
	MSG_SearchAttribute attrib,    /* attribute for this term                */
    MSG_SearchOperator op,         /* operator e.g. opContains               */
    MSG_SearchValue *value,       /* value e.g. "Fred"                   */
	XP_Bool booleanAND,			  /* set to TRUE if boolean operator is AND */
	char * arbitraryHeader)        /* arbitrary header provided by user. ignored unless attrib = attribOtherHeader */
{
	MSG_SearchTerm *newSearchTerm = new MSG_SearchTerm(attrib, op, value, booleanAND, arbitraryHeader);
	if (newSearchTerm == NULL)
		return FilterError_OutOfMemory;
	m_termList.Add(newSearchTerm);
	return FilterError_Success;
}

MSG_FilterError MSG_RuleGetNumTerms(MSG_Rule *rule, int32 *numTerms)
{
	if (NULL == rule || NULL == numTerms)
		return FilterError_NullPointer;

	return rule->GetNumTerms(numTerms);
}

MSG_FilterError MSG_Rule::GetNumTerms(int32 *numTerms)
{
	*numTerms = m_termList.GetSize();
	return FilterError_Success;
}

MSG_FilterError MSG_RuleGetTerm(MSG_Rule *rule, int32 termIndex, 
	MSG_SearchAttribute *attrib,    /* attribute for this term                */
    MSG_SearchOperator *op,         /* operator e.g. opContains               */
    MSG_SearchValue *value,        /* value e.g. "Fred"                   */
	XP_Bool * booleanAND,		   /* Boolean AND operator  				 */
	char ** arbitraryHeader)
{
	if (NULL == rule || NULL == attrib || NULL == op)
		return FilterError_NullPointer;
	return rule->GetTerm(termIndex, attrib, op, value, booleanAND, arbitraryHeader);
}

MSG_FilterError MSG_Rule::GetTerm(int32 termIndex,
	MSG_SearchAttribute *attrib,    /* attribute for this term                */
    MSG_SearchOperator *op,         /* operator e.g. opContains               */
    MSG_SearchValue *value,       /* value e.g. "Fred"                   */
	XP_Bool * BooleanAND,
	char ** arbitraryHeader)
{
	// we're going to leave the first element empty, like search does
	MSG_SearchTerm *term = (MSG_SearchTerm *) m_termList.GetAt(termIndex);
	if (term == NULL)
		return FilterError_InvalidIndex;
	*attrib = term->m_attribute;
	*op = term->m_operator;
	*value = term->m_value;
	if (BooleanAND)
		* BooleanAND = term->IsBooleanOpAND();
	if (arbitraryHeader)
		* arbitraryHeader = term->m_arbitraryHeader;
	return FilterError_Success;
}

MSG_FilterError MSG_RuleSetScope(MSG_Rule *rule, MSG_ScopeTerm *scope)
{
	if (NULL == rule || NULL == scope)
		return FilterError_NullPointer;

	return FilterError_NotImplemented;
}

MSG_FilterError MSG_RuleGetScope(MSG_Rule *rule, MSG_ScopeTerm **scope)
{
	if (NULL == rule || NULL == scope)
		return FilterError_NullPointer;

	return FilterError_NotImplemented;
}

/* if type is acChangePriority, value is a pointer to priority.
   If type is acMoveToFolder, value is pointer to folder name.
   Otherwise, value is ignored.
*/
MSG_FilterError MSG_RuleSetAction(MSG_Rule *rule, MSG_RuleActionType type, void *value)
{
	if (NULL == rule)		// value is sometimes legally NULL here...
		return FilterError_NullPointer;

	return rule->SetAction(type, value);
}


MSG_FilterError MSG_Rule::SetAction(MSG_RuleActionType type, void *value)
{
	m_action.m_type = type;
	switch (type)
	{
	case acMoveToFolder:
		if (value == NULL)
			return FilterError_NullPointer;
		m_action.m_value.m_folderName =  XP_STRDUP((char *) value);
		
		// reset m_action.m_originalServerPath to NULL, the SaveToTextFile
		// function will convert m_action.m_value.m_folderName to the right
		// m_action.m_originalServerPath and write it out.
		FREEIF(m_action.m_originalServerPath);
		m_action.m_originalServerPath = NULL;	// FREEIF does this but prevent
												// future changes from breaking this
		break;
	case acChangePriority:
		if (value == NULL)
			return FilterError_NullPointer;
		m_action.m_value.m_priority = (MSG_PRIORITY ) (int32) value;
		break;
	default:
		break;
	}
	return FilterError_Success;
}

MSG_FilterError MSG_RuleGetAction(MSG_Rule *rule, MSG_RuleActionType *type, void **value)
{
	if (NULL == rule || NULL == value || NULL == type)
		return FilterError_NullPointer;

	return rule->GetAction(type, value);
}

MSG_FilterError MSG_Rule::GetAction(MSG_RuleActionType *type, void **value)
{
	*type = m_action.m_type;
	switch (*type)
	{
	case acMoveToFolder:

		* (char **) value = (m_action.m_value.m_folderName);
		break;
	case acChangePriority:
		* (MSG_PRIORITY *) value = m_action.m_value.m_priority;
		break;
	default:
		break;
	}
	return FilterError_Success;
}

/* help FEs manage menu choices in Filter dialog box */
extern "C"
MSG_FilterError MSG_GetRuleActionMenuItems(
	MSG_FilterType type,		/* type of filter                          */
    MSG_RuleMenuItem *items,    /* array of caller-allocated structs       */
    uint16 *maxItems)            /* in- max array size; out- num returned   */ 
{
	if (NULL == items || NULL == maxItems)
		return FilterError_NullPointer;

	return MSG_Rule::GetActionMenuItems(type, items, maxItems);
}

// for each action, this table encodes the filterTypes that support the action.
struct RuleActionsTableEntry
{
	MSG_RuleActionType	action;
	MSG_FilterType		supportedTypes;
	int					xp_strIndex;
	const char			*actionFilingStr;	/* used for filing out filters, don't translate! */
};

// Because some native C++ compilers can't initialize static objects with ints,
//  we can't initialize this structure directly, so we have to do it in two phases.
static struct RuleActionsTableEntry ruleActionsTable[] =
{
	{ acMoveToFolder,	filterInbox,	0, /*XP_FILTER_MOVE_TO_FOLDER*/		"Move to folder" },
	{ acChangePriority,	filterInbox,	0, /*XP_FILTER_CHANGE_PRIORITY*/	"Change priority"},
	{ acDelete,			filterAll,		0, /*XP_FILTER_DELETE */			"Delete"},
	{ acMarkRead,		filterAll,		0, /*XP_FILTER_MARK_READ */			"Mark read"},
	{ acKillThread,		filterAll,		0, /*XP_FILTER_KILL_THREAD */		"Ignore thread"},
	{ acWatchThread,	filterAll,		0, /*XP_FILTER_WATCH_THREAD */		"Watch thread"}
};

/*static*/ void MSG_Rule::InitActionsTable()
{
	if (ruleActionsTable[0].xp_strIndex == 0)
	{
		ruleActionsTable[0].xp_strIndex = XP_FILTER_MOVE_TO_FOLDER;
		ruleActionsTable[1].xp_strIndex = XP_FILTER_CHANGE_PRIORITY;
		ruleActionsTable[2].xp_strIndex = XP_FILTER_DELETE;
		ruleActionsTable[3].xp_strIndex = XP_FILTER_MARK_READ;
		ruleActionsTable[4].xp_strIndex = XP_FILTER_KILL_THREAD;
		ruleActionsTable[5].xp_strIndex = XP_FILTER_WATCH_THREAD;
	}
}
/*static */char *MSG_Rule::GetActionStr(MSG_RuleActionType action)
{
	int	numActions = sizeof(ruleActionsTable) / sizeof(ruleActionsTable[0]);

	InitActionsTable();
	for (int i = 0; i < numActions; i++)
	{
		if (action == ruleActionsTable[i].action)
			return XP_GetString(ruleActionsTable[i].xp_strIndex);
	}
	return "";
}

/*static */const char *MSG_Rule::GetActionFilingStr(MSG_RuleActionType action)
{
	int	numActions = sizeof(ruleActionsTable) / sizeof(ruleActionsTable[0]);

	for (int i = 0; i < numActions; i++)
	{
		if (action == ruleActionsTable[i].action)
			return ruleActionsTable[i].actionFilingStr;
	}
	return "";
}


MSG_RuleActionType MSG_Rule::GetActionForFilingStr(const char *actionStr)
{
	int	numActions = sizeof(ruleActionsTable) / sizeof(ruleActionsTable[0]);

	for (int i = 0; i < numActions; i++)
	{
		if (!XP_STRCASECMP(ruleActionsTable[i].actionFilingStr, actionStr))
			return ruleActionsTable[i].action;
	}
	return acNone;
}

MSG_FilterError MSG_Rule::GetActionMenuItems( 
	MSG_FilterType type,
    MSG_RuleMenuItem *items,    /* array of caller-allocated structs       */
    uint16 *maxItems)            /* in- max array size; out- num returned   */ 
{
	int	numActions = sizeof(ruleActionsTable) / sizeof(ruleActionsTable[0]);
	int	numReturned = 0;

	InitActionsTable();
	for (int i = 0; i < numActions; i++)
	{
		if (numReturned >= *maxItems)
			break;
		if (type & ruleActionsTable[i].supportedTypes)
		{
			items[numReturned].attrib = ruleActionsTable[i].action;
			char *name = XP_GetString(ruleActionsTable[i].xp_strIndex);
			XP_STRNCPY_SAFE (items[numReturned].name, name, sizeof(items[numReturned].name));
			numReturned++;
		}
	}
	*maxItems = numReturned;
	return FilterError_Success;
}

extern "C" MSG_FilterError MSG_GetFilterWidgetForAction( MSG_RuleActionType action,
											  MSG_SearchValueWidget *widget )
{
	switch (action) {
	case acMoveToFolder:
    case acChangePriority:
		*widget = widgetMenu;
		break;
	case acNone:
	case acDelete:
	case acMarkRead:
	case acKillThread:
	case acWatchThread:
	default:
		*widget = widgetNone;
		break;
	}
	return FilterError_Success;
}

extern "C" MSG_SearchError MSG_GetValuesForAction( MSG_RuleActionType action,
											   MSG_SearchMenuItem *items, 
											   uint16 *maxItems)
{
	const MSG_PRIORITY aiPriorityValues[] = { MSG_NoPriority,
											  MSG_LowestPriority,
											  MSG_LowPriority,
											  MSG_NormalPriority,
											  MSG_HighPriority,
											  MSG_HighestPriority };

	uint16 nPriorities = sizeof(aiPriorityValues) / sizeof(MSG_PRIORITY);
	uint16 i;

	switch (action) {
	case acChangePriority:
		for ( i = 0; i < nPriorities && i < *maxItems; i++ ) {
			items[i].attrib = (int16) aiPriorityValues[i];
			MSG_GetUntranslatedPriorityName( (MSG_PRIORITY) items[i].attrib, 
											 items[i].name, 
											 sizeof( items[i].name ) / sizeof(char) );
			items[i].isEnabled = TRUE;
		}
		*maxItems = i;
		if ( i == nPriorities ) {
			return SearchError_Success;
		} else {
			return SearchError_ListTooSmall;
		}
	default:
		*maxItems = 0;
		return SearchError_InvalidAttribute;
	}
}

void			MSG_ViewFilterLog(MSG_Pane *pane)
{
	char	*filterLogName = WH_FileName("", xpMailFilterLog);
	char	*platformFileName = XP_PlatformFileToURL(filterLogName);
	URL_Struct	*url_struct = NET_CreateURLStruct(platformFileName, NET_NORMAL_RELOAD);
	pane->GetURL(url_struct, TRUE);
	FREEIF(filterLogName);
	FREEIF(platformFileName);
}


#ifdef DEBUG
void MSG_Rule::Dump()
{
	XP_Trace("action type = %d, \n", m_action.m_type);
	switch (m_action.m_type)
	{
	case acMoveToFolder:
		XP_Trace("dest folder = %s\n", m_action.m_value.m_folderName);
		break;
	case acChangePriority:
		XP_Trace("result priority = %d\n", m_action.m_value.m_priority);
		break;
	default:
		break;
	}

	for (int i = 0; i < m_termList.GetSize(); i++)
	{
		MSG_SearchTerm * term = (MSG_SearchTerm *) m_termList.GetAt(i);
		if (term == NULL)
			continue;
		XP_Trace("search term attr = %d op = %d\n", term->m_attribute,
			term->m_operator);
		switch (term->m_attribute)
		{
		case attribDate:
            XP_Trace("date = %ld\n", (long) term->m_value.u.date);
			break;
		case attribPriority:
			XP_Trace("priority = %d\n", term->m_value.u.priority);
			break;
		case attribMsgStatus:
			XP_Trace("msg_status = %lx\n", (long) term->m_value.u.msgStatus);
			break;
		case attribSender :
		case attribSubject:
		case attribBody:
		case attribTo:
		case attribCC:
		case attribToOrCC:
			XP_Trace("value = %s\n", term->m_value.u.string);
			break;
		default:
			break;
		}
	}
}
#endif

/* ---------- Private implementation ---------- */

uint32 MSG_Rule::m_expectedMagic = 0x44444444;
uint32 MSG_Filter::m_expectedMagic = 0x55555555;
uint32 MSG_FilterList::m_expectedMagic = 0x66666666;

