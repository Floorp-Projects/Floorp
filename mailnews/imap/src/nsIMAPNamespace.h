/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef _nsIMAPNamespace_H_
#define _nsIMAPNamespace_H_

class nsIMAPNamespace
{

public:
	nsIMAPNamespace(EIMAPNamespaceType type, const char *prefix, char delimiter, PRBool from_prefs);

	~nsIMAPNamespace();

	EIMAPNamespaceType	GetType() { return m_namespaceType; }
	const char *		GetPrefix() { return m_prefix; }
	char				GetDelimiter() { return m_delimiter; }
	void				SetDelimiter(char delimiter);
	PRBool				GetIsDelimiterFilledIn() { return m_delimiterFilledIn; }
	PRBool				GetIsNamespaceFromPrefs() { return m_fromPrefs; }

	// returns -1 if this box is not part of this namespace,
	// or the length of the prefix if it is part of this namespace
	int					MailboxMatchesNamespace(const char *boxname);

protected:
	EIMAPNamespaceType m_namespaceType;
	char	*m_prefix;
	char	m_delimiter;
	PRBool	m_fromPrefs;
	PRBool m_delimiterFilledIn;

};


// represents an array of namespaces for a given host
class nsIMAPNamespaceList
{
public:
	~nsIMAPNamespaceList();

	static nsIMAPNamespaceList *CreatensIMAPNamespaceList();

	nsresult InitFromString(const char *nameSpaceString, EIMAPNamespaceType nstype);
	nsresult OutputToString(nsCString &OutputString);
	int UnserializeNamespaces(const char *str, char **prefixes, int len);
	nsresult SerializeNamespaces(char **prefixes, int len, nsCString &serializedNamespace);

	void ClearNamespaces(PRBool deleteFromPrefsNamespaces, PRBool deleteServerAdvertisedNamespaces, PRBool reallyDelete);
	int	GetNumberOfNamespaces();
	int	GetNumberOfNamespaces(EIMAPNamespaceType);
	nsIMAPNamespace *GetNamespaceNumber(int nodeIndex);
	nsIMAPNamespace *GetNamespaceNumber(int nodeIndex, EIMAPNamespaceType);

	nsIMAPNamespace *GetDefaultNamespaceOfType(EIMAPNamespaceType type);
	int AddNewNamespace(nsIMAPNamespace *ns);
	nsIMAPNamespace *GetNamespaceForMailbox(const char *boxname);

protected:
	nsIMAPNamespaceList();	// use CreatensIMAPNamespaceList to create one

	nsVoidArray m_NamespaceList;

};


#endif
