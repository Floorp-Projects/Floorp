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

#include "msgCore.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"

#include "nsMsgSearchCore.h"
#include "nsIMsgSearchSession.h"
#include "nsMsgUtils.h"
#include "nsIMsgDatabase.h"
#include "nsMsgSearchTerm.h"
#include "nsMsgSearchScopeTerm.h"
#include "nsMsgBodyHandler.h"
#include "nsMsgResultElement.h"
#include "nsIMsgImapMailFolder.h"
#include "nsMsgSearchImap.h"
#include "nsMsgLocalSearch.h"
#include "nsMsgSearchNews.h"
#include "nsMsgSearchValue.h"
#include "nsMsgI18N.h"
#include "nsIMimeConverter.h"
#include "nsMsgMimeCID.h"
#include "nsTime.h"
#include "nsIPref.h"

//---------------------------------------------------------------------------
// nsMsgSearchTerm specifies one criterion, e.g. name contains phil
//---------------------------------------------------------------------------


//-----------------------------------------------------------------------------
//-------------------- Implementation of nsMsgSearchTerm -----------------------
//-----------------------------------------------------------------------------
#define MAILNEWS_CUSTOM_HEADERS "mailnews.customHeaders"

typedef struct
{
	nsMsgSearchAttribValue	attrib;
	const char			*attribName;
} nsMsgSearchAttribEntry;

nsMsgSearchAttribEntry SearchAttribEntryTable[] =
{
    {nsMsgSearchAttrib::Subject,		"subject"},
    {nsMsgSearchAttrib::Sender,		"from"},
    {nsMsgSearchAttrib::Body,		"body"},
    {nsMsgSearchAttrib::Date,		"date"},
    {nsMsgSearchAttrib::Priority,	"priority"},
    {nsMsgSearchAttrib::MsgStatus,	"status"},	
    {nsMsgSearchAttrib::To,			"to"},
    {nsMsgSearchAttrib::CC,			"cc"},
    {nsMsgSearchAttrib::ToOrCC,		"to or cc"}
};

// Take a string which starts off with an attribute
// return the matching attribute. If the string is not in the table, then we can conclude that it is an arbitrary header
nsresult NS_MsgGetAttributeFromString(const char *string, PRInt16 *attrib)
{
	if (NULL == string || NULL == attrib)
		return NS_ERROR_NULL_POINTER;
	PRBool found = PR_FALSE;
	for (int idxAttrib = 0; idxAttrib < (int)(sizeof(SearchAttribEntryTable) / sizeof(nsMsgSearchAttribEntry)); idxAttrib++)
	{
		if (!PL_strcasecmp(string, SearchAttribEntryTable[idxAttrib].attribName))
		{
			found = PR_TRUE;
			*attrib = SearchAttribEntryTable[idxAttrib].attrib;
			break;
		}
	}  
	if (!found)
  {
    nsresult rv;
    *attrib = nsMsgSearchAttrib::OtherHeader;
    nsCOMPtr <nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
    nsXPIDLCString headers;
    if (NS_SUCCEEDED(rv) && prefs)
      prefs->GetCharPref(MAILNEWS_CUSTOM_HEADERS, getter_Copies(headers));
    if (headers)
    {
      char *headersString = ToNewCString(headers);
      char *newStr=nsnull;
      char *token = nsCRT::strtok(headersString,": ", &newStr);
      PRUint32 i=1;  //headers start from 50 onwards...
      while (token)
      {
        if (nsCRT::strcasecmp(token, string) == 0)
        {
          *attrib = nsMsgSearchAttrib::OtherHeader+i;
          break;
        }
        token = nsCRT::strtok(newStr,": ", &newStr);
        i++;
      }
      nsMemory::Free(headersString);
    }
  }	
	return NS_OK;      // we always succeed now
}

nsresult NS_MsgGetStringForAttribute(PRInt16 attrib, const char **string)
{
	if (NULL == string)
		return NS_ERROR_NULL_POINTER;
	PRBool found = PR_FALSE;
	for (int idxAttrib = 0; idxAttrib < (int)(sizeof(SearchAttribEntryTable) / sizeof(nsMsgSearchAttribEntry)); idxAttrib++)
	{
		// I'm using the idx's as aliases into MSG_SearchAttribute and 
		// MSG_SearchOperator enums which is legal because of the way the
		// enums are defined (starts at 0, numItems at end)
		if (attrib == SearchAttribEntryTable[idxAttrib].attrib)
		{
			found = PR_TRUE;
			*string = SearchAttribEntryTable[idxAttrib].attribName;
			break;
		}
	}
	// we no longer return invalid attribute. If we cannot find the string in the table, 
	// then it is an arbitrary header. Return success regardless if found or not
//	return (found) ? SearchError_Success : SearchError_InvalidAttribute;
	return NS_OK;
}

typedef struct
{
	nsMsgSearchOpValue  op;
	const char			*opName;
} nsMsgSearchOperatorEntry;

nsMsgSearchOperatorEntry SearchOperatorEntryTable[] =
{
	{nsMsgSearchOp::Contains,	"contains"},
    {nsMsgSearchOp::DoesntContain,"doesn't contain"},
    {nsMsgSearchOp::Is,           "is"},
    {nsMsgSearchOp::Isnt,		"isn't"},
    {nsMsgSearchOp::IsEmpty,		"is empty"},
	{nsMsgSearchOp::IsBefore,    "is before"},
    {nsMsgSearchOp::IsAfter,		"is after"},
    {nsMsgSearchOp::IsHigherThan, "is higher than"},
    {nsMsgSearchOp::IsLowerThan,	"is lower than"},
    {nsMsgSearchOp::BeginsWith,  "begins with"},
	{nsMsgSearchOp::EndsWith,	"ends with"}
};

nsresult NS_MsgGetOperatorFromString(const char *string, PRInt16 *op)
{
	if (NULL == string || NULL == op)
		return NS_ERROR_NULL_POINTER;
	
	PRBool found = PR_FALSE;
	for (unsigned int idxOp = 0; idxOp < sizeof(SearchOperatorEntryTable) / sizeof(nsMsgSearchOperatorEntry); idxOp++)
	{
		// I'm using the idx's as aliases into MSG_SearchAttribute and 
		// MSG_SearchOperator enums which is legal because of the way the
		// enums are defined (starts at 0, numItems at end)
		if (!PL_strcasecmp(string, SearchOperatorEntryTable[idxOp].opName))
		{
			found = PR_TRUE;
			*op = SearchOperatorEntryTable[idxOp].op;
			break;
		}
	}
	return (found) ? NS_OK : NS_ERROR_INVALID_ARG;
}

nsresult NS_MsgGetStringForOperator(PRInt16 op, const char **string)
{
	if (NULL == string)
		return NS_ERROR_NULL_POINTER;
	PRBool found = PR_FALSE;
	for (unsigned int idxOp = 0; idxOp < sizeof(SearchOperatorEntryTable) / sizeof(nsMsgSearchOperatorEntry); idxOp++)
	{
		// I'm using the idx's as aliases into MSG_SearchAttribute and 
		// MSG_SearchOperator enums which is legal because of the way the
		// enums are defined (starts at 0, numItems at end)
		if (op == SearchOperatorEntryTable[idxOp].op)
		{
			found = PR_TRUE;
			*string = SearchOperatorEntryTable[idxOp].opName;
			break;
		}
	}

	return (found) ? NS_OK : NS_ERROR_INVALID_ARG;
}

void NS_MsgGetUntranslatedStatusName (uint32 s, nsCString *outName)
{
	char *tmpOutName = NULL;
#define MSG_STATUS_MASK (MSG_FLAG_READ | MSG_FLAG_REPLIED | MSG_FLAG_FORWARDED | MSG_FLAG_NEW)
	PRUint32 maskOut = (s & MSG_STATUS_MASK);

	// diddle the flags to pay attention to the most important ones first, if multiple
	// flags are set. Should remove this code from the winfe.
	if (maskOut & MSG_FLAG_NEW) 
		maskOut = MSG_FLAG_NEW;
	if ( maskOut & MSG_FLAG_REPLIED &&
		 maskOut & MSG_FLAG_FORWARDED ) 
		maskOut = MSG_FLAG_REPLIED|MSG_FLAG_FORWARDED;
	else if ( maskOut & MSG_FLAG_FORWARDED )
		maskOut = MSG_FLAG_FORWARDED;
	else if ( maskOut & MSG_FLAG_REPLIED ) 
		maskOut = MSG_FLAG_REPLIED;

	switch (maskOut)
	{
	case MSG_FLAG_READ:
		tmpOutName = "read";
		break;
	case MSG_FLAG_REPLIED:
		tmpOutName = "replied";
		break;
	case MSG_FLAG_FORWARDED:
		tmpOutName = "forwarded";
		break;
	case MSG_FLAG_FORWARDED|MSG_FLAG_REPLIED:
		tmpOutName = "replied and forwarded";
		break;
	case MSG_FLAG_NEW:
		tmpOutName = "new";
		break;
	default:
		// This is fine, status may be "unread" for example
        break;
	}

	if (tmpOutName)
		*outName = tmpOutName;
}


PRInt32 NS_MsgGetStatusValueFromName(char *name)
{
	if (PL_strcmp("read", name))
		return MSG_FLAG_READ;
	if (PL_strcmp("replied", name))
		return MSG_FLAG_REPLIED;
	if (PL_strcmp("forwarded", name))
		return MSG_FLAG_FORWARDED;
	if (PL_strcmp("replied and forwarded", name))
		return MSG_FLAG_FORWARDED|MSG_FLAG_REPLIED;
	if (PL_strcmp("new", name))
		return MSG_FLAG_NEW;
	return 0;
}


// Needed for DeStream method.
nsMsgSearchTerm::nsMsgSearchTerm()
{
    NS_INIT_REFCNT();

    // initialize this to zero
    m_value.string=nsnull;
    m_value.attribute=0;
    m_value.u.priority=0;
}

#if 0
nsMsgSearchTerm::nsMsgSearchTerm (
	nsMsgSearchAttribute attrib, 
	nsMsgSearchOperator op, 
	nsMsgSearchValue *val,
	PRBool booleanAND,
	char * arbitraryHeader) 
{
    NS_INIT_REFCNT();
	m_operator = op;
	m_booleanOp = (booleanAND) ? nsMsgSearchBooleanOp::BooleanAND : nsMsgSearchBooleanOp::BooleanOR;
	if (attrib > nsMsgSearchAttrib::OtherHeader  && attrib < nsMsgSearchAttrib::kNumMsgSearchAttributes && arbitraryHeader)
		m_arbitraryHeader = arbitraryHeader;
	m_attribute = attrib;

	nsMsgResultElement::AssignValues (val, &m_value);
}
#endif

nsMsgSearchTerm::nsMsgSearchTerm (
	nsMsgSearchAttribValue attrib, 
	nsMsgSearchOpValue op, 
	nsIMsgSearchValue *val,
	nsMsgSearchBooleanOperator boolOp,
	const char * arbitraryHeader) 
{
    NS_INIT_REFCNT();
	m_operator = op;
	m_attribute = attrib;
	m_booleanOp = boolOp;
	if (attrib > nsMsgSearchAttrib::OtherHeader  && attrib < nsMsgSearchAttrib::kNumMsgSearchAttributes && arbitraryHeader)
		m_arbitraryHeader = arbitraryHeader;
	nsMsgResultElement::AssignValues (val, &m_value);
}



nsMsgSearchTerm::~nsMsgSearchTerm ()
{
	if (IS_STRING_ATTRIBUTE (m_attribute) && m_value.string)
		Recycle(m_value.string);
}

NS_IMPL_ISUPPORTS1(nsMsgSearchTerm, nsIMsgSearchTerm)


// Perhaps we could find a better place for this?
// Caller needs to free.
/* static */char *nsMsgSearchTerm::EscapeQuotesInStr(const char *str)
{
	int	numQuotes = 0;
	for (const char *strPtr = str; *strPtr; strPtr++)
		if (*strPtr == '"')
			numQuotes++;
	int escapedStrLen = PL_strlen(str) + numQuotes;
	char	*escapedStr = (char *) PR_Malloc(escapedStrLen + 1);
	if (escapedStr)
	{
		char *destPtr;
		for (destPtr = escapedStr; *str; str++)
		{
			if (*str == '"')
				*destPtr++ = '\\';
			*destPtr++ = *str;
		}
		*destPtr = '\0';
	}
	return escapedStr;
}


nsresult nsMsgSearchTerm::OutputValue(nsCString &outputStr)
{
	if (IS_STRING_ATTRIBUTE(m_attribute) && m_value.string)
	{
		PRBool	quoteVal = PR_FALSE;
		// need to quote strings with ')' and strings starting with '"' or ' '
		// filter code will escape quotes
		if (PL_strchr(m_value.string, ')') ||
			(m_value.string[0] == ' ') ||
			(m_value.string[0] == '"'))
		{
			quoteVal = PR_TRUE;
			outputStr += "\"";
		}
		if (PL_strchr(m_value.string, '"'))
		{
			char *escapedString = nsMsgSearchTerm::EscapeQuotesInStr(m_value.string);
			if (escapedString)
			{
				outputStr += escapedString;
				PR_Free(escapedString);
			}

		}
		else
		{
			outputStr += m_value.string;
		}
		if (quoteVal)
			outputStr += "\"";
	}
	else
	{
	    switch (m_attribute)
		{
		case nsMsgSearchAttrib::Date:
		{
			PRExplodedTime exploded;
			PR_ExplodeTime(m_value.u.date, PR_LocalTimeParameters, &exploded);

			// wow, so tm_mon is 0 based, tm_mday is 1 based.
			char dateBuf[100];
			PR_FormatTimeUSEnglish (dateBuf, sizeof(dateBuf), "%d-%b-%Y", &exploded);
			outputStr += dateBuf;
			break;
		}
		case nsMsgSearchAttrib::MsgStatus:
		{
			nsCAutoString status;
			NS_MsgGetUntranslatedStatusName (m_value.u.msgStatus, &status);
			outputStr += status;
			break;
		}
		case nsMsgSearchAttrib::Priority:
		{
			nsAutoString priority;
			NS_MsgGetUntranslatedPriorityName( m_value.u.priority, 
											 &priority);
			outputStr.AppendWithConversion(priority);
			break;
		}
		default:
			NS_ASSERTION(PR_FALSE, "trying to output invalid attribute");
			break;
		}
	}
	return NS_OK;
}

nsresult nsMsgSearchTerm::EnStreamNew (nsCString &outStream)
{
	const char	*attrib, *operatorStr;
	nsCAutoString	outputStr;
	nsresult	ret;

	ret = NS_MsgGetStringForAttribute(m_attribute, &attrib);
	if (ret != NS_OK)
		return ret;

	if (m_attribute > nsMsgSearchAttrib::OtherHeader && m_attribute < nsMsgSearchAttrib::kNumMsgSearchAttributes)  // if arbitrary header, use it instead!
	{
		outputStr = "\"";
		outputStr += m_arbitraryHeader;
		outputStr += "\"";
	}
	else
		outputStr = attrib;

	outputStr += ',';

	ret = NS_MsgGetStringForOperator(m_operator, &operatorStr);
	if (ret != NS_OK)
		return ret;

	outputStr += operatorStr;
	outputStr += ',';

	OutputValue(outputStr);
	outStream = outputStr;
	return NS_OK;
}

// fill in m_value from the input stream.
nsresult nsMsgSearchTerm::ParseValue(char *inStream)
{
	if (IS_STRING_ATTRIBUTE(m_attribute))
	{
		PRBool	quoteVal = PR_FALSE;
    while (nsString::IsSpace(*inStream))
      inStream++;
		// need to remove pair of '"', if present
		if (*inStream == '"')
		{
			quoteVal = PR_TRUE;
			inStream++;
		}
		int valueLen = PL_strlen(inStream);
		if (quoteVal && inStream[valueLen - 1] == '"')
			valueLen--;

		m_value.string = (char *) PR_Malloc(valueLen + 1);
		PL_strncpy(m_value.string, inStream, valueLen + 1);
		m_value.string[valueLen] = '\0';
	}
	else
	{
	    switch (m_attribute)
		{
		case nsMsgSearchAttrib::Date:
			PR_ParseTimeString (inStream, PR_FALSE, &m_value.u.date);
			break;
		case nsMsgSearchAttrib::MsgStatus:
			m_value.u.msgStatus = NS_MsgGetStatusValueFromName(inStream);
			break;
		case nsMsgSearchAttrib::Priority:
			NS_MsgGetPriorityFromString(inStream, &m_value.u.priority);
			break;
		default:
			NS_ASSERTION(PR_FALSE, "invalid attribute parsing search term value");
			break;
		}
	}
	m_value.attribute = m_attribute;
	return NS_OK;
}

// find the operator code for this operator string.
nsMsgSearchOpValue
nsMsgSearchTerm::ParseOperator(char *inStream)
{
	PRInt16				operatorVal;
	nsresult		err;

	while (nsString::IsSpace(*inStream))
		inStream++;

	char *commaSep = PL_strchr(inStream, ',');

	if (commaSep)
		*commaSep = '\0';

	err = NS_MsgGetOperatorFromString(inStream, &operatorVal);
	return (nsMsgSearchOpValue) operatorVal;
}

// find the attribute code for this comma-delimited attribute. 
nsMsgSearchAttribValue
nsMsgSearchTerm::ParseAttribute(char *inStream)
{
	nsCAutoString			attributeStr;
	PRInt16				attributeVal;
	nsresult		err;

	while (nsString::IsSpace(*inStream))
		inStream++;

	// if we are dealing with an arbitrary header, it may be quoted....
	PRBool quoteVal = PR_FALSE;
	if (*inStream == '"')
	{
		quoteVal = PR_TRUE;
		inStream++;
	}

	char *separator;
	if (quoteVal)      // arbitrary headers are quoted...
		separator = PL_strchr(inStream, '"');
	else
		separator = PL_strchr(inStream, ',');
	
	if (separator)
		*separator = '\0';

	err = NS_MsgGetAttributeFromString(inStream, &attributeVal);
	nsMsgSearchAttribValue attrib = (nsMsgSearchAttribValue) attributeVal;
	
	if (attrib > nsMsgSearchAttrib::OtherHeader && attrib < nsMsgSearchAttrib::kNumMsgSearchAttributes)  // if we are dealing with an arbitrary header....
		m_arbitraryHeader =  inStream;
	
	return attrib;
}

// De stream one search term. If the condition looks like
// condition = "(to or cc, contains, r-thompson) AND (body, doesn't contain, fred)"
// This routine should get called twice, the first time
// with "to or cc, contains, r-thompson", the second time with
// "body, doesn't contain, fred"

nsresult nsMsgSearchTerm::DeStreamNew (char *inStream, PRInt16 /*length*/)
{
	char *commaSep = PL_strchr(inStream, ',');
	m_attribute = ParseAttribute(inStream);  // will allocate space for arbitrary header if necessary
	if (!commaSep)
		return NS_ERROR_INVALID_ARG;
	char *secondCommaSep = PL_strchr(commaSep + 1, ',');
	if (commaSep)
		m_operator = ParseOperator(commaSep + 1);
	if (secondCommaSep)
		ParseValue(secondCommaSep + 1);
	return NS_OK;
}


void nsMsgSearchTerm::StripQuotedPrintable (unsigned char *src)
{
	// decode quoted printable text in place

	unsigned char *dest = src;
	int srcIdx = 0, destIdx = 0;
	
	while (src[srcIdx] != 0)
	{
		if (src[srcIdx] == '=')
		{
			unsigned char *token = &src[srcIdx];
			unsigned char c = 0;

			// decode the first quoted char
			if (token[1] >= '0' && token[1] <= '9')
				c = token[1] - '0';
			else if (token[1] >= 'A' && token[1] <= 'F')
				c = token[1] - ('A' - 10);
			else if (token[1] >= 'a' && token[1] <= 'f')
				c = token[1] - ('a' - 10);
			else
			{
				// first char after '=' isn't hex. copy the '=' as a normal char and keep going
				dest[destIdx++] = src[srcIdx++]; // aka token[0]
				continue;
			}
			
			// decode the second quoted char
			c = (c << 4);
			if (token[2] >= '0' && token[2] <= '9')
				c += token[2] - '0';
			else if (token[2] >= 'A' && token[2] <= 'F')
				c += token[2] - ('A' - 10);
			else if (token[2] >= 'a' && token[2] <= 'f')
				c += token[2] - ('a' - 10);
			else
			{
				// second char after '=' isn't hex. copy the '=' as a normal char and keep going
				dest[destIdx++] = src[srcIdx++]; // aka token[0]
				continue;
			}

			// if we got here, we successfully decoded a quoted printable sequence,
			// so bump each pointer past it and move on to the next char;
			dest[destIdx++] = c; 
			srcIdx += 3;

		}
		else
			dest[destIdx++] = src[srcIdx++];
	}

	dest[destIdx] = src[srcIdx]; // null terminate
}

// Looks in the MessageDB for the user specified arbitrary header, if it finds the header, it then looks for a match against
// the value for the header. 
nsresult nsMsgSearchTerm::MatchArbitraryHeader (nsIMsgSearchScopeTerm *scope,
                                                PRUint32 offset,
                                                PRUint32 length /* in lines*/,
                                                const char *charset,
                                                PRBool charsetOverride,
                                                nsIMsgDBHdr *msg,
                                                nsIMsgDatabase* db,
                                                const char * headers, 
                                                PRUint32 headersSize,
                                                PRBool ForFiltering,
                                                PRBool *pResult)
{
	if (!pResult)
		return NS_ERROR_NULL_POINTER;
	*pResult = PR_FALSE;
	nsresult err = NS_OK;
	PRBool result;

	nsMsgBodyHandler * bodyHandler = new nsMsgBodyHandler (scope, offset,length, msg, db, headers, headersSize, ForFiltering);
	if (!bodyHandler)
		return NS_ERROR_OUT_OF_MEMORY;

	bodyHandler->SetStripHeaders (PR_FALSE);

    GetMatchAllBeforeDeciding(&result);

	const int kBufSize = 512; // max size of a line??
	char * buf = (char *) PR_Malloc(kBufSize);
	if (buf)
	{
		PRBool searchingHeaders = PR_TRUE;
		while (searchingHeaders && (bodyHandler->GetNextLine(buf, kBufSize) >=0))
		{
			char * buf_end = buf + PL_strlen(buf);
			int headerLength = m_arbitraryHeader.Length();
      if (!PL_strncasecmp(buf, m_arbitraryHeader.get(),headerLength))
			{
				char * headerValue = buf + headerLength; // value occurs after the header name...
				if (headerValue < buf_end && headerValue[0] == ':')  // + 1 to account for the colon which is MANDATORY
					headerValue++; 

				// strip leading white space
				while (headerValue < buf_end && nsString::IsSpace(*headerValue))
					headerValue++; // advance to next character
				
				// strip trailing white space
				char * end = buf_end - 1; 
				while (end > headerValue && nsString::IsSpace(*end)) // while we haven't gone back past the start and we are white space....
				{
					*end = '\0';	// eat up the white space
					end--;			// move back and examine the previous character....
				}
					
				if (headerValue < buf_end && *headerValue) // make sure buf has info besides just the header
				{
					PRBool result2;
					err = MatchRfc2047String(headerValue, charset, charsetOverride, &result2);  // match value with the other info...
					if (result != result2) // if we found a match
					{
						searchingHeaders = PR_FALSE;   // then stop examining the headers
						result = result2;
					}
				}
				else
					NS_ASSERTION(PR_FALSE, "error matching arbitrary headers"); // mscott --> i'd be curious if there is a case where this fails....
			}
			if (EMPTY_MESSAGE_LINE(buf))
				searchingHeaders = PR_FALSE;
		}
		delete bodyHandler;
		PR_Free(buf);
		*pResult = result;
		return err;
	}
	else
	{
		delete bodyHandler;
		return NS_ERROR_OUT_OF_MEMORY;
	}
}

nsresult nsMsgSearchTerm::MatchBody (nsIMsgSearchScopeTerm *scope, PRUint32 offset, PRUint32 length /*in lines*/, const char *folderCharset,
										   nsIMsgDBHdr *msg, nsIMsgDatabase* db, PRBool *pResult)
{
	if (!pResult)
		return NS_ERROR_NULL_POINTER;
	nsresult err = NS_OK;
	PRBool result = PR_FALSE;
	*pResult = PR_FALSE;

	// Small hack so we don't look all through a message when someone has
	// specified "BODY IS foo"
	if ((length > 0) && (m_operator == nsMsgSearchOp::Is || m_operator == nsMsgSearchOp::Isnt))
		length = PL_strlen (m_value.string);

	nsMsgBodyHandler * bodyHan  = new nsMsgBodyHandler (scope, offset, length, msg, db);
	if (!bodyHan)
		return NS_ERROR_OUT_OF_MEMORY;

	const int kBufSize = 512; // max size of a line???
	char *buf = (char*) PR_Malloc(kBufSize);
	if (buf)
	{
		PRBool endOfFile = PR_FALSE;  // if retValue == 0, we've hit the end of the file
		uint32 lines = 0;

		// Change the sense of the loop so we don't bail out prematurely
		// on negative terms. i.e. opDoesntContain must look at all lines
		PRBool boolContinueLoop;
        GetMatchAllBeforeDeciding(&boolContinueLoop);
        result = boolContinueLoop;

		// If there's a '=' in the search term, then we're not going to do
		// quoted printable decoding. Otherwise we assume everything is
		// quoted printable. Obviously everything isn't quoted printable, but
		// since we don't have a MIME parser handy, and we want to err on the
		// side of too many hits rather than not enough, we'll assume in that
		// general direction. Blech. ### FIX ME 
		// bug fix #88935: for stateful csids like JIS, we don't want to decode
		// quoted printable since it contains '='.
		PRBool isQuotedPrintable =  /*!(mail_csid & STATEFUL) && */
            (PL_strchr (m_value.string, '=') == nsnull);

		while (!endOfFile && result == boolContinueLoop)
		{
			if (bodyHan->GetNextLine(buf, kBufSize) >= 0)
			{
				// Do in-place decoding of quoted printable
				if (isQuotedPrintable)
					StripQuotedPrintable ((unsigned char*)buf);
			    nsCString  compare(buf);
//				ConvertToUnicode(charset, buf, compare);
				if (compare.Length() > 0) {
					char startChar = (char) compare.CharAt(0);
					if (startChar != nsCRT::CR && startChar != nsCRT::LF)
					{
						err = MatchString (compare.get(), folderCharset, &result);
						lines++; 
					}
				}
			}
			else 
				endOfFile = PR_TRUE;
		}
#ifdef DO_I18N
		if(conv) 
			INTL_DestroyCharCodeConverter(conv);
#endif
		PR_FREEIF(buf);
		delete bodyHan;
	}
	else
		err = NS_ERROR_OUT_OF_MEMORY;
	*pResult = result;
	return err;
}



// *pResult is PR_FALSE when strings don't match, PR_TRUE if they do.
nsresult nsMsgSearchTerm::MatchRfc2047String (const char *rfc2047string,
                                       const char *charset,
                                       PRBool charsetOverride,
                                       PRBool *pResult)
{
	static NS_DEFINE_CID(kCMimeConverterCID, NS_MIME_CONVERTER_CID);

	if (!pResult || !rfc2047string)
		return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIMimeConverter> mimeConverter = do_GetService(kCMimeConverterCID);
	char *stringToMatch = 0;
    nsresult res = mimeConverter->DecodeMimeHeader(rfc2047string,
                                                   &stringToMatch,
                                                   charset, charsetOverride,
                                                   PR_FALSE);

	res = MatchString(stringToMatch ? stringToMatch : rfc2047string,
                      nsnull, pResult);

    PR_FREEIF(stringToMatch);

	return res;
}

// *pResult is PR_FALSE when strings don't match, PR_TRUE if they do.
nsresult nsMsgSearchTerm::MatchString (const char *stringToMatch,
                                       const char *charset,
                                       PRBool *pResult)
{
	if (!pResult)
		return NS_ERROR_NULL_POINTER;
	PRBool result = PR_FALSE;

	nsresult err = NS_OK;
	nsCAutoString n_str;
	const char *utf8 = stringToMatch;
	if(nsMsgSearchOp::IsEmpty != m_operator)	// Save some performance for opIsEmpty
	{
		n_str = m_value.string;
		if (charset != nsnull)
		{
			nsAutoString  srcCharset;
			srcCharset.AssignWithConversion(charset);
			nsString out;
			ConvertToUnicode(srcCharset, stringToMatch ? stringToMatch : "", out);
			utf8 = ToNewUTF8String(out);
		}
	}

	switch (m_operator)
	{
	case nsMsgSearchOp::Contains:
		if ((nsnull != utf8) && ((n_str.get())[0]) && /* INTL_StrContains(csid, n_header, n_str) */
			PL_strcasestr(utf8, n_str.get()))
			result = PR_TRUE;
		break;
	case nsMsgSearchOp::DoesntContain:
		if ((nsnull != utf8) && ((n_str.get())[0]) &&  /* !INTL_StrContains(csid, n_header, n_str) */
			!PL_strcasestr(utf8, n_str.get()))
			result = PR_TRUE;
		break;
	case nsMsgSearchOp::Is:
		if(utf8)
		{
			if ((n_str.get())[0])
			{
				if (n_str.EqualsWithConversion(utf8, PR_TRUE /*ignore case*/) /* INTL_StrIs(csid, n_header, n_str)*/ )
					result = PR_TRUE;
			}
			else if (utf8[0] == '\0') // Special case for "is <the empty string>"
				result = PR_TRUE;
		}
		break;
	case nsMsgSearchOp::Isnt:
		if(utf8)
		{
			if ((n_str.get())[0])
			{
				if (!n_str.EqualsWithConversion(utf8, PR_TRUE)/* INTL_StrIs(csid, n_header, n_str)*/ )
					result = PR_TRUE;
			}
			else if (utf8[0] != '\0') // Special case for "isn't <the empty string>"
				result = PR_TRUE;
		}
		break;
	case nsMsgSearchOp::IsEmpty:
		if (!PL_strlen(utf8))
			result = PR_TRUE;
		break;
	case nsMsgSearchOp::BeginsWith:
#ifdef DO_I18N_YET
		if((nsnull != n_str) && (nsnull != utf8) && INTL_StrBeginWith(csid, utf8, n_str))
			result = PR_TRUE;
#else
		// ### DMB - not the  most efficient way to do this.
		if (PL_strncmp(utf8, n_str.get(), n_str.Length()) == 0)
			result = PR_TRUE;
#endif
		break;
	case nsMsgSearchOp::EndsWith: 
    {
      PRUint32 searchStrLen = (PRUint32) PL_strlen(utf8);
      if (n_str.Length() <= searchStrLen)
      {
        PRInt32 sourceStrOffset = searchStrLen - n_str.Length();
        if (PL_strcmp(utf8 + sourceStrOffset, n_str.get()) == 0)
          result = PR_TRUE;
      }
    }
#ifdef DO_I18N_YET
		{
		if((nsnull != n_str) && (nsnull != utf8) && INTL_StrEndWith(csid, utf8, n_str))
			result = PR_TRUE;
		}
#else

#endif
		break;
	default:
		NS_ASSERTION(PR_FALSE, "invalid operator matching search results");
	}

	if (utf8 != nsnull && utf8 != stringToMatch)
	  free((void *)utf8);

	*pResult = result;
	return err;
}

nsresult nsMsgSearchTerm::GetMatchAllBeforeDeciding (PRBool *aResult)
{
	if (m_operator == nsMsgSearchOp::DoesntContain || m_operator == nsMsgSearchOp::Isnt)
		*aResult = PR_TRUE;
    else
        *aResult = PR_FALSE;

    return NS_OK;
}


nsresult nsMsgSearchTerm::MatchRfc822String (const char *string, const char *charset, PRBool charsetOverride, PRBool *pResult)
{
	if (!pResult)
		return NS_ERROR_NULL_POINTER;
	*pResult = PR_FALSE;
	PRBool result;
	nsresult err = InitHeaderAddressParser();
	if (!NS_SUCCEEDED(err))
		return err;
	// Isolate the RFC 822 parsing weirdnesses here. MSG_ParseRFC822Addresses
	// returns a catenated string of null-terminated strings, which we walk
	// across, tring to match the target string to either the name OR the address

	char *names = nsnull, *addresses = nsnull;

	// Change the sense of the loop so we don't bail out prematurely
	// on negative terms. i.e. opDoesntContain must look at all recipients
	PRBool boolContinueLoop;
    GetMatchAllBeforeDeciding(&boolContinueLoop);
    result = boolContinueLoop;

	PRUint32 count;
	nsresult parseErr = m_headerAddressParser->ParseHeaderAddresses(charset, string, &names, &addresses, &count) ;

	if (NS_SUCCEEDED(parseErr) && count > 0)
	{
		NS_ASSERTION(names, "couldn't get names");
		NS_ASSERTION(addresses, "couldn't get addresses");
		if (!names || !addresses)
			return err;

		nsCAutoString walkNames;
		nsCAutoString walkAddresses;
		PRInt32 namePos = 0;
		PRInt32 addressPos = 0;
		for (PRUint32 i = 0; i < count && result == boolContinueLoop; i++)
		{
			walkNames = names + namePos;
			walkAddresses = addresses + addressPos;;
			err = MatchRfc2047String (walkNames.get(), charset, charsetOverride, &result);
			if (boolContinueLoop == result)
				err = MatchRfc2047String (walkAddresses.get(), charset, charsetOverride, &result);

			namePos += walkNames.Length() + 1;
			addressPos += walkAddresses.Length() + 1;
		}

		PR_FREEIF(names);
		PR_FREEIF(addresses);
	}
	*pResult = result;
	return err;
}


nsresult nsMsgSearchTerm::GetLocalTimes (PRTime a, PRTime b, PRExplodedTime &aExploded, PRExplodedTime &bExploded)
{
	PR_ExplodeTime(a, PR_LocalTimeParameters, &aExploded);
	PR_ExplodeTime(b, PR_LocalTimeParameters, &bExploded);
	return NS_OK;
}


nsresult nsMsgSearchTerm::MatchDate (PRTime dateToMatch, PRBool *pResult)
{
	if (!pResult)
		return NS_ERROR_NULL_POINTER;

	nsresult err = NS_OK;
	PRBool result = PR_FALSE;
	nsTime t_date(dateToMatch);
	
	switch (m_operator)
	{
	case nsMsgSearchOp::IsBefore:
		if (t_date < nsTime(m_value.u.date))
			result = PR_TRUE;
		break;
	case nsMsgSearchOp::IsAfter:
		{
			nsTime adjustedDate = nsTime(m_value.u.date);
			adjustedDate += 60*60*24; // we want to be greater than the next day....
			if (t_date > adjustedDate)
				result = PR_TRUE;
		}
		break;
	case nsMsgSearchOp::Is:
		{
			PRExplodedTime tmToMatch, tmThis;
			if (NS_OK == GetLocalTimes (dateToMatch, m_value.u.date, tmToMatch, tmThis))
			{
				if (tmThis.tm_year == tmToMatch.tm_year &&
					tmThis.tm_month == tmToMatch.tm_month &&
					tmThis.tm_mday == tmToMatch.tm_mday)
					result = PR_TRUE;
			}
		}
		break;
	case nsMsgSearchOp::Isnt:
		{
			PRExplodedTime tmToMatch, tmThis;
			if (NS_OK == GetLocalTimes (dateToMatch, m_value.u.date, tmToMatch, tmThis))
			{
				if (tmThis.tm_year != tmToMatch.tm_year ||
					tmThis.tm_month != tmToMatch.tm_month ||
					tmThis.tm_mday != tmToMatch.tm_mday)
					result = PR_TRUE;
			}
		}
		break;
	default:
		NS_ASSERTION(PR_FALSE, "invalid compare op for dates");
	}
	*pResult = result;
	return err;
}


nsresult nsMsgSearchTerm::MatchAge (PRTime msgDate, PRBool *pResult)
{
  if (!pResult)
    return NS_ERROR_NULL_POINTER;

  PRBool result = PR_FALSE;
  nsresult err = NS_OK;

  PRTime now = PR_Now();
  PRTime cutOffDay;

  PRInt64 microSecondsPerSecond, secondsInDays, microSecondsInDays;
	
  LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
  LL_UI2L(secondsInDays, 60 * 60 * 24 * m_value.u.age);
  LL_MUL(microSecondsInDays, secondsInDays, microSecondsPerSecond);

  LL_SUB(cutOffDay, now, microSecondsInDays); // = now - term->m_value.u.age * 60 * 60 * 24; 
  // so now cutOffDay is the PRTime cut-off point. Any msg with a time less than that will be past the age .

  switch (m_operator)
  {
    case nsMsgSearchOp::IsGreaterThan: // is older than 
      if (LL_CMP(msgDate, <, cutOffDay))
        result = PR_TRUE;
      break;
    case nsMsgSearchOp::IsLessThan: // is younger than 
      if (LL_CMP(msgDate, >, cutOffDay))
        result = PR_TRUE;
      break;
    case nsMsgSearchOp::Is:
      PRExplodedTime msgDateExploded;
      PRExplodedTime cutOffDayExploded;
      if (NS_SUCCEEDED(GetLocalTimes(msgDate, cutOffDay, msgDateExploded, cutOffDayExploded)))
      {
        if ((msgDateExploded.tm_mday == cutOffDayExploded.tm_mday) &&
            (msgDateExploded.tm_month == cutOffDayExploded.tm_month) &&
            (msgDateExploded.tm_year == cutOffDayExploded.tm_year))
          result = PR_TRUE;
      }
      break;
    default:
      NS_ASSERTION(PR_FALSE, "invalid compare op comparing msg age");
  }
  *pResult = result;
  return err;
}


nsresult nsMsgSearchTerm::MatchSize (PRUint32 sizeToMatch, PRBool *pResult)
{
	if (!pResult)
		return NS_ERROR_NULL_POINTER;

	PRBool result = PR_FALSE;
	switch (m_operator)
	{
	case nsMsgSearchOp::IsHigherThan:
		if (sizeToMatch > m_value.u.size)
			result = PR_TRUE;
		break;
	case nsMsgSearchOp::IsLowerThan:
		if (sizeToMatch < m_value.u.size)
			result = PR_TRUE;
		break;
	default:
		break;
	}
	*pResult = result;
	return NS_OK;
}


nsresult nsMsgSearchTerm::MatchStatus (PRUint32 statusToMatch, PRBool *pResult)
{
	if (!pResult)
		return NS_ERROR_NULL_POINTER;

	nsresult err = NS_OK;
	PRBool matches = PR_FALSE;

	if (statusToMatch & m_value.u.msgStatus)
		matches = PR_TRUE;

	switch (m_operator)
	{
	case nsMsgSearchOp::Is:
		break;
	case nsMsgSearchOp::Isnt:
		matches = !matches;
		break;
	default:
		err = NS_ERROR_FAILURE;
		NS_ASSERTION(PR_FALSE, "invalid comapre op for msg status");
	}

  *pResult = matches;
	return err;	
}


nsresult
nsMsgSearchTerm::MatchPriority (nsMsgPriorityValue priorityToMatch,
                                PRBool *pResult)
{
	if (!pResult)
		return NS_ERROR_NULL_POINTER;

	nsresult err = NS_OK;
	PRBool result=NS_OK;

	// Use this ugly little hack to get around the fact that enums don't have
	// integer compare operators
	int p1 = (priorityToMatch == nsMsgPriority::none) ? (int) nsMsgPriority::normal : (int) priorityToMatch;
	int p2 = (int) m_value.u.priority;

	switch (m_operator)
	{
	case nsMsgSearchOp::IsHigherThan:
		if (p1 > p2)
			result = PR_TRUE;
		break;
	case nsMsgSearchOp::IsLowerThan:
		if (p1 < p2)
			result = PR_TRUE;
		break;
	case nsMsgSearchOp::Is:
		if (p1 == p2)
			result = PR_TRUE;
		break;
	default:
		result = PR_FALSE;
		err = NS_ERROR_FAILURE;
		NS_ASSERTION(PR_FALSE, "invalid match operator");
	}
	*pResult = result;
	return err;
}

// Lazily initialize the rfc822 header parser we're going to use to do
// header matching.
nsresult nsMsgSearchTerm::InitHeaderAddressParser()
{
	nsresult res = NS_OK;
	NS_DEFINE_CID(kMsgHeaderParserCID, NS_MSGHEADERPARSER_CID);
    
	if (!m_headerAddressParser)
	{
    m_headerAddressParser = do_GetService(kMsgHeaderParserCID, &res);
	}
	return res;
}

NS_IMETHODIMP
nsMsgSearchTerm::GetAttrib(nsMsgSearchAttribValue *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = m_attribute;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchTerm::SetAttrib(nsMsgSearchAttribValue aValue)
{
    m_attribute = aValue;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchTerm::GetOp(nsMsgSearchOpValue *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = m_operator;
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchTerm::SetOp(nsMsgSearchOpValue aValue)
{
    m_operator = aValue;
    return NS_OK;
}


NS_IMETHODIMP
nsMsgSearchTerm::GetValue(nsIMsgSearchValue **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = new nsMsgSearchValueImpl(&m_value);
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchTerm::SetValue(nsIMsgSearchValue* aValue)
{
	nsMsgResultElement::AssignValues (aValue, &m_value);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchTerm::GetBooleanAnd(PRBool *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = (m_booleanOp == nsMsgSearchBooleanOp::BooleanAND);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchTerm::SetBooleanAnd(PRBool aValue)
{
    if (aValue)
        m_booleanOp = nsMsgSearchBooleanOperator(nsMsgSearchBooleanOp::BooleanAND);
    else
        m_booleanOp = nsMsgSearchBooleanOperator(nsMsgSearchBooleanOp::BooleanOR);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchTerm::GetArbitraryHeader(char* *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = ToNewCString(m_arbitraryHeader);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchTerm::SetArbitraryHeader(const char* aValue)
{
    m_arbitraryHeader = aValue;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsMsgSearchScopeTerm implementation
//-----------------------------------------------------------------------------
nsMsgSearchScopeTerm::nsMsgSearchScopeTerm (nsIMsgSearchSession *session,
                                            nsMsgSearchScopeValue attribute,
                                            nsIMsgFolder *folder)
{
	NS_INIT_REFCNT();
	m_attribute = attribute;
	m_folder = folder;
	m_searchServer = PR_TRUE;
    m_searchSession = getter_AddRefs(NS_GetWeakReference(session));
}

nsMsgSearchScopeTerm::nsMsgSearchScopeTerm ()
{
	NS_INIT_REFCNT();
	m_searchServer = PR_TRUE;
}

nsMsgSearchScopeTerm::~nsMsgSearchScopeTerm ()
{  
}

NS_IMPL_ISUPPORTS1(nsMsgSearchScopeTerm, nsIMsgSearchScopeTerm)

NS_IMETHODIMP
nsMsgSearchScopeTerm::GetFolder(nsIMsgFolder **aResult)
{
    *aResult = m_folder;
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchScopeTerm::GetSearchSession(nsIMsgSearchSession** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    nsCOMPtr<nsIMsgSearchSession> searchSession = do_QueryReferent (m_searchSession);
    *aResult = searchSession;
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}

nsresult nsMsgSearchScopeTerm::GetMailPath(nsIFileSpec **aFileSpec)
{
	return (m_folder) ? m_folder->GetPath(aFileSpec) : NS_ERROR_NULL_POINTER;
}

nsresult nsMsgSearchScopeTerm::TimeSlice (PRBool *aDone)
{
	return m_adapter->Search(aDone);
}


nsresult nsMsgSearchScopeTerm::InitializeAdapter (nsISupportsArray *termList)
{
  if (m_adapter)
    return NS_OK;
  
  nsresult err = NS_OK;
  
  switch (m_attribute)
  {
    case nsMsgSearchScope::onlineMail:    
        m_adapter = new nsMsgSearchOnlineMail (this, termList);
      break;
    case nsMsgSearchScope::offlineMail:
        m_adapter = new nsMsgSearchOfflineMail (this, termList);
      break;
    case nsMsgSearchScope::newsEx:
#ifdef DOING_EXNEWSSEARCH
        if (m_folder->KnowsSearchNntpExtension())
          m_adapter = new nsMsgSearchNewsEx (this, termList);
        else
          m_adapter = new nsMsgSearchNews(this, termList);
#endif
      NS_ASSERTION(PR_FALSE, "not supporting newsEx yet");
      break;
    case nsMsgSearchScope::news:
          m_adapter = new nsMsgSearchNews (this, termList);
        break;
    case nsMsgSearchScope::allSearchableGroups:
#ifdef DOING_EXNEWSSEARCH
      m_adapter = new msMsgSearchNewsEx (this, termList);
#endif
      NS_ASSERTION(PR_FALSE, "not supporting allSearchableGroups yet");
      break;
    case nsMsgSearchScope::LDAP:
      NS_ASSERTION(PR_FALSE, "not supporting LDAP yet");
      break;
    case nsMsgSearchScope::localNews:
      m_adapter = new nsMsgSearchOfflineNews (this, termList);
      break;
    default:
      NS_ASSERTION(PR_FALSE, "invalid scope");
      err = NS_ERROR_FAILURE;
  }
  
  if (m_adapter)
    err = m_adapter->ValidateTerms ();
  
  return err;
}


char *nsMsgSearchScopeTerm::GetStatusBarName ()
{
	return nsnull;
}


//-----------------------------------------------------------------------------
// nsMsgResultElement implementation
//-----------------------------------------------------------------------------


nsMsgResultElement::nsMsgResultElement(nsIMsgSearchAdapter *adapter)
{
    NS_NewISupportsArray(getter_AddRefs(m_valueList));
	m_adapter = adapter;
}


nsMsgResultElement::~nsMsgResultElement () 
{
}


nsresult nsMsgResultElement::AddValue (nsIMsgSearchValue *value)
{ 
	m_valueList->AppendElement (value); 
	return NS_OK;
}

nsresult nsMsgResultElement::AddValue (nsMsgSearchValue *value)
{
    nsMsgSearchValueImpl* valueImpl = new nsMsgSearchValueImpl(value);
    delete value;               // we keep the nsIMsgSearchValue, not
                                // the nsMsgSearchValue
    return AddValue(valueImpl);
}


nsresult nsMsgResultElement::AssignValues (nsIMsgSearchValue *src, nsMsgSearchValue *dst)
{
    NS_ENSURE_ARG_POINTER(src);
    NS_ENSURE_ARG_POINTER(dst);

	// Yes, this could be an operator overload, but nsMsgSearchValue is totally public, so I'd
	// have to define a derived class with nothing by operator=, and that seems like a bit much
	nsresult err = NS_OK;
    src->GetAttrib(&dst->attribute);
	switch (dst->attribute)
	{
	case nsMsgSearchAttrib::Priority:
		err = src->GetPriority(&dst->u.priority);
		break;
	case nsMsgSearchAttrib::Date:
        err = src->GetDate(&dst->u.date);
		break;
	case nsMsgSearchAttrib::MsgStatus:
        err = src->GetStatus(&dst->u.msgStatus);
		break;
	case nsMsgSearchAttrib::MessageKey:
        err = src->GetMsgKey(&dst->u.key);
		break;
	case nsMsgSearchAttrib::AgeInDays:
        err = src->GetAge(&dst->u.age);
		break;
	default:
		if (dst->attribute < nsMsgSearchAttrib::kNumMsgSearchAttributes)
		{
			NS_ASSERTION(IS_STRING_ATTRIBUTE(dst->attribute), "assigning non-string result");
            nsXPIDLString unicodeString;
			err = src->GetStr(getter_Copies(unicodeString));
            dst->string = ToNewUTF8String(unicodeString);
		}
		else
			err = NS_ERROR_INVALID_ARG;
	}
	return err;
}


nsresult nsMsgResultElement::GetValue (nsMsgSearchAttribValue attrib,
                                       nsMsgSearchValue **outValue) const
{
	nsresult err = NS_OK;
	nsCOMPtr<nsIMsgSearchValue> value;
	*outValue = NULL;

    PRUint32 count;
    m_valueList->Count(&count);
	for (PRUint32 i = 0; i < count && err != NS_OK; i++)
	{
		m_valueList->QueryElementAt(i, NS_GET_IID(nsIMsgSearchValue),
                                   (void **)getter_AddRefs(value));

        nsMsgSearchAttribValue valueAttribute;
        value->GetAttrib(&valueAttribute);
		if (attrib == valueAttribute)
		{
			*outValue = new nsMsgSearchValue;
			if (*outValue)
			{
				err = AssignValues (value, *outValue);
				err = NS_OK;
			}
			else
				err = NS_ERROR_OUT_OF_MEMORY;
		}
	}
#ifdef HAVE_SEARCH_PORT
	// No need to store the folderInfo separately; we can always get it if/when
	// we need it. This code is to support "view thread context" in the search dialog
	if (SearchError_ScopeAgreement == err && attrib == nsMsgSearchAttrib::FolderInfo)
	{
		nsMsgFolderInfo *targetFolder = m_adapter->FindTargetFolder (this);
		if (targetFolder)
		{
			*outValue = new nsMsgSearchValue;
			if (*outValue)
			{
				(*outValue)->u.folder = targetFolder;
				(*outValue)->attribute = nsMsgSearchAttrib::FolderInfo;
				err = NS_OK;
			}
		}
	}
#endif
	return err;
}


nsresult
nsMsgResultElement::GetValueRef (nsMsgSearchAttribValue attrib,
                                 nsIMsgSearchValue* *aResult) const 
{
	nsCOMPtr<nsIMsgSearchValue> value;
    PRUint32 count;
    m_valueList->Count(&count);
    nsresult rv;
	for (PRUint32 i = 0; i < count; i++)
	{
		rv = m_valueList->QueryElementAt(i, NS_GET_IID(nsIMsgSearchValue),
                                            getter_AddRefs(value));
        NS_ASSERTION(NS_SUCCEEDED(rv), "bad element of array");
        if (NS_FAILED(rv)) continue;
        
        nsMsgSearchAttribValue valueAttribute;
        value->GetAttrib(&valueAttribute);
		if (attrib == valueAttribute) {
            *aResult = value;
            NS_ADDREF(*aResult);
        }
	}
	return NS_ERROR_FAILURE;
}


nsresult nsMsgResultElement::GetPrettyName (nsMsgSearchValue **value)
{
	nsresult err = GetValue (nsMsgSearchAttrib::Location, value);
#ifdef HAVE_SEARCH_PORT
	if (NS_OK == err)
	{
		nsMsgFolderInfo *folder = m_adapter->m_scope->m_folder;
		nsMsgNewsHost *host = NULL;
		if (folder)
		{
			// Find the news host because only the host knows whether pretty
			// names are supported. 
			if (FOLDER_CONTAINERONLY == folder->GetType())
				host = ((nsMsgNewsFolderInfoContainer*) folder)->GetHost();
			else if (folder->IsNews())
				host = folder->GetNewsFolderInfo()->GetHost();

			// Ask the host whether it knows pretty names. It isn't strictly
			// necessary to avoid calling folder->GetPrettiestName() since it
			// does the right thing. But we do have to find the folder from the host.
			if (host && host->QueryExtension ("LISTPNAMES"))
			{
				folder = host->FindGroup ((*value)->u.string);
				if (folder)
				{
					char *tmp = nsCRT::strdup (folder->GetPrettiestName());
					if (tmp)
					{
						XP_FREE ((*value)->u.string);
						(*value)->u.utf8SstringZ = tmp;
					}
				}
			}
		}
	}
#endif // HAVE_SEARCH_PORT
	return err;
}

int nsMsgResultElement::CompareByFolderInfoPtrs (const void *e1, const void *e2)
{
#ifdef HAVE_SEARCH_PORT
	nsMsgResultElement * re1 = *(nsMsgResultElement **) e1;
	nsMsgResultElement * re2 = *(nsMsgResultElement **) e2;

	// get the src folder for each one
	
	const nsMsgSearchValue * v1 = re1->GetValueRef(attribFolderInfo);
	const nsMsgSearchValue * v2 = re2->GetValueRef(attribFolderInfo);

	if (!v1 || !v2)
		return 0;

	return (v1->u.folder - v2->u.folder);
#else
	return -1;
#endif // HAVE_SEARCH_PORT
}



int nsMsgResultElement::Compare (const void *e1, const void *e2)
{
	int ret = 0;
#ifdef HAVE_SEARCH_PORT
	// Bad karma to cast away const, but they're my objects anway.
	// Maybe if we go through and const everything this should be a const ptr.
	nsMsgResultElement *re1 = *(nsMsgResultElement**) e1;
	nsMsgResultElement *re2 = *(nsMsgResultElement**) e2;

	NS_ASSERTION(re1->IsValid(), "invalid result element1 in resultElement::Compare");
	NS_ASSERTION(re2->IsValid(), "invalid result element2 in resultElement::Compare");

	nsMsgSearchAttribute attrib = re1->m_adapter->m_scope->m_frame->m_sortAttribute;

	const nsMsgSearchValue *v1 = re1->GetValueRef (attrib);
	const nsMsgSearchValue *v2 = re2->GetValueRef (attrib);

	if (!v1 || !v2)
		return ret; // search result doesn't contain the attrib we want to sort on

	switch (attrib)
	{
	case nsMsgSearchAttrib::Date:
		{
			// on Win 3.1, the requisite 'int' return type is a short, so use a 
			// real time_t for comparison
			time_t date = v1->u.date - v2->u.date;
			if (date)
				ret = ((long)date) < 0 ? -1 : 1;
			else
				ret = 0;
		}
		break;
	case nsMsgSearchAttrib::Priority:
		ret = v1->u.priority - v2->u.priority;
		break;
	case nsMsgSearchAttrib::MsgStatus:
		{
			// Here's a totally arbitrary sorting protocol for msg status
			uint32 s1, s2;

			s1 = v1->u.msgStatus & ~MSG_FLAG_REPLIED;
			s2 = v2->u.msgStatus & ~MSG_FLAG_REPLIED;
			if (s1 || s2)
				ret = s1 - s2;
			else
			{
				s1 = v1->u.msgStatus & ~MSG_FLAG_FORWARDED;
				s2 = v2->u.msgStatus & ~MSG_FLAG_FORWARDED;
				if (s1 || s2)
					ret = s1 - s2;
				else
				{
					s1 = v1->u.msgStatus & ~MSG_FLAG_READ;
					s2 = v2->u.msgStatus & ~MSG_FLAG_READ;
					if (s1 || s2)
						ret = s1 - s2;
					else
						// statuses don't contain any flags we're interested in, 
						// so they're equal as far as we care
						ret = 0;
				}
			}
		}
		break;
	default:
		if (attrib == nsMsgSearchAttrib::Subject)
		{
			// Special case for subjects, so "Re:foo" sorts under 'f' not 'r'
			const char *s1 = v1->u.string;
			const char *s2 = v2->u.string;
			NS_MsgStripRE (&s1, NULL);
			NS_MsgStripRE (&s2, NULL);
			ret = PL_strcasecomp (s1, s2);
		}
		else
			ret = strcasecomp (v1->u.string, v2->u.string);
	}
	// ### need different hack for this.
	// qsort's default sort order is ascending, so in order to get descending
	// behavior, we'll tell qsort a lie and reverse the comparison order.
	if (re1->m_adapter->m_scope->m_frame->m_descending && ret != 0)
		if (ret < 0)
			ret = 1;
		else
			ret = -1;

	// <0 --> e1 less than e2
	// 0  --> e1 equal to e2
	// >0 --> e1 greater than e2
#endif
	return ret;
}

#ifdef HAVE_SEARCH_PORT
MWContextType nsMsgResultElement::GetContextType ()
{
	MWContextType type=(MWContextType)0;
	switch (m_adapter->m_scope->m_attribute)
	{
	case nsMsgSearchScopeMailFolder:
		type = MWContextMailMsg;
		break;
	case nsMsgSearchScopeOfflineNewsgroup:    // added by mscott could be bug fix...
	case nsMsgSearchScopeNewsgroup:
	case nsMsgSearchScopeAllSearchableGroups:
		type = MWContextNewsMsg;
		break;
	case nsMsgSearchScopeLdapDirectory:
		type = MWContextBrowser;
		break;
	default:
		NS_ASSERTION(PR_FALSE, "invalid scope"); // should never happen
	}
	return type;
}

#endif
nsresult nsMsgResultElement::Open (void *window)
{
#ifdef HAVE_SEARCH_PORT
	// ###phil this is a little ugly, but I'm not inclined to invest more in it
	// until the libnet rework is done and I know what kind of context we'll end up with

	if (window)
	{
		if (m_adapter->m_scope->m_attribute != nsMsgSearchScopeLdapDirectory)
		{
			msgPane = (MSG_MessagePane *) window; 
			PR_ASSERT (MSG_MESSAGEPANE == msgPane->GetPaneType());
			return m_adapter->OpenResultElement (msgPane, this);
		}
		else
		{
			context = (MWContext*) window;
			PR_ASSERT (MWContextBrowser == context->type);
			msg_SearchLdap *thisAdapter = (msg_SearchLdap*) m_adapter;
			return thisAdapter->OpenResultElement (context, this);
		}
	}
#endif
	return NS_ERROR_NULL_POINTER;
}


