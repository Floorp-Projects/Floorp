/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// this file implements the nsMsgFilterList interface 

#include "nsTextFormatter.h"

#include "msgCore.h"
#include "nsMsgFilterList.h"
#include "nsMsgFilter.h"
#include "nsIMsgFilterHitNotify.h"
#include "nsFileStream.h"
#include "nsMsgUtils.h"
#include "nsMsgSearchTerm.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIImportService.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgFilterService.h"

static NS_DEFINE_CID(kMsgFilterServiceCID, NS_MSGFILTERSERVICE_CID);

// unicode "%s" format string
static const PRUnichar unicodeFormatter[] = {
    (PRUnichar)'%',
    (PRUnichar)'s',
    (PRUnichar)0,
};


nsMsgFilterList::nsMsgFilterList() :
    m_fileVersion(0)
{
	// I don't know how we're going to report this error if we failed to create the isupports array...
	nsresult rv;
	rv = NS_NewISupportsArray(getter_AddRefs(m_filters));
	m_loggingEnabled = PR_FALSE;
	m_curFilter = nsnull;
	NS_INIT_REFCNT();
}

NS_IMPL_ADDREF(nsMsgFilterList)
NS_IMPL_RELEASE(nsMsgFilterList)
NS_IMPL_QUERY_INTERFACE1(nsMsgFilterList, nsIMsgFilterList)

NS_IMETHODIMP nsMsgFilterList::CreateFilter(const PRUnichar *name,class nsIMsgFilter **aFilter)
{
	NS_ENSURE_ARG_POINTER(aFilter);

	nsMsgFilter *filter = new nsMsgFilter;
    NS_ENSURE_TRUE(filter, NS_ERROR_OUT_OF_MEMORY);
    
	*aFilter = filter;
    NS_ADDREF(*aFilter);
    
    filter->SetFilterName(name);
    filter->SetFilterList(this);
    
	return NS_OK;
}

NS_IMETHODIMP nsMsgFilterList::SetLoggingEnabled(PRBool enable)
{
	m_loggingEnabled = enable;
	return NS_OK;
}

NS_IMETHODIMP nsMsgFilterList::GetFolder(nsIMsgFolder **aFolder)
{
  NS_ENSURE_ARG(aFolder);
  *aFolder = m_folder;
  NS_IF_ADDREF(*aFolder);
  return NS_OK;
}

NS_IMETHODIMP nsMsgFilterList::SetFolder(nsIMsgFolder *aFolder)
{
  m_folder = aFolder;
  return NS_OK;
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
	return SaveTextFilters(stream);
}

NS_IMETHODIMP
nsMsgFilterList::ApplyFiltersToHdr(nsMsgFilterTypeType filterType,
                                   nsIMsgDBHdr *msgHdr,
                                   nsIMsgFolder *folder,
                                   nsIMsgDatabase *db, 
                                   const char *headers,
                                   PRUint32 headersSize,
                                   nsIMsgFilterHitNotify *listener,
                                   nsIMsgWindow *msgWindow)
{
	nsCOMPtr <nsIMsgFilter>	filter;
	PRUint32		filterCount = 0;
	nsresult		ret = NS_OK;

	GetFilterCount(&filterCount);

	for (PRUint32 filterIndex = 0; filterIndex < filterCount; filterIndex++)
	{
		if (NS_SUCCEEDED(GetFilterAt(filterIndex, getter_AddRefs(filter))))
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

					ret  = listener->ApplyFilterHit(filter, msgWindow, &applyMore);
					if (!NS_SUCCEEDED(ret) || !applyMore)
						break;
				}
			}
		}
	}
	return ret;
}

NS_IMETHODIMP
nsMsgFilterList::SetDefaultFile(nsIFileSpec *aFileSpec)
{
    nsresult rv;
    m_defaultFile = 
        do_CreateInstance(NS_FILESPEC_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = m_defaultFile->FromFileSpec(aFileSpec);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

NS_IMETHODIMP
nsMsgFilterList::GetDefaultFile(nsIFileSpec **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    nsresult rv;
    nsCOMPtr<nsIFileSpec> fileSpec =
        do_CreateInstance(NS_FILESPEC_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = fileSpec->FromFileSpec(m_defaultFile);
    NS_ENSURE_SUCCESS(rv, rv);
    
    *aResult = fileSpec;
    NS_ADDREF(*aResult);

    return NS_OK;
}

NS_IMETHODIMP
nsMsgFilterList::SaveToDefaultFile()
{
    nsresult rv;
    nsCOMPtr<nsIMsgFilterService> filterService =
        do_GetService(kMsgFilterServiceCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    return filterService->SaveFilterList(this, m_defaultFile);
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
	nsMsgFilterFileAttribValue	attrib;
	const char			*attribName;
} FilterFileAttribEntry;

static FilterFileAttribEntry FilterFileAttribTable[] =
{
	{nsIMsgFilterList::attribNone,			""},
	{nsIMsgFilterList::attribVersion,		"version"},
	{nsIMsgFilterList::attribLogging,		"logging"},
	{nsIMsgFilterList::attribName,			"name"},
	{nsIMsgFilterList::attribEnabled,		"enabled"},
	{nsIMsgFilterList::attribDescription,	"description"},
	{nsIMsgFilterList::attribType,			"type"},
	{nsIMsgFilterList::attribScriptFile,	"scriptName"},
	{nsIMsgFilterList::attribAction,		"action"},
	{nsIMsgFilterList::attribActionValue,	"actionValue"},
	{nsIMsgFilterList::attribCondition,		"condition"}
};

// If we want to buffer file IO, wrap it in here.
char nsMsgFilterList::ReadChar(nsIOFileStream *aStream)
{
	char	newChar;
	*aStream >> newChar; 
	return (aStream->eof() ? -1 : newChar);
}

char nsMsgFilterList::SkipWhitespace(nsIOFileStream *aStream)
{
	char ch;
	do
	{
		ch = ReadChar(aStream);
	} while (nsCRT::IsAsciiSpace((PRUnichar)ch));
	return ch;
}

PRBool nsMsgFilterList::StrToBool(nsCString &str)
{
	return str.Equals("yes") ;
}

char nsMsgFilterList::LoadAttrib(nsMsgFilterFileAttribValue &attrib, nsIOFileStream *aStream)
{
	char	attribStr[100];
	char	curChar;
	
	curChar = SkipWhitespace(aStream);
	int i;
	for (i = 0; i + 1 < (int)(sizeof(attribStr)); )
	{
		if (curChar == (char) -1 || nsCRT::IsAsciiSpace((PRUnichar)curChar) || curChar == '=')
			break;
		attribStr[i++] = curChar;
		curChar = ReadChar(aStream);
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

const char *nsMsgFilterList::GetStringForAttrib(nsMsgFilterFileAttribValue attrib)
{
	for (int tableIndex = 0; tableIndex < (int)(sizeof(FilterFileAttribTable) / sizeof(FilterFileAttribTable[0])); tableIndex++)
	{
		if (attrib == FilterFileAttribTable[tableIndex].attrib)
			return FilterFileAttribTable[tableIndex].attribName;
	}
	return nsnull;
}

nsresult nsMsgFilterList::LoadValue(nsCString &value, nsIOFileStream *aStream)
{
	nsCAutoString	valueStr;
	char	curChar;
	value = "";
	curChar = SkipWhitespace(aStream);
	if (curChar != '"')
	{
		NS_ASSERTION(PR_FALSE, "expecting quote as start of value");
		return NS_MSG_FILTER_PARSE_ERROR;
	}
	curChar = ReadChar(aStream);
	do
	{
		if (curChar == '\\')
		{
			char nextChar = ReadChar(aStream);
			if (nextChar == '"')
				curChar = '"';
			else if (nextChar == '\\')	// replace "\\" with "\"
			{
				curChar = ReadChar(aStream);
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
		curChar = ReadChar(aStream);
	}
	while (!aStream->eof());
	return NS_OK;
}

nsresult nsMsgFilterList::LoadTextFilters(nsIOFileStream *aStream)
{
	nsresult	err = NS_OK;
	nsMsgFilterFileAttribValue attrib;
    nsCOMPtr<nsIImportService> impSvc;
	// We'd really like to move lot's of these into the objects that they refer to.
	aStream->seek(PR_SEEK_SET, 0);
	do 
	{
		nsCAutoString	value;
		PRInt32 intToStringResult;

		char curChar;

        curChar = LoadAttrib(attrib, aStream);
		if (attrib == nsIMsgFilterList::attribNone)
			break;
		err = LoadValue(value, aStream);
		if (err != NS_OK)
			break;
		switch(attrib)
		{
		case nsIMsgFilterList::attribNone:
			break;
		case nsIMsgFilterList::attribVersion:
			m_fileVersion = value.ToInteger(&intToStringResult, 10);
			if (intToStringResult != 0)
			{
				attrib = nsIMsgFilterList::attribNone;
				NS_ASSERTION(PR_FALSE, "error parsing filter file version");
			}
            if (m_fileVersion == k45Version)
            {
              impSvc = do_GetService(NS_IMPORTSERVICE_CONTRACTID);
              NS_ASSERTION(impSvc, "cannot get importService");
            }
 			break;
		case nsIMsgFilterList::attribLogging:
			m_loggingEnabled = StrToBool(value);
			break;
		case nsIMsgFilterList::attribName:
		{
			nsMsgFilter *filter = new nsMsgFilter;
			if (filter == nsnull)
			{
				err = NS_ERROR_OUT_OF_MEMORY;
				break;
			}
			filter->SetFilterList(NS_STATIC_CAST(nsIMsgFilterList*,this));
            if (m_fileVersion == k45Version && impSvc)
            {
                nsAutoString unicodeStr;
                impSvc->SystemStringToUnicode(value.get(), unicodeStr);
                filter->SetFilterName(unicodeStr.get());
            }
            else
            {  
              PRUnichar *unicodeString =
                nsTextFormatter::smprintf(unicodeFormatter, value.get());
			  filter->SetFilterName(unicodeString);
              nsTextFormatter::smprintf_free(unicodeString);
            }
			m_curFilter = filter;
			m_filters->AppendElement(NS_STATIC_CAST(nsISupports*,filter));
		}
			break;
		case nsIMsgFilterList::attribEnabled:
			if (m_curFilter)
				m_curFilter->SetEnabled(StrToBool(value));
			break;
		case nsIMsgFilterList::attribDescription:
			if (m_curFilter)
				m_curFilter->SetFilterDesc(value.get());
			break;
		case nsIMsgFilterList::attribType:
			if (m_curFilter)
			{
				m_curFilter->SetType((nsMsgFilterTypeType) value.ToInteger(&intToStringResult, 10));
			}
			break;
		case nsIMsgFilterList::attribScriptFile:
			if (m_curFilter)
				m_curFilter->SetFilterScript(&value);
			break;
		case nsIMsgFilterList::attribAction:
            if (m_curFilter)
              m_curFilter->m_action.m_type = nsMsgFilter::GetActionForFilingStr(value);
            break;
		case nsIMsgFilterList::attribActionValue:
            if (m_curFilter)
            {
              if (m_curFilter->m_action.m_type == nsMsgFilterAction::MoveToFolder)
                err = m_curFilter->ConvertMoveToFolderValue(value);
              else if (m_curFilter->m_action.m_type == nsMsgFilterAction::ChangePriority)
              {
                nsMsgPriorityValue outPriority;
                nsresult res = NS_MsgGetPriorityFromString(value.get(), &outPriority);
                if (NS_SUCCEEDED(res))
                {
                  m_curFilter->SetAction(m_curFilter->m_action.m_type);
                  m_curFilter->SetActionPriority(outPriority);
                }
                else
                  NS_ASSERTION(PR_FALSE, "invalid priority in filter file");
              }
            }
            break;
		case nsIMsgFilterList::attribCondition:
            if (m_curFilter)
            {
              if ( m_fileVersion == k45Version && impSvc)
              {
                nsAutoString unicodeStr;
                impSvc->SystemStringToUnicode(value.get(), unicodeStr);
                char *utf8 = ToNewUTF8String(unicodeStr);
                value.Assign(utf8);
                nsMemory::Free(utf8);
              }
              err = ParseCondition(value);
            }
            break;
		}
	} while (attrib != nsIMsgFilterList::attribNone);
	return err;
}

// parse condition like "(subject, contains, fred) AND (body, isn't, "foo)")"
// values with close parens will be quoted.
// what about values with close parens and quotes? e.g., (body, isn't, "foo")")
// I guess interior quotes will need to be escaped - ("foo\")")
// which will get written out as (\"foo\\")\") and read in as ("foo\")"
nsresult nsMsgFilterList::ParseCondition(nsCString &value)
{
	PRBool	done = PR_FALSE;
	nsresult	err = NS_OK;
	const char *curPtr = value.get();
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
                    m_curFilter->AppendTerm(newTerm);
            }
			PR_FREEIF(termDup);
		}
		else
			break;
	}
	return err;
}

nsresult nsMsgFilterList::WriteIntAttr(nsMsgFilterFileAttribValue attrib, int value, nsIOFileStream *aStream)
{
	const char *attribStr = GetStringForAttrib(attrib);
	if (attribStr)
	{
		*aStream << attribStr;
		*aStream << "=\"";
		*aStream << value;
		*aStream << "\"" MSG_LINEBREAK;
	}
//		XP_FilePrintf(fid, "%s=\"%d\"%s", attribStr, value, LINEBREAK);
	return NS_OK;
}

nsresult
nsMsgFilterList::WriteStrAttr(nsMsgFilterFileAttribValue attrib,
                              const char *str, nsIOFileStream *aStream)
{
	if (str && str[0] && aStream) // only proceed if we actually have a string to write out. 
	{
		char *escapedStr = nsnull;
		if (PL_strchr(str, '"'))
			escapedStr = nsMsgSearchTerm::EscapeQuotesInStr(str);

		const char *attribStr = GetStringForAttrib(attrib);
		if (attribStr)
		{
			*aStream << attribStr;
			*aStream << "=\"";
			*aStream << ((escapedStr) ? escapedStr : (const char *) str);
			*aStream << "\"" MSG_LINEBREAK;
//			XP_FilePrintf(fid, "%s=\"%s\"%s", attribStr, (escapedStr) ? escapedStr : str, LINEBREAK);
		}
		PR_FREEIF(escapedStr);
	}
	return NS_OK;
}

nsresult nsMsgFilterList::WriteBoolAttr(nsMsgFilterFileAttribValue attrib, PRBool boolVal, nsIOFileStream *aStream)
{
	nsCString strToWrite((boolVal) ? "yes" : "no");
	return WriteStrAttr(attrib, strToWrite, aStream);
}

nsresult
nsMsgFilterList::WriteWstrAttr(nsMsgFilterFileAttribValue attrib,
                               const PRUnichar *aFilterName, nsIOFileStream *aStream)
{
    WriteStrAttr(attrib, NS_ConvertUCS2toUTF8(aFilterName).get(), aStream);
    return NS_OK;
}

nsresult nsMsgFilterList::SaveTextFilters(nsIOFileStream *aStream)
{
	nsresult	err = NS_OK;
	const char *attribStr;
	PRUint32			filterCount;
	m_filters->Count(&filterCount);

	attribStr = GetStringForAttrib(nsIMsgFilterList::attribVersion);
	err = WriteIntAttr(nsIMsgFilterList::attribVersion, kFileVersion, aStream);
	err = WriteBoolAttr(nsIMsgFilterList::attribLogging, m_loggingEnabled, aStream);
	for (PRUint32 i = 0; i < filterCount; i ++)
	{
		nsMsgFilter *filter;
		if (GetMsgFilterAt(i, &filter) == NS_OK && filter != nsnull)
		{
			filter->SetFilterList(this);
			if ((err = filter->SaveToTextFile(aStream)) != NS_OK)
				break;
			NS_RELEASE(filter);
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
			PR_ASSERT(status >= 0);
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
    NS_ENSURE_ARG_POINTER(filter);
    
	PRUint32			filterCount;
	m_filters->Count(&filterCount);
    NS_ENSURE_ARG(filterCount >= filterIndex);

	return m_filters->QueryElementAt(filterIndex, NS_GET_IID(nsIMsgFilter),
                                     (void **)filter);
}

nsresult
nsMsgFilterList::GetFilterNamed(const PRUnichar *aName, nsIMsgFilter **aResult)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(aName);
    NS_ENSURE_ARG_POINTER(aResult);
    PRUint32 count=0;
    m_filters->Count(&count);

    *aResult = nsnull;
    PRUint32 i;
    for (i=0; i<count; i++) {
        nsCOMPtr<nsISupports> filterSupports;
        rv = m_filters->GetElementAt(i, getter_AddRefs(filterSupports));
        if (NS_FAILED(rv)) continue;
        
        // cast is safe because array is private
        nsIMsgFilter *filter = (nsIMsgFilter *)filterSupports.get();
        nsXPIDLString filterName;
        filter->GetFilterName(getter_Copies(filterName));
        if (nsCRT::strcmp(filterName, aName) == 0) {
            *aResult = filter;
            break;
        }
    }

    NS_IF_ADDREF(*aResult);
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

nsresult
nsMsgFilterList::RemoveFilter(nsIMsgFilter *aFilter)
{
    return m_filters->RemoveElement(NS_STATIC_CAST(nsISupports*, aFilter));
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
    NS_ENSURE_ARG((motion == nsMsgFilterMotion::up) ||
                  (motion == nsMsgFilterMotion::down));

    nsresult rv;

	PRUint32			filterCount;
	m_filters->Count(&filterCount);
    
    NS_ENSURE_ARG(filterCount >= filterIndex);

    PRUint32 newIndex = filterIndex;
    
	if (motion == nsMsgFilterMotion::up)
	{
        newIndex = filterIndex - 1;

        // are we already at the top?
		if (filterIndex == 0) return NS_OK;
	}
	else if (motion == nsMsgFilterMotion::down)
	{
        newIndex = filterIndex + 1;
        
        // are we already at the bottom?
		if (newIndex > filterCount - 1) return NS_OK;
	}
    m_filters->MoveElement(filterIndex,newIndex);
	return NS_OK;
}

nsresult nsMsgFilterList::MoveFilter(nsIMsgFilter *aFilter,
                                     nsMsgFilterMotionValue motion)
{
    nsresult rv;

    PRInt32 filterIndex;
    rv = m_filters->GetIndexOf(NS_STATIC_CAST(nsISupports*,aFilter),
                               &filterIndex);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_ARG(filterIndex >= 0);
        

    return MoveFilterAt(filterIndex, motion);
}

nsresult
nsMsgFilterList::GetVersion(PRInt16 *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = m_fileVersion;
    return NS_OK;
}

NS_IMETHODIMP nsMsgFilterList::MatchOrChangeFilterTarget(const char *oldFolderUri, const char *newFolderUri, PRBool caseInsensitive, PRBool *found)
{
  nsresult rv = NS_OK;
  PRUint32 numFilters;
  rv = m_filters->Count(&numFilters);
  NS_ENSURE_SUCCESS(rv,rv);
  nsCOMPtr <nsIMsgFilter> filter;
  nsMsgRuleActionType actionType;
  nsXPIDLCString folderUri;
  nsCOMPtr <nsISupports> filterSupports;
  for (PRUint32 index = 0; index < numFilters; index++)
  {
    filterSupports = getter_AddRefs(m_filters->ElementAt(index));
    filter = do_QueryInterface(filterSupports, &rv);
    if (NS_SUCCEEDED(rv) && filter)
    {
      rv = filter->GetAction(&actionType);
      if (NS_SUCCEEDED(rv) && actionType == nsMsgFilterAction::MoveToFolder)
      {
        rv = filter->GetActionTargetFolderUri(getter_Copies(folderUri));
        if (NS_SUCCEEDED(rv) && folderUri)
          if (caseInsensitive)
          {
            if (PL_strcasecmp(folderUri,oldFolderUri) == 0 ) //local
            {
              if (newFolderUri)  //in the case where we just want to match the uri's newFolderUri will be null
                rv = filter->SetActionTargetFolderUri(newFolderUri);
              NS_ENSURE_SUCCESS(rv,rv);
              *found =PR_TRUE;
            }
          }
          else
          {
            if (PL_strcmp(folderUri,oldFolderUri) == 0 )  //imap
            {
              if (newFolderUri) //in the case where we just want to match the uri's newFolderUri will be null
                rv = filter->SetActionTargetFolderUri(newFolderUri);
              NS_ENSURE_SUCCESS(rv,rv);
              *found =PR_TRUE;
            }
          }
      }
    }
  }
  return rv;
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
		{
			filter->Dump();
			NS_RELEASE(filter);
		}
	}

}
#endif

// ------------ End FilterList methods ------------------
