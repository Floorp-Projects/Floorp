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
#include "nsTextFormatter.h"
#include "nsMsgSearchCore.h"
#include "nsMsgSearchAdapter.h"
#include "nsMsgSearchScopeTerm.h"
#include "nsMsgI18N.h"
#include "nsIPref.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsMsgSearchTerm.h"
#include "nsMsgSearchBoolExpression.h"
#include "nsIIOService.h"
#include "nsNetCID.h"
#include "prprf.h"

// This stuff lives in the base class because the IMAP search syntax 
// is used by the Dredd SEARCH command as well as IMAP itself

// km - the NOT and HEADER strings are not encoded with a trailing
//      <space> because they always precede a mnemonic that has a
//      preceding <space> and double <space> characters cause some
//		imap servers to return an error.
const char *nsMsgSearchAdapter::m_kImapBefore   = " SENTBEFORE ";
const char *nsMsgSearchAdapter::m_kImapBody     = " BODY ";
const char *nsMsgSearchAdapter::m_kImapCC       = " CC ";
const char *nsMsgSearchAdapter::m_kImapFrom     = " FROM ";
const char *nsMsgSearchAdapter::m_kImapNot      = " NOT";
const char *nsMsgSearchAdapter::m_kImapUnDeleted= " UNDELETED";
const char *nsMsgSearchAdapter::m_kImapOr       = " OR";
const char *nsMsgSearchAdapter::m_kImapSince    = " SENTSINCE ";
const char *nsMsgSearchAdapter::m_kImapSubject  = " SUBJECT ";
const char *nsMsgSearchAdapter::m_kImapTo       = " TO ";
const char *nsMsgSearchAdapter::m_kImapHeader   = " HEADER";
const char *nsMsgSearchAdapter::m_kImapAnyText  = " TEXT ";
const char *nsMsgSearchAdapter::m_kImapKeyword  = " KEYWORD ";
const char *nsMsgSearchAdapter::m_kNntpKeywords = " KEYWORDS "; //ggrrrr...
const char *nsMsgSearchAdapter::m_kImapSentOn = " SENTON ";
const char *nsMsgSearchAdapter::m_kImapSeen = " SEEN ";
const char *nsMsgSearchAdapter::m_kImapAnswered = " ANSWERED ";
const char *nsMsgSearchAdapter::m_kImapNotSeen = " UNSEEN ";
const char *nsMsgSearchAdapter::m_kImapNotAnswered = " UNANSWERED ";
const char *nsMsgSearchAdapter::m_kImapCharset = " CHARSET ";

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

NS_IMETHODIMP nsMsgSearchAdapter::FindTargetFolder(const nsMsgResultElement *,nsIMsgFolder * *)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgSearchAdapter::ModifyResultElement(nsMsgResultElement *, nsMsgSearchValue *)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgSearchAdapter::OpenResultElement(nsMsgResultElement *)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMPL_ISUPPORTS1(nsMsgSearchAdapter, nsIMsgSearchAdapter)

nsMsgSearchAdapter::nsMsgSearchAdapter(nsIMsgSearchScopeTerm *scope, nsISupportsArray *searchTerms) 
	: m_searchTerms(searchTerms)
{
  NS_INIT_ISUPPORTS();
  m_scope = scope;
}

nsMsgSearchAdapter::~nsMsgSearchAdapter()
{
}

NS_IMETHODIMP nsMsgSearchAdapter::ValidateTerms ()
{
  // all this used to do is check if the object had been deleted - we can skip that.
	return NS_OK;
}

NS_IMETHODIMP nsMsgSearchAdapter::Abort ()
{
	return NS_ERROR_NOT_IMPLEMENTED;

}
NS_IMETHODIMP nsMsgSearchAdapter::Search (PRBool *aDone) 
{
  return NS_OK; 
}

NS_IMETHODIMP nsMsgSearchAdapter::SendUrl () 
{
  return NS_OK; 
}

/* void CurrentUrlDone (in long exitCode); */
NS_IMETHODIMP nsMsgSearchAdapter::CurrentUrlDone(PRInt32 exitCode)
{
  // base implementation doesn't need to do anything.
  return NS_OK;
}

NS_IMETHODIMP nsMsgSearchAdapter::GetEncoding (char **encoding) 
{
  return NS_OK; 
}

NS_IMETHODIMP nsMsgSearchAdapter::AddResultElement (nsIMsgDBHdr *pHeaders)
{
    NS_ASSERTION(PR_FALSE, "shouldn't call this base class impl");
    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsMsgSearchAdapter::AddHit(nsMsgKey key)
{
  NS_ASSERTION(PR_FALSE, "shouldn't call this base class impl");
  return NS_ERROR_FAILURE;
}


char *
nsMsgSearchAdapter::GetImapCharsetParam(const PRUnichar *destCharset)
{
	char *result = nsnull;

	// Specify a character set unless we happen to be US-ASCII.
  if (nsCRT::strcmp(destCharset, NS_LITERAL_STRING("us-ascii").get()))
	    result = PR_smprintf("%s%S", nsMsgSearchAdapter::m_kImapCharset, destCharset);

	return result;
}

/* 
   09/21/2000 - taka@netscape.com
   This method is bogus. Escape must be done against char * not PRUnichar *
   should be rewritten later.
   for now, just duplicate the string.
*/
PRUnichar *nsMsgSearchAdapter::EscapeSearchUrl (const PRUnichar *nntpCommand)
{
	return nsCRT::strdup(nntpCommand);
#if 0
	PRUnichar *result = nsnull;
	// max escaped length is two extra characters for every character in the cmd.
  PRUnichar *scratchBuf = (PRUnichar*) PR_Malloc(sizeof(PRUnichar) * (3*nsCRT::strlen(nntpCommand) + 1));
	if (scratchBuf)
	{
		PRUnichar *scratchPtr = scratchBuf;
		while (PR_TRUE)
		{
			PRUnichar ch = *nntpCommand++;
			if (!ch)
				break;
			if (ch == '#' || ch == '?' || ch == '@' || ch == '\\')
			{
				*scratchPtr++ = '\\';
                nsTextFormatter::snprintf(scratchPtr, 2,
                                          NS_ConvertASCIItoUCS2("%2.2X").get(), ch);
                                   /* Reviewed 4.51 safe use of sprintf */
				scratchPtr += 2;
			}
			else
				*scratchPtr++ = ch;
		}
		*scratchPtr = '\0';
		result = nsCRT::strdup (scratchBuf); // realloc down to smaller size
        nsCRT::free (scratchBuf);
	}
	return result;
#endif
}

/* 
   09/21/2000 - taka@netscape.com
   This method is bogus. Escape must be done against char * not PRUnichar *
   should be rewritten later.
   for now, just duplicate the string.
*/
PRUnichar *
nsMsgSearchAdapter::EscapeImapSearchProtocol(const PRUnichar *imapCommand)
{
	return nsCRT::strdup(imapCommand);
#if 0
	PRUnichar *result = nsnull;
	// max escaped length is one extra character for every character in the cmd.
    PRUnichar *scratchBuf =
        (PRUnichar*) PR_Malloc (sizeof(PRUnichar) * (2*nsCRT::strlen(imapCommand) + 1));
	if (scratchBuf)
	{
		PRUnichar *scratchPtr = scratchBuf;
		while (1)
		{
			PRUnichar ch = *imapCommand++;
			if (!ch)
				break;
			if (ch == (PRUnichar)'\\')
			{
				*scratchPtr++ = (PRUnichar)'\\';
				*scratchPtr++ = (PRUnichar)'\\';
			}
			else
				*scratchPtr++ = ch;
		}
		*scratchPtr = 0;
        result = nsCRT::strdup (scratchBuf); // realloc down to smaller size
        nsCRT::free (scratchBuf);
	}
	return result;
#endif
}

/* 
   09/21/2000 - taka@netscape.com
   This method is bogus. Escape must be done against char * not PRUnichar *
   should be rewritten later.
   for now, just duplicate the string.
*/
PRUnichar *
nsMsgSearchAdapter::EscapeQuoteImapSearchProtocol(const PRUnichar *imapCommand)
{
	return nsCRT::strdup(imapCommand);
#if 0
	PRUnichar *result = nsnull;
	// max escaped length is one extra character for every character in the cmd.
    PRUnichar *scratchBuf =
        (PRUnichar*) PR_Malloc (sizeof(PRUnichar) * (2*nsCRT::strlen(imapCommand) + 1));
	if (scratchBuf)
	{
		PRUnichar *scratchPtr = scratchBuf;
		while (1)
		{
			PRUnichar ch = *imapCommand++;
			if (!ch)
				break;
			if (ch == '"')
			{
				*scratchPtr++ = '\\';
				*scratchPtr++ = '"';
			}
			else
				*scratchPtr++ = ch;
		}
		*scratchPtr = '\0';
    result = nsCRT::strdup (scratchBuf); // realloc down to smaller size
    nsCRT::free (scratchBuf);
	}
	return result;
#endif
}


char *nsMsgSearchAdapter::UnEscapeSearchUrl (const char *commandSpecificData)
{
  char *result = (char*) PR_Malloc (nsCRT::strlen(commandSpecificData) + 1);
	if (result)
	{
		char *resultPtr = result;
		while (1)
		{
			char ch = *commandSpecificData++;
			if (!ch)
				break;
			if (ch == '\\')
			{
				char scratchBuf[3];
				scratchBuf[0] = (char) *commandSpecificData++;
				scratchBuf[1] = (char) *commandSpecificData++;
				scratchBuf[2] = '\0';
				int accum = 0;
				sscanf (scratchBuf, "%X", &accum);
				*resultPtr++ = (char) accum;
			}
			else
				*resultPtr++ = ch;
		}
		*resultPtr = '\0';
	}
	return result;
}


void
nsMsgSearchAdapter::GetSearchCharsets(nsString &srcCharset, nsString& dstCharset)
{
    nsresult rv;
	nsAutoString defaultCharset; 
	char *search_charset = nsMsgI18NGetDefaultMailCharset();
	defaultCharset.AssignWithConversion(search_charset);
	PR_Free((void *)search_charset);
	srcCharset = defaultCharset;
	dstCharset = defaultCharset;

	if (m_scope)
	{
    // ### DMB is there a way to get the charset for the "window"?

        nsCOMPtr<nsIMsgFolder> folder;
        rv = m_scope->GetFolder(getter_AddRefs(folder));
        
		// Ask the newsgroup/folder for its csid.
		if (NS_SUCCEEDED(rv) && folder)
        {
            nsXPIDLString folderCharset;
            folder->GetCharset(getter_Copies(folderCharset));
            dstCharset.Assign(folderCharset);
        }
	}


	// If 
	// the destination is still CS_DEFAULT, make the destination match
	// the source. (CS_DEFAULT is an indication that the charset
	// was undefined or unavailable.)
  // ### well, it's not really anymore. Is there an equivalent?
  if (!nsCRT::strcmp(dstCharset.get(), defaultCharset.get()))
		dstCharset = srcCharset;

	PRBool forceAscii = PR_FALSE;

	nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID, &rv));

	if (NS_SUCCEEDED(rv))
	{
		rv = prefs->GetBoolPref("mailnews.force_ascii_search", &forceAscii);
	}

	if (forceAscii)
	{
		// Special cases to use in order to force US-ASCII searching with Latin1
		// or MacRoman text. Eurgh. This only has to happen because IMAP
		// and Dredd servers currently (4/23/97) only support US-ASCII.
		// 
		// If the dest csid is ISO Latin 1 or MacRoman, attempt to convert the 
		// source text to US-ASCII. (Not for now.)
		// if ((dst_csid == CS_LATIN1) || (dst_csid == CS_MAC_ROMAN))
			dstCharset.AssignWithConversion("us-ascii");
	}
}

nsresult nsMsgSearchAdapter::EncodeImapTerm (nsIMsgSearchTerm *term, PRBool reallyDredd, const PRUnichar *srcCharset, const PRUnichar *destCharset, char **ppOutTerm)
{
	nsresult err = NS_OK;
	PRBool useNot = PR_FALSE;
	PRBool useQuotes = PR_FALSE;
	PRBool excludeHeader = PR_FALSE;
	PRBool ignoreValue = PR_FALSE;
	char *arbitraryHeader = nsnull;
	const char *whichMnemonic = nsnull;
	const char *orHeaderMnemonic = nsnull;

	*ppOutTerm = nsnull;

  nsCOMPtr <nsIMsgSearchValue> searchValue;
  nsresult rv = term->GetValue(getter_AddRefs(searchValue));

  if (!NS_SUCCEEDED(rv))
    return rv;

  nsMsgSearchOpValue op;
  term->GetOp(&op);

	if (op == nsMsgSearchOp::DoesntContain || op == nsMsgSearchOp::Isnt)
		useNot = PR_TRUE;

  nsMsgSearchAttribValue attrib;
  term->GetAttrib(&attrib);

	switch (attrib)
	{
	case nsMsgSearchAttrib::ToOrCC:
		orHeaderMnemonic = m_kImapCC;
		// fall through to case nsMsgSearchAttrib::To:
	case nsMsgSearchAttrib::To:
		whichMnemonic = m_kImapTo;
		break;
	case nsMsgSearchAttrib::CC:
		whichMnemonic = m_kImapCC;
		break;
	case nsMsgSearchAttrib::Sender:
		whichMnemonic = m_kImapFrom;
		break;
	case nsMsgSearchAttrib::Subject:
		whichMnemonic = m_kImapSubject;
		break;
	case nsMsgSearchAttrib::Body:
		whichMnemonic = m_kImapBody;
		excludeHeader = PR_TRUE;
		break;
	case nsMsgSearchAttrib::OtherHeader:  // arbitrary header? if so create arbitrary header string
    {
      nsXPIDLCString arbitraryHeaderTerm;
      term->GetArbitraryHeader(getter_Copies(arbitraryHeaderTerm));
      if (nsCRT::strlen((const char *) arbitraryHeaderTerm) > 0)
		  {
			  arbitraryHeader = new char [nsCRT::strlen((const char *)arbitraryHeaderTerm) + 6];  // 6 bytes for SPACE \" .... \" SPACE
			  if (!arbitraryHeader)
				  return NS_ERROR_OUT_OF_MEMORY;
			  arbitraryHeader[0] = '\0';
			  PL_strcat(arbitraryHeader, " \"");
			  PL_strcat(arbitraryHeader, (const char *)arbitraryHeaderTerm);
			  PL_strcat(arbitraryHeader, "\" ");
			  whichMnemonic = arbitraryHeader;
		  }
		  else
			  return NS_ERROR_FAILURE;
    }
		break;
	case nsMsgSearchAttrib::AgeInDays:  // added for searching online for age in days...
		// for AgeInDays, we are actually going to perform a search by date, so convert the operations for age
		// to the IMAP mnemonics that we would use for date!
		switch (op)
		{
		case nsMsgSearchOp::IsGreaterThan:
			whichMnemonic = m_kImapBefore;
			break;
		case nsMsgSearchOp::IsLessThan:
			whichMnemonic = m_kImapSince;
			break;
		case nsMsgSearchOp::Is:
			whichMnemonic = m_kImapSentOn;
			break;
		default:
			NS_ASSERTION(PR_FALSE, "invalid search operator");
			return NS_ERROR_INVALID_ARG;
		}
		excludeHeader = PR_TRUE;
		break;
	case nsMsgSearchAttrib::Date:
		switch (op)
		{
		case nsMsgSearchOp::IsBefore:
			whichMnemonic = m_kImapBefore;
			break;
		case nsMsgSearchOp::IsAfter:
			whichMnemonic = m_kImapSince;
			break;
		case nsMsgSearchOp::Isnt:  /* we've already added the "Not" so just process it like it was a date is search */
		case nsMsgSearchOp::Is:
			whichMnemonic = m_kImapSentOn;
			break;
		default:
			NS_ASSERTION(PR_FALSE, "invalid search operator");
			return NS_ERROR_INVALID_ARG;
		}
		excludeHeader = PR_TRUE;
		break;
	case nsMsgSearchAttrib::AnyText:
		whichMnemonic = m_kImapAnyText;
		excludeHeader = PR_TRUE;
		break;
	case nsMsgSearchAttrib::Keywords:
		whichMnemonic = m_kNntpKeywords;
		break;
	case nsMsgSearchAttrib::MsgStatus:
		useNot = PR_FALSE; // bizarrely, NOT SEEN is wrong, but UNSEEN is right.
		ignoreValue = PR_TRUE; // the mnemonic is all we need
		excludeHeader = PR_TRUE;
    PRUint32 status;
    searchValue->GetStatus(&status);

		switch (status)
		{
		case MSG_FLAG_READ:
			whichMnemonic = op == nsMsgSearchOp::Is ? m_kImapSeen : m_kImapNotSeen;
			break;
		case MSG_FLAG_REPLIED:
			whichMnemonic = op == nsMsgSearchOp::Is ? m_kImapAnswered : m_kImapNotAnswered;
			break;
		default:
			NS_ASSERTION(PR_FALSE, "invalid search operator");
			return NS_ERROR_INVALID_ARG;
		}
		break;
	default:
			NS_ASSERTION(PR_FALSE, "invalid search operator");
		return NS_ERROR_INVALID_ARG;
	}

	char *value = "";
	char dateBuf[100];
	dateBuf[0] = '\0';
	
	PRBool valueWasAllocated = PR_FALSE;
	if (attrib == nsMsgSearchAttrib::Date)
	{
		// note that there used to be code here that encoded an RFC822 date for imap searches.
		// The IMAP RFC 2060 is misleading to the point that it looks like it requires an RFC822
		// date but really it expects dd-mmm-yyyy, like dredd, and refers to the RFC822 date only in that the
		// dd-mmm-yyyy date will match the RFC822 date within the message.

		PRTime adjustedDate;
    searchValue->GetDate(&adjustedDate);
		if (whichMnemonic == m_kImapSince)
		{
			// it looks like the IMAP server searches on Since includes the date in question...
			// our UI presents Is, IsGreater and IsLessThan. For the IsGreater case (m_kImapSince)
			// we need to adjust the date so we get greater than and not greater than or equal to which
			// is what the IMAP server wants to search on
      // won't work on Mac.
      // ack, is this right? is PRTime seconds or microseconds?
	    PRInt64 microSecondsPerSecond, secondsInDay, microSecondsInDay;
	    
	    LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
      LL_UI2L(secondsInDay, 60 * 60 * 24);
	    LL_MUL(microSecondsInDay, secondsInDay, microSecondsPerSecond);
			LL_ADD(adjustedDate, adjustedDate, microSecondsInDay); // bump up to the day after this one...
		}

   	PRExplodedTime exploded;
    PR_ExplodeTime(adjustedDate, PR_LocalTimeParameters, &exploded);
    PR_FormatTimeUSEnglish(dateBuf, sizeof(dateBuf), "%d-%b-%Y", &exploded);
//		strftime (dateBuf, sizeof(dateBuf), "%d-%b-%Y", localtime (/* &term->m_value.u.date */ &adjustedDate));
		value = dateBuf;
	}
	else
	{
		if (attrib == nsMsgSearchAttrib::AgeInDays)
		{
			// okay, take the current date, subtract off the age in days, then do an appropriate Date search on 
			// the resulting day.
      PRUint32 ageInDays;

      searchValue->GetAge(&ageInDays);

			PRTime now = PR_Now();
			PRTime matchDay;

	    PRInt64 microSecondsPerSecond, secondsInDays, microSecondsInDay;
	    
	    LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
      LL_UI2L(secondsInDays, 60 * 60 * 24 * ageInDays);
	    LL_MUL(microSecondsInDay, secondsInDays, microSecondsPerSecond);

			LL_SUB(matchDay, now, microSecondsInDay); // = now - term->m_value.u.age * 60 * 60 * 24; 
   	PRExplodedTime exploded;
    PR_ExplodeTime(matchDay, PR_LocalTimeParameters, &exploded);
    PR_FormatTimeUSEnglish(dateBuf, sizeof(dateBuf), "%d-%b-%Y", &exploded);
//			strftime (dateBuf, sizeof(dateBuf), "%d-%b-%Y", localtime (&matchDay));
			value = dateBuf;
		}
		else

		if (IsStringAttribute(attrib))
		{
			PRUnichar *convertedValue; // = reallyDredd ? MSG_EscapeSearchUrl (term->m_value.u.string) : msg_EscapeImapSearchProtocol(term->m_value.u.string);
            nsXPIDLString searchTermValue;
            searchValue->GetStr(getter_Copies(searchTermValue));
			// Ugly switch for Korean mail/news charsets.
			// We want to do this here because here is where
			// we know what charset we want to use.
#ifdef DOING_CHARSET
			if (reallyDredd)
				dest_csid = INTL_DefaultNewsCharSetID(dest_csid);
			else
				dest_csid = INTL_DefaultMailCharSetID(dest_csid);
#endif

            // do all sorts of crazy escaping
			convertedValue = reallyDredd ? EscapeSearchUrl (searchTermValue) :
                EscapeImapSearchProtocol(searchTermValue);
			useQuotes = !reallyDredd ||
                (nsAutoString(convertedValue).FindChar((PRUnichar)' ') != -1);
			// now convert to char* and escape quoted_specials
			nsresult rv = ConvertFromUnicode(nsAutoString(destCharset), nsAutoString(convertedValue), &value);
			if (NS_SUCCEEDED(rv) && value)
			{
				char *oldValue = value;
				// max escaped length is one extra character for every character in the cmd.
				char *newValue = (char*)PR_Malloc(sizeof(char) * (2*nsCRT::strlen(value) + 1));
				if (newValue)
				{
					char *p = newValue;
					while (1)
					{
						char ch = *value++;
						if (!ch)
							break;
						if ((useQuotes ? ch == '"' : 0) || ch == '\\')
							*p++ = '\\';
						*p++ = ch;
					}
					*p = '\0';
					value = nsCRT::strdup(newValue); // realloc down to smaller size
					nsCRT::free(newValue);
					nsCRT::free(oldValue);
				}
			}
			else
				value = nsCRT::strdup("");
			nsCRT::free(convertedValue);
			valueWasAllocated = PR_TRUE;

		}
	}

	int len = nsCRT::strlen(whichMnemonic) + nsCRT::strlen(value) + (useNot ? nsCRT::strlen(m_kImapNot) : 0) + 
		(useQuotes ? 2 : 0) + nsCRT::strlen(m_kImapHeader) + 
		(orHeaderMnemonic ? (nsCRT::strlen(m_kImapHeader) + nsCRT::strlen(m_kImapOr) + (useNot ? nsCRT::strlen(m_kImapNot) : 0) + 
		nsCRT::strlen(orHeaderMnemonic) + nsCRT::strlen(value) + 2 /*""*/) : 0) + 1;
	char *encoding = new char[len];
	if (encoding)
	{
		encoding[0] = '\0';
		// Remember: if ToOrCC and useNot then the expression becomes NOT To AND Not CC as opposed to (NOT TO) || (NOT CC)
		if (orHeaderMnemonic && !useNot)
			PL_strcat(encoding, m_kImapOr);
		if (useNot)
			PL_strcat (encoding, m_kImapNot);
		if (!excludeHeader)
			PL_strcat (encoding, m_kImapHeader);
		PL_strcat (encoding, whichMnemonic);
		if (!ignoreValue)
			err = EncodeImapValue(encoding, value, useQuotes, reallyDredd);
		
		if (orHeaderMnemonic)
		{
			if (useNot)
				PL_strcat(encoding, m_kImapNot);
			
			PL_strcat (encoding, m_kImapHeader);

			PL_strcat (encoding, orHeaderMnemonic);
			if (!ignoreValue)
				err = EncodeImapValue(encoding, value, useQuotes, reallyDredd);
		}
		
		// kmcentee, don't let the encoding end with whitespace, 
		// this throws off later url STRCMP
		if (*encoding && *(encoding + nsCRT::strlen(encoding) - 1) == ' ')
			*(encoding + nsCRT::strlen(encoding) - 1) = '\0';
	}

	if (value && valueWasAllocated)
		PR_Free (value);
	if (arbitraryHeader)
		delete arbitraryHeader;

	*ppOutTerm = encoding;

	return err;
}

nsresult nsMsgSearchAdapter::EncodeImapValue(char *encoding, const char *value, PRBool useQuotes, PRBool reallyDredd)
{
	// By NNTP RFC, SEARCH HEADER SUBJECT "" is legal and means 'find messages without a subject header'
	if (!reallyDredd)
	{
		// By IMAP RFC, SEARCH HEADER SUBJECT "" is illegal and will generate an error from the server
		if (!value || !value[0])
			return NS_ERROR_NULL_POINTER;
	}
	
	if (useQuotes)
		PL_strcat(encoding, "\"");
	PL_strcat (encoding, value);
	if (useQuotes)
		PL_strcat(encoding, "\"");

	return NS_OK;
}


nsresult nsMsgSearchAdapter::EncodeImap (char **ppOutEncoding, nsISupportsArray *searchTerms, const PRUnichar *srcCharset, const PRUnichar *destCharset, PRBool reallyDredd)
{
	// i've left the old code (before using CBoolExpression for debugging purposes to make sure that
	// the new code generates the same encoding string as the old code.....

	nsresult err = NS_OK;
	*ppOutEncoding = nsnull;

	PRUint32 termCount;
  searchTerms->Count(&termCount);
	PRUint32 i = 0;
	int encodingLength = 0;

	// Build up an array of encodings, one per query term
	char **termEncodings = new char *[termCount];
	if (!termEncodings)
		return NS_ERROR_OUT_OF_MEMORY;
	// create our expression
	nsMsgSearchBoolExpression * expression = new nsMsgSearchBoolExpression();
	if (!expression)
		return NS_ERROR_OUT_OF_MEMORY;

	for (i = 0; i < termCount && NS_SUCCEEDED(err); i++)
	{
    nsCOMPtr<nsIMsgSearchTerm> pTerm;
    searchTerms->QueryElementAt(i, NS_GET_IID(nsIMsgSearchTerm),
                             (void **)getter_AddRefs(pTerm));
		err = EncodeImapTerm (pTerm, reallyDredd, srcCharset, destCharset, &termEncodings[i]);
		if (NS_SUCCEEDED(err) && nsnull != termEncodings[i])
		{
			encodingLength += nsCRT::strlen(termEncodings[i]) + 1;
			expression = expression->AddSearchTerm(pTerm,termEncodings[i]);
		}
	}

	if (NS_SUCCEEDED(err)) 
	{
		// Catenate the intermediate encodings together into a big string
		char *totalEncoding = new char [encodingLength + (!reallyDredd ? nsCRT::strlen(m_kImapUnDeleted) : 0) + 1];
		nsCString encodingBuff;

		if (totalEncoding)
		{
			totalEncoding[0] = '\0';

			int offset = 0;       // offset into starting place for the buffer
			if (!reallyDredd)
				PL_strcat(totalEncoding, m_kImapUnDeleted);

			if (!reallyDredd)
			{
				encodingBuff.Append(m_kImapUnDeleted);
				offset = nsCRT::strlen(m_kImapUnDeleted);
			}

			expression->GenerateEncodeStr(&encodingBuff);

			for (i = 0; i < termCount; i++)
			{
				if (termEncodings[i])
				{
					PL_strcat (totalEncoding, termEncodings[i]);
					delete [] termEncodings[i];
				}
			}
		}
		else
			err = NS_ERROR_OUT_OF_MEMORY;

		delete totalEncoding;
		delete expression;

		// Set output parameter if we encoded the query successfully
		if (NS_SUCCEEDED(err))
			*ppOutEncoding = ToNewCString(encodingBuff);
	}

	delete [] termEncodings;

	return err;
}


char *nsMsgSearchAdapter::TransformSpacesToStars (const char *spaceString, msg_TransformType transformType)
{
	char *starString;
	
	if (transformType == kOverwrite)
	{
		if ((starString = nsCRT::strdup(spaceString)) != nsnull)
		{
			char *star = starString;
			while ((star = PL_strchr(star, ' ')) != nsnull)
				*star = '*';
		}
	}
	else
	{
		int i, count;

		for (i = 0, count = 0; spaceString[i]; )
		{
			if (spaceString[i++] == ' ')
			{
				count++;
				while (spaceString[i] && spaceString[i] == ' ') i++;
			}
		}

		if (transformType == kSurround)
			count *= 2;

		if (count > 0)
		{
			if ((starString = (char *)PR_Malloc(i + count + 1)) != nsnull)
			{
				int j;

				for (i = 0, j = 0; spaceString[i]; )
				{
					if (spaceString[i] == ' ')
					{
						starString[j++] = '*';
						starString[j++] = ' ';
						if (transformType == kSurround)
							starString[j++] = '*';

						i++;
						while (spaceString[i] && spaceString[i] == ' ')
							i++;
					}
					else
						starString[j++] = spaceString[i++];
				}
				starString[j] = 0;
			}
		}
		else
			starString = nsCRT::strdup(spaceString);
	}

	return starString;
}

//-----------------------------------------------------------------------------
//------------------- Validity checking for menu items etc. -------------------
//-----------------------------------------------------------------------------

nsMsgSearchValidityTable::nsMsgSearchValidityTable ()
{
    NS_INIT_ISUPPORTS();
	// Set everything to be unavailable and disabled
	for (int i = 0; i < nsMsgSearchAttrib::kNumMsgSearchAttributes; i++)
    for (int j = 0; j < nsMsgSearchOp::kNumMsgSearchOperators; j++)
		{
			SetAvailable (i, j, PR_FALSE);
			SetEnabled (i, j, PR_FALSE);
			SetValidButNotShown (i,j, PR_FALSE);
		}
	m_numAvailAttribs = 0;   // # of attributes marked with at least one available operator
}

NS_IMPL_ISUPPORTS1(nsMsgSearchValidityTable, nsIMsgSearchValidityTable)


nsresult
nsMsgSearchValidityTable::GetNumAvailAttribs(PRInt32 *aResult)
{
	m_numAvailAttribs = 0;
	for (int i = 0; i < nsMsgSearchAttrib::kNumMsgSearchAttributes; i++)
		for (int j = 0; j < nsMsgSearchOp::kNumMsgSearchOperators; j++) {
            PRBool available;
            GetAvailable(i, j, &available);
			if (available)
			{
				m_numAvailAttribs++;
				break;
			}
        }
	*aResult = m_numAvailAttribs;
    return NS_OK;
}

nsresult
nsMsgSearchValidityTable::ValidateTerms (nsISupportsArray *searchTerms)
{
	nsresult err = NS_OK;
  PRUint32 count;

  NS_ENSURE_ARG(searchTerms);

  searchTerms->Count(&count);
	for (PRUint32 i = 0; i < count; i++)
	{
    nsCOMPtr<nsIMsgSearchTerm> pTerm;
    searchTerms->QueryElementAt(i, NS_GET_IID(nsIMsgSearchTerm),
                             (void **)getter_AddRefs(pTerm));

		nsIMsgSearchTerm *iTerm = pTerm;
		nsMsgSearchTerm *term = NS_STATIC_CAST(nsMsgSearchTerm *, iTerm);
//		XP_ASSERT(term->IsValid());
        PRBool enabled;
        PRBool available;
        GetEnabled(term->m_attribute, term->m_operator, &enabled);
        GetAvailable(term->m_attribute, term->m_operator, &available);
		if (!enabled || !available)
		{
            PRBool validNotShown;
            GetValidButNotShown(term->m_attribute, term->m_operator,
                                &validNotShown);
            if (!validNotShown)
				err = NS_MSG_ERROR_INVALID_SEARCH_SCOPE;
		}
	}

	return err;
}

nsresult
nsMsgSearchValidityTable::GetAvailableAttributes(PRUint32 *length,
                                                 nsMsgSearchAttribValue **aResult)
{
    // count first
    PRUint32 totalAttributes=0;
    PRInt32 i, j;
    for (i = 0; i< nsMsgSearchAttrib::kNumMsgSearchAttributes; i++) {
        for (j=0; j< nsMsgSearchOp::kNumMsgSearchOperators; j++) {
            if (m_table[i][j].bitAvailable) {
                totalAttributes++;
                break;
            }
        }
    }

    nsMsgSearchAttribValue *array = (nsMsgSearchAttribValue*)
        nsMemory::Alloc(sizeof(nsMsgSearchAttribValue) * totalAttributes);
    NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);
    
    PRUint32 numStored=0;
    for (i = 0; i< nsMsgSearchAttrib::kNumMsgSearchAttributes; i++) {
        for (j=0; j< nsMsgSearchOp::kNumMsgSearchOperators; j++) {
            if (m_table[i][j].bitAvailable) {
                array[numStored++] = i;
                break;
            }
        }
    }

    NS_ASSERTION(totalAttributes == numStored, "Search Attributes not lining up");
    *length = totalAttributes;
    *aResult = array;

    return NS_OK;
}

nsresult
nsMsgSearchValidityTable::GetAvailableOperators(nsMsgSearchAttribValue aAttribute,
                                                PRUint32 *aLength,
                                                nsMsgSearchOpValue **aResult)
{
    PRUint32 totalOperators=0;
    PRInt32 i;
    for (i=0; i<nsMsgSearchOp::kNumMsgSearchOperators; i++) {
        if (m_table[aAttribute][i].bitAvailable)
            totalOperators++;
    }

    nsMsgSearchOpValue *array = (nsMsgSearchOpValue*)
        nsMemory::Alloc(sizeof(nsMsgSearchOpValue) * totalOperators);
    NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);
    
    PRUint32 numStored = 0;
    for (i=0; i<nsMsgSearchOp::kNumMsgSearchOperators;i++) {
        if (m_table[aAttribute][i].bitAvailable)
            array[numStored++] = i;
    }

    NS_ASSERTION(totalOperators == numStored, "Search Operators not lining up");
    *aLength = totalOperators;
    *aResult = array;
    return NS_OK;
}


nsMsgSearchValidityManager::nsMsgSearchValidityManager ()
{
    NS_INIT_ISUPPORTS();
}


nsMsgSearchValidityManager::~nsMsgSearchValidityManager ()
{
    // tables released by nsCOMPtr
}

NS_IMPL_ISUPPORTS1(nsMsgSearchValidityManager, nsIMsgSearchValidityManager)

//-----------------------------------------------------------------------------
// Bottleneck accesses to the objects so we can allocate and initialize them
// lazily. This way, there's no heap overhead for the validity tables until the
// user actually searches that scope.
//-----------------------------------------------------------------------------

nsresult nsMsgSearchValidityManager::GetTable (int whichTable, nsIMsgSearchValidityTable **ppOutTable)
{
	NS_ENSURE_ARG(ppOutTable);

	nsresult err = NS_OK;
    *ppOutTable = nsnull;

	switch (whichTable)
	{
	case nsMsgSearchScope::offlineMail:
		if (!m_offlineMailTable)
			err = InitOfflineMailTable ();
		*ppOutTable = m_offlineMailTable;
		break;
	case nsMsgSearchScope::onlineMail:
		if (!m_onlineMailTable)
			err = InitOnlineMailTable ();
		*ppOutTable = m_onlineMailTable;
		break;
	case nsMsgSearchScope::onlineMailFilter:
		if (!m_onlineMailFilterTable)
			err = InitOnlineMailFilterTable ();
		*ppOutTable = m_onlineMailFilterTable;
		break;
	case nsMsgSearchScope::news:
		if (!m_newsTable)
			err = InitNewsTable ();
		*ppOutTable = m_newsTable;
		break;
	case nsMsgSearchScope::localNews:
		if (!m_localNewsTable)
			err = InitLocalNewsTable();
		*ppOutTable = m_localNewsTable;
		break;
#ifdef DOING_EXNEWSSEARCH
	case nsMsgSearchScope::newsEx:
		if (!m_newsExTable)
			err = InitNewsExTable ();
		*ppOutTable = m_newsExTable;
		break;
#endif
#ifdef DOING_LDAP
	case nsMsgSearchScope::LDAP:
		if (!m_ldapTable)
			err = InitLdapTable ();
		*ppOutTable = m_ldapTable;
		break;
#endif
	default:                 
		NS_ASSERTION(PR_FALSE, "invalid table type");
		err = NS_MSG_ERROR_INVALID_SEARCH_TERM;
	}

    NS_IF_ADDREF(*ppOutTable);
	return err;
}



nsresult
nsMsgSearchValidityManager::NewTable(nsIMsgSearchValidityTable **aTable)
{
	NS_ENSURE_ARG (aTable);
	*aTable = new nsMsgSearchValidityTable;
	if (nsnull == *aTable)
		return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*aTable);
    return NS_OK;
}


