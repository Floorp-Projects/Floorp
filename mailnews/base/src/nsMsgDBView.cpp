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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

#include "msgCore.h"
#include "nsMsgDBView.h"
#include "nsISupports.h"
/* Implementation file */

NS_IMPL_ISUPPORTS1(nsMsgDBView, nsIMsgDBView)

nsMsgDBView::nsMsgDBView()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  m_sortValid = PR_TRUE;
  m_sortOrder = nsMsgViewSortOrder::none;
  //m_viewFlags = (ViewFlags) 0;
}

nsMsgDBView::~nsMsgDBView()
{
  /* destructor code */
}

NS_IMETHODIMP nsMsgDBView::Open(nsIMsgDatabase *msgDB, nsMsgViewSortType *viewType, PRInt32 *count)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgDBView::Close()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgDBView::Init(PRInt32 *pCount)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgDBView::AddKeys(nsMsgKey *pKeys, PRInt32 *pFlags, const char *pLevels, nsMsgViewSortType *sortType, PRInt32 numKeysToAdd)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgDBView::Sort(nsMsgViewSortType *sortType, nsMsgViewSortOrder *sortOrder)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

