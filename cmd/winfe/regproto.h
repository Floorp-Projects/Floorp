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

#ifndef __PROTOREG_H
//	Avoid include redundancy
//
#define __PROTOREG_H

//	Purpose:	Handle registration of external protocol registrations.
//	Comments:	Made to interact with DDE servers, and OLE automation servers.
//	Revision History:
//		01-18-95	created GAB
//

//	Required Includes
//

//	Constants
//

//	Structures
//
class CProtocolRegistry	{
protected:
	static CPtrList m_Registry;
	POSITION m_rIndex;

	CProtocolRegistry();
	~CProtocolRegistry();
};

class CProtocolItem : public CProtocolRegistry	{	
protected:
	enum	{
		m_DDE,
		m_OLE
	};
	int m_iType;
	CString m_csProtocol;
    CString m_csServiceName;

	CProtocolItem(CString& csProtocol, CString& csServiceName, int iType);
    ~CProtocolItem();
	
public:
	int GetType();
    CString GetServiceName();
	
	static CProtocolItem *Find(CString& csProtocol);
	virtual BOOL OpenURL(URL_Struct *pURL, MWContext *pContext, FO_Present_Types iFormatOut) = 0;
};

class COLEProtocolItem : public CProtocolItem   {
protected:
    COLEProtocolItem(CString& csProtocol, CString& csServiceName);

public:
    //  Consider these the contructor, destructor, but with retvals.
    static BOOL OLERegister(CString& csProtocol, CString& csServiceName);
    static BOOL OLEUnRegister(CString& csProtocol, CString& csServiceName);

    //  Must override.
    BOOL OpenURL(URL_Struct *pURL, MWContext *pContext, FO_Present_Types iFormatOut);
};

class CDDEProtocolItem : public CProtocolItem	{
protected:
	CDDEProtocolItem(CString& csProtocol, CString& csServiceName);
	
public:
	//	Consider these the constructor, destructor, but with retvals.
	static BOOL DDERegister(CString &csProtocol, CString &csServiceName);
	static BOOL DDEUnRegister(CString &csProtocol, CString &csServiceName);
	
	//	Must override.
	BOOL OpenURL(URL_Struct *pURL, MWContext *pContext, FO_Present_Types iFormatOut);
};

//	Global variables
//

//	Macros
//

//	Function declarations
//

#endif // __PROTOREG_H
