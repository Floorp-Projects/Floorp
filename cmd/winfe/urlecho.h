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

#ifndef __URLECHO_H
//	Avoid include redundancy
//
#define __URLECHO_H

//	Purpose:	Monitor loaded URLs and their referrers
//	Comments:	Implementing since API through netlib finally made clear.
//	Revision History:
//		01-18-95	created GAB
//

//	Required Includes
//

//	Constants
//

//	Structures
//
class CEchoRegistry	{
protected:
	static CPtrList m_Registry;
	POSITION m_rIndex;

	CEchoRegistry()	{
	}
	~CEchoRegistry()	{
		m_Registry.RemoveAt(m_rIndex);
	}
};

class CEchoItem : public CEchoRegistry	{	
protected:
	enum	{
		m_DDE,
		m_OLE
	};
	int m_iType;

	CEchoItem(int iType) : CEchoRegistry()	{
		m_iType = iType;
	}
	
	virtual void EchoURL(CString& csURL, CString& csMimeType, DWORD dwWindowID, CString& csReferrer) = 0;	
public:
	int GetType()	{
		return(m_iType);
	}
	
	static void Echo(CString& csURL, CString& csMimeType, DWORD dwWindowID, CString& csReferrer);
};

class CDDEEchoItem : public CEchoItem	{
	CString m_csServiceName;
	
protected:
	CDDEEchoItem(CString& csServiceName) : CEchoItem(m_DDE)	{
		m_rIndex = m_Registry.AddTail(this);
		m_csServiceName = csServiceName;
	}

	//	Must override.
	void EchoURL(CString& csURL, CString& csMimeType, DWORD dwWindowID, CString& csReferrer);
	
public:
	CString GetServiceName()	{
		return(m_csServiceName);
	}

	//	Consider these the constructor, destructor.
	static void DDERegister(CString &csServiceName);
	static BOOL DDEUnRegister(CString &csServiceName);	
};

//	Global variables
//

//	Macros
//

//	Function declarations
//

#endif // __URLECHO_H
