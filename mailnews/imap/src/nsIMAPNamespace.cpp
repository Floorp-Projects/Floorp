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

#include "msgCore.h"  // for pre-compiled headers

#include "nsImapCore.h"
#include "nsIMAPNamespace.h"
#include "nsImapProtocol.h"
#include "nsIMAPGenericParser.h"
#include "nsString2.h"

//////////////////// nsIMAPNamespace  /////////////////////////////////////////////////////////////



nsIMAPNamespace::nsIMAPNamespace(EIMAPNamespaceType type, const char *prefix, char delimiter, XP_Bool from_prefs)
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
	m_delimiterFilledIn = TRUE;
}

// returns -1 if this box is not part of this namespace,
// or the length of the prefix if it is part of this namespace
int nsIMAPNamespace::MailboxMatchesNamespace(const char *boxname)
{
	if (!boxname) return -1;

	// If the namespace is part of the boxname
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

int nsIMAPNamespaceList::GetNumberOfNamespaces(EIMAPNamespaceType type)
{
	int nodeIndex = 0, count = 0;
	for (nodeIndex=m_NamespaceList.Count(); nodeIndex > 0; nodeIndex--)
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
		for (nodeIndex=m_NamespaceList.Count(); nodeIndex > 0; nodeIndex--)
		{
			nsIMAPNamespace *nspace = (nsIMAPNamespace *) m_NamespaceList.ElementAt(nodeIndex);
			if (nspace->GetIsNamespaceFromPrefs())
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

	int nodeIndex = 1, count = m_NamespaceList.Count();
	for (nodeIndex= 1; nodeIndex <= count && !rv; nodeIndex++)
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
	ClearNamespaces(TRUE, TRUE, TRUE);
}

// ClearNamespaces removes and deletes the namespaces specified, and if there are no namespaces left,
void nsIMAPNamespaceList::ClearNamespaces(PRBool deleteFromPrefsNamespaces, PRBool deleteServerAdvertisedNamespaces, PRBool reallyDelete)
{
	int nodeIndex = 0;
	
	for (nodeIndex=m_NamespaceList.Count(); nodeIndex > 0; nodeIndex--)
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
	NS_ASSERTION(nodeIndex >= 1 && nodeIndex <= total, "invalid IMAP namespace node index");
	if (nodeIndex < 1) nodeIndex = 1;
	if (nodeIndex > total) nodeIndex = total;

	return 	(nsIMAPNamespace *) m_NamespaceList.ElementAt(nodeIndex);
}

nsIMAPNamespace *nsIMAPNamespaceList::GetNamespaceNumber(int nodeIndex, EIMAPNamespaceType type)
{
	int nodeCount = 0, count = 0;
	for (nodeCount=m_NamespaceList.Count(); nodeCount > 0; nodeCount--)
	{
		nsIMAPNamespace *nspace = (nsIMAPNamespace *) m_NamespaceList.ElementAt(nodeCount);
		if (nspace->GetType() == type)
		{
			count++;
			if (count == nodeIndex)
				return nspace;
		}
	}
	return NULL;
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
	nsIMAPNamespace *rv = NULL;
	int nodeIndex = 0;

	if (!PL_strcasecmp(boxname, "INBOX"))
		return GetDefaultNamespaceOfType(kPersonalNamespace);

	for (nodeIndex=m_NamespaceList.Count(); nodeIndex > 0; nodeIndex--)
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

