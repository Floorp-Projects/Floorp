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

// this file implements the nsMsgFilterService interface 

#include "msgCore.h"
#include "nsMsgFilterService.h"
#include "nsFileStream.h"
#include "nsMsgFilterList.h"

NS_IMPL_ADDREF(nsMsgFilterService)
NS_IMPL_RELEASE(nsMsgFilterService)

NS_BEGIN_EXTERN_C

nsresult
NS_NewMsgFilterService(const nsIID& iid, void **result)
{
	nsMsgFilterService *ids = new nsMsgFilterService();
	return ids->QueryInterface(iid, result);
}

NS_END_EXTERN_C


NS_IMETHODIMP nsMsgFilterService::QueryInterface(REFNSIID aIID, void** aResult)
{   
    if (aResult == NULL)  
        return NS_ERROR_NULL_POINTER;  

    if (aIID.Equals(nsIMsgFilterService::GetIID()) ||
        aIID.Equals(::nsISupports::GetIID())) {
        *aResult = NS_STATIC_CAST(nsIMsgFilterService*, this);   
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}   


NS_IMETHODIMP nsMsgFilterService::OpenFilterList(nsFileSpec *filterFile, nsIMsgFilterList **resultFilterList)
{
	nsresult ret;

	nsIOFileStream *fileStream = new nsIOFileStream(*filterFile);
	if (!fileStream)
		return NS_ERROR_OUT_OF_MEMORY;

	nsMsgFilterList *filterList = new nsMsgFilterList(fileStream);
	if (!filterList)
		return NS_ERROR_OUT_OF_MEMORY;
	ret = filterList->LoadTextFilters();
	if (NS_SUCCEEDED(ret))
		*resultFilterList = filterList;
	else
		NS_RELEASE(filterList);
	return ret;
}

NS_IMETHODIMP nsMsgFilterService::CloseFilterList(nsIMsgFilterList *filterList)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

	/* save without deleting */
NS_IMETHODIMP	nsMsgFilterService::SaveFilterList(nsIMsgFilterList *filterList)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgFilterService::CancelFilterList(nsIMsgFilterList *filterList)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}



