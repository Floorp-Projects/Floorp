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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _nsIMAPNamespace_H_
#define _nsIMAPNamespace_H_


// represents an array of namespaces for a given host
class nsIMAPNamespaceList
{
public:
	~nsIMAPNamespaceList();

	static nsIMAPNamespaceList *CreatensIMAPNamespaceList();

	void ClearNamespaces(PRBool deleteFromPrefsNamespaces, PRBool deleteServerAdvertisedNamespaces);
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
