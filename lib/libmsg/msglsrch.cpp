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
// Implementation of search for LDAP directories
//

#include "rosetta.h"
#include "msg.h"
#include "xpgetstr.h"
#include "pmsgsrch.h" // private search API 
#include "msgutils.h" // for msg_GetURL
#include "dirprefs.h"
#include "libi18n.h"
#include HG77677
#include "intl_csi.h"

#ifdef MOZ_LDAP
	#define NEEDPROTOS
	#define LDAP_REFERRALS
	#include "lber.h"
	#include "ldap.h"
	#include "disptmpl.h"
#endif /* MOZ_LDAP */

extern "C"
{
	extern int XP_PROGRESS_CONTACTHOST;

	extern int XP_LDAP_SIZELIMIT_EXCEEDED;
	extern int XP_LDAP_SERVER_SIZELIMIT_EXCEEDED;
	extern int XP_LDAP_OPEN_ENTRY_FAILED;
	extern int XP_LDAP_OPEN_SERVER_FAILED;
	extern int XP_LDAP_BIND_FAILED;
	extern int XP_LDAP_SEARCH_FAILED;
	extern int XP_LDAP_MODIFY_FAILED;

	extern int MK_MSG_SEARCH_STATUS;
	extern int XP_ALERT_OFFLINE_MODE_SELECTED;
	extern int MK_LDAP_AUTH_PROMPT;
}


#ifdef MOZ_LDAP
// This class holds data needed across the modify operations
class LdapModifyBucket
{
public:
	LdapModifyBucket () 
	{
		dnValue = NULL;
		modList[0] = &mod;
		berValList[0] = &ber;
		modList[1] = NULL;
		berValList[1] = NULL;
	}
	~LdapModifyBucket () 
	{
		MSG_DestroySearchValue (dnValue);
		XP_FREE (ber.bv_val);
	}

	LDAPMod *modList[2];
	LDAPMod mod;

	struct berval *berValList[2];
	struct berval ber;
	MSG_SearchValue *dnValue;
};
#endif /* MOZ_LDAP */


msg_SearchLdap::msg_SearchLdap (MSG_ScopeTerm *scope, MSG_SearchTermArray &termList) : msg_SearchAdapter (scope, termList) 
{
#ifdef MOZ_LDAP
	m_ldap = NULL;
	m_message = NULL;
	m_modifyBucket = NULL;
	m_filter = NULL;

	m_password = NULL;
	m_authDn = NULL;
	m_valueUsedToFindDn = NULL;

	m_nextState = kInitialize;
#endif /* MOZ_LDAP */
}


msg_SearchLdap::~msg_SearchLdap ()
{
	XP_FREEIF(m_filter);
	XP_FREEIF(m_password);
	XP_FREEIF(m_valueUsedToFindDn);
	XP_FREEIF(m_authDn);

    ldap_msgfree (m_message);

#ifdef MOZ_LDAP
	UnbindRequest ();
#endif /* MOZ_LDAP */
}


MSG_SearchError msg_SearchLdap::ValidateTerms ()
{
	XP_FREEIF(m_filter);
	m_filter = NULL;

	MSG_SearchError err = msg_SearchAdapter::ValidateTerms ();
	if (SearchError_Success == err)
		err = Encode (&m_filter);
	return err;
}


MSG_SearchError msg_SearchLdap::Search ()
{
#ifndef MOZ_LDAP
	return SearchError_NotImplemented;
#else
	MSG_SearchError err = SearchError_Success;
	switch (m_nextState)
	{
	case kInitialize:
		err = Initialize();
		break;
	case kPreAuthBindRequest:
		err = PreAuthBindRequest ();
		break;
	case kPreAuthBindResponse:
		err = PreAuthBindResponse ();
		break;
	case kPreAuthSearchRequest:
		err = PreAuthSearchRequest ();
		break;
	case kPreAuthSearchResponse:
		err = PreAuthSearchResponse ();
		break;
	case kAnonymousBindRequest:
		err = AnonymousBindRequest();
		break;
	case kAuthenticatedBindRequest:
		err = AuthenticatedBindRequest ();
		break;
	case kBindResponse:
		err = BindResponse ();
		break;
	case kSearchRequest:
		if (   GetSearchType() == searchNormal
			|| (   GetSearchType() != searchRootDSE
				&& DIR_TestFlag(GetDirServer(), DIR_LDAP_ROOTDSE_PARSED)))
			err = SearchRequest ();
		else
			err = SearchRootDSERequest ();
		break;
	case kSearchVLVSearchRequest:
		err = SearchVLVSearchRequest ();
		break;
	case kSearchVLVIndexRequest:
		err = SearchVLVIndexRequest ();
		break;
	case kSearchResponse:
		if (   GetSearchType() == searchNormal
			|| (   GetSearchType() != searchRootDSE
				&& DIR_TestFlag(GetDirServer(), DIR_LDAP_ROOTDSE_PARSED)))
			err = SearchResponse ();
		else
			err = SearchRootDSEResponse ();
		break;
	case kSearchVLVSearchResponse:
		err = SearchVLVSearchResponse ();
		break;
	case kSearchVLVIndexResponse:
		err = SearchVLVIndexResponse ();
		break;
	case kModifyRequest :
		err = ModifyRequest ();
		break;
	case kModifyResponse:
		err = ModifyResponse ();
		break;
	case kUnbindRequest:
		err = UnbindRequest ();
		break;
	default:
		XP_ASSERT(FALSE);
	}

	if (SearchError_ScopeDone == err)
	{
		UnbindRequest();
		m_scope->m_frame->EndCylonMode();
	}
	return err;
#endif /* MOZ_LDAP */
}

	
DIR_Server *msg_SearchLdap::GetDirServer () 
{ 
	return m_scope->m_server; 
}

typedef struct AttribToIdMap
{
	MSG_SearchAttribute attrib;
	DIR_AttributeId id;
} AttribToIdMap;


// string overhead per encoded term is 
// req'd: '=' + '(' + ')' + '\0' + optional: at most two '*' + at most one '!' + () for '!' terms
#define kLdapTermOverhead 9


MSG_SearchError msg_SearchLdap::EncodeCustomFilter (const char *value, const char *filter, char **outEncoding)
{
	MSG_SearchError err = SearchError_Success;
	*outEncoding = NULL;

	// Find out how many occurrences of %s there are
	const char *walkFilter = filter;
	int substCount = 0;
	while (*walkFilter)
	{
		if (!XP_STRNCASECMP(walkFilter, "%s", 2))
		{
			substCount++;
			walkFilter += 2;
		}
		else
			walkFilter++;
	}

	// Allocate a string large enough to substitute the value as many times as necessary
	int valueLength = XP_STRLEN(value);
	int length = XP_STRLEN(filter) + (substCount * valueLength);
	*outEncoding = (char*) XP_ALLOC(length + 1);

	// Build the encoding. ### it would be nice to allow %$1 formatting here
	if (*outEncoding)
	{
		char *walkEncoding = *outEncoding;
		walkFilter = filter;
		while (*walkFilter)
		{
			if (!XP_STRNCASECMP(walkFilter, "%s", 2))
			{
				XP_STRCPY(walkEncoding, value);
				walkEncoding += valueLength;
				walkFilter += 2;
			}
			else
				*walkEncoding++ = *walkFilter++;
		}
		*walkEncoding = '\0';
	}
	else
		err = SearchError_OutOfMemory;

	return err;
}


MSG_SearchError msg_SearchLdap::EncodeDwim (const char *mnemonic, const char *value, 
											char **ppOutEncoding, int *pEncodingLength)
{
	MSG_SearchError err = SearchError_Success;
	DIR_Server *dirServer = GetDirServer();

	const char *filter = DIR_GetFilterString (dirServer);
	if (TRUE /*!DIR_RepeatFilterForTokens (dirServer, filter)*/) // I'm not sure this makes sense anymore
	{
		char *wildValue = NULL;
		XP_Bool valueContainedSpaces = FALSE;
		if (value)
		{
			valueContainedSpaces = (XP_STRCHR(value, ' ') != NULL);
			wildValue = TransformSpacesToStars (value);
			if (!wildValue)
				return SearchError_OutOfMemory;
		}

		if (filter)
			err = EncodeCustomFilter (wildValue, filter, ppOutEncoding);
		else
		{
			// There are a bunch of heuristics happening here. I don't know if this is all correct, but
			// here's the current thinking:
			// - If there's no value, don't try to sprintf it
			// - If there's an empty value let it through
			// - If the LDAP server supports VLV, we want a typedown style search, without a leading *
			// - If the user typed a phrase separated by spaces, we want a typedown style search, without a leading *
			// - If the user just typed one word, we'll drop down to a contains style search, with a leading *
			if (!value)
				*ppOutEncoding = PR_smprintf ("%s=*");
			else if (GetSearchType()== searchLdapVLV || !*wildValue || valueContainedSpaces)
				*ppOutEncoding = PR_smprintf ("%s=%s*", mnemonic, wildValue);
			else
				*ppOutEncoding = PR_smprintf ("%s=*%s*", mnemonic, wildValue);
		}

		XP_FREEIF(wildValue);

		if (SearchError_Success == err && *ppOutEncoding)
			*pEncodingLength += XP_STRLEN(*ppOutEncoding);
		else
		{
			*pEncodingLength = 0;
			err = SearchError_OutOfMemory;
		}
		return err;
	}

	char *token = NULL;
	char *allocatedToken = NULL;

	msg_StringArray commaSeparatedTokens (TRUE /*owns memory for substrings*/);
	commaSeparatedTokens.ImportTokenList (value, ",;");

	XPPtrArray *singleWordTokens = new XPPtrArray [commaSeparatedTokens.GetSize()];
	if (singleWordTokens)
	{
		int cchSingleWords = 0;
		int i;

		for (i = 0; i < commaSeparatedTokens.GetSize(); i++)
		{
			char *subValue = XP_STRDUP (commaSeparatedTokens.GetAt(i)); // make a copy since strtok will change it
			if (!subValue)
				break;

			token = XP_STRTOK(subValue, " ");
			while (token)
			{
				if (!XP_STRCMP(token, "*"))
				{
					// A special case for the user typing just one star
					allocatedToken = PR_smprintf ("(%s=%s)", mnemonic, token);
				}
				else
				{
					// In the general case, just encode the filter
					const char *filterString = DIR_GetFilterString (GetDirServer());
					if (filterString)
					{
						allocatedToken = NULL;
						err = EncodeCustomFilter (token, filterString, &allocatedToken);
					}
					else
						allocatedToken = PR_smprintf ("(%s=%s*)", mnemonic, token);
				}
		
				if (allocatedToken)
				{
					cchSingleWords += XP_STRLEN(allocatedToken) + 1;
					singleWordTokens[i].Add(allocatedToken);
				}

				token = XP_STRTOK(NULL, " ");
			}
			XP_FREE(subValue);
		}

		*pEncodingLength = cchSingleWords + (commaSeparatedTokens.GetSize() * 4) + 4;
		*ppOutEncoding = (char*) XP_ALLOC (*pEncodingLength); 
		if (*ppOutEncoding)
		{
			if (commaSeparatedTokens.GetSize() > 1)
				XP_STRCPY (*ppOutEncoding, "(|");
			else
				*ppOutEncoding[0] = '\0';
			for (i = 0; i < commaSeparatedTokens.GetSize(); i++)
			{
				if (singleWordTokens[i].GetSize() > 1)
					XP_STRCAT (*ppOutEncoding, "(&");
				for (int j = 0; j < singleWordTokens[i].GetSize(); j++)
				{
					char *word = (char*) /*hack*/ singleWordTokens[i].GetAt(j);
					XP_STRCAT (*ppOutEncoding, word);
					XP_FREE(word);
				}
				if (singleWordTokens[i].GetSize() > 1)
					XP_STRCAT (*ppOutEncoding, ")");
			}
			if (commaSeparatedTokens.GetSize() > 1)
				XP_STRCAT (*ppOutEncoding, ")");
		}
		else
			err = SearchError_OutOfMemory;

		delete [] singleWordTokens;
	}
	else
		err = SearchError_OutOfMemory;

	return err;
}


MSG_SearchError msg_SearchLdap::EncodeMnemonic (MSG_SearchTerm *pTerm, const char *whichMnemonic, char **ppOutEncoding, int *pEncodingLength)
{
	MSG_SearchError err = SearchError_Success;
	char *utf8String = NULL;
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_scope->m_frame->GetContext());

	*ppOutEncoding = NULL;
	*pEncodingLength = 0;

	if (IsStringAttribute(pTerm->m_attribute)) 
	{
		utf8String = DIR_ConvertToServerCharSet (m_scope->m_server, pTerm->m_value.u.string, INTL_GetCSIWinCSID(c));
		if (!utf8String)
			return SearchError_InvalidSearchTerm;
	}

	// If we're doing the simple, Address Book search, use a different encoding system
	if (opLdapDwim == pTerm->m_operator)
		err = EncodeDwim (whichMnemonic, utf8String, ppOutEncoding, pEncodingLength);
	else
	{
		// Build the actual LDAP filter encoding for this attribute
		*pEncodingLength = XP_STRLEN (utf8String) + XP_STRLEN (whichMnemonic) + kLdapTermOverhead;
		*ppOutEncoding = (char*) XP_ALLOC (*pEncodingLength);

		if (*ppOutEncoding)
		{
			// Figure out which operators apply. Our negative operators should
			// be mapped to a prefix '!', and our contains operators should be
			// mapped to '*' around the value string
			XP_Bool useBang = FALSE;
			XP_Bool useTilde = FALSE;
			XP_Bool leadingStar = FALSE;
			XP_Bool trailingStar = FALSE;
			switch (pTerm->m_operator)
			{
			case opDoesntContain:
				leadingStar = trailingStar = TRUE;
				// intentional fall-through
			case opIsnt:
				useBang = TRUE;
				break;
			case opContains:
				leadingStar = trailingStar = TRUE;
				break;
			case opIs:
				// nothing req'd, 
				break;
			case opSoundsLike:
				useTilde = TRUE;
				break;
			case opBeginsWith:
				trailingStar = TRUE;
				break;
			case opEndsWith:
				leadingStar = TRUE;
				break;
			default:
				err = SearchError_ScopeAgreement;
			}
			XP_STRCPY (*ppOutEncoding, "(");
			if (useBang)
				XP_STRCAT (*ppOutEncoding, "!(");
			XP_STRCAT (*ppOutEncoding, whichMnemonic);
			if (useTilde)
				XP_STRCAT (*ppOutEncoding, "~");
			XP_STRCAT (*ppOutEncoding, "=");
			if ((leadingStar || trailingStar) && XP_STRCMP (utf8String, "*"))
			{
				char *wildValue = NULL;
				if (pTerm->m_operator == opContains)
					wildValue = TransformSpacesToStars(utf8String);
				else
					wildValue = XP_STRDUP(utf8String);
				if (wildValue)
				{
					if (leadingStar)
						XP_STRCAT (*ppOutEncoding, "*");
					XP_STRCAT (*ppOutEncoding, wildValue);
					if (trailingStar)
						XP_STRCAT (*ppOutEncoding, "*");

					XP_FREE(wildValue);
				}
				else
					err = SearchError_OutOfMemory;
			}
			else
				XP_STRCAT (*ppOutEncoding, utf8String);
			XP_STRCAT (*ppOutEncoding, ")");
			if (useBang)
				XP_STRCAT (*ppOutEncoding, ")");
		}
		else
			err = SearchError_OutOfMemory;
	}

	XP_FREEIF (utf8String);

	return err;
}


MSG_SearchError msg_SearchLdap::EncodeTerm (MSG_SearchTerm *pTerm, char **ppOutEncoding, int *pEncodingLength)
{
	// In the general case, there is only one LDAP attribute per search term. However, with 
	// site-configurable LDAP preferences, administrators can set up several attributes, such
	// that saying "E-mail address" really means (|(mail=foo)(xmail=foo)(ymail=foo)). Dealing
	// with that substitution is the role of this function.

	MSG_SearchError err = SearchError_Success;
	*pEncodingLength = 0;
	DIR_Server *server = GetDirServer();

	DIR_AttributeId id;
	err = MSG_SearchAttribToDirAttrib (pTerm->m_attribute, &id);

	if (SearchError_Success == err)
	{
		const char **mnemonicList = DIR_GetAttributeStrings (server, id);
		int count = 0;
		while (mnemonicList[count])
			count++;
		char **encodingsPerMnemonic = (char**) XP_ALLOC (count * sizeof (char*));
		if (encodingsPerMnemonic)
		{
			int encodingLength = 0;
			int totalEncodingLength = 0;
			int i;
			for (i = 0; i < count; i++)
			{
				err = EncodeMnemonic (pTerm, mnemonicList[i], &encodingsPerMnemonic[i], &encodingLength);
				totalEncodingLength += encodingLength;
			}

			*pEncodingLength = totalEncodingLength + 1;
			if (count > 1)
				*pEncodingLength += 3;
			*ppOutEncoding = (char*) XP_ALLOC (*pEncodingLength);
			if (*ppOutEncoding)
			{
				*ppOutEncoding[0] = '\0';
				if (count > 1)
					XP_STRCAT (*ppOutEncoding, "(|");
				for (i = 0; i < count; i++)
				{
					if (encodingsPerMnemonic[i])
					{
						XP_STRCAT(*ppOutEncoding, encodingsPerMnemonic[i]);
						XP_FREE (encodingsPerMnemonic[i]);
					}
				}
				XP_FREE (encodingsPerMnemonic);
				if (count > 1)
					XP_STRCAT(*ppOutEncoding, ")");
			}
			else
				err = SearchError_OutOfMemory;
		}
		else
			err = SearchError_OutOfMemory;
	}

	return err;
}


MSG_SearchError msg_SearchLdap::Encode (char **ppOutEncoding)
{
	// Generate the LDAP search encodings as specified in rfc-1558.

	if (!ppOutEncoding)
		return SearchError_NullPointer;
	MSG_SearchError err = SearchError_Success;

	// list of encodings, one per term
	int cchTermEncodings = 0;
	char **termEncodings = (char**) XP_ALLOC (m_searchTerms.GetSize() * sizeof (char*));  
	if (!termEncodings)
		return SearchError_OutOfMemory;

	XP_Bool booleanAND = TRUE;    // should the terms be ANDed or ORed together?
	
	// Encode each term in the list into its string form
	for (int i = 0; i < m_searchTerms.GetSize() && SearchError_Success == err; i++)
	{
		MSG_SearchTerm *pTerm = m_searchTerms.GetAt(i);
		XP_ASSERT (pTerm->IsValid());
		if (pTerm->IsValid())
		{
			booleanAND = pTerm->IsBooleanOpAND();
			int cchThisEncoding = 0;
			err = EncodeTerm (pTerm, &termEncodings[i], &cchThisEncoding);
			cchTermEncodings += cchThisEncoding;
		}
	}

	// Build up the string containing all the term encodings catenated together

	int idx = 0;
	char *pTotalEncoding = (char*) XP_ALLOC (cchTermEncodings + kLdapTermOverhead + 1); // +1 for '&' between terms
	if (pTotalEncoding && SearchError_Success == err)
	{
		pTotalEncoding[0] = '\0';

		if (m_searchTerms.GetSize() > 1)
		{
			// since a search term can have AND or OR boolean operators, check the first term to determine which
			// symbol we need.
			if (booleanAND)
				XP_STRCPY (pTotalEncoding, "(&");   // use AND symbol
			else
				XP_STRCPY (pTotalEncoding, "(|");	// use OR symbol

			for (idx = 0; idx < m_searchTerms.GetSize(); idx++)
				XP_STRCAT (pTotalEncoding, termEncodings[idx]);
			XP_STRCAT (pTotalEncoding, ")");
		}
		else
			XP_STRCAT (pTotalEncoding, termEncodings[0]);
	}
	else
		err = SearchError_OutOfMemory;

	// Clean up the intermediate encodings
	for (idx = 0; idx < m_searchTerms.GetSize(); idx++)
		XP_FREEIF (termEncodings[idx]);
	XP_FREE (termEncodings);

	// Only return the total encoding if we're sure it's right
	if (SearchError_Success == err)
		*ppOutEncoding = pTotalEncoding;
	else
	{
 		XP_FREE (pTotalEncoding);
		*ppOutEncoding = NULL;
	}

	return err;
}


MSG_SearchError msg_SearchLdap::BuildUrl (const char *dn, char **outUrl, XP_Bool forAddToAB)
{
	MSG_SearchError err = SearchError_Success;
	*outUrl = DIR_BuildUrl (m_scope->m_server, dn, forAddToAB);
	if (NULL == *outUrl)
		err = SearchError_OutOfMemory;
	return err;
}


MSG_SearchError msg_SearchLdap::OpenResultElement(MSG_MessagePane *,
												  MSG_ResultElement *)
{
	XP_ASSERT(FALSE); // shouldn't get here with a messagePane
	return SearchError_NotImplemented;
}


MSG_SearchError msg_SearchLdap::OpenResultElement(MWContext *context, MSG_ResultElement *result)
{
	MSG_SearchError err = SearchError_NotImplemented;

#ifdef MOZ_LDAP
	// fire a URL at a browser context, and use mkldap.cpp to build HTML 
	MSG_SearchValue *dnValue = NULL;
	err = result->GetValue (attribDistinguishedName, &dnValue);
	if (SearchError_Success == err)
	{
		char *url = NULL;
		err = BuildUrl (dnValue->u.string, &url, FALSE /*addToAB*/);
		if (SearchError_Success == err)
		{
			URL_Struct *url_s = NET_CreateURLStruct (url, NET_NORMAL_RELOAD);
			if (url_s)
			{
				msg_GetURL (context, url_s, FALSE);
				XP_FREE(url);
			}
		}
		MSG_DestroySearchValue (dnValue);
	}
#endif /* MOZ_LDAP */

	return err;
}


MSG_SearchError msg_SearchLdap::ModifyResultElement(MSG_ResultElement *result, MSG_SearchValue *value)
{
	MSG_SearchError err = SearchError_Success;
#ifdef MOZ_LDAP
	char *platformName = value->u.string + XP_STRLEN("file://");
	XP_File file = XP_FileOpen (platformName, xpMailFolder, XP_FILE_READ_BIN);
	if (file)
	{
		XP_FileSeek (file, 0, SEEK_END);
		int length = XP_FileTell (file);
		char *jpegBuf = (char*) XP_ALLOC (length + 1);
		if (jpegBuf)
		{
			XP_FileSeek (file, 0, SEEK_SET);
			XP_FileRead (jpegBuf, length, file);
			XP_FileClose (file);

			MSG_SearchValue *value = NULL;
			err = result->GetValue (attribDistinguishedName, &value);
			if (SearchError_Success == err)
			{
				m_modifyBucket = new LdapModifyBucket;
				if (m_modifyBucket)
				{
					m_modifyBucket->dnValue = value; // deleted in bucket dtor

					m_modifyBucket->ber.bv_len = length;
					m_modifyBucket->ber.bv_val = jpegBuf;

					m_modifyBucket->mod.mod_op = LDAP_MOD_REPLACE | LDAP_MOD_BVALUES;
					m_modifyBucket->mod.mod_type = "jpegphoto";
					m_modifyBucket->mod.mod_vals.modv_bvals = m_modifyBucket->berValList;

					Initialize();
					m_nextState = kPreAuthBindRequest;
					CollectUserCredentials();

					m_scope->m_frame->m_parallelScopes.Add(m_scope);
					err = m_scope->m_frame->GetUrl ();
				}
				else
					err = SearchError_OutOfMemory; 
			}
			else
				err = SearchError_ScopeAgreement; 
		}
		else
			err = SearchError_OutOfMemory;
	}

#endif /* MOZ_LDAP */
	return err;
}


#ifdef MOZ_LDAP

int msg_SearchLdap::PollConnection ()
{
	struct timeval timeout;
	XP_BZERO (&timeout, sizeof(timeout));
    ldap_msgfree (m_message);
	int err = ldap_result (m_ldap, m_currentMessage, 0, &timeout, &m_message);
	if (err == 0)
		err = LDAP_TIMEOUT;
	else
		if (err != -1 && err != LDAP_RES_SEARCH_ENTRY /*&& err != LDAP_RES_SEARCH_RESULT*/)
		{
			int err2 = ldap_result2error (m_ldap, m_message, 0);
			if (err2 != 0)
				err = err2;
		}
	return err;
}


MSG_SearchError msg_SearchLdap::Initialize ()
{
	if (NET_IsOffline())
	{
		FE_Alert (m_scope->m_frame->GetContext(), XP_GetString(XP_ALERT_OFFLINE_MODE_SELECTED));
		return SearchError_ScopeDone;
	}

	MSG_SearchError err = SearchError_Success;
	m_scope->m_frame->UpdateStatusBar (XP_PROGRESS_CONTACTHOST);
	m_scope->m_frame->BeginCylonMode();

	m_ldap = ldap_init ((char*) GetHostName(), GetPort());
	if (m_ldap)
	{
		XP_Bool wantReferrals = TRUE;
		ldap_set_option (m_ldap, LDAP_OPT_REFERRALS, &wantReferrals);

		// Set up the max number of entries to return here. I don't
		// think we should have a time limit since we have a Stop button.
		int maxHits = GetMaxHits();
		ldap_set_option (m_ldap, LDAP_OPT_SIZELIMIT, &maxHits);

		if (DIR_TestFlag(GetDirServer(), DIR_LDAP_VERSION3))
		{
			int version = LDAP_VERSION3;
			ldap_set_option (m_ldap, LDAP_OPT_PROTOCOL_VERSION, &version);
		}

		if (GetEnableAuth() && SearchError_Success == CollectUserCredentials())
		{
			if (m_authDn && m_password)
			{
				// Got the credentials directly from prefs; go right to auth 
				// bind since there's no need to go fish for the DN
				m_nextState = kAuthenticatedBindRequest;
			}
			else
				m_nextState = kPreAuthBindRequest;
		}
		else 
			m_nextState = kAnonymousBindRequest;
	}
	else
	{
		DisplayError (XP_LDAP_OPEN_SERVER_FAILED, GetHostDescription(), LDAP_SERVER_DOWN);
		err = SearchError_ScopeDone; 
	}

	return err;
}


MSG_SearchError msg_SearchLdap::PreAuthBindRequest ()
{
	MSG_SearchError err = BindRequest (NULL, NULL); // turns out to mean the same thing
	if (SearchError_Success == err)
		m_nextState = kPreAuthBindResponse;
	return err;
}


MSG_SearchError msg_SearchLdap::PreAuthBindResponse ()
{
	MSG_SearchError err = BindResponse (); // turns out to mean the same thing
	m_nextState = kPreAuthSearchRequest;
	return err;
}


MSG_SearchError msg_SearchLdap::PreAuthSearchRequest ()
{
	MSG_SearchError err = SearchError_Success;

	char *attribs[2];
	DIR_Server *server = GetDirServer();
	attribs[0] = XP_STRDUP(DIR_GetFirstAttributeString (server, cn));
	attribs[1] = NULL;

	char *filter = PR_smprintf ("(%s=%s)", DIR_GetFirstAttributeString (server, auth), m_valueUsedToFindDn);

	if (attribs[0] && filter)
		m_currentMessage = ldap_search (m_ldap, (char*) GetSearchBase(), LDAP_SCOPE_SUBTREE, filter, &attribs[0], 0);
	else
		err = SearchError_OutOfMemory;

	int i = 0;
	while (attribs[i])
		XP_FREE(attribs[i++]);

	XP_FREEIF(filter);

	m_nextState = kPreAuthSearchResponse;
	return err;
}


MSG_SearchError msg_SearchLdap::PreAuthSearchResponse ()
{
	MSG_SearchError err = SearchError_Success;

	int msgId = PollConnection ();
	switch (msgId)
	{
	case LDAP_TIMEOUT:			// search still pending
		break;
	case LDAP_RES_SEARCH_ENTRY:  // got a hit, find its DN
		if (!m_authDn)
		{
			char *authDn = ldap_get_dn (m_ldap, m_message);
			m_authDn = XP_STRDUP(authDn);
			ldap_memfree(authDn);
		}
		else
		{
			// Better not have more than one hit for this search.
			// We don't have a way for the user to choose which "phil" they are
			HandleSizeLimitExceeded();
			err = SearchError_ScopeDone;
		}
		break;
	case LDAP_RES_SEARCH_RESULT: // search finished
		m_nextState = m_authDn ? kAuthenticatedBindRequest : kUnbindRequest;
		break;
	case LDAP_SIZELIMIT_EXCEEDED:
		// Better not have more than one hit for this search.
		// We don't have a way for the user to choose which "phil" they are
		HandleSizeLimitExceeded();
		m_nextState = m_authDn ? kAuthenticatedBindRequest : kUnbindRequest;
		err = SearchError_ScopeDone;
		break;
	default:
		DisplayError (XP_LDAP_SEARCH_FAILED, GetHostDescription(), msgId);
		err = SearchError_ScopeDone; 
	}

	return err;
}


MSG_SearchError msg_SearchLdap::SaveCredentialsToPrefs ()
{
	DIR_Server *server = GetDirServer();

	if (m_authDn)
		DIR_SetAuthDN (server, m_authDn);

	if (server->savePassword && m_password)
		DIR_SetPassword (server, m_password);

	return SearchError_Success;
}


MSG_SearchError msg_SearchLdap::CollectUserCredentials ()
{
	MSG_SearchError err = SearchError_Success;
	DIR_Server *server = GetDirServer();

	// First look in the DIR_Server we read out of the prefs. If it already
	// knows the user's credentials, there's no need to ask again here.
	if (server->authDn && XP_STRLEN(server->authDn))
		m_authDn = XP_STRDUP (server->authDn);
	if (server->savePassword && server->password && XP_STRLEN(server->password))
		m_password = XP_STRDUP (server->password);
	if (m_authDn && m_password)
		return err;

	char *username = NULL;
	char *password = NULL;
	const char *anonAttrName = DIR_GetAttributeName (server, auth);

	char *prompt = PR_smprintf (XP_GetString(MK_LDAP_AUTH_PROMPT), anonAttrName, server->description);
	if (prompt)
	{
		if (FE_PromptUsernameAndPassword (m_scope->m_frame->GetContext(), prompt, &username, &password))
		{
			if (password && username)
			{
				m_password = HG72267 (password);
				XP_FREE(password);
				m_valueUsedToFindDn = username;
			}
			else
				m_nextState = kAnonymousBindRequest; // no credentials -- let 'em try it anonymously
		}
		else
			m_nextState = kAnonymousBindRequest; // no credentials -- let 'em try it anonymously

		XP_FREE(prompt);
	}
	else
		err = SearchError_OutOfMemory;

	return err;
}


MSG_SearchError msg_SearchLdap::AuthenticatedBindRequest ()
{
	return BindRequest (m_authDn, m_password);
	
}


MSG_SearchError msg_SearchLdap::AnonymousBindRequest()
{
	return BindRequest (NULL, NULL);
}


MSG_SearchError msg_SearchLdap::BindRequest (const char *dn, const char *password)
{
	MSG_SearchError err = SearchError_Success;
	char *unmungedPassword = NULL;
	
	if (password)
		unmungedPassword = HG72224 (password);

	m_currentMessage = ldap_simple_bind (m_ldap, dn, unmungedPassword);

	if (m_currentMessage > -1)
		m_nextState = kBindResponse;
	else
	{
		DisplayError (XP_LDAP_BIND_FAILED, GetHostDescription(), ldap_get_lderrno(m_ldap,NULL,NULL));
		err = SearchError_ScopeDone;
	}

	XP_FREEIF(unmungedPassword);
	return err;
}


MSG_SearchError msg_SearchLdap::BindResponse ()
{
	MSG_SearchError err = SearchError_Success;
	int msgId = PollConnection ();
	switch (msgId)
	{
	case LDAP_TIMEOUT:		// bind still pending
		break;
	case LDAP_RES_BIND:		// bind succeeded
		m_nextState = m_modifyBucket ? kModifyRequest : kSearchRequest;
		break;
	default:
		DisplayError (XP_LDAP_BIND_FAILED, GetHostDescription(), msgId);
		err = SearchError_ScopeDone; 
	}
	return err;
}


MSG_SearchError msg_SearchLdap::SearchRootDSERequest ()
{
	m_nextState = kSearchResponse;

	char *attribs[1];
	attribs[0] = NULL;

	m_currentMessage = ldap_search (m_ldap, "", LDAP_SCOPE_BASE, "(objectClass=*)", &attribs[0], 0);

	return SearchError_Success;
}

MSG_SearchError msg_SearchLdap::SearchVLVSearchRequest ()
{
	m_nextState = kSearchVLVSearchResponse;

	char *attribs[4];
	attribs[0] = XP_STRDUP(DIR_GetFirstAttributeString (GetDirServer(), cn));
	attribs[1] = "vlvBase";
	attribs[2] = "vlvFilter";
	attribs[3] = NULL;

	m_currentPair = 0;
	m_currentMessage = ldap_search (m_ldap, LDAP_VLV_BASE, LDAP_SCOPE_ONELEVEL, LDAP_VLV_SEARCHCLASS, &attribs[0], 0);

	XP_FREE(attribs[0]);

	return SearchError_Success;
}

MSG_SearchError msg_SearchLdap::SearchVLVIndexRequest ()
{
	MSG_SearchError err = SearchError_Success;

	m_nextState = kSearchVLVIndexResponse;

	msg_LdapPair *pair = (msg_LdapPair *)m_pairList.GetAt(m_currentPair);
	if (pair)
	{
		char *attribs[3];
		attribs[0] = "vlvSort";
		attribs[1] = "vlvEnabled";
		attribs[2] = NULL;

		char *base = PR_smprintf ("%s=%s,%s", DIR_GetFirstAttributeString (GetDirServer(), cn), pair->cn, LDAP_VLV_BASE);
		m_currentMessage = ldap_search (m_ldap, base, LDAP_SCOPE_SUBTREE, LDAP_VLV_INDEXCLASS, &attribs[0], 0);
		XP_FREE(base);
	}
	else
		err = SearchError_ScopeDone;

	return err;
}

MSG_SearchError msg_SearchLdap::SearchRequest ()
{
	m_scope->m_frame->UpdateStatusBar (MK_MSG_SEARCH_STATUS);

	// This array serves to prune the returned attributes to only the ones
	// we're interested in. If the user wants to see the whole entry, 
	// he/she can double-click on the result element.
	char *attribs[8];
	DIR_Server *server = GetDirServer();
	attribs[0] = XP_STRDUP(DIR_GetFirstAttributeString (server, cn));
	attribs[1] = XP_STRDUP(DIR_GetFirstAttributeString (server, telephonenumber));
	attribs[2] = XP_STRDUP(DIR_GetFirstAttributeString (server, mail));
	attribs[3] = XP_STRDUP(DIR_GetFirstAttributeString (server, l));
	attribs[4] = XP_STRDUP(DIR_GetFirstAttributeString (server, o));
	attribs[5] = XP_STRDUP(DIR_GetFirstAttributeString (server, givenname));
	attribs[6] = XP_STRDUP(DIR_GetFirstAttributeString (server, sn));
	attribs[7] = NULL;

	char *filter = m_filter;
    LDAPControl	*sort_request = NULL;
   	LDAPControl *vlv_request = NULL;
	LDAPControl *pldapCtrlList[3] = { NULL, NULL, NULL };
	if (IsValidVLVSearch())
	{
		char *sort = "cn";
		LDAPsortkey **keylist = NULL;
		LDAPVirtualList *pLdapVLV = GetLdapVLVData();

		/* We use the extra data pointer for passing the search filter and
		 * sort order.
		 */
		if (pLdapVLV->ldvlist_extradata)
		{
			filter = (char *)pLdapVLV->ldvlist_extradata;
			sort = XP_STRCHR(filter, ':');
			sort++[0] = '\0';

			for (int i = 0; sort[i]; i++)
				if (sort[i] != '-' && !isalnum(sort[i]))
					sort[i] = ' ';

			pLdapVLV->ldvlist_extradata = NULL;
		}

		ldap_create_sort_keylist (&keylist, sort);
		ldap_create_sort_control (m_ldap, keylist, 0, &sort_request);
		ldap_free_sort_keylist (keylist);

		ldap_create_virtuallist_control (m_ldap, pLdapVLV, &vlv_request);

		pldapCtrlList[0] = sort_request;
		pldapCtrlList[1] = vlv_request;
	}

    int msgid;
	if (ldap_search_ext (m_ldap, (char *) GetSearchBase(), LDAP_SCOPE_SUBTREE,
	                     filter, &attribs[0], 0,
		                 (pldapCtrlList[0] ? pldapCtrlList : (LDAPControl **)NULL),
                         (LDAPControl **)NULL, (struct timeval *)NULL,
	                     -1, &msgid) == LDAP_SUCCESS)
		m_currentMessage = msgid;
	else
		m_currentMessage = -1;

	if (sort_request)
		ldap_control_free (sort_request);
	if (vlv_request)
		ldap_control_free (vlv_request);
	if (filter != m_filter)
		XP_FREE(filter);


	int i = 0;
	while (attribs[i])
		XP_FREE(attribs[i++]);

	m_nextState = kSearchResponse;

	return SearchError_Success;
}


MSG_SearchError msg_SearchLdap::SearchRootDSEResponse ()
{
	XP_Bool keepPolling;
	MSG_SearchError err = SearchError_Success;

	do 
	{
		int msgId = PollConnection ();

		keepPolling = FALSE;
		switch (msgId)
		{
		case LDAP_TIMEOUT:			// search still pending
			break;
		case LDAP_RES_SEARCH_ENTRY:
			DIR_ParseRootDSE (GetDirServer(), m_ldap, m_message);
			keepPolling = TRUE;
			break;
		case LDAP_RES_SEARCH_RESULT:// search complete
		default:                    // some error occurred, consider search complete
			DIR_SetFlag (GetDirServer(), DIR_LDAP_ROOTDSE_PARSED);

			if (GetSearchType() == searchRootDSE)
			{
				m_nextState = kUnbindRequest;
				err = SearchError_ScopeDone;
			}
			else if (   GetSearchType() == searchLdapVLV
			         && !DIR_TestFlag(GetDirServer(), DIR_LDAP_VIRTUALLISTVIEW))
				RestartFailedVLVSearch();
			else
				m_nextState = kSearchVLVSearchRequest;
		}
	} while (keepPolling);

	return err;
}

MSG_SearchError msg_SearchLdap::SearchVLVSearchResponse ()
{
	XP_Bool keepPolling;
	MSG_SearchError err = SearchError_Success;

	do 
	{
		int msgId = PollConnection ();

		keepPolling = FALSE;
		switch (msgId)
		{
		case LDAP_TIMEOUT:			// search still pending
			break;
		case LDAP_RES_SEARCH_ENTRY:
			keepPolling = TRUE;
			AddPair (ldap_first_entry (m_ldap, m_message));
			break;
		case LDAP_RES_SEARCH_RESULT:// search complete
			if (m_pairList.GetSize() == 0)
			{
				DIR_ClearFlag(GetDirServer(), DIR_LDAP_VIRTUALLISTVIEW);
				RestartFailedVLVSearch();
			}
			else
				m_nextState = kSearchVLVIndexRequest;
			break;
		default:                    // some error occurred, consider search complete
			err = SearchError_ScopeDone;
			UnbindRequest();
			break;
		}
	} while (keepPolling);

	return err;
}

MSG_SearchError msg_SearchLdap::SearchVLVIndexResponse ()
{
	XP_Bool keepPolling;
	MSG_SearchError err = SearchError_Success;

	do 
	{
		int msgId = PollConnection ();

		keepPolling = FALSE;
		switch (msgId)
		{
		case LDAP_TIMEOUT:			// search still pending
			break;
		case LDAP_RES_SEARCH_ENTRY:
			keepPolling = TRUE;
			AddSortToPair((msg_LdapPair *)(m_pairList[m_currentPair]), ldap_first_entry (m_ldap, m_message));
			break;
		case LDAP_RES_SEARCH_RESULT:// search complete
			/* If there are more pairs to process, do so.
			 */
			if (m_pairList.GetSize() > ++m_currentPair)
			{
				m_nextState = kSearchVLVIndexRequest;
				break;
			}

			/* We're done processing the VLV pairs, we can now build the VLV pair string.
			 */
			ParsePairList();
			/* Fall-through */
		default:                    // some error occurred, consider search complete
			UnbindRequest();
			err = Initialize();
		}
	} while (keepPolling);

	return err;
}

MSG_SearchError msg_SearchLdap::SearchResponse ()
{
	MSG_SearchError err = SearchError_Success;

	// The "keepPolling" logic is designed to read as many entries as we can
	// right now. However, on a high-speed connection to an idle server, it
	// can spew entries so fast that the user is likely to be mad that we're
	// not giving them control of their machine. So we'll read in chunks, or
	// until we need to pause for read.
	const int chunkSize = 50;
	XP_Bool keepPolling;
	int numEntriesReceived = 0;

	do 
	{
		keepPolling = FALSE;
		int msgId = PollConnection ();
		switch (msgId)
		{
		case LDAP_TIMEOUT:			// search still pending
			break;
		case LDAP_RES_SEARCH_ENTRY:  // got some hits, search continues
			AddMessageToResults (ldap_first_entry (m_ldap, m_message));
			keepPolling = ++numEntriesReceived < chunkSize;
			break;
		case LDAP_RES_SEARCH_RESULT: // search finished
			SaveCredentialsToPrefs ();

			if (IsValidVLVSearch())
				ProcessVLVResults (m_message);

			m_nextState = kUnbindRequest;
			err = SearchError_ScopeDone;
			break;
		case LDAP_SIZELIMIT_EXCEEDED:
			HandleSizeLimitExceeded();
			m_nextState = kUnbindRequest;
			err = SearchError_ScopeDone;
			break;
		default:
			DisplayError (XP_LDAP_SEARCH_FAILED, GetHostDescription(), msgId);
			err = SearchError_ScopeDone; 
		}
	} while (keepPolling);

	return err;
}


MSG_SearchError msg_SearchLdap::ModifyRequest ()
{
	MSG_SearchError err = SearchError_Success;
	XP_ASSERT(m_modifyBucket);
	if (m_modifyBucket)
	{
		m_currentMessage = ldap_modify (m_ldap, m_modifyBucket->dnValue->u.string, m_modifyBucket->modList);
		m_nextState = kModifyResponse;
	}
	else
		err = SearchError_NullPointer;
	return err;
}


MSG_SearchError msg_SearchLdap::ModifyResponse ()
{
	MSG_SearchError err = SearchError_Success;
	int msgId = PollConnection ();
	switch (msgId)
	{
	case LDAP_TIMEOUT:			// modify still pending
		break;
	case LDAP_RES_MODIFY:		// modify is complete
		m_nextState = kUnbindRequest;
		err = SearchError_ScopeDone;

		// Free memory shared across the async modify operation
		delete m_modifyBucket;
		m_modifyBucket = NULL;

		break;
	default:
		DisplayError (XP_LDAP_MODIFY_FAILED, GetHostDescription(), msgId);
		err = SearchError_ScopeDone; 
	}
	return err;
}


MSG_SearchError msg_SearchLdap::UnbindRequest ()
{	
	if (NULL != m_ldap)
	{
		ldap_unbind (m_ldap);
		m_ldap = NULL;
	}

	return SearchError_ScopeDone;
}


#if !defined(MOZ_NO_LDAP)
void msg_SearchLdap::AddMessageToResults (LDAPMessage *messageChain)
{
	if (!messageChain)
		return;  // Stop recursing on this chain

	MSG_ResultElement *result = new MSG_ResultElement (this);
	if (result)
	{
		XP_ASSERT (result);
		MSG_SearchValue *value = NULL;
		char **scratchArray = NULL;
		XP_Bool needOrg = TRUE;
		XP_Bool needCn = TRUE;

    	DIR_Server *server = GetDirServer();
		INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_scope->m_frame->GetContext());
		int16 win_csid = INTL_GetCSIWinCSID(c);

		scratchArray = ldap_get_values (m_ldap, messageChain, DIR_GetFirstAttributeString(server, cn));
		if (scratchArray && scratchArray[0])
		{
			value = new MSG_SearchValue;
			value->attribute = attribCommonName;
			value->u.string = DIR_ConvertFromServerCharSet (m_scope->m_server, scratchArray[0], win_csid);
			result->AddValue (value);
			ldap_value_free (scratchArray);

			needCn = FALSE;
		}

		scratchArray = ldap_get_values (m_ldap, messageChain, DIR_GetFirstAttributeString(server, givenname));
		if (scratchArray && scratchArray[0])
		{
			value = new MSG_SearchValue;
			value->attribute = attribGivenName;
			value->u.string = DIR_ConvertFromServerCharSet (m_scope->m_server, scratchArray[0], win_csid);
			result->AddValue (value);
			ldap_value_free (scratchArray);
		}

		scratchArray = ldap_get_values (m_ldap, messageChain, DIR_GetFirstAttributeString(server, sn));
		if (scratchArray && scratchArray[0])
		{
			value = new MSG_SearchValue;
			value->attribute = attribSurname;
			value->u.string = DIR_ConvertFromServerCharSet (m_scope->m_server, scratchArray[0], win_csid);
			result->AddValue (value);
			ldap_value_free (scratchArray);
		}

		scratchArray = ldap_get_values (m_ldap, messageChain, DIR_GetFirstAttributeString(server, mail));
		if (scratchArray && scratchArray[0])
		{
			char *mailbox = MSG_ExtractRFC822AddressMailboxes (scratchArray[0]);
			if (mailbox)
			{
				value = new MSG_SearchValue;
				value->attribute = attrib822Address;
				value->u.string = DIR_ConvertFromServerCharSet (m_scope->m_server, mailbox, win_csid);
				result->AddValue (value);
				XP_FREE(mailbox);
			}
			ldap_value_free (scratchArray);
		}

		scratchArray = ldap_get_values (m_ldap, messageChain, DIR_GetFirstAttributeString(server, telephonenumber));
		if (scratchArray && scratchArray[0])
		{
			value = new MSG_SearchValue;
			value->attribute = attribPhoneNumber;
			value->u.string = DIR_ConvertFromServerCharSet (m_scope->m_server, scratchArray[0], win_csid);
			result->AddValue (value);
			ldap_value_free (scratchArray);
		}

		scratchArray = ldap_get_values (m_ldap, messageChain, DIR_GetFirstAttributeString(server, l));
		if (scratchArray && scratchArray[0])
		{
			value = new MSG_SearchValue;
			value->attribute = attribLocality;
			value->u.string = DIR_ConvertFromServerCharSet (m_scope->m_server, scratchArray[0], win_csid);
			result->AddValue (value);
			ldap_value_free (scratchArray);
		}

		scratchArray = ldap_get_values (m_ldap, messageChain, DIR_GetFirstAttributeString(server, o));
		if (scratchArray && scratchArray[0])
		{
			value = new MSG_SearchValue;
			value->attribute = attribOrganization;
			value->u.string = DIR_ConvertFromServerCharSet (m_scope->m_server, scratchArray[0], win_csid);
			result->AddValue (value);
			ldap_value_free (scratchArray);

			needOrg = FALSE;
		}

		// Save off the DN so the server can get back to this entry quickly
		// if we need to get more attributes
		char *distinguishedName = ldap_get_dn (m_ldap, messageChain);
		if (distinguishedName)
		{
			value = new MSG_SearchValue;
			if (value)
			{
				value->attribute = attribDistinguishedName;
				value->u.string = DIR_ConvertFromServerCharSet (m_scope->m_server, distinguishedName, win_csid);
				result->AddValue (value);
			}

			// If we didn't get some attributes we wanted, try to pick them up from the DN
			if (needOrg || needCn)
			{
				if ((scratchArray = ldap_explode_dn (distinguishedName, FALSE /*want types*/)) != NULL)
				{
					char *dnComponent = NULL;
					int i = 0;
					while ((dnComponent = scratchArray[i++]) != NULL)
					{
						if (needOrg && !strncasecomp (dnComponent, "o=", 2))
						{
							if ((value = new MSG_SearchValue) != NULL)
							{
								value->attribute = attribOrganization;
								value->u.string = DIR_ConvertFromServerCharSet (m_scope->m_server, dnComponent+2, win_csid);
								result->AddValue (value);
								needOrg = FALSE;
							}
						}
						if (needCn && !strncasecomp (dnComponent, "cn=", 3))
						{
							if ((value = new MSG_SearchValue) != NULL)
							{
								value->attribute = attribCommonName;
								value->u.string = DIR_ConvertFromServerCharSet (m_scope->m_server, dnComponent+3, win_csid);
								result->AddValue (value);
								needCn = FALSE;
							}
						}
					}
					ldap_value_free (scratchArray);
				}
			}

			ldap_memfree (distinguishedName);
		}

		// Add this result element to the list we show the user
		m_scope->m_frame->AddResultElement (result);
	}

	// Recurse down other possible messages in this chain
	AddMessageToResults (ldap_next_entry (m_ldap, messageChain));
}

void msg_SearchLdap::AddPair (LDAPMessage *messageChain)
{
	if (!messageChain)
		return;  // Stop recursing on this chain

	msg_LdapPair *pair = (msg_LdapPair *)XP_CALLOC(1, sizeof(msg_LdapPair));
	if (pair)
	{
		XP_Bool baseMatches = FALSE;
		char **scratchArray = NULL;

    	DIR_Server *server = GetDirServer();
		INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_scope->m_frame->GetContext());
		int16 win_csid = INTL_GetCSIWinCSID(c);

		/* Ignore VLVs whose base does not match the search base in the server.
		 * Unfortunately we can't just compare the search base with strcmp, LDAP attribute
		 * values are insensitive to extra whitespace.
		 */
		scratchArray = ldap_get_values (m_ldap, messageChain, "vlvBase");
		if (scratchArray && scratchArray[0])
		{
			int i;
			char *base[2], *start, *end, *compend;

			base[0] = XP_STRDUP(server->searchBase),
			base[1] = DIR_ConvertFromServerCharSet (m_scope->m_server, scratchArray[0], win_csid);
			ldap_value_free (scratchArray);

			/* Strip any non-significant whitespace from the search base strings.
			 */
			for (i = 0; i < 2 && base[i]; i++)
			{
				for (start = end = base[i]; *end; )
				{
					/* Get rid of space at the beginning of the string and
					 * after '=' and ','.
					 */
					while (*end && XP_IS_SPACE(*end))
						end++;

					if (*end == '=' || *end == ',')
					{
						*start++ = *end++;
					}
					else if (*end)
					{
						/* Find the beginning of the next token/component
						 */
						for (compend = end; *compend && *compend != '=' && *compend != ','; )
							compend++;

						/* Back up to the last non-whitespace character.
						 */
						while (compend > end && XP_IS_SPACE(*(compend-1)))
							compend--;

						/* Copy the component.
						 */
						while (end < compend)
							*start++ = *end++;
					}
				}
				*start = '\0';
			}

			baseMatches = (XP_STRCASECMP(base[0], base[1]) == 0);

			XP_FREE(base[0]);
			XP_FREE(base[1]);
		}
		
		if (baseMatches)
		{
			int numAttributesLeft = 2;

			scratchArray = ldap_get_values (m_ldap, messageChain, DIR_GetFirstAttributeString(server, cn));
			if (scratchArray && scratchArray[0])
			{
				pair->cn = XP_STRDUP(DIR_ConvertFromServerCharSet (m_scope->m_server, scratchArray[0], win_csid));
				ldap_value_free (scratchArray);
				numAttributesLeft--;
			}
			scratchArray = ldap_get_values (m_ldap, messageChain, "vlvFilter");
			if (scratchArray && scratchArray[0])
			{
				pair->filter = XP_STRDUP(DIR_ConvertFromServerCharSet (m_scope->m_server, scratchArray[0], win_csid));
				ldap_value_free (scratchArray);
				numAttributesLeft--;
			}

			if (numAttributesLeft > 0)
				FreePair(pair);
		}

		if (baseMatches)
		{
			pair->sort = NULL;
			m_pairList.Add(pair);
		}
	}

	// Recurse down other possible messages in this chain
	AddPair (ldap_next_entry (m_ldap, messageChain));
}

void msg_SearchLdap::AddSortToPair (msg_LdapPair *pair, LDAPMessage *messageChain)
{
	if (!messageChain)
		return;  // Stop recursing on this chain

	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_scope->m_frame->GetContext());
	int16 win_csid = INTL_GetCSIWinCSID(c);

	XP_Bool enabled = FALSE;
	char **scratchArray = NULL;
    DIR_Server *server = GetDirServer();

	scratchArray = ldap_get_values (m_ldap, messageChain, "vlvEnabled");
	if (scratchArray && scratchArray[0])
	{
		enabled = (XP_Bool)XP_ATOI(scratchArray[0]);
		ldap_value_free (scratchArray);
	}

	if (enabled)
	{
		/* Only the first vlvSort attribute is recognized by the server.
		 */
		scratchArray = ldap_get_values (m_ldap, messageChain, "vlvSort");
		if (scratchArray && scratchArray[0])
		{
			char *sort;
			char *newsort = DIR_ConvertFromServerCharSet (m_scope->m_server, scratchArray[0], win_csid);
		
			if (pair->sort)
			{
				sort = PR_smprintf ("%s;%s:%s", pair->sort, pair->filter, newsort);
				XP_FREE(pair->sort);
			}
			else
				sort = PR_smprintf ("%s:%s", pair->filter, newsort);

			pair->sort = sort;
			ldap_value_free (scratchArray);
		}
	}

	// Recurse down other possible messages in this chain
	AddSortToPair (pair, ldap_next_entry (m_ldap, messageChain));
}

void msg_SearchLdap::FreePair (msg_LdapPair *pair)
{
	XP_ASSERT(pair);
	XP_FREEIF(pair->cn);
	XP_FREEIF(pair->filter);
	XP_FREEIF(pair->sort);
	XP_FREE(pair);
}

void msg_SearchLdap::ParsePairList ()
{
	uint32 i, offset, size;
	msg_LdapPair *pair;

	/* First allocate space for the complete VLV pair list.
	 */
	for (i = 0, size = 0; i < m_pairList.GetSize(); i++)
	{
		pair = (msg_LdapPair *)(m_pairList[i]);
		if (pair->sort)
			size += XP_STRLEN(pair->sort) + 1;
	}
	size++;

	char *pairList = (char *)XP_ALLOC(size);
	if (pairList)
	{
		/* coallesce the VLV pairs into one string.
		 */
		pairList[0] = '\0';
		for (i = 0, offset = 0; i < m_pairList.GetSize(); i++)
		{
			pair = (msg_LdapPair *)(m_pairList[i]);
			if (pair->sort)
				offset += PR_snprintf(&pairList[offset], size - offset, "%s;", pair->sort);
		}

		/* Store the pair string in the DIR_Server
		 */
		DIR_Server *server = GetDirServer();
		XP_FREEIF(server->searchPairList);
		server->searchPairList = pairList;
		DIR_SaveServerPreferences (FE_GetDirServers());
	}

	/* Finally, free the pair list.
	 */
	for (i = 0; i < m_pairList.GetSize(); i++)
		FreePair((msg_LdapPair *)(m_pairList[i]));
	m_pairList.SetSize(0);
}

LDAPVirtualList *msg_SearchLdap::GetLdapVLVData ()
{
	return (LDAPVirtualList *)(m_scope->m_frame->GetSearchParam());
}

MSG_SearchType msg_SearchLdap::GetSearchType ()
{
	return m_scope->m_frame->GetSearchType();
}

MSG_SearchError msg_SearchLdap::SetSearchParam (MSG_SearchType type, void *param)
{
	return m_scope->m_frame->SetSearchParam(type, param);
}

XP_Bool msg_SearchLdap::IsValidVLVSearch ()
{
	return    DIR_TestFlag(m_scope->m_server, DIR_LDAP_VIRTUALLISTVIEW)
	       && m_scope->m_frame->GetSearchType() == searchLdapVLV
	       && m_scope->m_frame->GetSearchParam();
}

MSG_SearchError msg_SearchLdap::RestartFailedVLVSearch ()
{
	MSG_SearchError err = SearchError_ScopeDone;

	/* We attempted a VLV search on a server that doesn't support VLV, we need
	 * to remap the search term to the VLV attribute value.
	 */
	LDAPVirtualList *pLdapVLV = GetLdapVLVData();
	if (pLdapVLV && pLdapVLV->ldvlist_attrvalue)
	{
		MSG_SearchTerm *term = m_scope->m_frame->m_termList.GetAt(0);
			
		XP_ASSERT(m_scope->m_frame->m_termList.GetSize() == 1);
		XP_ASSERT(term->m_value.u.string[0] == '\0');
		term->m_value.u.string = XP_STRDUP(pLdapVLV->ldvlist_attrvalue);

		SetSearchParam (searchNormal, NULL);
		if (ValidateTerms() == SearchError_Success)
		{
			UnbindRequest();
			err = Initialize();
		}
	}

	if (err != SearchError_Success)
	{
		m_nextState = kUnbindRequest;
		err = SearchError_ScopeDone;
	}

	return err;
}

void msg_SearchLdap::ProcessVLVResults (LDAPMessage *messageChain)
{
	int rc;
	if (( rc = ldap_result2error (m_ldap, messageChain, 0)) != LDAP_SUCCESS)
	{
		SetSearchParam (searchLdapVLV, NULL);
    }

	/* Check for the sort and VLV response controls
	 */
   	LDAPControl **ldapCtrlList;
	ldap_parse_result (m_ldap, messageChain, &rc, NULL, NULL, NULL,
		               &ldapCtrlList, FALSE);

	int error;
	unsigned long result = 0, index, size;
	char *attrib;

	if (   ldap_parse_sort_control (m_ldap, ldapCtrlList, &result, &attrib) == LDAP_SUCCESS
	    && ldap_parse_virtuallist_control (m_ldap, ldapCtrlList, &index, &size, &error) == LDAP_SUCCESS)
	{
		LDAPVirtualList *pldapVL = GetLdapVLVData ();
		if (pldapVL)
		{
			pldapVL->ldvlist_index = index;
			pldapVL->ldvlist_size = size;
		}
	}
	else
	{
		SetSearchParam (searchLdapVLV, NULL);
	}
	
	ldap_controls_free (ldapCtrlList);
}
#endif

void msg_SearchLdap::DisplayError (int templateId, const char *contextString, int ldapErrorId, XP_Bool /*isTemplate*/)
{
	// This builds up error dialogs which look like:
	// "Failed to open connection to 'ldap.mcom.com' due to LDAP error 'bad DN name' (0x22)"

	char *templateString = XP_GetString (templateId);
	char *ldapErrString = NULL;

	m_scope->m_frame->m_handlingError = TRUE;

//	if (isTemplate)
//		ldapErrString = ldap_tmplerr2string (ldapErrorId);
//	else
		ldapErrString = ldap_err2string (ldapErrorId);

	if (templateString && contextString && ldapErrString)
	{
		int len = XP_STRLEN(templateString) + XP_STRLEN(ldapErrString) + XP_STRLEN(contextString) + 10;
		char *theBigString = (char*) XP_ALLOC (len);
		if (theBigString)
		{
			PR_snprintf (theBigString, len, templateString, contextString, ldapErrString, ldapErrorId);
			FE_Alert (m_scope->m_frame->GetContext(), theBigString);
			XP_FREE(theBigString);
		}
	}

	m_scope->m_frame->m_handlingError = FALSE;
}


#endif /* MOZ_LDAP */


int msg_SearchLdap::Abort ()
{
#ifdef MOZ_LDAP
	int result = 0;
	if (m_ldap)
		result = ldap_abandon (m_ldap, m_currentMessage);
	if (0 == result)
		result = msg_SearchAdapter::Abort ();
	m_scope->m_frame->EndCylonMode();
	return result;
#else
	return msg_SearchAdapter::Abort ();
#endif /* MOZ_LDAP */
}


const char *msg_SearchLdap::GetHostName() 
{ 
	return m_scope->m_server->serverName; 
}


const char *msg_SearchLdap::GetSearchBase () 
{ 
	return m_scope->m_server->searchBase; 
}


int msg_SearchLdap::GetMaxHits () 
{ 
	return m_scope->m_server->maxHits; 
}

XP_Bool msg_SearchLdap::GetEnableAuth ()
{
	return m_scope->m_server->enableAuth;
}

int msg_SearchLdap::GetPort ()
{ 
#ifdef MOZ_LDAP
	if (m_scope->m_server->port)
		return m_scope->m_server->port;
	return GetStandardPort();
#else
	return 0;
#endif /* MOZ_LDAP */
}


int msg_SearchLdap::GetStandardPort()
{
	HG25167
}


HG98726


const char *msg_SearchLdap::GetUrlScheme (XP_Bool forAddToAB)
{
	HG82798
	return forAddToAB ? "addbook-ldap:" : "ldap:";
}


const char *msg_SearchLdap::GetHostDescription()
{
	// since the description is optional, it might be null, so pick up
	// the server's DNS name if necessary
	char *desc = m_scope->m_server->description;
	if (desc && *desc)
		return desc;
	return GetHostName();
}


void msg_SearchLdap::HandleSizeLimitExceeded ()
{
	if (m_scope->m_frame->m_resultList.GetSize() < GetMaxHits())
	{
		// If we've exceeded a size limit less than our client-imposed limit,
		// we must have blown through the "look-through limit" on the server

		FE_Progress (m_scope->m_frame->GetContext(), XP_GetString (XP_LDAP_SERVER_SIZELIMIT_EXCEEDED));
	}
	else
	{
		// If we have actually gotten hits back, we've probably exceeded
		// the client-imposed limit stored in the DIR_Server

		char *prompt = PR_smprintf (XP_GetString (XP_LDAP_SIZELIMIT_EXCEEDED), GetMaxHits());
		if (prompt)
		{
			FE_Progress (m_scope->m_frame->GetContext(), prompt);
			XP_FREE (prompt);
		}
	}
}
 

void msg_SearchValidityManager::EnableLdapAttribute (MSG_SearchAttribute attrib, XP_Bool enableIt)
{
	m_ldapTable->SetAvailable (attrib, opContains, enableIt);
	m_ldapTable->SetEnabled   (attrib, opContains, enableIt);
	m_ldapTable->SetAvailable (attrib, opDoesntContain, enableIt);
	m_ldapTable->SetEnabled   (attrib, opDoesntContain, enableIt);
	m_ldapTable->SetAvailable (attrib, opIs, enableIt);
	m_ldapTable->SetEnabled   (attrib, opIs, enableIt);
	m_ldapTable->SetAvailable (attrib, opIsnt, enableIt);
	m_ldapTable->SetEnabled   (attrib, opIsnt, enableIt);
	m_ldapTable->SetAvailable (attrib, opSoundsLike, enableIt);
	m_ldapTable->SetEnabled   (attrib, opSoundsLike, enableIt);
	m_ldapTable->SetAvailable (attrib, opBeginsWith, enableIt);
	m_ldapTable->SetEnabled   (attrib, opBeginsWith, enableIt);
	m_ldapTable->SetAvailable (attrib, opEndsWith, enableIt);
	m_ldapTable->SetEnabled   (attrib, opEndsWith, enableIt);
}


MSG_SearchError msg_SearchValidityManager::InitLdapTable (DIR_Server *server)
{
	MSG_SearchError err = SearchError_Success;

	if (NULL == m_ldapTable)
		err = NewTable (&m_ldapTable);

	if (SearchError_Success == err)
	{
		EnableLdapAttribute (attribCommonName);
		EnableLdapAttribute (attrib822Address);
		EnableLdapAttribute (attribPhoneNumber);
		EnableLdapAttribute (attribOrganization);
		EnableLdapAttribute (attribOrgUnit);
		EnableLdapAttribute (attribLocality);
		EnableLdapAttribute (attribStreetAddress);

		if (server)
		{
			EnableLdapAttribute (attribCustom1, DIR_UseCustomAttribute (server, custom1));
			EnableLdapAttribute (attribCustom2, DIR_UseCustomAttribute (server, custom2));
			EnableLdapAttribute (attribCustom3, DIR_UseCustomAttribute (server, custom3));
			EnableLdapAttribute (attribCustom4, DIR_UseCustomAttribute (server, custom4));
			EnableLdapAttribute (attribCustom5, DIR_UseCustomAttribute (server, custom5));
		}
	}

	return err;
}

MSG_SearchError msg_SearchValidityManager::PostProcessValidityTable (DIR_Server *server)
{
	return InitLdapTable (server);
}
