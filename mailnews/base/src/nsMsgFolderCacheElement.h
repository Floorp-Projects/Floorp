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

#ifndef nsMsgFolderCacheElement_H
#define nsMsgFolderCacheElement_H

#include "nsIMsgFolderCacheElement.h"
#include "nsMsgFolderCache.h"
#include "mdb.h"

class nsMsgFolderCacheElement : public nsIMsgFolderCacheElement
{

public:
	nsMsgFolderCacheElement();
	virtual ~nsMsgFolderCacheElement();

	NS_DECL_ISUPPORTS

	//nsMsgFolderCacheElement

	NS_IMETHOD GetStringProperty(const char *propertyName, char **result);
	NS_IMETHOD GetInt32Property(const char *propertyName, PRInt32 *result);
	NS_IMETHOD SetStringProperty(const char *propertyName, const char *propertyValue);
	NS_IMETHOD SetInt32Property(const char *propertyName, PRInt32 propertyValue);

  /* readonly attribute string URI; */
	NS_IMETHOD GetURI(char * *aURI);

	NS_IMETHOD SetURI(char *aURI);
protected:
	nsIMdbRow	*m_mdbRow;
	nsMsgFolderCache *m_owningCache;	// this will be ref-counted. Is this going to be a problem?
										// I want to avoid circular references, but since this is
										// scriptable, I think I have to ref-count it.
	char *m_folderURI;

};


#endif
