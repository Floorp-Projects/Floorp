/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Jungshik Shin <jshin@mailaps.org>
 *   David Bienvenu <bienvenu@nventure.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
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
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIMsgFilterPlugin.h"
#include "nsIFileSpec.h"
#include "nsIRDFService.h"
#include "nsISupportsObsolete.h"
#include "nsNetCID.h"
#include "nsIFileStreams.h"
#include "nsUnicharUtils.h"
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
    {nsMsgSearchAttrib::Subject,    "subject"},
    {nsMsgSearchAttrib::Sender,     "from"},
    {nsMsgSearchAttrib::Body,       "body"},
    {nsMsgSearchAttrib::Date,       "date"},
    {nsMsgSearchAttrib::Priority,   "priority"},
    {nsMsgSearchAttrib::MsgStatus,  "status"},
    {nsMsgSearchAttrib::To,         "to"},
    {nsMsgSearchAttrib::CC,         "cc"},
    {nsMsgSearchAttrib::ToOrCC,     "to or cc"},
    {nsMsgSearchAttrib::AgeInDays,  "age in days"},
    {nsMsgSearchAttrib::Label,      "label"},
    {nsMsgSearchAttrib::Keywords,   "tag"},
    {nsMsgSearchAttrib::Size,       "size"},
    // this used to be nsMsgSearchAttrib::SenderInAddressBook
    // we used to have two Sender menuitems
    // for backward compatability, we can still parse
    // the old style.  see bug #179803
    {nsMsgSearchAttrib::Sender,     "from in ab"}, 
    {nsMsgSearchAttrib::JunkStatus, "junk status"},
    {nsMsgSearchAttrib::HasAttachmentStatus, "has attachment status"},
};

// Take a string which starts off with an attribute
// return the matching attribute. If the string is not in the table, then we can conclude that it is an arbitrary header
nsresult NS_MsgGetAttributeFromString(const char *string, PRInt16 *attrib)
{
  NS_ENSURE_ARG_POINTER(string);
  NS_ENSURE_ARG_POINTER(attrib);
  
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
    PRBool goodHdr;
    IsRFC822HeaderFieldName(string, &goodHdr);
    if (!goodHdr)
      return NS_MSG_INVALID_CUSTOM_HEADER;
    //49 is for showing customize... in ui, headers start from 50 onwards up until 99.
    *attrib = nsMsgSearchAttrib::OtherHeader+1;

    nsCOMPtr<nsIPrefService> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrefBranch> prefBranch;
    rv = prefService->GetBranch(nsnull, getter_AddRefs(prefBranch));
    NS_ENSURE_SUCCESS(rv, rv);

    nsXPIDLCString headers;
    prefBranch->GetCharPref(MAILNEWS_CUSTOM_HEADERS, getter_Copies(headers));

    if (!headers.IsEmpty())
    {
      char *headersString = ToNewCString(headers);

      nsCAutoString hdrStr;
      hdrStr.Adopt(headersString);
      hdrStr.StripWhitespace();  //remove whitespace before parsing

      char *newStr=nsnull;
      char *token = nsCRT::strtok(headersString,":", &newStr);
      PRUint32 i=0;
      while (token)
      {
        if (nsCRT::strcasecmp(token, string) == 0)
        {
          *attrib += i; //we found custom header in the pref
          found = PR_TRUE;
          break;
        }
        token = nsCRT::strtok(newStr,":", &newStr);
        i++;
      }
    }
  }
  // If we didn't find the header in MAILNEWS_CUSTOM_HEADERS, we're 
  // going to return NS_OK and an attrib of nsMsgSearchAttrib::OtherHeader+1.
  // in case it's a client side spam filter description filter, 
  // which doesn't add its headers to mailnews.customMailHeaders.
  // We've already checked that it's a valid header and returned
  // an error if so.

  return NS_OK;
}

nsresult NS_MsgGetStringForAttribute(PRInt16 attrib, const char **string)
{
  NS_ENSURE_ARG_POINTER(string);

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
	return NS_OK;
}

typedef struct
{
	nsMsgSearchOpValue  op;
	const char			*opName;
} nsMsgSearchOperatorEntry;

nsMsgSearchOperatorEntry SearchOperatorEntryTable[] =
{
  {nsMsgSearchOp::Contains, "contains"},
  {nsMsgSearchOp::DoesntContain,"doesn't contain"},
  {nsMsgSearchOp::Is,"is"},
  {nsMsgSearchOp::Isnt,	"isn't"},
  {nsMsgSearchOp::IsEmpty, "is empty"},
  {nsMsgSearchOp::IsBefore, "is before"},
  {nsMsgSearchOp::IsAfter, "is after"},
  {nsMsgSearchOp::IsHigherThan, "is higher than"},
  {nsMsgSearchOp::IsLowerThan, "is lower than"},
  {nsMsgSearchOp::BeginsWith, "begins with"},
  {nsMsgSearchOp::EndsWith, "ends with"},
  {nsMsgSearchOp::IsInAB, "is in ab"},
  {nsMsgSearchOp::IsntInAB, "isn't in ab"},
  {nsMsgSearchOp::IsGreaterThan, "is greater than"},
  {nsMsgSearchOp::IsLessThan, "is less than"}
};

nsresult NS_MsgGetOperatorFromString(const char *string, PRInt16 *op)
{
  NS_ENSURE_ARG_POINTER(string);
  NS_ENSURE_ARG_POINTER(op);
	
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
  NS_ENSURE_ARG_POINTER(string);
  
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
	const char *tmpOutName = NULL;
#define MSG_STATUS_MASK (MSG_FLAG_READ | MSG_FLAG_REPLIED | MSG_FLAG_FORWARDED | MSG_FLAG_NEW | MSG_FLAG_MARKED)
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
        case MSG_FLAG_MARKED:
                tmpOutName = "flagged";
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
  if (!strcmp("read", name))
    return MSG_FLAG_READ;
  if (!strcmp("replied", name))
    return MSG_FLAG_REPLIED;
  if (!strcmp("forwarded", name))
    return MSG_FLAG_FORWARDED;
  if (!strcmp("replied and forwarded", name))
    return MSG_FLAG_FORWARDED|MSG_FLAG_REPLIED;
  if (!strcmp("new", name))
    return MSG_FLAG_NEW;
  if (!strcmp("flagged", name))
    return MSG_FLAG_MARKED;
  return 0;
}


// Needed for DeStream method.
nsMsgSearchTerm::nsMsgSearchTerm()
{
    // initialize this to zero
    m_value.string=nsnull;
    m_value.attribute=0;
    m_value.u.priority=0;
    m_attribute = nsMsgSearchAttrib::Default;
    mBeginsGrouping = PR_FALSE;
    mEndsGrouping = PR_FALSE;
    m_matchAll = PR_FALSE;
}

nsMsgSearchTerm::nsMsgSearchTerm (
                                  nsMsgSearchAttribValue attrib, 
                                  nsMsgSearchOpValue op, 
                                  nsIMsgSearchValue *val,
                                  nsMsgSearchBooleanOperator boolOp,
                                  const char * arbitraryHeader) 
{
  m_operator = op;
  m_attribute = attrib;
  m_booleanOp = boolOp;
  if (attrib > nsMsgSearchAttrib::OtherHeader  && attrib < nsMsgSearchAttrib::kNumMsgSearchAttributes && arbitraryHeader)
    m_arbitraryHeader = arbitraryHeader;
  nsMsgResultElement::AssignValues (val, &m_value);
  m_matchAll = PR_FALSE;
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
    case nsMsgSearchAttrib::AgeInDays:
      {
        outputStr.AppendInt(m_value.u.age);
        break;
      }
    case nsMsgSearchAttrib::Label:
      {
        outputStr.AppendInt(m_value.u.label);
        break;
      }
    case nsMsgSearchAttrib::JunkStatus:
      {
        outputStr.AppendInt(m_value.u.junkStatus); // only if we write to disk, right?
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
        nsCAutoString priority;
        NS_MsgGetUntranslatedPriorityName(m_value.u.priority, priority);
        outputStr += priority;
        break;
      }
    case nsMsgSearchAttrib::HasAttachmentStatus:
      {
        outputStr.Append("true");  // don't need anything here, really
        break;
      }
    case nsMsgSearchAttrib::Size:
      {
        outputStr.AppendInt(m_value.u.size);
        break;
      }
    default:
      NS_ASSERTION(PR_FALSE, "trying to output invalid attribute");
      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgSearchTerm::GetTermAsString (nsACString &outStream)
{
  const char	*attrib, *operatorStr;
  nsCAutoString	outputStr;
  nsresult	ret;
  
  if (m_matchAll)
  {
    outStream = "ALL";
    return NS_OK;
  }
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
    while (nsCRT::IsAsciiSpace(*inStream))
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
      NS_MsgGetPriorityFromString(inStream, m_value.u.priority);
      break;
    case nsMsgSearchAttrib::AgeInDays:
      m_value.u.age = atoi(inStream);
      break;
    case nsMsgSearchAttrib::Label:
      m_value.u.label = atoi(inStream);
      break;
    case nsMsgSearchAttrib::JunkStatus:
      m_value.u.junkStatus = atoi(inStream); // only if we read from disk, right?
      break;
    case nsMsgSearchAttrib::HasAttachmentStatus:
      m_value.u.msgStatus = MSG_FLAG_ATTACHMENT;
      break; // this should always be true.
    case nsMsgSearchAttrib::Size:
      m_value.u.size = atoi(inStream);
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
nsresult
nsMsgSearchTerm::ParseOperator(char *inStream, nsMsgSearchOpValue *value)
{
  NS_ENSURE_ARG_POINTER(value);
  PRInt16				operatorVal;
  while (nsCRT::IsAsciiSpace(*inStream))
    inStream++;
  
  char *commaSep = PL_strchr(inStream, ',');
  
  if (commaSep)
    *commaSep = '\0';
  
  nsresult err = NS_MsgGetOperatorFromString(inStream, &operatorVal);
  *value = (nsMsgSearchOpValue) operatorVal;
  return err;
}

// find the attribute code for this comma-delimited attribute. 
nsresult
nsMsgSearchTerm::ParseAttribute(char *inStream, nsMsgSearchAttribValue *attrib)
{   
    while (nsCRT::IsAsciiSpace(*inStream))
        inStream++;
    
    // if we are dealing with an arbitrary header, it may be quoted....
    PRBool quoteVal = PR_FALSE;
    if (*inStream == '"')
    {
        quoteVal = PR_TRUE;
        inStream++;
    }
    
    // arbitrary headers are quoted
    char *separator = strchr(inStream, quoteVal ? '"' : ',');

    if (separator)
        *separator = '\0';
    
    PRInt16 attributeVal;
    nsresult rv = NS_MsgGetAttributeFromString(inStream, &attributeVal);
    NS_ENSURE_SUCCESS(rv, rv);
    
    *attrib = (nsMsgSearchAttribValue) attributeVal;
    
    if (*attrib > nsMsgSearchAttrib::OtherHeader && *attrib < nsMsgSearchAttrib::kNumMsgSearchAttributes)  // if we are dealing with an arbitrary header....
        m_arbitraryHeader = inStream;
    
    return rv;
}

// De stream one search term. If the condition looks like
// condition = "(to or cc, contains, r-thompson) AND (body, doesn't contain, fred)"
// This routine should get called twice, the first time
// with "to or cc, contains, r-thompson", the second time with
// "body, doesn't contain, fred"

nsresult nsMsgSearchTerm::DeStreamNew (char *inStream, PRInt16 /*length*/)
{
  if (!strcmp(inStream, "ALL"))
  {
    m_matchAll = PR_TRUE;
    return NS_OK;
  }
  char *commaSep = PL_strchr(inStream, ',');
  nsresult rv = ParseAttribute(inStream, &m_attribute);  // will allocate space for arbitrary header if necessary
  NS_ENSURE_SUCCESS(rv, rv);
  if (!commaSep)
    return NS_ERROR_INVALID_ARG;
  char *secondCommaSep = PL_strchr(commaSep + 1, ',');
  if (commaSep)
    rv = ParseOperator(commaSep + 1, &m_operator);
  NS_ENSURE_SUCCESS(rv, rv);
  // convert label filters and saved searches to keyword equivalents
  if (secondCommaSep)
    ParseValue(secondCommaSep + 1);
  if (m_attribute == nsMsgSearchAttrib::Label)
  {
    nsCAutoString keyword("$label");
    m_value.attribute = m_attribute = nsMsgSearchAttrib::Keywords;
    keyword.Append('0' + m_value.u.label);
    m_value.string = PL_strdup(keyword.get());
  }
  return NS_OK;
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
  NS_ENSURE_ARG_POINTER(pResult);
  *pResult = PR_FALSE;
  nsresult err = NS_OK;
  PRBool result;
  
  nsMsgBodyHandler * bodyHandler = new nsMsgBodyHandler (scope, offset,length, msg, db, headers, headersSize, ForFiltering);
  if (!bodyHandler)
    return NS_ERROR_OUT_OF_MEMORY;
  
  bodyHandler->SetStripHeaders (PR_FALSE);
  
  GetMatchAllBeforeDeciding(&result);
  
  nsCAutoString buf;
  nsCAutoString curMsgHeader;
  PRBool searchingHeaders = PR_TRUE;
  while (searchingHeaders && (bodyHandler->GetNextLine(buf) >=0))
  {
    char * buf_end = (char *) (buf.get() + buf.Length());
    int headerLength = m_arbitraryHeader.Length();
    PRBool isContinuationHeader = NS_IsAsciiWhitespace(buf.CharAt(0));
    // this handles wrapped header lines, which start with whitespace. 
    // If the line starts with whitespace, then we use the current header.
    if (!isContinuationHeader)
    {
      PRUint32 colonPos = buf.FindChar(':');
      buf.Left(curMsgHeader, colonPos);
    }

    if (curMsgHeader.Equals(m_arbitraryHeader, nsCaseInsensitiveCStringComparator()))
    {
      // value occurs after the header name or whitespace continuation char.
      const char * headerValue = buf.get() + (isContinuationHeader ? 1 : headerLength); 
      if (headerValue < buf_end && headerValue[0] == ':')  // + 1 to account for the colon which is MANDATORY
        headerValue++; 
      
      // strip leading white space
      while (headerValue < buf_end && nsCRT::IsAsciiSpace(*headerValue))
        headerValue++; // advance to next character
      
      // strip trailing white space
      char * end = buf_end - 1; 
      while (end > headerValue && nsCRT::IsAsciiSpace(*end)) // while we haven't gone back past the start and we are white space....
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
  *pResult = result;
  return err;
}

nsresult nsMsgSearchTerm::MatchBody (nsIMsgSearchScopeTerm *scope, PRUint32 offset, PRUint32 length /*in lines*/, const char *folderCharset,
                                      nsIMsgDBHdr *msg, nsIMsgDatabase* db, PRBool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);
  nsresult err = NS_OK;
  
  PRBool result = PR_FALSE;
  *pResult = PR_FALSE;
  
  // Small hack so we don't look all through a message when someone has
  // specified "BODY IS foo". ### Since length is in lines, this is not quite right.
  if ((length > 0) && (m_operator == nsMsgSearchOp::Is || m_operator == nsMsgSearchOp::Isnt))
    length = PL_strlen (m_value.string);
  
  nsMsgBodyHandler * bodyHan  = new nsMsgBodyHandler (scope, offset, length, msg, db);
  if (!bodyHan)
    return NS_ERROR_OUT_OF_MEMORY;
  
  nsCAutoString buf;
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
  // bug fix #314637: for stateful charsets like ISO-2022-JP, we don't
  // want to decode quoted printable since it contains '='.
  PRBool isQuotedPrintable = !nsMsgI18Nstateful_charset(folderCharset) &&
    (PL_strchr (m_value.string, '=') == nsnull);
  
  nsCString compare;
  while (!endOfFile && result == boolContinueLoop)
  {
    if (bodyHan->GetNextLine(buf) >= 0)
    {
      PRBool softLineBreak = PR_FALSE;
      // Do in-place decoding of quoted printable
      if (isQuotedPrintable)
      {
        softLineBreak = StringEndsWith(buf, NS_LITERAL_CSTRING("="));
        MsgStripQuotedPrintable ((unsigned char*)buf.get());
        if (softLineBreak)
          buf.Truncate(buf.Length() - 1);
      }
      compare.Append(buf);
      // If this line ends with a soft line break, loop around
      // and get the next line before looking for the search string.
      // This assumes the message can't end on a QP soft-line break.
      // That seems like a pretty safe assumption.
      if (softLineBreak) 
        continue;
      if (!compare.IsEmpty())
      {
        char startChar = (char) compare.CharAt(0);
        if (startChar != nsCRT::CR && startChar != nsCRT::LF)
        {
          err = MatchString (compare.get(), folderCharset, &result);
          lines++; 
        }
        compare.Truncate();
      }
    }
    else 
      endOfFile = PR_TRUE;
  }
#ifdef DO_I18N
  if(conv) 
    INTL_DestroyCharCodeConverter(conv);
#endif
  delete bodyHan;
  *pResult = result;
  return err;
}

nsresult nsMsgSearchTerm::InitializeAddressBook()
{
  // the search attribute value has the URI for the address book we need to load. 
  // we need both the database and the directory.
  nsresult rv = NS_OK;

  if (mDirectory)
  {
    nsXPIDLCString dirURI;
    mDirectory->GetDirUri(getter_Copies(dirURI));
    if (strcmp(dirURI.get(), m_value.string))
      mDirectory = nsnull; // clear out the directory....we are no longer pointing to the right one
  }
  if (!mDirectory)
  {  
    nsCOMPtr <nsIRDFService> rdfService = do_GetService("@mozilla.org/rdf/rdf-service;1",&rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr <nsIRDFResource> resource;
    rv = rdfService->GetResource(nsDependentCString(m_value.string), getter_AddRefs(resource));
    NS_ENSURE_SUCCESS(rv, rv);

    mDirectory = do_QueryInterface(resource, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult nsMsgSearchTerm::MatchInAddressBook(const char * aAddress, PRBool *pResult)
{
  nsresult rv = InitializeAddressBook(); 
  *pResult = PR_FALSE;

  // Some junkmails have empty From: fields.
  if (aAddress == NULL || strlen(aAddress) == 0)
    return rv;

  if (mDirectory)
  {
    PRBool cardExists = PR_FALSE;
    rv = mDirectory->HasCardForEmailAddress(aAddress, &cardExists);
    if ( (m_operator == nsMsgSearchOp::IsInAB && cardExists) || (m_operator == nsMsgSearchOp::IsntInAB && !cardExists))
      *pResult = PR_TRUE;
  }
  
  return rv;
}

// *pResult is PR_FALSE when strings don't match, PR_TRUE if they do.
nsresult nsMsgSearchTerm::MatchRfc2047String (const char *rfc2047string,
                                       const char *charset,
                                       PRBool charsetOverride,
                                       PRBool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);
  NS_ENSURE_ARG_POINTER(rfc2047string);

    nsCOMPtr<nsIMimeConverter> mimeConverter = do_GetService(NS_MIME_CONVERTER_CONTRACTID);
	char *stringToMatch = 0;
    nsresult res = mimeConverter->DecodeMimeHeader(rfc2047string,
                                                   &stringToMatch,
                                                   charset, charsetOverride,
                                                   PR_FALSE);

    if (m_attribute == nsMsgSearchAttrib::Sender && 
        (m_operator == nsMsgSearchOp::IsInAB ||
         m_operator == nsMsgSearchOp::IsntInAB))
    {
      res = MatchInAddressBook(stringToMatch ? stringToMatch : rfc2047string, pResult);
    }
    else
	  res = MatchString(stringToMatch ? stringToMatch : rfc2047string,
                      nsnull, pResult);

    PR_Free(stringToMatch);

	return res;
}

// *pResult is PR_FALSE when strings don't match, PR_TRUE if they do.
nsresult nsMsgSearchTerm::MatchString (const char *stringToMatch,
                                       const char *charset,
                                       PRBool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);
  PRBool result = PR_FALSE;
  
  nsresult err = NS_OK;
  nsAutoString utf16StrToMatch;
  nsAutoString needle; 

  // Save some performance for opIsEmpty
  if(nsMsgSearchOp::IsEmpty != m_operator)  
  {
    NS_ASSERTION(IsUTF8(nsDependentCString(m_value.string)),
                        "m_value.string is not UTF-8");
    CopyUTF8toUTF16(m_value.string, needle);

    if (charset != nsnull)
    {
      ConvertToUnicode(charset, stringToMatch ? stringToMatch : "",
                       utf16StrToMatch);
    }
    else { 
      NS_ASSERTION(IsUTF8(nsDependentCString(stringToMatch)),
                   "stringToMatch is not UTF-8");
      CopyUTF8toUTF16(stringToMatch, utf16StrToMatch);
    }
  }
  
  switch (m_operator)
  {
  case nsMsgSearchOp::Contains:
    if (CaseInsensitiveFindInReadable(needle, utf16StrToMatch))
      result = PR_TRUE;
    break;
  case nsMsgSearchOp::DoesntContain:
    if(!CaseInsensitiveFindInReadable(needle, utf16StrToMatch))
      result = PR_TRUE;
    break;
  case nsMsgSearchOp::Is:
    if(needle.Equals(utf16StrToMatch, nsCaseInsensitiveStringComparator()))
      result = PR_TRUE;
    break;
  case nsMsgSearchOp::Isnt:
    if(!needle.Equals(utf16StrToMatch, nsCaseInsensitiveStringComparator()))
      result = PR_TRUE;
    break;
  case nsMsgSearchOp::IsEmpty:
    // For IsEmpty, we didn't copy stringToMatch to utf16StrToMatch.
    if (!PL_strlen(stringToMatch))
      result = PR_TRUE;
    break;
  case nsMsgSearchOp::BeginsWith:
    if (StringBeginsWith(utf16StrToMatch, needle,
                         nsCaseInsensitiveStringComparator()))
      result = PR_TRUE;
    break;
  case nsMsgSearchOp::EndsWith: 
    if (StringEndsWith(utf16StrToMatch, needle,
                       nsCaseInsensitiveStringComparator()))
      result = PR_TRUE;
    break;
  default:
    NS_ASSERTION(PR_FALSE, "invalid operator matching search results");
  }
  
  *pResult = result;
  return err;
}

NS_IMETHODIMP nsMsgSearchTerm::GetMatchAllBeforeDeciding (PRBool *aResult)
{
 *aResult = (m_operator == nsMsgSearchOp::DoesntContain || m_operator == nsMsgSearchOp::Isnt);
 return NS_OK;
}
                                       
 nsresult nsMsgSearchTerm::MatchRfc822String (const char *string, const char *charset, PRBool charsetOverride, PRBool *pResult)
 {
   NS_ENSURE_ARG_POINTER(pResult);
   *pResult = PR_FALSE;
   PRBool result;
   nsresult err = InitHeaderAddressParser();
   if (NS_FAILED(err))
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
       walkAddresses = addresses + addressPos;
       if (m_attribute == nsMsgSearchAttrib::Sender && 
         (m_operator == nsMsgSearchOp::IsInAB ||
         m_operator == nsMsgSearchOp::IsntInAB))
       {
         err = MatchRfc2047String (walkAddresses.get(), charset, charsetOverride, &result);
       }
       else
       {
         err = MatchRfc2047String (walkNames.get(), charset, charsetOverride, &result);
         if (boolContinueLoop == result)
           err = MatchRfc2047String (walkAddresses.get(), charset, charsetOverride, &result);
       }
       
       namePos += walkNames.Length() + 1;
       addressPos += walkAddresses.Length() + 1;
     }
     
     PR_Free(names);
     PR_Free(addresses);
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
  NS_ENSURE_ARG_POINTER(pResult);
  
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
  NS_ENSURE_ARG_POINTER(pResult);

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
      NS_ASSERTION(PR_FALSE, "invalid compare op for msg age");
  }
  *pResult = result;
  return err;
}


nsresult nsMsgSearchTerm::MatchSize (PRUint32 sizeToMatch, PRBool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);

  PRBool result = PR_FALSE;
  // We reduce the sizeToMatch rather than supplied size
  // as then we can do an exact match on the displayed value
  // which will be less confusing to the user.
  PRUint32 sizeToMatchKB = sizeToMatch;

  if (sizeToMatchKB < 1024)
    sizeToMatchKB = 1024;

  sizeToMatchKB /= 1024;

  switch (m_operator)
  {
  case nsMsgSearchOp::IsGreaterThan:
    if (sizeToMatchKB > m_value.u.size)
      result = PR_TRUE;
    break;
  case nsMsgSearchOp::IsLessThan:
    if (sizeToMatchKB < m_value.u.size)
      result = PR_TRUE;
    break;
  case nsMsgSearchOp::Is:
    if (sizeToMatchKB == m_value.u.size)
      result = PR_TRUE;
    break;
  default:
    break;
  }
  *pResult = result;
  return NS_OK;
}

nsresult nsMsgSearchTerm::MatchJunkStatus(const char *aJunkScore, PRBool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);

  nsMsgJunkStatus junkStatus;  
  if (aJunkScore && *aJunkScore) {
      // cut off set at 50. this may change
      // it works for our bayesian plugin, as "0" is good, and "100" is junk
      // but it might need tweaking for other plugins
      if ( atoi(aJunkScore) > 50 ) {
          junkStatus = nsIJunkMailPlugin::JUNK;
      } else {
          junkStatus = nsIJunkMailPlugin::GOOD;
      }
  }
  else {
    // the in UI, we only show "junk" or "not junk"
    // unknown, or nsIJunkMailPlugin::UNCLASSIFIED is shown as not junk
    // so for the search to work as expected, treat unknown as not junk
    junkStatus = nsIJunkMailPlugin::GOOD;
  }

  nsresult rv = NS_OK;
  PRBool matches = (junkStatus == m_value.u.junkStatus);

  switch (m_operator)
  {
    case nsMsgSearchOp::Is:
      break;
    case nsMsgSearchOp::Isnt:
      matches = !matches;
      break;
    default:
      rv = NS_ERROR_FAILURE;
      NS_ASSERTION(PR_FALSE, "invalid compare op for junk status");
  }

  *pResult = matches;
  return rv;	
}

nsresult nsMsgSearchTerm::MatchLabel(nsMsgLabelValue aLabelValue, PRBool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);
  PRBool result = PR_FALSE;
  switch (m_operator)
  {
  case nsMsgSearchOp::Is:
    if (m_value.u.label == aLabelValue)
      result = PR_TRUE;
    break;
  default:
    if (m_value.u.label != aLabelValue)
      result = PR_TRUE;
    break;
  }
  
  *pResult = result;
  return NS_OK;	
}

nsresult nsMsgSearchTerm::MatchStatus(PRUint32 statusToMatch, PRBool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);

  nsresult rv = NS_OK;
  PRBool matches = (statusToMatch & m_value.u.msgStatus);

  switch (m_operator)
  {
  case nsMsgSearchOp::Is:
    break;
  case nsMsgSearchOp::Isnt:
    matches = !matches;
    break;
  default:
    rv = NS_ERROR_FAILURE;
    NS_ERROR("invalid compare op for msg status");
  }

  *pResult = matches;
  return rv;	
}

nsresult nsMsgSearchTerm::MatchKeyword(const char *keyword, PRBool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);

  nsresult rv = NS_OK;
  nsCAutoString keys;

  PRBool matches = PR_FALSE;

  switch (m_operator)
  {
  case nsMsgSearchOp::Is:
    matches = !strcmp(keyword, m_value.string);
    break;
  case nsMsgSearchOp::Isnt:
    matches = strcmp(keyword, m_value.string);
    break;
  case nsMsgSearchOp::DoesntContain:
  case nsMsgSearchOp::Contains:
    {
      const char *keywordLoc = PL_strstr(keyword, m_value.string);
      const char *startOfKeyword = keyword;
      PRUint32 keywordLen = strlen(m_value.string);
      while (keywordLoc)
      {
        // if the keyword is at the beginning of the string, then it's a match if 
        // it is either the whole string, or is followed by a space, it's a match.
        if (keywordLoc == startOfKeyword || (keywordLoc[-1] == ' '))
        {
          matches = keywordLen == strlen(keywordLoc) || (keywordLoc[keywordLen] == ' ');
          if (matches)
            break;
        }
        startOfKeyword = keywordLoc + keywordLen;
        keywordLoc = PL_strstr(keyword, keywordLoc + keywordLen + 1);
      }
    }
    break;
  default:
    rv = NS_ERROR_FAILURE;
    NS_ERROR("invalid compare op for msg status");
  }

  *pResult = (m_operator == nsMsgSearchOp::DoesntContain) ? !matches : matches;
  return rv;	
}

nsresult
nsMsgSearchTerm::MatchPriority (nsMsgPriorityValue priorityToMatch,
                                PRBool *pResult)
{
  NS_ENSURE_ARG_POINTER(pResult);

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
    
	if (!m_headerAddressParser)
	{
    m_headerAddressParser = do_GetService(NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID, &res);
	}
	return res;
}

NS_IMPL_GETSET(nsMsgSearchTerm, Attrib, nsMsgSearchAttribValue, m_attribute)
NS_IMPL_GETSET(nsMsgSearchTerm, Op, nsMsgSearchOpValue, m_operator)
NS_IMPL_GETSET(nsMsgSearchTerm, MatchAll, PRBool, m_matchAll)

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

NS_IMPL_GETSET(nsMsgSearchTerm, BeginsGrouping, PRBool, mEndsGrouping)
NS_IMPL_GETSET(nsMsgSearchTerm, EndsGrouping, PRBool, mEndsGrouping)

//-----------------------------------------------------------------------------
// nsMsgSearchScopeTerm implementation
//-----------------------------------------------------------------------------
nsMsgSearchScopeTerm::nsMsgSearchScopeTerm (nsIMsgSearchSession *session,
                                            nsMsgSearchScopeValue attribute,
                                            nsIMsgFolder *folder)
{
  m_attribute = attribute;
  m_folder = folder;
  m_searchServer = PR_TRUE;
  m_searchSession = do_GetWeakReference(session);
}

nsMsgSearchScopeTerm::nsMsgSearchScopeTerm ()
{
  m_searchServer = PR_TRUE;
}

nsMsgSearchScopeTerm::~nsMsgSearchScopeTerm ()
{  
  if (m_inputStream)
    m_inputStream->Close();
  m_inputStream = nsnull;
}

NS_IMPL_ISUPPORTS1(nsMsgSearchScopeTerm, nsIMsgSearchScopeTerm)

NS_IMETHODIMP
nsMsgSearchScopeTerm::GetFolder(nsIMsgFolder **aResult)
{
    NS_IF_ADDREF(*aResult = m_folder);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgSearchScopeTerm::GetSearchSession(nsIMsgSearchSession** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    nsCOMPtr<nsIMsgSearchSession> searchSession = do_QueryReferent (m_searchSession);
    NS_IF_ADDREF(*aResult = searchSession);
    return NS_OK;
}

NS_IMETHODIMP nsMsgSearchScopeTerm::GetMailFile(nsILocalFile **aLocalFile)
{
  NS_ENSURE_ARG_POINTER(aLocalFile);
  if (!m_localFile)
  {
    if (!m_folder)
     return NS_ERROR_NULL_POINTER;

    nsCOMPtr <nsIFileSpec> fileSpec;
    m_folder->GetPath(getter_AddRefs(fileSpec));
    nsFileSpec realSpec;
    fileSpec->GetFileSpec(&realSpec);
    NS_FileSpecToIFile(&realSpec, getter_AddRefs(m_localFile));
  }
  if (m_localFile)
  {
    NS_ADDREF(*aLocalFile = m_localFile);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMsgSearchScopeTerm::GetInputStream(nsIInputStream **aInputStream)
{
  NS_ENSURE_ARG_POINTER(aInputStream);
  nsresult rv = NS_OK;
  if (!m_inputStream)
  {
     nsCOMPtr <nsILocalFile> localFile;
     rv = GetMailFile(getter_AddRefs(localFile));
     NS_ENSURE_SUCCESS(rv, rv);
     nsCOMPtr<nsIFileInputStream> fileStream = do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv);
     NS_ENSURE_SUCCESS(rv, rv);

     rv = fileStream->Init(localFile,  PR_RDONLY, 0664, PR_FALSE);  //just have to read the messages
     m_inputStream = do_QueryInterface(fileStream);
   
  }
  NS_IF_ADDREF(*aInputStream = m_inputStream);
  return rv;
}

NS_IMETHODIMP nsMsgSearchScopeTerm::SetInputStream(nsIInputStream *aInputStream)
{
  if (!aInputStream && m_inputStream)
    m_inputStream->Close();
  m_inputStream = aInputStream;
  return NS_OK;
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
      NS_ASSERTION(PR_FALSE, "not supporting newsEx yet");
      break;
    case nsMsgSearchScope::news:
          m_adapter = new nsMsgSearchNews (this, termList);
        break;
    case nsMsgSearchScope::allSearchableGroups:
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
  case nsMsgSearchAttrib::HasAttachmentStatus:
  case nsMsgSearchAttrib::MsgStatus:
    err = src->GetStatus(&dst->u.msgStatus);
    break;
  case nsMsgSearchAttrib::MessageKey:
    err = src->GetMsgKey(&dst->u.key);
    break;
  case nsMsgSearchAttrib::AgeInDays:
    err = src->GetAge(&dst->u.age);
    break;
  case nsMsgSearchAttrib::Label:
    err = src->GetLabel(&dst->u.label);
    break;
  case nsMsgSearchAttrib::JunkStatus:
    err = src->GetJunkStatus(&dst->u.junkStatus);
    break;
  case nsMsgSearchAttrib::Size:
    err = src->GetSize(&dst->u.size);
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
  return err;
}

nsresult nsMsgResultElement::GetPrettyName (nsMsgSearchValue **value)
{
  return GetValue (nsMsgSearchAttrib::Location, value);
}

nsresult nsMsgResultElement::Open (void *window)
{
  return NS_ERROR_NULL_POINTER;
}


