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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "stdafx.h"
#include "wfedde.h"

//	This file used to define the ncapi_data structure
//		in the URL_Struct.

//	Initialize
CNcapiUrlData::CNcapiUrlData(CAbstractCX *pCX, URL_Struct *pUrl)
{
	ASSERT(pCX);
	ASSERT(pUrl);

	//	Can delete the URL_Struct initially.
	m_bDelete = TRUE;

	//	Assign in the URL, both ways.
	m_pUrl = pUrl;
	m_pUrl->ncapi_data = (void *)this;

	//	Assign in the context, both ways.
	m_pCX = pCX;
	m_pCX->m_pNcapiUrlData = this;

	//	Set up so that we haven't initialized any progress.
	m_bInitializedProgress = FALSE;
	m_dwTransactionID = 0;
}

//	Destroy
CNcapiUrlData::~CNcapiUrlData()
{
	//	Clear us out of the URL struct.
	m_pUrl->ncapi_data = NULL;

	//	Clear us out of the context.
	m_pCX->m_pNcapiUrlData = NULL;
}

//	Return whether or not it's ok to NET_FreeUrlStruct
BOOL CNcapiUrlData::CanFreeUrl() const
{
	return(m_bDelete);
}

//	Set that we shouldn't delete the URL in the exit routine.
void CNcapiUrlData::DontFreeUrl()
{
	m_bDelete = FALSE;
}

//	Change contexts.
void CNcapiUrlData::ChangeContext(CAbstractCX *pNewCX)
{
	ASSERT(pNewCX);

	//	Clear out the old context.
	m_pCX->m_pNcapiUrlData = NULL;

	//	Bring in the new.
	m_pCX = pNewCX;
	m_pCX->m_pNcapiUrlData = this;
}

//	Set a DDE progress server for alert and progress messages.
//	An empty CString will clear the progress server if it needs to be that way.
void CNcapiUrlData::SetProgressServer(CString& csProgressServer)
{
	//	Just copy it.
	m_csProgressServer = csProgressServer;
}

//	Report whether or not we have a progress server to report to.
BOOL CNcapiUrlData::HasProgressServer() const
{
	return(m_csProgressServer.IsEmpty() == FALSE);
}

//	Clear the progress server.
void CNcapiUrlData::ClearProgressServer()
{
	m_csProgressServer.Empty();
}

//	Initialize the progress of the progress app.
void CNcapiUrlData::InitializeProgress()
{
	//	Only do this if we have a progress server.
	if(HasProgressServer() && m_bInitializedProgress == FALSE)	{
		m_dwTransactionID = CDDEWrapper::BeginProgress(this,
			(const char *)m_csProgressServer,
			m_pCX->GetContextID(),
			szLoadString(IDS_LOADING));

		//	If it didn't work, it would have cleared the progress server.
		if(HasProgressServer())	{
			//	Set the range.
			CDDEWrapper::SetProgressRange(this,
				(const char *)m_csProgressServer,
				m_dwTransactionID,
				100);
		}
	}

	//	Mark progress as being attempted to initialize.
	m_bInitializedProgress = TRUE;
}

//	Tell the progress server about our progress.
void CNcapiUrlData::MakingProgress(const char *pMessage, int iPercent)
{
	//	Only do this if need be.
	if(HasProgressServer() && m_bInitializedProgress == TRUE)	{
		if(CDDEWrapper::MakingProgress(this,
			(const char *)m_csProgressServer,
			m_dwTransactionID,
			pMessage,
			(DWORD)iPercent))	{
			//	They would like us to discontinue the load.
			m_pCX->Interrupt();
		}
	}
}

//	End the progress.
void CNcapiUrlData::EndProgress()
{
	//	Only do if need be.
	if(HasProgressServer() && m_bInitializedProgress == TRUE)	{
		CDDEWrapper::EndProgress(this,
			(const char *)m_csProgressServer,
			m_dwTransactionID);
	}
}

//	Attempt to pass off any alerts to the external app.
//	Return non zero on their handling of the message.
BOOL CNcapiUrlData::Alert(const char *pMessage)
{
	//	Only do this if need be.
	if(HasProgressServer())	{
		return((BOOL)CDDEWrapper::AlertProgress(this,
			(const char *)m_csProgressServer,
			pMessage));
	}

	return(FALSE);
}

//	Attempt to pass off any confirms to the external app.
//	Return a non BOOL value to indicate failure.
BOOL CNcapiUrlData::Confirm(const char *pMessage)
{
	//	Only do this if need be.
	if(HasProgressServer())	{
		DWORD dwResult = CDDEWrapper::ConfirmProgress(this,
			(const char *)m_csProgressServer,
			pMessage);

		//	Convert to a return value.
		if(dwResult == CDDEWrapper::m_PushedYes)	{
			return(TRUE);
		}
		else if(dwResult == CDDEWrapper::m_PushedNo)	{
			return(FALSE);
		}
	}

	return((BOOL)-1);
}

//	Return our transaction ID.
//	Value of 0 is invalid.
DWORD CNcapiUrlData::GetTransactionID() const
{
	if(HasProgressServer() && m_bInitializedProgress)	{
		return(m_dwTransactionID);
	}

	return(0);
}
