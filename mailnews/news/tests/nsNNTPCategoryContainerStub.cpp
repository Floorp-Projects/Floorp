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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

 /* This is a stub event sink for a NNTP Newsgroup introduced by mscott to test
   the NNTP protocol */

#include "nsISupports.h" /* interface nsISupports */
#include "nsINNTPCategoryContainer.h"
#include "nsINNTPNewsgroup.h" /* interface nsINNTPNewsgroup */

#include "nscore.h"
#include "plstr.h"
#include "prmem.h"
#include <stdio.h>

static NS_DEFINE_IID(kINNTPCategoryContainerIID, NS_INNTPCATEGORYCONTAINER_IID);


class nsNNTPCategoryContainerStub : public nsISupports {
 public: 
	nsNNTPCategoryContainerStub();
	virtual ~nsNNTPCategoryContainerStub();
	 
	NS_DECL_ISUPPORTS
	NS_IMETHOD GetRootCategory(nsINNTPNewsgroup * *aRootCategory);
	NS_IMETHOD SetRootCategory(nsINNTPNewsgroup * aRootCategory);

 protected:
	 nsINNTPNewsgroup * m_newsgroup;

};

NS_IMPL_ISUPPORTS(nsNNTPCategoryContainerStub, kINNTPCategoryContainerIID);

nsNNTPCategoryContainerStub::nsNNTPCategoryContainerStub()
{
	NS_INIT_REFCNT();
	m_newsgroup = nsnull;
}

nsNNTPCategoryContainerStub::~nsNNTPCategoryContainerStub()
{
	printf("Destroying category container. \n");
	NS_IF_RELEASE(m_newsgroup);
}

nsresult nsNNTPCategoryContainerStub::GetRootCategory(nsINNTPNewsgroup * *aRootCategory)
{
	if (aRootCategory)
	{
		*aRootCategory = m_newsgroup;
		NS_IF_ADDREF(m_newsgroup);
	}

	return NS_OK;
}


nsresult nsNNTPCategoryContainerStub::SetRootCategory(nsINNTPNewsgroup * aRootCategory)
{
	if (aRootCategory)
	{
		char * name = nsnull;
		aRootCategory->GetName(&name);
		printf("Setting root category for container to %s", name ? name : "unspecified");
		m_newsgroup = aRootCategory;
		NS_IF_ADDREF(m_newsgroup);
	}
	return NS_OK;

}

extern "C" {

nsresult NS_NewCategoryContainerFromNewsgroup(nsINNTPCategoryContainer ** aInstancePtr, nsINNTPNewsgroup* group)
{
	nsresult rv = NS_OK;
	nsNNTPCategoryContainerStub * stub = nsnull;
	if (aInstancePtr)
	{
		stub = new nsNNTPCategoryContainerStub();
		stub->SetRootCategory(group);
		rv = stub->QueryInterface(kINNTPCategoryContainerIID, (void **) aInstancePtr);
	}

	return rv;
}

}
