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

#ifndef __NCAPI_URL_DATA_H
#define __NCAPI_URL_DATA_H

//	This file used to define the data structure assigned into the ncapi_data
//		of the URL_Struct for the windows front end.

class CNcapiUrlData	{
public:
	CNcapiUrlData(CAbstractCX *pCX, URL_Struct *pUrl);
	~CNcapiUrlData();

//	Context that we're tracking.
private:
	CAbstractCX *m_pCX;
public:
	void ChangeContext(CAbstractCX *pNewCX);

//	Url struct that we're tracking.
private:
	URL_Struct *m_pUrl;

//	Block deletion of a URL struct in some cases.
private:
	BOOL m_bDelete;
public:
	BOOL CanFreeUrl() const;
	void DontFreeUrl();

//	Setting of a progress application.
private:
	CString m_csProgressServer;
public:
	void SetProgressServer(CString& csProgressServer);
	BOOL HasProgressServer() const;
	void ClearProgressServer();

//	Progress callbacks into us, which we then route to an ncapi
private:
	DWORD m_dwTransactionID;
	BOOL m_bInitializedProgress;
public:
	BOOL Alert(const char *pMessage);
	BOOL Confirm(const char *pMessage);
	void MakingProgress(const char *pMessage, int iPercent);
	void InitializeProgress();
	void EndProgress();

//	How to get the transaction ID.
public:
	DWORD GetTransactionID() const;
};

#endif // __NCAPI_URL_DATA_H
