/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
    NS_DECL_NSIMSGFOLDERCACHEELEMENT

	void		SetMDBRow(nsIMdbRow	*row);
	void		SetOwningCache(nsMsgFolderCache *owningCache);
protected:
	nsIMdbRow	*m_mdbRow;

	nsMsgFolderCache *m_owningCache;	// this will be ref-counted. Is this going to be a problem?
										// I want to avoid circular references, but since this is
										// scriptable, I think I have to ref-count it.
	char *m_folderURI;

};


#endif
