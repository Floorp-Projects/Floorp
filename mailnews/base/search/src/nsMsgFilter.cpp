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

// this file implements the nsMsgFilterList interface 

#include "msgCore.h"
#include "nsMsgFilterList.h"
#include "nsMsgFilter.h"

nsMsgFilter::nsMsgFilter()
{
	m_filterList = nsnull;
}

nsMsgFilter::SetFilterList(nsMsgFilterList *filterList)
{
	m_filterList = filterList;
}

nsresult nsMsgFilter::ConvertMoveToFolderValue(nsString2 &relativePath)
{

	m_action.m_value.m_folderName = relativePath;
	return NS_OK;
	// set m_action.m_value.m_folderName
}
