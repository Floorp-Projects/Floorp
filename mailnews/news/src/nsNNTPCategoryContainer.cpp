/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

 /* This is a stub event sink for a NNTP Newsgroup introduced by mscott to test
   the NNTP protocol */

#include "nsNNTPCategoryContainer.h"

NS_IMPL_ISUPPORTS(nsNNTPCategoryContainer, NS_GET_IID(nsINNTPCategoryContainer));

nsNNTPCategoryContainer::nsNNTPCategoryContainer()
{
	NS_INIT_REFCNT();
	m_newsgroup = nsnull;
}

nsNNTPCategoryContainer::~nsNNTPCategoryContainer()
{
	printf("Destroying category container. \n");
	NS_IF_RELEASE(m_newsgroup);
}

nsresult nsNNTPCategoryContainer::GetRootCategory(nsINNTPNewsgroup * *aRootCategory)
{
	if (aRootCategory)
	{
		*aRootCategory = m_newsgroup;
		NS_IF_ADDREF(m_newsgroup);
	}

	return NS_OK;
}


nsresult nsNNTPCategoryContainer::Initialize(nsINNTPNewsgroup * aRootCategory)
{
    // for now, just set the root category.
    return SetRootCategory(aRootCategory);
}

nsresult nsNNTPCategoryContainer::SetRootCategory(nsINNTPNewsgroup * aRootCategory)
{
	if (aRootCategory)
	{
		char * name = nsnull;
		aRootCategory->GetName(&name);
#ifdef DEBUG_NEWS
		printf("Setting root category for container to %s", name ? name : "unspecified");
#endif
		m_newsgroup = aRootCategory;
		NS_IF_ADDREF(m_newsgroup);
	}
	return NS_OK;

}

