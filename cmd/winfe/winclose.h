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

#ifndef __WINCLOSE_H
//	Avoid include redundancy
//
#define __WINCLOSE_H

//	Purpose:	Provide a registry to monitor when a window is actually closed.
//	Comments:	Designed to work with other protocol types, DDE, OLE, whatever.
//	Revision History:
//		01-19-95	created GAB
//

//	Required Includes
//

//	Constants
//

//	Structures
//
class CWindowChangeRegistry	{
protected:
	static CPtrList m_Registry;
	POSITION m_rIndex;

	CWindowChangeRegistry()	{}
	~CWindowChangeRegistry()	{
		m_Registry.RemoveAt(m_rIndex);
	}
};

class CWindowChangeItem : public CWindowChangeRegistry	{	
protected:
	enum	{
		m_DDE,
		m_OLE
	};
	int m_iType;
	DWORD m_dwWindowID;

	CWindowChangeItem(DWORD dwWindowID, int iType);
	
	virtual void WindowClosing(BOOL bExiting) = 0;
	virtual void WindowSizing(DWORD dwX, DWORD dwY, DWORD dwCX, DWORD dwCY) = 0;
	virtual void WindowMaximizing() = 0;
	virtual void WindowMinimizing() = 0;
	virtual void WindowNormalizing() = 0;
public:
	int GetType()	{
		return(m_iType);
	}
	DWORD GetWindowID()	{
		return(m_dwWindowID);
	}

    enum    {
        m_Close,
        m_Size,
        m_Maximize,
        m_Minimize,
        m_Normalize
    };
	static void Closing(DWORD dwWindowID);
	static void Sizing(DWORD dwWindowID, DWORD dwX, DWORD dwY, DWORD dwCX, DWORD dwCY);
	static void Maximizing(DWORD dwWindowID);
	static void Minimizing(DWORD dwWindowID);
	static void Normalizing(DWORD dwWindowID);
};

class CDDEWindowChangeItem : public CWindowChangeItem	{
	CString m_csServiceName;
	
protected:
	CDDEWindowChangeItem(CString& csServiceName, DWORD dwWindowID);

	//	Must override.
	void WindowClosing(BOOL bExiting);
	void WindowSizing(DWORD dwX, DWORD dwY, DWORD dwCX, DWORD dwCY);
	void WindowMaximizing();
	void WindowMinimizing();
	void WindowNormalizing();
	
public:
	CString GetServiceName()	{
		return(m_csServiceName);
	}

	//	Consider these the constructor, destructor.
	static BOOL DDERegister(CString &csServiceName, DWORD dwWindowID);
	static BOOL DDEUnRegister(CString &csServiceName, DWORD dwWindowID);
};

//	Global variables
//

//	Macros
//

//	Function declarations
//

#endif // __WINCLOSE_H
