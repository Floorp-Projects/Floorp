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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

// this file implements the nsMsgFilterService interface 

#include "msgCore.h"
#include "nsMsgFilterService.h"
#include "nsFileStream.h"
#include "nsMsgFilterList.h"

NS_IMPL_ADDREF(nsMsgFilterService)
NS_IMPL_RELEASE(nsMsgFilterService)




nsMsgFilterService::nsMsgFilterService()
{
	NS_INIT_REFCNT();
}

nsMsgFilterService::~nsMsgFilterService()
{
}

NS_IMETHODIMP nsMsgFilterService::QueryInterface(REFNSIID aIID, void** aResult)
{   
    if (aResult == NULL)  
        return NS_ERROR_NULL_POINTER;  

    if (aIID.Equals(NS_GET_IID(nsIMsgFilterService)) ||
        aIID.Equals(NS_GET_IID(nsISupports))) {
        *aResult = NS_STATIC_CAST(nsIMsgFilterService*, this);   
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}   


NS_IMETHODIMP nsMsgFilterService::OpenFilterList(nsFileSpec *filterFile, nsIMsgFilterList **resultFilterList)
{
	nsresult ret = NS_OK;

	nsIOFileStream *fileStream = new nsIOFileStream(*filterFile);
	if (!fileStream)
		return NS_ERROR_OUT_OF_MEMORY;

	nsMsgFilterList *filterList = new nsMsgFilterList(fileStream);
	if (!filterList)
		return NS_ERROR_OUT_OF_MEMORY;
	NS_ADDREF(filterList);
	if (filterFile->GetFileSize() > 0)
		ret = filterList->LoadTextFilters();
	if (NS_SUCCEEDED(ret))
		*resultFilterList = filterList;
	else
		NS_RELEASE(filterList);
	return ret;
}

NS_IMETHODIMP nsMsgFilterService::CloseFilterList(nsIMsgFilterList *filterList)
{
	NS_ASSERTION(PR_FALSE,"CloseFilterList doesn't do anything yet");
	return NS_OK;
}

/* save without deleting */
NS_IMETHODIMP	nsMsgFilterService::SaveFilterList(nsIMsgFilterList *filterList, nsFileSpec *filterFile)
{
	nsresult ret = NS_OK;

	nsIOFileStream *fileStream = new nsIOFileStream(*filterFile);
	if (!fileStream)
		return NS_ERROR_OUT_OF_MEMORY;

	return ret;
}

NS_IMETHODIMP nsMsgFilterService::CancelFilterList(nsIMsgFilterList *filterList)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}



