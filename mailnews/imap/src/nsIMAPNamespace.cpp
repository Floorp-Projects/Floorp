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

#include "msgCore.h"  // for pre-compiled headers

#include "nsImapCore.h"
#include "nsIMAPNamespace.h"
#include "nsImapProtocol.h"
#include "nsIMAPGenericParser.h"
#include "nsString.h"

//////////////////// nsIMAPNamespace  /////////////////////////////////////////////////////////////



nsIMAPNamespace::nsIMAPNamespace(EIMAPNamespaceType type, const char *prefix, char delimiter, PRBool from_prefs)
{
	m_namespaceType = type;
	m_prefix = PL_strdup(prefix);
	m_fromPrefs = from_prefs;

	m_delimiter = delimiter;
	m_delimiterFilledIn = !m_fromPrefs;	// if it's from the prefs, we can't be sure about the delimiter until we list it.
}

nsIMAPNamespace::~nsIMAPNamespace()
{
	PR_FREEIF(m_prefix);
}

void nsIMAPNamespace::SetDelimiter(char delimiter)
{
	m_delimiter = delimiter;
	m_delimiterFilledIn = PR_TRUE;
}

// returns -1 if this box is not part of this namespace,
// or the length of the prefix if it is part of this namespace
int nsIMAPNamespace::MailboxMatchesNamespace(const char *boxname)
{
	if (!boxname) return -1;

	// If the namespace is part of the boxname
    if (PL_strlen(m_prefix) == 0)
        return 0;

	if (PL_strstr(boxname, m_prefix) == boxname)
		return PL_strlen(m_prefix);

	// If the boxname is part of the prefix
	// (Used for matching Personal mailbox with Personal/ namespace, etc.)
	if (PL_strstr(m_prefix, boxname) == m_prefix)
		return PL_strlen(boxname);
	return -1;
}


nsIMAPNamespaceList *nsIMAPNamespaceList::CreatensIMAPNamespaceList()
{
	nsIMAPNamespaceList *rv = new nsIMAPNamespaceList();
	return rv;
}

nsIMAPNamespaceList::nsIMAPNamespaceList()
{
}

int nsIMAPNamespaceList::GetNumberOfNamespaces()
{
	return m_NamespaceList.Count();
}


nsresult nsIMAPNamespaceList::InitFromString(const char *nameSpaceString, EIMAPNamespaceType nstype)
{
	nsresult rv = NS_OK;
	if (nameSpaceString)
	{
		int numNamespaces = UnserializeNamespaces(nameSpaceString, nsnull, 0);
		char **prefixes = (char**) PR_CALLOC(numNamespaces * sizeof(char*));
		if (prefixes)
		{
			int len = UnserializeNamespaces(nameSpaceString, prefixes, numNamespaces);
			for (int i = 0; i < len; i++)
			{
				char *thisns = prefixes[i];
				char delimiter = '/';	// a guess
				if (PL_strlen(thisns) >= 1)
					delimiter = thisns[PL_strlen(thisns)-1];
				nsIMAPNamespace *ns = new nsIMAPNamespace(nstype, thisns, delimiter, PR_TRUE);
				if (ns)
					AddNewNamespace(ns);
				PR_FREEIF(thisns);
			}
			PR_Free(prefixes);
		}
	}

	return rv;
}

nsresult nsIMAPNamespaceList::OutputToString(nsCString &string)
{
	nsresult rv = NS_OK;

	return rv;
}


int nsIMAPNamespaceList::GetNumberOfNamespaces(EIMAPNamespaceType type)
{
	int nodeIndex = 0, count = 0;
	for (nodeIndex=m_NamespaceList.Count()-1; nodeIndex >= 0; nodeIndex--)
	{
		nsIMAPNamespace *nspace = (nsIMAPNamespace *) m_NamespaceList.ElementAt(nodeIndex);
		if (nspace->GetType() == type)
		{
			count++;
		}
	}
	return count;
}

int nsIMAPNamespaceList::AddNewNamespace(nsIMAPNamespace *ns)
{
	// If the namespace is from the NAMESPACE response, then we should see if there
	// are any namespaces previously set by the preferences, or the default namespace.  If so, remove these.

	if (!ns->GetIsNamespaceFromPrefs())
	{
		int nodeIndex = 0;
		for (nodeIndex=m_NamespaceList.Count()-1; nodeIndex >= 0; nodeIndex--)
		{
			nsIMAPNamespace *nspace = (nsIMAPNamespace *) m_NamespaceList.ElementAt(nodeIndex);
			if (nspace && nspace->GetIsNamespaceFromPrefs())
			{
				m_NamespaceList.RemoveElement(nspace);
				delete nspace; 
			}
		}
	}

	// Add the new namespace to the list.  This must come after the removing code,
	// or else we could never add the initial kDefaultNamespace type to the list.
	m_NamespaceList.AppendElement(ns);

	return 0;
}


// chrisf - later, fix this to know the real concept of "default" namespace of a given type
nsIMAPNamespace *nsIMAPNamespaceList::GetDefaultNamespaceOfType(EIMAPNamespaceType type)
{
	nsIMAPNamespace *rv = 0, *firstOfType = 0;

	int nodeIndex = 0, count = m_NamespaceList.Count();
	for (nodeIndex= 0; nodeIndex < count && !rv; nodeIndex++)
	{
		nsIMAPNamespace *ns = (nsIMAPNamespace *) m_NamespaceList.ElementAt(nodeIndex);
		if (ns->GetType() == type)
		{
			if (!firstOfType)
				firstOfType = ns;
			if (!(*(ns->GetPrefix())))
			{
				// This namespace's prefix is ""
				// Therefore it is the default
				rv = ns;
			}
		}
	}
	if (!rv)
		rv = firstOfType;
	return rv;
}

nsIMAPNamespaceList::~nsIMAPNamespaceList()
{
	ClearNamespaces(PR_TRUE, PR_TRUE, PR_TRUE);
}

// ClearNamespaces removes and deletes the namespaces specified, and if there are no namespaces left,
void nsIMAPNamespaceList::ClearNamespaces(PRBool deleteFromPrefsNamespaces, PRBool deleteServerAdvertisedNamespaces, PRBool reallyDelete)
{
	int nodeIndex = 0;
	
	for (nodeIndex=m_NamespaceList.Count()-1; nodeIndex >= 0; nodeIndex--)
	{
		nsIMAPNamespace *ns = (nsIMAPNamespace *) m_NamespaceList.ElementAt(nodeIndex);
		if (ns->GetIsNamespaceFromPrefs())
		{
			if (deleteFromPrefsNamespaces)
			{
				m_NamespaceList.RemoveElement(ns);
				if (reallyDelete)
					delete ns;
			}
		}
		else if (deleteServerAdvertisedNamespaces)
		{
			m_NamespaceList.RemoveElement(ns);
			if (reallyDelete)
				delete ns;
		}
	}
}

nsIMAPNamespace *nsIMAPNamespaceList::GetNamespaceNumber(int nodeIndex)
{
	int total = GetNumberOfNamespaces();
	NS_ASSERTION(nodeIndex >= 0 && nodeIndex < total, "invalid IMAP namespace node index");
	if (nodeIndex < 0) nodeIndex = 0;
	if (nodeIndex > total) nodeIndex = total;

	return 	(nsIMAPNamespace *) m_NamespaceList.ElementAt(nodeIndex);
}

nsIMAPNamespace *nsIMAPNamespaceList::GetNamespaceNumber(int nodeIndex, EIMAPNamespaceType type)
{
	int nodeCount = 0, count = 0;
	for (nodeCount=m_NamespaceList.Count()-1; nodeCount >= 0; nodeCount--)
	{
		nsIMAPNamespace *nspace = (nsIMAPNamespace *) m_NamespaceList.ElementAt(nodeCount);
		if (nspace->GetType() == type)
		{
			count++;
			if (count == nodeIndex)
				return nspace;
		}
	}
	return nsnull;
}

nsIMAPNamespace *nsIMAPNamespaceList::GetNamespaceForMailbox(const char *boxname)
{
	// We want to find the LONGEST substring that matches the beginning of this mailbox's path.
	// This accounts for nested namespaces  (i.e. "Public/" and "Public/Users/")
	
	// Also, we want to match the namespace's mailbox to that namespace also:
	// The Personal box will match the Personal/ namespace, etc.

	// these lists shouldn't be too long (99% chance there won't be more than 3 or 4)
	// so just do a linear search

	int lengthMatched = -1;
	int currentMatchedLength = -1;
	nsIMAPNamespace *rv = nsnull;
	int nodeIndex = 0;

	if (!PL_strcasecmp(boxname, "INBOX"))
		return GetDefaultNamespaceOfType(kPersonalNamespace);

	for (nodeIndex=m_NamespaceList.Count()-1; nodeIndex >= 0; nodeIndex--)
	{
		nsIMAPNamespace *nspace = (nsIMAPNamespace *) m_NamespaceList.ElementAt(nodeIndex);
		currentMatchedLength = nspace->MailboxMatchesNamespace(boxname);
		if (currentMatchedLength > lengthMatched)
		{
			rv = nspace;
			lengthMatched = currentMatchedLength;
		}
	}

	return rv;
}

#define SERIALIZER_SEPARATORS ","

/* prefixes is an array of strings;  len is the length of that array.
   If there is only one string, simply copy it and return it.
   Otherwise, put them in quotes and comma-delimit them. 
   Returns a newly allocated string. */
nsresult nsIMAPNamespaceList::SerializeNamespaces(char **prefixes, int len, nsCString &serializedNamespaces)
{
	nsresult rv = NS_OK;
	if (len <= 0)
		return rv;
	if (len == 1)
	{
		serializedNamespaces = prefixes[0];
		return rv;
	}
	else
	{
		for (int i = 0; i < len; i++)
		{
			char *temp = nsnull;
			if (i == 0)
			{
				serializedNamespaces += "\"";

				temp = PR_smprintf("\"%s\"",prefixes[i]);	/* quote the string */
			}
			else
			{
				serializedNamespaces += ',';
			}
			serializedNamespaces += prefixes[i];
			serializedNamespaces += "\"";
		}
		return rv;
	}
}

/* str is the string which needs to be unserialized.
   If prefixes is NULL, simply returns the number of namespaces in str.  (len is ignored)
   If prefixes is not NULL, it should be an array of length len which is to be filled in
   with newly-allocated string.  Returns the number of strings filled in.
*/
int nsIMAPNamespaceList::UnserializeNamespaces(const char *str, char **prefixes, int len)
{
	if (!str)
		return 0;
	if (!prefixes)
	{
		if (str[0] != '"')
			return 1;
		else
		{
			int count = 0;
			char *ourstr = PL_strdup(str);
			char *origOurStr = ourstr;
			if (ourstr)
			{
				char *token = nsCRT::strtok( ourstr, SERIALIZER_SEPARATORS, &ourstr );
				while (token != nsnull)
				{
					token = nsCRT::strtok( ourstr, SERIALIZER_SEPARATORS, &ourstr );
					count++;
				}
				PR_Free(origOurStr);
			}
			return count;
		}
	}
	else
	{
		if ((str[0] != '"') && (len >= 1))
		{
			prefixes[0] = PL_strdup(str);
			return 1;
		}
		else
		{
			int count = 0;
			char *ourstr = PL_strdup(str);
			char *origOurStr = ourstr;
			if (ourstr)
			{
				char *token = nsCRT::strtok( ourstr, SERIALIZER_SEPARATORS, &ourstr );
				while ((count < len) && (token != nsnull))
				{

					char *current = PL_strdup(token), *where = current;
					if (where[0] == '"')
						where++;
					if (where[PL_strlen(where)-1] == '"')
						where[PL_strlen(where)-1] = 0;
					prefixes[count] = PL_strdup(where);
					PR_FREEIF(current);
					token = nsCRT::strtok( ourstr, SERIALIZER_SEPARATORS, &ourstr );
					count++;
				}
				PR_Free(origOurStr);
			}
			return count;
		}
	}
}

