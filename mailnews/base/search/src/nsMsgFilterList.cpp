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
#include "nsIMsgFilterHitNotify.h"
#include "nsFileStream.h"
#include "nsMsgUtils.h"
#include "nsMsgSearchTerm.h"

const int16 kFileVersion = 7;
const int16 k45Version = 6;


nsMsgFilterList::nsMsgFilterList(nsIOFileStream *fileStream)
{
	m_fileStream = fileStream;
	// I don't know how we're going to report this error if we failed to create the isupports array...
	nsresult rv;
	rv = NS_NewISupportsArray(getter_AddRefs(m_filters));
	m_loggingEnabled = PR_FALSE;
	m_curFilter = nsnull;
	NS_INIT_REFCNT();
}

NS_IMPL_ADDREF(nsMsgFilterList)
NS_IMPL_RELEASE(nsMsgFilterList)

NS_IMETHODIMP nsMsgFilterList::QueryInterface(REFNSIID aIID, void** aResult)
{   
    if (aResult == NULL)  
        return NS_ERROR_NULL_POINTER;  

    if (aIID.Equals(nsIMsgFilterList::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
	{
        *aResult = NS_STATIC_CAST(nsMsgFilterList*, this);   
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}   

NS_IMETHODIMP nsMsgFilterList::CreateFilter(const char *name,class nsIMsgFilter **aFilter)
{
	if (!aFilter)
		return NS_ERROR_NULL_POINTER;

	nsMsgFilter *filter = new nsMsgFilter;
	*aFilter = filter;
	if (filter)
	{
		nsString2 strName(name, eOneByte);
		filter->SetName(&strName);
		return NS_OK;
	}
	return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsMsgFilterList::SetLoggingEnabled(PRBool enable)
{
	m_loggingEnabled = enable;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFilterList::GetFolderForFilterList(class nsIMsgFolder **aFolder)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFilterList::GetLoggingEnabled(PRBool *aResult)
{
	if (!aResult)
		return NS_ERROR_NULL_POINTER;
	*aResult = m_loggingEnabled;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFilterList::SaveToFile(nsIOFileStream *stream)
{
	if (!stream)
		return NS_ERROR_NULL_POINTER;

	m_fileStream = stream;
	return SaveTextFilters();
}

NS_IMETHODIMP
nsMsgFilterList::ApplyFiltersToHdr(nsMsgFilterTypeType filterType,
                                   nsIMsgDBHdr *msgHdr,
                                   nsIMsgFolder *folder,
                                   nsIMsgDatabase *db, 
                                   const char *headers,
                                   PRUint32 headersSize,
                                   nsIMsgFilterHitNotify *listener)
{
	nsIMsgFilter	*filter;
	PRUint32		filterCount = 0;
	nsresult		ret = NS_OK;

	GetFilterCount(&filterCount);

	for (PRUint32 filterIndex = 0; filterIndex < filterCount; filterIndex++)
	{
		if (NS_SUCCEEDED(GetFilterAt(filterIndex, &filter)))
		{
			PRBool isEnabled;
			nsMsgFilterTypeType curFilterType;

			filter->GetEnabled(&isEnabled);
			if (!isEnabled)
				continue;

			filter->GetFilterType(&curFilterType);  
			 if (curFilterType & filterType)
			{
				nsresult matchTermStatus = NS_OK;
				PRBool result;

				matchTermStatus = filter->MatchHdr(msgHdr, folder, db, headers, headersSize, &result);
				if (NS_SUCCEEDED(matchTermStatus) && result && listener)
				{
					PRBool applyMore;

					ret  = listener->ApplyFilterHit(filter, &applyMore);
					if (!NS_SUCCEEDED(ret) || !applyMore)
						break;
				}
			}
		}
	}
	return ret;
}

#if 0
nsresult nsMsgFilterList::Open(nsMsgFilterTypeType type, nsIMsgFolder *folder, nsIMsgFilterList **filterList)
{
	nsresult	err = NS_OK;
	nsMsgFilterList	*newFilterList;

	if (type != filterInbox
		&& type != filterNews)
		return FilterError_InvalidFilterType;

	if (nsnull == filterList)
		return NS_ERROR_NULL_POINTER;

	newFilterList = new nsMsgFilterList;
	if (newFilterList == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;

	newFilterList->m_master = master;

	// hack up support for news filters by checking the current folder of the pane and massaging input params.
	if (pane != nsnull && folderInfo == nsnull)
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
		MSG_IMAPFolderInfoMail *imapMailFolder = (folderInfo) ? folderInfo->GetIMAPFolderInfoMail() : (MSG_IMAPFolderInfoMail *)nsnull;

		newFilterList->m_filterFileName = "";
		newFilterList->m_fileType = xpMailSort;
		if (imapMailFolder)
		{
			MSG_IMAPHost *defaultHost = imapMailFolder->GetMaster()->GetIMAPHostTable()->GetDefaultHost();
			if (imapMailFolder->GetIMAPHost() == defaultHost)
			{
				PRBool defaultHostFiltersExist = !XP_Stat(imapMailFolder->GetIMAPHost()->GetHostName(), &outStat, newFilterList->m_fileType);
				if (!defaultHostFiltersExist)
					upgradeIMAPFiltersDestFileName = imapMailFolder->GetIMAPHost()->GetHostName();
			}

			// if it's not the default imap host or there are no filters for the default host, or the old local mail filters 
			// don't exist, set the filter file name to the filter name for the imap host.
			if (!upgradeIMAPFiltersDestFileName || XP_Stat(newFilterList->m_filterFileName, &outStat, newFilterList->m_fileType))
				newFilterList->m_filterFileName = imapMailFolder->GetIMAPHost()->GetHostName();
		}

	}

	if (XP_Stat(newFilterList->m_filterFileName, &outStat, newFilterList->m_fileType))
	{
		// file must not exist - no rules, we're done.
		return NS_OK;
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

extern "C" MSG_FolderInfo *MSG_GetFolderInfoForFilterList(nsMsgFilterList *filterList)
{
	return filterList ? filterList->GetFolderInfo() : (MSG_FolderInfo *)nsnull;
}
#endif


typedef struct
{
	nsMsgFilterFileAttrib	attrib;
	const char			*attribName;
} FilterFileAttribEntry;

static FilterFileAttribEntry FilterFileAttribTable[] =
{
	{nsMsgFilterAttribNone,			""},
	{nsMsgFilterAttribVersion,		"version"},
	{nsMsgFilterAttribLogging,		"logging"},
	{nsMsgFilterAttribName,			"name"},
	{nsMsgFilterAttribEnabled,		"enabled"},
	{nsMsgFilterAttribDescription,	"description"},
	{nsMsgFilterAttribType,			"type"},
	{nsMsgFilterAttribScriptFile,	"scriptName"},
	{nsMsgFilterAttribAction,		"action"},
	{nsMsgFilterAttribActionValue,	"actionValue"},
	{nsMsgFilterAttribCondition,		"condition"}
};

// If we want to buffer file IO, wrap it in here.
char nsMsgFilterList::ReadChar()
{
	char	newChar;
	*m_fileStream >> newChar; 
	return (m_fileStream->eof() ? -1 : newChar);
}

PRBool nsMsgFilterList::IsWhitespace(char ch)
{
	return (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t');
}

char nsMsgFilterList::SkipWhitespace()
{
	char ch;
	do
	{
		ch = ReadChar();
	} while (IsWhitespace(ch));
	return ch;
}

PRBool nsMsgFilterList::StrToBool(nsString2 &str)
{
	return str.Equals("yes") ;
}

char nsMsgFilterList::LoadAttrib(nsMsgFilterFileAttrib &attrib)
{
	char	attribStr[100];
	char	curChar;
	
	curChar = SkipWhitespace();
	int i;
	for (i = 0; i + 1 < (int)(sizeof(attribStr)); )
	{
		if (curChar == (char) -1 || IsWhitespace(curChar) || curChar == '=')
			break;
		attribStr[i++] = curChar;
		curChar = ReadChar();
	}
	attribStr[i] = '\0';
	for (int tableIndex = 0; tableIndex < (int)(sizeof(FilterFileAttribTable) / sizeof(FilterFileAttribTable[0])); tableIndex++)
	{
		if (!PL_strcasecmp(attribStr, FilterFileAttribTable[tableIndex].attribName))
		{
			attrib = FilterFileAttribTable[tableIndex].attrib;
			break;
		}
	}
	return curChar;
}

const char *nsMsgFilterList::GetStringForAttrib(nsMsgFilterFileAttrib attrib)
{
	for (int tableIndex = 0; tableIndex < (int)(sizeof(FilterFileAttribTable) / sizeof(FilterFileAttribTable[0])); tableIndex++)
	{
		if (attrib == FilterFileAttribTable[tableIndex].attrib)
			return FilterFileAttribTable[tableIndex].attribName;
	}
	return nsnull;
}

nsresult nsMsgFilterList::LoadValue(nsString2 &value)
{
	nsString2	valueStr(eOneByte);
	char	curChar;
	value = "";
	curChar = SkipWhitespace();
	if (curChar != '"')
	{
		NS_ASSERTION(PR_FALSE, "expecting quote as start of value");
		return NS_MSG_FILTER_PARSE_ERROR;
	}
	curChar = ReadChar();
	do
	{
		if (curChar == '\\')
		{
			char nextChar = ReadChar();
			if (nextChar == '"')
				curChar = '"';
			else if (nextChar == '\\')	// replace "\\" with "\"
			{
				curChar = ReadChar();
			}
			else
			{
				valueStr += curChar;
				curChar = nextChar;
			}
		}
		else
		{
			if (curChar == (char) -1 || curChar == '"' || curChar == '\n' || curChar == '\r')
			{
			    value += valueStr;
				break;
			}
		}
		valueStr += curChar;
		curChar = ReadChar();
	}
	while (!m_fileStream->eof());
	return NS_OK;
}

nsresult nsMsgFilterList::LoadTextFilters()
{
	nsresult	err = NS_OK;
	nsMsgFilterFileAttrib attrib;

	// We'd really like to move lot's of these into the objects that they refer to.
	m_fileStream->seek(PR_SEEK_SET, 0);
	do 
	{
		nsString2	value(eOneByte);
		PRInt32 intToStringResult;

		char curChar;

        curChar = LoadAttrib(attrib);
		if (attrib == nsMsgFilterAttribNone)
			break;
		err = LoadValue(value);
		if (err != NS_OK)
			break;
		switch(attrib)
		{
		case nsMsgFilterAttribNone:
			break;
		case nsMsgFilterAttribVersion:
			m_fileVersion = value.ToInteger(&intToStringResult, 10);
			if (intToStringResult != 0)
			{
				attrib = nsMsgFilterAttribNone;
				NS_ASSERTION(PR_FALSE, "error parsing filter file version");
			}
			break;
		case nsMsgFilterAttribLogging:
			m_loggingEnabled = StrToBool(value);
			break;
		case nsMsgFilterAttribName:
		{
			nsMsgFilter *filter = new nsMsgFilter;
			if (filter == nsnull)
			{
				err = NS_ERROR_OUT_OF_MEMORY;
				break;
			}
			filter->SetFilterList(this);
			filter->SetName(&value);
			m_curFilter = filter;
			m_filters->AppendElement(filter);
		}
			break;
		case nsMsgFilterAttribEnabled:
			if (m_curFilter)
				m_curFilter->SetEnabled(StrToBool(value));
			break;
		case nsMsgFilterAttribDescription:
			if (m_curFilter)
				m_curFilter->SetDescription(&value);
			break;
		case nsMsgFilterAttribType:
			if (m_curFilter)
			{
				m_curFilter->SetType((nsMsgFilterTypeType) value.ToInteger(&intToStringResult, 10));
			}
			break;
		case nsMsgFilterAttribScriptFile:
			if (m_curFilter)
				m_curFilter->SetFilterScript(&value);
			break;
		case nsMsgFilterAttribAction:
			m_curFilter->m_action.m_type = nsMsgFilter::GetActionForFilingStr(value);
			break;
		case nsMsgFilterAttribActionValue:
			if (m_curFilter->m_action.m_type == nsMsgFilterAction::MoveToFolder)
				err = m_curFilter->ConvertMoveToFolderValue(value);
			else if (m_curFilter->m_action.m_type == nsMsgFilterAction::ChangePriority)
			{
				nsMsgPriority outPriority;
				nsresult res = NS_MsgGetPriorityFromString(value.GetBuffer(), &outPriority);
				if (NS_SUCCEEDED(res))
				{
					m_curFilter->SetAction(m_curFilter->m_action.m_type, (void *) (int32) outPriority);
				}
				else
					NS_ASSERTION(PR_FALSE, "invalid priority in filter file");

			}
			break;
		case nsMsgFilterAttribCondition:
			err = ParseCondition(value);
			break;
		}
	} while (attrib != nsMsgFilterAttribNone);
	return err;
}

// parse condition like "(subject, contains, fred) AND (body, isn't, "foo)")"
// values with close parens will be quoted.
// what about values with close parens and quotes? e.g., (body, isn't, "foo")")
// I guess interior quotes will need to be escaped - ("foo\")")
// which will get written out as (\"foo\\")\") and read in as ("foo\")"
nsresult nsMsgFilterList::ParseCondition(nsString2 &value)
{
	PRBool	done = PR_FALSE;
	nsresult	err = NS_OK;
	const char *curPtr = value.GetBuffer();
	while (!done)
	{
		// insert code to save the boolean operator if there is one for this search term....
		const char *openParen = PL_strchr(curPtr, '(');
		const char *orTermPos = PL_strchr(curPtr, 'O');		// determine if an "OR" appears b4 the openParen...
		PRBool ANDTerm = PR_TRUE;
		if (orTermPos && orTermPos < openParen) // make sure OR term falls before the '('
			ANDTerm = PR_FALSE;

		char *termDup = nsnull;
		if (openParen)
		{
			PRBool foundEndTerm = PR_FALSE;
			PRBool inQuote = PR_FALSE;
			for (curPtr = openParen +1; *curPtr; curPtr++)
			{
				if (*curPtr == '\\' && *(curPtr + 1) == '"')
					curPtr++;
				else if (*curPtr == ')' && !inQuote)
				{
					foundEndTerm = PR_TRUE;
					break;
				}
				else if (*curPtr == '"')
					inQuote = !inQuote;
			}
			if (foundEndTerm)
			{
				int termLen = curPtr - openParen - 1;
				termDup = (char *) PR_Malloc(termLen + 1);
				if (termDup)
				{
					PL_strncpy(termDup, openParen + 1, termLen + 1);
					termDup[termLen] = '\0';
				}
				else
				{
					err = NS_ERROR_OUT_OF_MEMORY;
					break;
				}
			}
		}
		else
			break;
		if (termDup)
		{
			nsMsgSearchTerm	*newTerm = new nsMsgSearchTerm;
            
			if (newTerm) {
                if (ANDTerm) {
                    newTerm->m_booleanOp = nsMsgSearchBooleanOp::BooleanAND;
                }
                else {
                    newTerm->m_booleanOp = nsMsgSearchBooleanOp::BooleanOR;
                }
                
                if (newTerm->DeStreamNew(termDup, PL_strlen(termDup)) == NS_OK)
                    m_curFilter->GetTermList()->AppendElement(newTerm);
            }
			PR_FREEIF(termDup);
		}
		else
			break;
	}
	return err;
}

nsresult nsMsgFilterList::WriteIntAttr(nsMsgFilterFileAttrib attrib, int value)
{
	const char *attribStr = GetStringForAttrib(attrib);
	if (attribStr)
	{
		*m_fileStream << attribStr;
		*m_fileStream << "=\"";
		*m_fileStream << value;
		*m_fileStream << "\"" MSG_LINEBREAK;
	}
//		XP_FilePrintf(fid, "%s=\"%d\"%s", attribStr, value, LINEBREAK);
	return NS_OK;
}

nsresult nsMsgFilterList::WriteStrAttr(nsMsgFilterFileAttrib attrib, nsString2 &str2)
{
	const char *str = str2.GetBuffer();
	if (str && m_fileStream) // only proceed if we actually have a string to write out. 
	{
		char *escapedStr = nsnull;
		if (PL_strchr(str, '"'))
			escapedStr = nsMsgSearchTerm::EscapeQuotesInStr(str);

		const char *attribStr = GetStringForAttrib(attrib);
		if (attribStr)
		{
			*m_fileStream << attribStr;
			*m_fileStream << "=\"";
			*m_fileStream << ((escapedStr) ? escapedStr : str);
			*m_fileStream << "\"" MSG_LINEBREAK;
//			XP_FilePrintf(fid, "%s=\"%s\"%s", attribStr, (escapedStr) ? escapedStr : str, LINEBREAK);
		}
		PR_FREEIF(escapedStr);
	}
	return NS_OK;
}

nsresult nsMsgFilterList::WriteBoolAttr(nsMsgFilterFileAttrib attrib, PRBool boolVal)
{
	nsString2 strToWrite((boolVal) ? "yes" : "no", eOneByte);
	return WriteStrAttr(attrib, strToWrite);
}

nsresult nsMsgFilterList::SaveTextFilters()
{
	nsresult	err = NS_OK;
	const char *attribStr;
	PRUint32			filterCount;
	m_filters->Count(&filterCount);

	attribStr = GetStringForAttrib(nsMsgFilterAttribVersion);
	err = WriteIntAttr(nsMsgFilterAttribVersion, kFileVersion);
	err = WriteBoolAttr(nsMsgFilterAttribLogging, m_loggingEnabled);
	for (PRUint32 i = 0; i < filterCount; i ++)
	{
		nsMsgFilter *filter;
		if (GetMsgFilterAt(i, &filter) == NS_OK && filter != nsnull)
		{
			filter->SetFilterList(this);
			if ((err = filter->SaveToTextFile(m_fileStream)) != NS_OK)
				break;
		}
		else
			break;
	}
	return err;
}

nsMsgFilterList::~nsMsgFilterList()
{

	// filters should be released for free, because only isupports array
	// is holding onto them, right?
//	PRUint32			filterCount;
//	m_filters->Count(&filterCount);
//	for (PRUint32 i = 0; i < filterCount; i++)
//	{
//		nsIMsgFilter *filter;
//		if (GetFilterAt(i, &filter) == NS_OK)
//			NS_RELEASE(filter);
//	}
}

nsresult nsMsgFilterList::Close()
{
#ifdef HAVE_PORT
	nsresult err = FilterError_FileError;
	XP_File			fid;
	XP_FileType		retType;
	const char		*finalName = m_filterFileName;
	char			*tmpName = (finalName) ? FE_GetTempFileFor(nsnull, finalName, m_fileType, &retType) : (char *)nsnull;


	if (!tmpName || !finalName) 
		return NS_ERROR_OUT_OF_MEMORY;
	m_fileStream = new nsIOFileStream
	fid = XP_FileOpen(tmpName, xpTemporary,
								 XP_FILE_TRUNCATE_BIN);
	if (fid) 
	{
		err = SaveTextFilters(fid);
		XP_FileClose(fid);
		if (err == NS_OK)
		{
			int status = XP_FileRename(tmpName, xpTemporary, finalName, m_fileType);
			XP_ASSERT(status >= 0);
		}
	}

	PR_FREEIF(tmpName);
	// tell open DB's that the filter list might have changed.
	NewsGroupDB::NotifyOpenDBsOfFilterChange(m_folderInfo);

	return err;
#else
	return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

nsresult nsMsgFilterList::GetFilterCount(PRUint32 *pCount)
{
	return m_filters->Count(pCount);
}

nsresult nsMsgFilterList::GetMsgFilterAt(PRUint32 filterIndex, nsMsgFilter **filter)
{

	PRUint32			filterCount;
	m_filters->Count(&filterCount);
	if (! (filterCount >= filterIndex))
		return NS_ERROR_INVALID_ARG;
	if (filter == nsnull)
		return NS_ERROR_NULL_POINTER;
	*filter = (nsMsgFilter *) m_filters->ElementAt(filterIndex);
	return NS_OK;
}

nsresult nsMsgFilterList::GetFilterAt(PRUint32 filterIndex, nsIMsgFilter **filter)
{
	PRUint32			filterCount;
	m_filters->Count(&filterCount);
	if (! (filterCount >= filterIndex))
		return NS_ERROR_INVALID_ARG;
	if (filter == nsnull)
		return NS_ERROR_NULL_POINTER;
	*filter = (nsIMsgFilter *) m_filters->ElementAt(filterIndex);
	return NS_OK;
}

nsresult nsMsgFilterList::SetFilterAt(PRUint32 filterIndex, nsIMsgFilter *filter)
{
	m_filters->ReplaceElementAt(filter, filterIndex);
	return NS_OK;
}


nsresult nsMsgFilterList::RemoveFilterAt(PRUint32 filterIndex)
{
	m_filters->RemoveElementAt(filterIndex);
	return NS_OK;
}

nsresult nsMsgFilterList::InsertFilterAt(PRUint32 filterIndex, nsIMsgFilter *filter)
{
	m_filters->InsertElementAt(filter, filterIndex);
	return NS_OK;
}

// Attempt to move the filter at index filterIndex in the specified direction.
// If motion not possible in that direction, we still return success.
// We could return an error if the FE's want to beep or something.
nsresult nsMsgFilterList::MoveFilterAt(PRUint32 filterIndex, 
										 nsMsgFilterMotionValue motion)
{
	nsIMsgFilter	*tempFilter;

	PRUint32			filterCount;
	m_filters->Count(&filterCount);
	if (! (filterCount >= filterIndex))
		return NS_ERROR_INVALID_ARG;

	tempFilter = (nsIMsgFilter *) m_filters->ElementAt(filterIndex);
	if (motion == nsMsgFilterMotion::Up)
	{
		if (filterIndex == 0)
			return NS_OK;
		m_filters->ReplaceElementAt(m_filters->ElementAt(filterIndex - 1), filterIndex);
		m_filters->ReplaceElementAt(tempFilter, filterIndex - 1);
	}
	else if (motion == nsMsgFilterMotion::Down)
	{
		if (filterIndex + 1 > filterCount - 1)
			return NS_OK;
		m_filters->ReplaceElementAt(m_filters->ElementAt(filterIndex + 1), filterIndex);
		m_filters->ReplaceElementAt(tempFilter, filterIndex + 1);
	}
	else
	{
		return NS_ERROR_INVALID_ARG;
	}
	return NS_OK;
}


#ifdef DEBUG
void nsMsgFilterList::Dump()
{
	PRUint32			filterCount;
	m_filters->Count(&filterCount);
	printf("%d filters\n", filterCount);

	for (PRUint32 i = 0; i < filterCount; i++)
	{
		nsMsgFilter *filter;
		if (GetMsgFilterAt(i, &filter) == NS_OK)
			filter->Dump();
	}

}
#endif

// ------------ End FilterList methods ------------------
