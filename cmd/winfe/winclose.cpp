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

#include "winclose.h"
#include "wfedde.h"
#include "mainfrm.h"

//	Static property.
CPtrList CWindowChangeRegistry::m_Registry;

CWindowChangeItem::CWindowChangeItem(DWORD dwWindowID, int iType) : CWindowChangeRegistry()	{
		m_dwWindowID = dwWindowID;
		m_iType = iType;
}
	
void CWindowChangeItem::Closing(DWORD dwWindowID)	{
//	Purpose:	Go through all registered windows, and tell them if they are the one closing.
//	Arguments:	dwWindowID	The window closing.
//	Returns:	void
//	Comments:
//	Revision History:
//		01-19-95	created GAB
//

	//	Go through the registry, call the member WindowClosing, vtables call the correct class.
	POSITION rIndex = m_Registry.GetHeadPosition();
	CWindowChangeItem *pClose;
	while(rIndex != NULL)	{
		pClose = (CWindowChangeItem *)m_Registry.GetNext(rIndex);
		if(pClose->GetWindowID() == dwWindowID)	{
			pClose->WindowClosing(theApp.m_bExit);
			
			//	Remove it from the list, no longer needed, as window doesn't exist.
			delete pClose;
		}
	}
}

void CWindowChangeItem::Sizing(DWORD dwWindowID, DWORD dwX, DWORD dwY, DWORD dwCX, DWORD dwCY)	{
//	Purpose:	Go through all registered windows, and tell them if they are the one is changing.
//	Arguments:	dwWindowID	The window sizing.
//              dw* The sizes.
//	Returns:	void
//	Comments:
//	Revision History:
//		02-01-95	created GAB
//

	//	Go through the registry, call the member WindowSizing, vtables call the correct class.
	POSITION rIndex = m_Registry.GetHeadPosition();
	CWindowChangeItem *pClose;
	while(rIndex != NULL)	{
		pClose = (CWindowChangeItem *)m_Registry.GetNext(rIndex);
		if(pClose->GetWindowID() == dwWindowID)	{
			pClose->WindowSizing(dwX, dwY, dwCX, dwCY);
		}
	}
}

void CWindowChangeItem::Maximizing(DWORD dwWindowID)	{
//	Purpose:	Go through all registered windows, and tell them if they are the one is changing.
//	Arguments:	dwWindowID	The window maximizing.
//	Returns:	void
//	Comments:
//	Revision History:
//		02-01-95	created GAB
//

	//	Go through the registry, call the member WindowMaximizing, vtables call the correct class.
	POSITION rIndex = m_Registry.GetHeadPosition();
	CWindowChangeItem *pClose;
	while(rIndex != NULL)	{
		pClose = (CWindowChangeItem *)m_Registry.GetNext(rIndex);
		if(pClose->GetWindowID() == dwWindowID)	{
			pClose->WindowMaximizing();
		}
	}
}

void CWindowChangeItem::Minimizing(DWORD dwWindowID)	{
//	Purpose:	Go through all registered windows, and tell them if they are the one is changing.
//	Arguments:	dwWindowID	The window minimizing.
//	Returns:	void
//	Comments:
//	Revision History:
//		02-01-95	created GAB
//

	//	Go through the registry, call the member WindowMinimizing, vtables call the correct class.
	POSITION rIndex = m_Registry.GetHeadPosition();
	CWindowChangeItem *pClose;
	while(rIndex != NULL)	{
		pClose = (CWindowChangeItem *)m_Registry.GetNext(rIndex);
		if(pClose->GetWindowID() == dwWindowID)	{
			pClose->WindowMinimizing();
		}
	}
}

void CWindowChangeItem::Normalizing(DWORD dwWindowID)	{
//	Purpose:	Go through all registered windows, and tell them if they are the one is changing.
//	Arguments:	dwWindowID	The window maximizing.
//	Returns:	void
//	Comments:
//	Revision History:
//		02-01-95	created GAB
//

	//	Go through the registry, call the member WindowNormalizing, vtables call the correct class.
	POSITION rIndex = m_Registry.GetHeadPosition();
	CWindowChangeItem *pClose;
	while(rIndex != NULL)	{
		pClose = (CWindowChangeItem *)m_Registry.GetNext(rIndex);
		if(pClose->GetWindowID() == dwWindowID)	{
			pClose->WindowNormalizing();
		}
	}
}

CDDEWindowChangeItem::CDDEWindowChangeItem(CString& csServiceName, DWORD dwWindowID) : CWindowChangeItem(dwWindowID, m_DDE)	{
		m_rIndex = m_Registry.AddTail(this);
		m_csServiceName = csServiceName;
}

void CDDEWindowChangeItem::WindowClosing(BOOL bExiting)	{
//	Purpose:	Tell the DDE implementation that the window is closing, and the exit status.
//	Arguments:	bExiting	If the app is exiting, this is TRUE.
//	Returns:	void
//	Comments:
//	Revision History:
//		01-19-95	created GAB
//

	//	Just call the DDE implementation, it will handle the details.
	CDDEWrapper::WindowChange(this, m_Close, (TwoByteBool)bExiting);
}

void CDDEWindowChangeItem::WindowSizing(DWORD dwX, DWORD dwY, DWORD dwCX, DWORD dwCY)   {
//	Purpose:	Tell the DDE implementation that the window is resized, and the new coordinates.
//	Arguments:	dwX The x position.
//              dwY The y position.
//              dwCX    The width.
//              dwCY    The height.
//	Returns:	void
//	Comments:
//	Revision History:
//		02-01-95	created GAB
//

	//	Just call the DDE implementation, it will handle the details.
	CDDEWrapper::WindowChange(this, m_Size, FALSE, dwX, dwY, dwCX, dwCY);
}

void CDDEWindowChangeItem::WindowMaximizing()   {
//	Purpose:	Tell the DDE implementation that the window is resized, and the new coordinates.
//	Arguments:  void
//	Returns:	void
//	Comments:
//	Revision History:
//		02-01-95	created GAB
//

	//	Just call the DDE implementation, it will handle the details.
	CDDEWrapper::WindowChange(this, m_Maximize);
}

void CDDEWindowChangeItem::WindowMinimizing()   {
//	Purpose:	Tell the DDE implementation that the window is resized, and the new coordinates.
//	Arguments:  void
//	Returns:	void
//	Comments:
//	Revision History:
//		02-01-95	created GAB
//

	//	Just call the DDE implementation, it will handle the details.
	CDDEWrapper::WindowChange(this, m_Minimize);
}

void CDDEWindowChangeItem::WindowNormalizing()   {
//	Purpose:	Tell the DDE implementation that the window is resized, and the new coordinates.
//	Arguments:  void
//	Returns:	void
//	Comments:
//	Revision History:
//		02-01-95	created GAB
//

	//	Just call the DDE implementation, it will handle the details.
	CDDEWrapper::WindowChange(this, m_Normalize);
}

BOOL CDDEWindowChangeItem::DDERegister(CString& csServiceName, DWORD dwWindowID)	{
//	Purpose:	Register a server to monitor when a window closes.
//	Arguments:	csServiceName	The server to callback.
//				dwWindowID	The window to monitor.
//	Returns:	BOOL	Wether or not registration was successful.
//	Comments:	Constructor, basically.
//	Revision History:
//		01-19-95	created GAB
//

    //  Don't allow the mimimized window to be monitored.
    if(dwWindowID == 0) {
        return(FALSE);
    }

	//	Make sure such a window exists.
	CAbstractCX *pCX = CAbstractCX::FindContextByID(dwWindowID);
	if(NULL == pCX)	{
		return(FALSE);
	}
	if(pCX->GetContext()->type != MWContextBrowser)	{
		return(FALSE);
	}

	//	Looks like it will work.
	CDDEWindowChangeItem *pDontCare = new CDDEWindowChangeItem(csServiceName, dwWindowID);
	return(TRUE);
}

BOOL CDDEWindowChangeItem::DDEUnRegister(CString& csServiceName, DWORD dwWindowID)	{
//	Purpose:	UnRegister a server to monitor when a window closes.
//	Arguments:	csServiceName	The server not wanting to monitor.
//				dwWindowID	The window to stop monitoring.
//	Returns:	BOOL	False if never registered, otherwise TRUE.
//	Comments:	Destructor, basically.
//	Revision History:
//		01-19-95	created GAB
//

	//	Allright, we need to go through ever entry in our registry.
	POSITION rIndex = m_Registry.GetHeadPosition();
	CWindowChangeItem *pCmp;
	CDDEWindowChangeItem *pDelme = NULL;
	while(rIndex != NULL)	{
		pCmp = (CWindowChangeItem *)m_Registry.GetNext(rIndex);
		if(pCmp->GetType() != m_DDE)	{
			continue;
		}
		pDelme = (CDDEWindowChangeItem *)pCmp;
		
		if(pDelme->GetServiceName() == csServiceName && pDelme->GetWindowID() == dwWindowID)	{
			break;
		}
		pDelme = NULL;
	}
	
    if(pDelme == NULL)	{
    	return(FALSE);
    }
    else	{
    	delete(pDelme);
    	return(TRUE);
    }
}
