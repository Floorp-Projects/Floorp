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
///
//
// $Revision: 3.1 $
//
// TASKBAR.CPP
//
// DESCRIPTION:
//		This file contains the implementations of the various task bar related
//		classes.
//
// AUTHOR: Scott Jones
//
///


/**	INCLUDE	**/
#include "stdafx.h"
#include "taskbar.h"
#include "statbar.h"
#include "hiddenfr.h"
#include "feutil.h"
#include "prefapi.h"

#ifdef _DEBUG
#undef THIS_FILE
static char	BASED_CODE THIS_FILE[] = __FILE__;
#endif

#define SPACE_BETWEEN_ICONS 3
#define TASKBAR_ADDMENU (WM_USER + 200)

/****************************************************************************
*
*	Class: CTaskBarButtonDropTarget
*
*	DESCRIPTION:
*		
*		Some taskbar buttons may want to be drop targets.  So here's the class.
*
****************************************************************************/

DROPEFFECT CTaskBarButtonDropTarget::ProcessDragEnter(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point)
{

	DROPEFFECT deReturn = DROPEFFECT_NONE;
	
	// Only interested in bookmarks
	if (pDataObject->IsDataAvailable(
		::RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT)))
	{
		deReturn = DROPEFFECT_COPY;

	}

	return(deReturn);

}

DROPEFFECT CTaskBarButtonDropTarget::ProcessDragOver(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point)
{
	DROPEFFECT deReturn = DROPEFFECT_NONE;
	
	// Only interested in bookmarks
	if (pDataObject->IsDataAvailable(
		::RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT)))
	{
		deReturn = DROPEFFECT_COPY;
	}

	return(deReturn);

}

BOOL CBrowserButtonDropTarget::ProcessDrop(CWnd *pWnd, COleDataObject *pDataObject, 
			DROPEFFECT dropEffect, CPoint point)
{
	BOOL bRtn = FALSE;
	
	// Only interested in bookmarks
    CLIPFORMAT cfBookmark = ::RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT);
	if (pDataObject->IsDataAvailable(cfBookmark))
	{
		// TODO: for now, just dump it at the top like the CTRL-D accelerator
		// does. Later, we want to put it in a designated scrap folder or
		// something.
		HGLOBAL hBookmark = pDataObject->GetGlobalData(cfBookmark);
		LPBOOKMARKITEM pBookmark = (LPBOOKMARKITEM)::GlobalLock(hBookmark);
		ASSERT(pBookmark != NULL);

		CGenericFrame * pFrame = (CGenericFrame * )FEU_GetLastActiveFrame();
		if (pFrame) {
			char *urlType = NET_ParseURL(pBookmark->szAnchor, GET_PROTOCOL_PART);

			BOOL bNewWindow = (XP_STRCASECMP(urlType, "file:") == 0) ||
				(XP_STRCASECMP(urlType, "http:") == 0) || 
				(XP_STRCASECMP(urlType, "https:") == 0);


			if(bNewWindow)
                // This checks for existing window with given URL
                FE_LoadUrl(pBookmark->szAnchor, LOAD_URL_NAVIGATOR);
			else
				pFrame->GetMainContext()->NormalGetUrl(pBookmark->szAnchor);
		}
		//this needs to become what we are dropping onto
		::GlobalUnlock(hBookmark);

		bRtn = TRUE;
	}
	return bRtn;
}

DROPEFFECT CComposerButtonDropTarget::ProcessDragEnter(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point)
{

	DROPEFFECT deReturn = DROPEFFECT_NONE;
	
	// Only interested in bookmarks
    CLIPFORMAT cfBookmark = ::RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT);

	if (pDataObject->IsDataAvailable(cfBookmark))
	{
		HGLOBAL hBookmark = pDataObject->GetGlobalData(cfBookmark);
		LPBOOKMARKITEM pBookmark = (LPBOOKMARKITEM)::GlobalLock(hBookmark);
		ASSERT(pBookmark != NULL);

		if(pBookmark)
		{
			char *urlType = NET_ParseURL(pBookmark->szAnchor, GET_PROTOCOL_PART);

			BOOL bAccept = (XP_STRCASECMP(urlType, "file:") == 0) ||
				(XP_STRCASECMP(urlType, "http:") == 0) || 
				(XP_STRCASECMP(urlType, "https:") == 0);

			if(bAccept)
				deReturn = DROPEFFECT_COPY;
		}

	}

	return(deReturn);

}

DROPEFFECT CComposerButtonDropTarget::ProcessDragOver(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point)
{
	DROPEFFECT deReturn = DROPEFFECT_NONE;
	
	// Only interested in bookmarks
    CLIPFORMAT cfBookmark = ::RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT);

	if (pDataObject->IsDataAvailable(cfBookmark))
	{
		HGLOBAL hBookmark = pDataObject->GetGlobalData(cfBookmark);
		LPBOOKMARKITEM pBookmark = (LPBOOKMARKITEM)::GlobalLock(hBookmark);
		ASSERT(pBookmark != NULL);

		if(pBookmark)
		{
			char *urlType = NET_ParseURL(pBookmark->szAnchor, GET_PROTOCOL_PART);

			BOOL bAccept = (XP_STRCASECMP(urlType, "file:") == 0) ||
				(XP_STRCASECMP(urlType, "http:") == 0) || 
				(XP_STRCASECMP(urlType, "https:") == 0);

			if(bAccept)
				deReturn = DROPEFFECT_COPY;
		}
	}

	return(deReturn);

}

BOOL CComposerButtonDropTarget::ProcessDrop(CWnd *pWnd, COleDataObject *pDataObject, 
			DROPEFFECT dropEffect, CPoint point)
{
	BOOL bRtn = FALSE;
	
	// Only interested in bookmarks
    CLIPFORMAT cfBookmark = ::RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT);
	if (pDataObject->IsDataAvailable(cfBookmark))
	{
		// TODO: for now, just dump it at the top like the CTRL-D accelerator
		// does. Later, we want to put it in a designated scrap folder or
		// something.
		HGLOBAL hBookmark = pDataObject->GetGlobalData(cfBookmark);
		LPBOOKMARKITEM pBookmark = (LPBOOKMARKITEM)::GlobalLock(hBookmark);
		ASSERT(pBookmark != NULL);
		CGenericFrame * pFrame = (CGenericFrame * )FEU_GetLastActiveFrame();

		if(pFrame)
		{
            FE_LoadUrl(pBookmark->szAnchor, LOAD_URL_COMPOSER);
		}

		//this needs to become what we are dropping onto
		::GlobalUnlock(hBookmark);

		bRtn = TRUE;
	}
	return bRtn;
}

DROPEFFECT CMessengerButtonDropTarget::ProcessDragEnter(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point)
{

	DROPEFFECT deReturn = DROPEFFECT_NONE;
	
	// Only interested in bookmarks
    CLIPFORMAT cfBookmark = ::RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT);

	if (pDataObject->IsDataAvailable(cfBookmark))
	{
		HGLOBAL hBookmark = pDataObject->GetGlobalData(cfBookmark);
		LPBOOKMARKITEM pBookmark = (LPBOOKMARKITEM)::GlobalLock(hBookmark);
		ASSERT(pBookmark != NULL);

		if(pBookmark)
		{
			char *urlType = NET_ParseURL(pBookmark->szAnchor, GET_PROTOCOL_PART);

			BOOL bAccept = (XP_STRCASECMP(urlType, "mailbox:") == 0);

			if(bAccept)
				deReturn = DROPEFFECT_COPY;
		}
	}
	return(deReturn);

}

DROPEFFECT CMessengerButtonDropTarget::ProcessDragOver(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point)
{
	DROPEFFECT deReturn = DROPEFFECT_NONE;
	
	// Only interested in bookmarks
    CLIPFORMAT cfBookmark = ::RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT);

	if (pDataObject->IsDataAvailable(cfBookmark))
	{
		HGLOBAL hBookmark = pDataObject->GetGlobalData(cfBookmark);
		LPBOOKMARKITEM pBookmark = (LPBOOKMARKITEM)::GlobalLock(hBookmark);
		ASSERT(pBookmark != NULL);

		if(pBookmark)
		{
			char *urlType = NET_ParseURL(pBookmark->szAnchor, GET_PROTOCOL_PART);

			BOOL bAccept = (XP_STRCASECMP(urlType, "mailbox:") == 0);

			if(bAccept)
				deReturn = DROPEFFECT_COPY;
		}
	}
	return(deReturn);

}

BOOL CMessengerButtonDropTarget::ProcessDrop(CWnd *pWnd, COleDataObject *pDataObject, 
			DROPEFFECT dropEffect, CPoint point)
{
	BOOL bRtn = FALSE;
	
	// Only interested in bookmarks
    CLIPFORMAT cfBookmark = ::RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT);
	if (pDataObject->IsDataAvailable(cfBookmark))
	{
		// TODO: for now, just dump it at the top like the CTRL-D accelerator
		// does. Later, we want to put it in a designated scrap folder or
		// something.
		HGLOBAL hBookmark = pDataObject->GetGlobalData(cfBookmark);
		LPBOOKMARKITEM pBookmark = (LPBOOKMARKITEM)::GlobalLock(hBookmark);
		ASSERT(pBookmark != NULL);

		CGenericFrame * pFrame = (CGenericFrame * )FEU_GetLastActiveFrame();
		if (pFrame) {
			char *urlType = NET_ParseURL(pBookmark->szAnchor, GET_PROTOCOL_PART);
	
			BOOL bAccept = (XP_STRCASECMP(urlType, "mailbox:") == 0);

			if(bAccept)
				pFrame->GetMainContext()->NormalGetUrl(pBookmark->szAnchor);
	

		}
		//this needs to become what we are dropping onto
		::GlobalUnlock(hBookmark);

		bRtn = TRUE;
	}
	return bRtn;
}

DROPEFFECT CCollabraButtonDropTarget::ProcessDragEnter(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point)
{

	DROPEFFECT deReturn = DROPEFFECT_NONE;
	
	// Only interested in bookmarks
    CLIPFORMAT cfBookmark = ::RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT);

	if (pDataObject->IsDataAvailable(cfBookmark))
	{
		HGLOBAL hBookmark = pDataObject->GetGlobalData(cfBookmark);
		LPBOOKMARKITEM pBookmark = (LPBOOKMARKITEM)::GlobalLock(hBookmark);
		ASSERT(pBookmark != NULL);

		if(pBookmark)
		{
			char *urlType = NET_ParseURL(pBookmark->szAnchor, GET_PROTOCOL_PART);

			BOOL bAccept = (XP_STRCASECMP(urlType, "news:") == 0) ||
							   (XP_STRCASECMP(urlType, "snews:") == 0);

			if(bAccept)
				deReturn = DROPEFFECT_COPY;
		}
	}
	return(deReturn);

}

DROPEFFECT CCollabraButtonDropTarget::ProcessDragOver(CWnd *pWnd, COleDataObject *pDataObject, 
			DWORD dwKeyState, CPoint point)
{
	DROPEFFECT deReturn = DROPEFFECT_NONE;
	
	// Only interested in bookmarks
    CLIPFORMAT cfBookmark = ::RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT);

	if (pDataObject->IsDataAvailable(cfBookmark))
	{
		HGLOBAL hBookmark = pDataObject->GetGlobalData(cfBookmark);
		LPBOOKMARKITEM pBookmark = (LPBOOKMARKITEM)::GlobalLock(hBookmark);
		ASSERT(pBookmark != NULL);

		if(pBookmark)
		{
			char *urlType = NET_ParseURL(pBookmark->szAnchor, GET_PROTOCOL_PART);

			BOOL bAccept = (XP_STRCASECMP(urlType, "news:") == 0) ||
							   (XP_STRCASECMP(urlType, "snews:") == 0);

			if(bAccept)
				deReturn = DROPEFFECT_COPY;
		}
	}
	return(deReturn);

}

BOOL CCollabraButtonDropTarget::ProcessDrop(CWnd *pWnd, COleDataObject *pDataObject, 
			DROPEFFECT dropEffect, CPoint point)
{
	BOOL bRtn = FALSE;
	
	// Only interested in bookmarks
    CLIPFORMAT cfBookmark = ::RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT);
	if (pDataObject->IsDataAvailable(cfBookmark))
	{
		// TODO: for now, just dump it at the top like the CTRL-D accelerator
		// does. Later, we want to put it in a designated scrap folder or
		// something.
		HGLOBAL hBookmark = pDataObject->GetGlobalData(cfBookmark);
		LPBOOKMARKITEM pBookmark = (LPBOOKMARKITEM)::GlobalLock(hBookmark);
		ASSERT(pBookmark != NULL);
		
		CGenericFrame * pFrame = (CGenericFrame * )FEU_GetLastActiveFrame();
		if (pFrame) {
	
			if (pFrame) {
				char *urlType = NET_ParseURL(pBookmark->szAnchor, GET_PROTOCOL_PART);
			
				BOOL bAccept = (XP_STRCASECMP(urlType, "news:") == 0) ||
							   (XP_STRCASECMP(urlType, "snews:") == 0);

				if(bAccept)
					pFrame->GetMainContext()->NormalGetUrl(pBookmark->szAnchor);
			}
		}
		//this needs to become what we are dropping onto
		::GlobalUnlock(hBookmark);

		bRtn = TRUE;
	}
	return bRtn;
}


/****************************************************************************
*
*	Class: CTaskIcon
*
*	DESCRIPTION:
*		This class represents the abstraction of a task icon object. It
*		encapsulates the data that is used to construct CTaskIconWnd objects.
*
****************************************************************************/

/****************************************************************************
*
*	CTaskIcon::CTaskIcon
*
*	PARAMETERS:
*		idTask		- task identifier
*		pwndNotify	- pointer to window desiring notification
*		dwMessage	- user-defined callback message
*		idBmpLarge	- bitmap resource ID for large icon
*		idBmpSmall	- bitmap resource ID for small icon
*		idTip		- string resource ID of tool tip text
*
* 	RETURNS:
*		N/A
*
*	DESCRIPTION:
*   	Constructor.
*
****************************************************************************/

CTaskIcon::CTaskIcon(UINT idTask, CWnd * pwndNotify, DWORD dwMessage,
	UINT idBmpLarge, int indexBitmapLarge, UINT idBmpSmall, int indexBitmapSmall, UINT idHorizText,
	UINT idVertText, UINT idDockedTip, UINT idFloatingTip)
{
	m_idTask = idTask;
	m_pwndNotify = pwndNotify;
	m_dwMessage = dwMessage;
	m_idBmpLarge = idBmpLarge;
	m_idBmpSmall = idBmpSmall;
	m_indexBmpLarge = indexBitmapLarge;
	m_indexBmpSmall = indexBitmapSmall;
	m_idHorizText = idHorizText;
	m_idVertText = idVertText;
	m_idDockedTip = idDockedTip;
	m_idFloatingTip = idFloatingTip;

} // END OF FUNCTION CTaskIcon::CTaskIcon()

/****************************************************************************
*
*  CTaskIcon::~CTaskIcon
*
*  PARAMETERS:
*     N/A
*
*  RETURNS:
*     N/A
*
*  DESCRIPTION:
*     Destructor.
*
****************************************************************************/

CTaskIcon::~CTaskIcon()
{
} // END OF FUNCTION CTaskIcon::~CTaskIcon()


/****************************************************************************
*
*	Class: CTaskIconArray
*
*	DESCRIPTION:
*		This is a container class for holding CTaskIcon objects.
*
****************************************************************************/

/****************************************************************************
*
*	CTaskIconArray::FindByID
*
*	PARAMETERS:
*		idTask	- id of icon object to be found
*
*	RETURNS:
*		Index of the object if found, -1 if not.
*
*	DESCRIPTION:
*		This function is called to search the list for an icon object with
*		the given task identifier.
*
****************************************************************************/

int CTaskIconArray::FindByID(UINT idTask)
{
	int nRtn = -1;
	
	BOOL bFound = FALSE;
	for (int i = 0; (i < GetSize()) && !bFound; i++)
	{
		if (Get(i)->GetTaskID() == idTask)
		{
			bFound = TRUE;
			nRtn = i;
		}
	}
	
	return(nRtn);

} // END OF	FUNCTION CTaskIconArray::FindByID()

/****************************************************************************
*
*	CTaskIconArray::DeleteAll
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This functions is called to delete all of the CTaskIcon objects
*		in the container.
*
****************************************************************************/

void CTaskIconArray::DeleteAll()
{
	int nSize = GetSize();

	for (int i = 0; i < nSize; i++)
	{
		CTaskIcon * pIcon = Get(i);
		delete pIcon;
	}
	RemoveAll();

} // END OF	FUNCTION CTaskIconArray::DeleteAll()


/****************************************************************************
*
*	Class: CTaskIconWnd
*
*	DESCRIPTION:
*		This object represents a notification icon. It can be embeded within
*		a task bar and provides mouse notifications to a given window. It is
*		a window that paints its own bitmap, displays tool tip text, processes
*		mouse events, etc.
*
****************************************************************************/

BEGIN_MESSAGE_MAP(CTaskIconWnd, CTaskIconWndBase)
	//{{AFX_MSG_MAP(CTaskIconWnd)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEMOVE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/****************************************************************************
*
*	CTaskIconWnd::CTaskIconWnd
*
*	PARAMETERS:
*		None
*
* 	RETURNS:
*		N/A
*
*	DESCRIPTION:
*   	Constructor. Instantiate the object, then call the Create() function.
*
****************************************************************************/

CTaskIconWnd::CTaskIconWnd()
{

} // END OF FUNCTION CTaskIconWnd::CTaskIconWnd()

/****************************************************************************
*
*  CTaskIconWnd::~CTaskIconWnd
*
*  PARAMETERS:
*     N/A
*
*  RETURNS:
*     N/A
*
*  DESCRIPTION:
*     Destructor.
*
****************************************************************************/

CTaskIconWnd::~CTaskIconWnd()
{
	
} // END OF FUNCTION CTaskIconWnd::~CTaskIconWnd()

/****************************************************************************
*
*	CTaskIconWnd::Create
*
*	PARAMETERS:
*		idTask		- task identifier
*		pwndNotify	- pointer to window desiring notification
*		dwMessage	- user-defined callback message
*		idBmp		- bitmap resource ID
*		idTip		- string resource ID of tool tip text
*		pParent		- pointer to parent
*		rc			- window rectangle, if desired
*
*	RETURNS:
*		TRUE if creation is successful.
*
*	DESCRIPTION:
*		This function should be called after construction to actually create
*		the task icon window.
*
****************************************************************************/

BOOL CTaskIconWnd::Create(UINT idTask, CWnd * pwndNotify, DWORD dwMessage,
	UINT idBmp, int nBitmapIndex, UINT idHorizText, UINT idVertText,
	UINT idText, UINT idTip, int nToolbarStyle, CSize noviceSize, CSize advancedSize,
	CSize bitmapSize, CWnd * pParent, const CRect & rc /*= (0,0,0,0)*/)
{
	m_idTask = idTask;
	m_pwndNotify = pwndNotify;
	m_dwMessage = dwMessage;
	m_idHorizText = idHorizText;
	m_idVertText = idVertText;

	CString text , tip;

	text.LoadString(idText);
	tip.LoadString(idTip);

	BOOL bRtn = CTaskIconWndBase::Create(pParent, nToolbarStyle, noviceSize, advancedSize,
										 (const char*)text, (const char*)tip, "",
										 idBmp, nBitmapIndex, bitmapSize, FALSE, idTask, 
										 SHOW_ALL_CHARACTERS, 0);

	if(bRtn)
	{
		AddDropTargetIfStandardButton();
	}

 	return(bRtn);
   
} // END OF FUNCTION CTaskIconWnd::Create()

/****************************************************************************
*
*	CTaskIconWnd::PostNcDestroy
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		We override this function to add auto-cleanup to the window.
*
****************************************************************************/

void CTaskIconWnd::PostNcDestroy()
{
	delete this;

} // END OF	FUNCTION CTaskIconWnd::PostNcDestroy()

void CTaskIconWnd::AddDropTargetIfStandardButton(void)
{
	switch(m_idTask)
	{
		case ID_TOOLS_WEB:
			SetDropTarget(new(CBrowserButtonDropTarget));
			break;
		case ID_TOOLS_EDITOR:
			SetDropTarget(new(CComposerButtonDropTarget));
			break;
		case ID_TOOLS_INBOX:
			SetDropTarget(new(CMessengerButtonDropTarget));
			break;
		case ID_TOOLS_NEWS:
			SetDropTarget(new(CCollabraButtonDropTarget));
			break;
	}

}

/****************************************************************************
*
*	CTaskIconWnd::OnLButtonDown
*
*	PARAMETERS:
*		nFlags	- not used
*		point	- not used
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Handler for the WM_LBUTTONDOWN message. We send the notification to
*		the window that's set to get them from this icon.
*
****************************************************************************/

void CTaskIconWnd::OnLButtonDown(UINT nFlags, CPoint point) 
{
	CTaskIconWndBase::OnLButtonDown(nFlags, point);

	ASSERT(m_pwndNotify != NULL);
	if (m_pwndNotify != NULL)
	{
		m_pwndNotify->SendMessage(CASTUINT(m_dwMessage), m_idTask, WM_LBUTTONDOWN);
	}
	
} // END OF	FUNCTION CTaskIconWnd::OnLButtonDown()

/****************************************************************************
*
*	CTaskIconWnd::OnLButtonUp
*
*	PARAMETERS:
*		nFlags	- not used
*		point	- not used
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Handler for the WM_LBUTTONUP message. We send the notification to the
*		window that's set to get them from this icon.
*
****************************************************************************/

void CTaskIconWnd::OnLButtonUp(UINT nFlags, CPoint point) 
{
	CTaskIconWndBase::OnLButtonUp(nFlags, point);

	ASSERT(m_pwndNotify != NULL);
	if (m_pwndNotify != NULL)
	{
		m_pwndNotify->SendMessage(CASTUINT(m_dwMessage), m_idTask, WM_LBUTTONUP);
	}
	
} // END OF	FUNCTION CTaskIconWnd::OnLButtonUp()

/****************************************************************************
*
*	CTaskIconWnd::OnRButtonDown
*
*	PARAMETERS:
*		nFlags	- not used
*		point	- not used
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Handler for the WM_RBUTTONDOWN message. We just translate it to a
*		notification for the window that's set to get them from this icon.
*
****************************************************************************/

void CTaskIconWnd::OnRButtonDown(UINT nFlags, CPoint point) 
{
	CTaskIconWndBase::OnRButtonDown(nFlags, point);

	ASSERT(m_pwndNotify != NULL);
	if (m_pwndNotify != NULL)
	{
		m_pwndNotify->SendMessage(CASTUINT(m_dwMessage), m_idTask, WM_RBUTTONDOWN);
	}
	
} // END OF	FUNCTION CTaskIconWnd::OnRButtonDown()

/****************************************************************************
*
*	CTaskIconWnd::OnRButtonUp
*
*	PARAMETERS:
*		nFlags	- not used
*		point	- not used
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Handler for the WM_RBUTTONUP message. We just translate it to a
*		notification for the window that's set to get them from this icon.
*
****************************************************************************/

void CTaskIconWnd::OnRButtonUp(UINT nFlags, CPoint point) 
{
	CTaskIconWndBase::OnRButtonUp(nFlags, point);

	ASSERT(m_pwndNotify != NULL);
	if (m_pwndNotify != NULL)
	{
		m_pwndNotify->SendMessage(CASTUINT(m_dwMessage), m_idTask, WM_RBUTTONUP);
	}
	
} // END OF	FUNCTION CTaskIconWnd::OnRButtonUp()

/****************************************************************************
*
*	CTaskIconWnd::OnMouseMove
*
*	PARAMETERS:
*		nFlags	- not used
*		point	- not used
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Handler for the WM_MOUSEMOVE message. We send the notification to the
*		window that's set to get them from this icon.
*
****************************************************************************/

void CTaskIconWnd::OnMouseMove(UINT nFlags, CPoint point) 
{
	CTaskIconWndBase::OnMouseMove(nFlags, point);
	
	ASSERT(m_pwndNotify != NULL);
	if (m_pwndNotify != NULL)
	{
		m_pwndNotify->SendMessage(CASTUINT(m_dwMessage), m_idTask, WM_MOUSEMOVE);
	}
	
} // END OF	FUNCTION CTaskIconWnd::OnMouseMove()


/****************************************************************************
*
*	Class: CTaskIconWndArray
*
*	DESCRIPTION:
*		This is a container class for holding CTaskIconWnd objects.
*
****************************************************************************/

/****************************************************************************
*
*	CTaskIconWndArray::FindByID
*
*	PARAMETERS:
*		idTask	- id of icon object to be found
*
*	RETURNS:
*		Index of the object if found, -1 if not.
*
*	DESCRIPTION:
*		This function is called to search the list for an icon window with
*		the given task identifier.
*
****************************************************************************/

int CTaskIconWndArray::FindByID(UINT idTask)
{
	int nRtn = -1;
	
	BOOL bFound = FALSE;
	for (int i = 0; (i < GetSize()) && !bFound; i++)
	{
		if (Get(i)->GetTaskID() == idTask)
		{
			bFound = TRUE;
			nRtn = i;
		}
	}
	
	return(nRtn);

} // END OF	FUNCTION CTaskIconWndArray::FindByID()

/****************************************************************************
*
*	CTaskIconWndArray::DeleteAll
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This functions is called to delete all of the CTaskIconWnd objects
*		in the container.
*
****************************************************************************/

void CTaskIconWndArray::DeleteAll()
{
	for (int i = 0; i < GetSize(); i++)
	{
		CTaskIconWnd * pIcon = Get(i++);
		if (pIcon != NULL)
		{
			pIcon->DestroyWindow();
			// no need to call delete - pIcon has auto cleanup
		}
	}
	RemoveAll();

} // END OF	FUNCTION CTaskIconWndArray::DeleteAll()


/****************************************************************************
*
*	Class: CTaskBar
*
*	DESCRIPTION:
*		This is the base class for task bar objects. All polymorphic functions
*		common to the floating, docked, or other derived task bars are
*		implemented here.
*
*		This is an abstract base class - you must instantiate one of the
*		derived types. Also, objects of this class are auto-deleting, you
*		must allocate them on the heap.
*
****************************************************************************/

BEGIN_MESSAGE_MAP(CTaskBar, CTaskBarBase)
	//{{AFX_MSG_MAP(CTaskBar)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_PALETTECHANGED()
	ON_WM_SYSCOLORCHANGE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/****************************************************************************
*
*	CTaskBar::CTaskBar
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		N/A
*
*	DESCRIPTION:
*		Constructor
*
****************************************************************************/

CTaskBar::CTaskBar(int nToolbarStyle)
{
	// Derived classes must override these
	m_noviceButtonSize.cx = 0;
	m_noviceButtonSize.cy = 0;
	m_advancedButtonSize.cx = 0;
	m_advancedButtonSize.cy = 0;

	m_IconSize.cx = 0;
	m_IconSize.cy = 0;

	m_nDragBarWidth = 0;
	m_nIconSpace = 0;
	m_nToolbarStyle = nToolbarStyle;

	m_nMaxButtonWidth = m_nMaxButtonHeight = 0;

	m_bHorizontal = TRUE;

} // END OF	FUNCTION CTaskBar::CTaskBar()

/****************************************************************************
*
*	CTaskBar::~CTaskBar
*
*	PARAMETERS:
*		N/A
*
*	RETURNS:
*		N/A
*
*	DESCRIPTION:
*		Destructor.
*
****************************************************************************/

CTaskBar::~CTaskBar()
{

} // END OF	FUNCTION CTaskBar::~CTaskBar()


/****************************************************************************
*
*	CTaskBar::PostNcDestroy
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		We override this function to add auto-cleanup to the window.
*
****************************************************************************/

void CTaskBar::PostNcDestroy()
{
	delete this;

} // END OF	FUNCTION CTaskBar::PostNcDestroy()

/****************************************************************************
*
*	CTaskBar::OnSize
*
*	PARAMETERS:
*		nType, cs, cy	- not used
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		We process the WM_SIZE message so we can reposition our icons.
*
****************************************************************************/

void CTaskBar::OnSize(UINT nType, int cx, int cy) 
{
	CTaskBarBase::OnSize(nType, cx, cy);
	LayoutIcons();

} // END OF	FUNCTION CTaskBar::OnSize()

/****************************************************************************
*
*	CTaskBar::OnPaint
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Overridden to do special painting for the task bar. This includes
*		faking a title bar and a minimize control.
*
****************************************************************************/

void CTaskBar::OnPaint() 
{
	// Do not call CTaskBarBase::OnPaint() for painting messages
	CPaintDC dc(this); // device context for painting

	HPALETTE hPalette = WFE_GetUIPalette(GetParentFrame());
	HPALETTE hOldPalette = ::SelectPalette(dc.m_hDC, hPalette, FALSE);
	DoPaint(dc);
	// Should get unselect when destroy.
	::SelectPalette(dc.m_hDC, hOldPalette, TRUE);
	
} // END OF	FUNCTION CTaskBar::OnPaint()

void CTaskBar::OnPaletteChanged( CWnd* pFocusWnd )
{
	if (pFocusWnd != this) {
		HPALETTE hPalette = WFE_GetUIPalette(GetParentFrame());
		if (WFE_IsGlobalPalette(hPalette)) {
			HDC hDC = ::GetDC(m_hWnd);
			HPALETTE hOldPalette = ::SelectPalette(hDC, hPalette, FALSE);
			::SelectPalette(hDC, hOldPalette, TRUE);
			::ReleaseDC(m_hWnd, hDC);
		}
		Invalidate(FALSE);
	}
}


void CTaskBar::OnSysColorChange(void)
{

	theApp.GetTaskBarMgr().ReloadIconBitmaps(this);

}

/****************************************************************************
*
*	CTaskBar::AddTaskIcon
*
*	PARAMETERS:
*		idTask		- task identifier
*		pwndNotify	- pointer to window desiring notification
*		dwMessage	- user-defined callback message
*		idBmp		- bitmap resource ID
*		idTip		- string resource ID of tool tip text
*
*	RETURNS:
*		TRUE if successful, FALSE if not.
*
*	DESCRIPTION:
*		This function is called to add an icon to the task bar.	Notifications
*		(mouse events) are then sent to the window identified by pwndNotify.
*
****************************************************************************/

BOOL CTaskBar::AddTaskIcon(UINT idTask, CWnd * pwndNotify, DWORD dwMessage,
	HBITMAP hBitmap, int nBitmapIndex, UINT idHorizText, UINT idVertText,
	UINT idText, UINT idTip, int nToolbarStyle)
{
	BOOL bRtn = FALSE;
	
	// Create new task icon object and add it to the list
	CTaskIconWnd * pIcon = new CTaskIconWnd;
	ASSERT(pIcon != NULL);
	if (pIcon != NULL)
	{
		bRtn = pIcon->Create(idTask, pwndNotify, dwMessage, 0, nBitmapIndex, idHorizText, idVertText,
							 idText, idTip, nToolbarStyle, m_noviceButtonSize, m_advancedButtonSize,
							 m_IconSize, this);

		pIcon->SetBitmap(hBitmap, TRUE);

		CSize size = pIcon->GetRequiredButtonSize();
		
		if(m_nToolbarStyle != TB_PICTURESANDTEXT)
		{
			//for the moment we need to make the docked bar smaller than it should be
			//in order to make things fit
			size.cy -= 2;
		}

		if(size.cx > m_nMaxButtonWidth)
			m_nMaxButtonWidth = size.cx;

		if(size.cy > m_nMaxButtonHeight)
			m_nMaxButtonHeight = size.cy;

		m_TaskIconWndList.Add(pIcon);
		
		// Note: our icons will be sized/positioned in our OnSize()
	}
	
	return(bRtn);

} // END OF	FUNCTION CTaskBar::AddTaskIcon()

/****************************************************************************
*
*	CTaskBar::ReplaceTaskIcon
*
*	PARAMETERS:
*		idTask	- task identifier
*		idBmp	- new bitmap resource ID
*
*	RETURNS:
*		TRUE if the icon was successfully replaced, FALSE if not (such as
*		when no existing icon with the given ID is found).
*
*	DESCRIPTION:
*		This function is called to replace an existing icon in the task bar
*		whenever it is desirable to show a different image, but retain the
*		notification handler and other information for the icon.
*
****************************************************************************/

BOOL CTaskBar::ReplaceTaskIcon(UINT idTask, UINT idBmp, int nBitmapIndex)
{
	BOOL bRtn = FALSE;
	
	int i = m_TaskIconWndList.FindByID(idTask);
	if (i > -1)
	{
		bRtn = TRUE;
		
		CTaskIconWnd * pIcon = m_TaskIconWndList.Get(i);
		pIcon->SetBmpID(idBmp);
		pIcon->ReplaceBitmap(idBmp, nBitmapIndex);
	}
	
	return(bRtn);

} // END OF	FUNCTION CTaskBar::ReplaceTaskIcon()

/****************************************************************************
*
*	CTaskBar::RemoveTaskIcon
*
*	PARAMETERS:
*		idTask	- task identifier
*
*	RETURNS:
*		TRUE if the icon was successfully removed from the task bar, FALSE
*		if not (such as when no existing icon with the given ID is found).
*
*	DESCRIPTION:
*		This function is called to remove an icon from the task bar when it
*		is no longer desirable to have it displayed. This is the only case
*		where this function is useful, since cleanup is performed
*		automatically when the task bar is destroyed.
*
****************************************************************************/

BOOL CTaskBar::RemoveTaskIcon(UINT idTask)
{
	BOOL bRtn = FALSE;
	
	int i = m_TaskIconWndList.FindByID(idTask);
	if (i > -1)
	{
		bRtn = TRUE;
		
		CTaskIconWnd * pIcon = m_TaskIconWndList.Get(i);
		pIcon->DestroyWindow();	// no need to delete - it's auto cleanup
		m_TaskIconWndList.RemoveAt(i);
		
		// Note: our icons will be re-sized/positioned in our OnSize()
	}
	
	return(bRtn);

} // END OF	FUNCTION CTaskBar::RemoveTaskIcon()

/****************************************************************************
*
*	CTaskBar::CalcDesiredDim
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		The width and height that the task bar requires to draw itself
*
*	DESCRIPTION:
*		This function is called to determine the dimensions that the Task
*		Bar window will require.
*
****************************************************************************/

CSize CTaskBar::CalcDesiredDim()
{
	CSize sizeRtn;
	CSize sizeButton;

	// Make it wide enough for all icons, plus the margin
	int nNumIcons = m_TaskIconWndList.GetSize();

	CSize buttonSize(m_nMaxButtonWidth, m_nMaxButtonHeight);

	if(m_bHorizontal)
	{
		sizeRtn.cy = 0;
		sizeRtn.cx = 0;
		for(int i = 0; i < nNumIcons; i++)
		{
			CTaskIconWnd *pIcon = (CTaskIconWnd*)m_TaskIconWndList[i];

			sizeButton = pIcon->GetRequiredButtonSize();
			sizeRtn.cx += sizeButton.cx; 

		}

		sizeRtn.cx += ((nNumIcons + 1) * m_nIconSpace);

		// Must allow for drag bar
		sizeRtn.cx += m_nDragBarWidth;
		
		// Make height proportional to the icon height
		sizeRtn.cy = buttonSize.cy + 6;
	}
	else // if vertical
	{
		sizeRtn.cy = sizeRtn.cx = 0;

		for(int i = 0; i < nNumIcons; i++)
		{
			CTaskIconWnd *pIcon = (CTaskIconWnd*)m_TaskIconWndList[i];

			sizeButton = pIcon->GetRequiredButtonSize();
			sizeRtn.cy += sizeButton.cy; 

		}

		sizeRtn.cy += ((nNumIcons + 1) * m_nIconSpace);

		// Must all for drag bar
		sizeRtn.cy += m_nDragBarWidth;

		//currently magic numbers to make it look the same size as when in horizontal position
		sizeRtn.cx = buttonSize.cx + 6;
	}
	
	#ifdef WIN32
		int nBorderWidth = ::GetSystemMetrics(SM_CXSIZEFRAME);
	#else
		int nBorderWidth = ::GetSystemMetrics(SM_CXFRAME);
	#endif

	sizeRtn.cx += nBorderWidth;
	//add the title bar
	sizeRtn.cy += GetSystemMetrics(SM_CYCAPTION);
	return(sizeRtn);
	
} // END OF	FUNCTION CTaskBar::CalcDesiredDim()

/****************************************************************************
*
*	CTaskBar::GetButtonDimensions
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		The size of a button depending upon the mode we are in
*
*	DESCRIPTION:
*		This function is called to determine the dimensions of a button on
*		the taskbar
*
****************************************************************************/
CSize CTaskBar::GetButtonDimensions(void)
{
		return( m_nToolbarStyle == TB_PICTURESANDTEXT ? m_noviceButtonSize : m_advancedButtonSize);
} // END OF FUNCTION CTaskBar::GetButtonDimensions()

/****************************************************************************
*
*	CTaskBar::SetTaskBarStyle
*
*	PARAMETERS:
*		nToolbarStyle The style we are changing to
*
*	RETURNS:
*		nothing
*
*	DESCRIPTION:
*		This function is called to change the taskbar style to nToolbarStyle
*
****************************************************************************/

void CTaskBar::SetTaskBarStyle(int nToolbarStyle)
{
	m_nToolbarStyle = nToolbarStyle;
	m_bShowText = m_nToolbarStyle != TB_PICTURES;
	ChangeButtonStyle();
}

  /****************************************************************************
*
*	CTaskBar::LayoutIcons
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This protected helper function is called to arrange the task icons
*		appropriately within the task bar.
*
****************************************************************************/

void CTaskBar::LayoutIcons()
{
	int nX, nY;
	int nHSpace, nVSpace;

	// The idea is to space the icons evenly, allowing for the grab bar.
	CRect rc;
	GetClientRect(&rc);
	
	int nNumIcons = m_TaskIconWndList.GetSize();
	// If taskbar is horizontal, buttons have same height but different widths.  If
	// vertical, buttons have same width, but different heights.
	CSize sameSize = CSize(m_nMaxButtonWidth, m_nMaxButtonHeight);
	CSize individualButtonSize;

	if(m_bHorizontal)
	{
		rc.left += m_nDragBarWidth;

		nX = rc.left + m_nIconSpace;
		nVSpace = ((rc.Height() - (sameSize.cy)) + 1) / 2;
		nVSpace = (nVSpace > 0) ? nVSpace : 0;
		nY = rc.top + nVSpace;
	}
	else // if vertical
	{
		rc.top += m_nDragBarHeight;

		nY = rc.top + m_nIconSpace;
		nHSpace = ((rc.Width() - sameSize.cx) + 1) / 2;
		nHSpace = (nHSpace > 0) ? nHSpace : 0;
		nX = rc.left + nHSpace;
	}

	// Now re-size & re-position the icons
	for (int i = 0; i < m_TaskIconWndList.GetSize(); i++)
	{
		CTaskIconWnd * pIcon = m_TaskIconWndList.Get(i);
		individualButtonSize = pIcon->GetRequiredButtonSize();


		if(m_bHorizontal)
		{
			pIcon->MoveWindow(nX, nY, individualButtonSize.cx, sameSize.cy);
			nX += (individualButtonSize.cx + m_nIconSpace);
		}
		else
		{
			pIcon->MoveWindow(nX, nY, sameSize.cx, individualButtonSize.cy);
			nY += (individualButtonSize.cy + m_nIconSpace);
		}
	}

} // END OF	FUNCTION CTaskBar::LayoutIcons()

/****************************************************************************
*
*	CTaskBar::DragBarHitTest
*
*	PARAMETERS:
*		pt	- point to test
*
*	RETURNS:
*		TRUE if the given point lies within our drag bar, FALSE if not
*
*	DESCRIPTION:
*		This protected helper function is called to determine whether a
*		given point lies within our drag bar area.
*
****************************************************************************/

BOOL CTaskBar::DragBarHitTest(const CPoint & pt)
{
	CRect rc;
	GetClientRect(&rc);

	if(m_bHorizontal)
	{
		rc.right = m_nDragBarWidth;
	}
	else
	{
		rc.bottom = m_nDragBarHeight;
	}
	
	return(rc.PtInRect(pt));

} // END OF	FUNCTION CTaskBar::DragBarHitTest()


void CTaskBar::ChangeButtonStyle(void)
{
	int nCount = m_TaskIconWndList.GetSize();

	m_nMaxButtonWidth = m_nMaxButtonHeight = 0;

	for(int i = 0; i < nCount; i++)
	{
		CTaskIconWnd *pIcon = (CTaskIconWnd*)m_TaskIconWndList[i];

		pIcon->SetButtonMode(m_nToolbarStyle);

		CSize size = pIcon->GetRequiredButtonSize();
		
		if(m_nToolbarStyle != TB_PICTURESANDTEXT)
		{
			//for the moment we need to make the docked bar smaller than it should be
			//in order to make things fit
			size.cy -= 2;
		}

		if(size.cx > m_nMaxButtonWidth)
			m_nMaxButtonWidth = size.cx;

		if(size.cy > m_nMaxButtonHeight)
			m_nMaxButtonHeight = size.cy;

	}

}

void CTaskBar::ChangeButtonText(void)
{

	int nCount = m_TaskIconWndList.GetSize();

	m_nMaxButtonWidth = m_nMaxButtonHeight = 0;

	for(int i = 0; i < nCount; i++)
	{
		CTaskIconWnd *pIcon = (CTaskIconWnd*)m_TaskIconWndList[i];

		int idText = m_bHorizontal ? pIcon->GetHorizTextID() : pIcon->GetVertTextID();
		CString csText;
		csText.LoadString(idText);

		pIcon->SetText(csText);
		
		// Need to recalculate the maximum sizes
		CSize size = pIcon->GetRequiredButtonSize();
		
		if(size.cx > m_nMaxButtonWidth)
			m_nMaxButtonWidth = size.cx;

		if(size.cy > m_nMaxButtonHeight)
			m_nMaxButtonHeight = size.cy;
	}
}

void CTaskBar::ReplaceButtonBitmap(int nIndex, HBITMAP hBitmap)
{

	CTaskIconWnd *pIcon = (CTaskIconWnd*)m_TaskIconWndList[nIndex];

	pIcon->SetBitmap(hBitmap, TRUE);

	pIcon->Invalidate();

}

/****************************************************************************
*
*	Class: CFloatingTaskBar
*
*	DESCRIPTION:
*		This derived version of CTaskBar provides a "floating" task bar. It
*		is in the form of a custom popup window.
*
****************************************************************************/

/****************************************************************************
*
*	CONSTANTS
*
****************************************************************************/
static const UINT uID_DOCK = 0xff;	// Docking button ID
static const int nDOCK_BTN_W = 13;
static const int nDOCK_BTN_H = 13;

BEGIN_MESSAGE_MAP(CFloatingTaskBar, CFloatingTaskBarBase)
	//{{AFX_MSG_MAP(CFloatingTaskBar)
	ON_WM_CLOSE()
	ON_WM_MOUSEMOVE()
	ON_BN_CLICKED(uID_DOCK, OnDock)
	ON_WM_LBUTTONDBLCLK()
	ON_WM_RBUTTONUP()
	ON_WM_MOVE()
	ON_COMMAND(ID_TBAR_ALWAYSONTOP, OnAlwaysOnTop)
	ON_COMMAND(ID_TBAR_SHOWTEXT, OnShowText)
	ON_COMMAND(ID_TBAR_POSITION, OnPosition)
	ON_MESSAGE(TASKBAR_ADDMENU, OnAddMenu)
	ON_WM_SYSCOMMAND()
	ON_WM_INITMENU()
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/****************************************************************************
*
*	CFloatingTaskBar::CFloatingTaskBar
*
*	PARAMETERS:
*		bOnTop		- TRUE if the 'always on top' style should be used
*		bShowText	- TRUE if button text should be shown
*		bHorizontal	- TRUE if task bar is oriented horizontally
*
*	RETURNS:
*		N/A
*
*	DESCRIPTION:
*		Constructor.
*
****************************************************************************/

CFloatingTaskBar::CFloatingTaskBar(int nToolbarStyle, BOOL bOnTop /*=TRUE*/, BOOL bHorizontal /*=TRUE*/)
	: CTaskBar(nToolbarStyle)
{
	m_noviceButtonSize.cx = 65;
	m_noviceButtonSize.cy = 43;
	m_advancedButtonSize.cx = 34;
	m_advancedButtonSize.cy = 24;

	m_IconSize.cx = 32;
	m_IconSize.cy = 22;

	m_nDragBarWidth = m_nDragBarHeight = 0;

	m_nIconSpace = 4;
	m_bActive = FALSE;
	
	m_bOnTop = bOnTop;
	m_bHorizontal = bHorizontal;
	m_bShowText = nToolbarStyle != TB_PICTURES;
	
} // END OF	FUNCTION CFloatingTaskBar::CFloatingTaskBar()

/****************************************************************************
*
*	CFloatingTaskBar::~CFloatingTaskBar
*
*	PARAMETERS:
*		N/A
*
*	RETURNS:
*		N/A
*
*	DESCRIPTION:
*		Destructor.
*
****************************************************************************/

CFloatingTaskBar::~CFloatingTaskBar()
{
	int test = 0;
} // END OF	FUNCTION CFloatingTaskBar::~CFloatingTaskBar()

/****************************************************************************
*
*	CFloatingTaskBar::Create
*
*	PARAMETERS:
*		pParent	- pointer to parent window
*
*	RETURNS:
*		TRUE if creation is successful, FALSE if not.
*
*	DESCRIPTION:
*		This function must be called after construction to create the task
*		bar.
*
****************************************************************************/

BOOL CFloatingTaskBar::Create(CWnd * pParent)
{
	BOOL bRtn = FALSE;
	
	CBrush brush;

	CString strClass = theApp.NSToolBarClass;
	
	CString strName("");
	DWORD dwExStyle = m_bOnTop ? WS_EX_TOPMOST : 0;

#ifdef WIN32
	bRtn = CFloatingTaskBarBase::CreateEx(dwExStyle | WS_EX_TOOLWINDOW | WS_EX_WINDOWEDGE, (const char *)strClass,
	(const char *)strName, WS_DLGFRAME | WS_BORDER | WS_POPUP | WS_SYSMENU ,
		0, 0, 0, 0, pParent->GetSafeHwnd(), NULL);
#else 
	bRtn = CFloatingTaskBarBase::CreateEx(dwExStyle, (const char *)strClass,
	(const char *)strName, WS_CAPTION | WS_POPUP | WS_SYSMENU ,
		0, 0, 0, 0, pParent->GetSafeHwnd(), NULL);
#endif


	return(bRtn);
	
} // END OF	FUNCTION CFloatingTaskBar::Create()

int CFloatingTaskBar::OnCreate( LPCREATESTRUCT lpCreateStruct )
{
	CFloatingTaskBarBase::OnCreate(lpCreateStruct);

	PostMessage(TASKBAR_ADDMENU, 0, 0L);
	return 0;
}
 

LRESULT CFloatingTaskBar::OnAddMenu(WPARAM wParam, LPARAM lParam)
{
	CMenu *pSysMenu = GetSystemMenu(FALSE);

	pSysMenu->RemoveMenu(SC_MAXIMIZE, MF_BYCOMMAND);
	pSysMenu->RemoveMenu(SC_MINIMIZE, MF_BYCOMMAND);
	pSysMenu->RemoveMenu(SC_RESTORE, MF_BYCOMMAND);
	pSysMenu->RemoveMenu(SC_SIZE, MF_BYCOMMAND);

	pSysMenu->InsertMenu(0, CASTUINT(MF_BYPOSITION | MF_STRING), ID_TBAR_ALWAYSONTOP, szLoadString(IDS_TASKBAR_ONTOP));
	pSysMenu->InsertMenu(1, CASTUINT(MF_BYPOSITION | MF_STRING), CASTUINT(ID_TBAR_POSITION), szLoadString(CASTUINT(m_bHorizontal ? IDS_VERTICAL : IDS_HORIZONTAL)));
	pSysMenu->InsertMenu(2, CASTUINT(MF_BYPOSITION | MF_STRING), CASTUINT(ID_TBAR_SHOWTEXT), szLoadString(CASTUINT(m_bShowText ? IDS_TASKBAR_HIDETEXT : IDS_TASKBAR_SHOWTEXT))); 
	pSysMenu->InsertMenu(3, CASTUINT(MF_BYPOSITION | MF_SEPARATOR));

	return 1;
}
/****************************************************************************
*
*	CFloatingTaskBar::OnClose
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		We process WM_CLOSE so we can actually dock instead.
*
****************************************************************************/

void CFloatingTaskBar::OnClose()
{
	// Tell the task bar Manager to dock
	OnDock();
	
} // END OF	FUNCTION CFloatingTaskBar::OnClose()

/****************************************************************************
*
*	CFloatingTaskBar::OnDock
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This is the handler for the dock button. It can be called to dock
*		the task bar (change to docked state).
*
****************************************************************************/

void CFloatingTaskBar::OnDock()
{
	((CNetscapeApp *)AfxGetApp())->GetTaskBarMgr().OnDockTaskBar();

} // END OF	FUNCTION CFloatingTaskBar::OnDock()

/****************************************************************************
*
*	CFloatingTaskBar::DoPaint
*
*	PARAMETERS:
*		dc	- reference to dc for painting
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Virtual handler for the paint message. Here's where we do all our
*		specialized drawing for our task bar.
*
****************************************************************************/

void CFloatingTaskBar::DoPaint(CPaintDC & dc)
{
	
} // END OF	FUNCTION CFloatingTaskBar::DoPaint()

/****************************************************************************
*
*	CFloatingTaskBar::PaintDragBar
*
*	PARAMETERS:
*		pdc	- pointer to dc to draw on
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Protected helper for painting our custom window caption.
*
****************************************************************************/

void CFloatingTaskBar::PaintDragBar(CDC * pdc)
{
	CRect rcClient;
	GetClientRect(&rcClient);
	
	if(m_bHorizontal)
	{
		// Draw a 3-d border to the right of the drag bar
		CRect rcTmp(m_nDragBarWidth, 0, m_nDragBarWidth + 1, rcClient.bottom);
		CBrush brShadow(::GetSysColor(COLOR_BTNSHADOW));
		pdc->FillRect(rcTmp, &brShadow);
		rcTmp.OffsetRect(1, 0);
		CBrush brHilight(::GetSysColor(COLOR_BTNHIGHLIGHT));
		pdc->FillRect(rcTmp, &brHilight);

		// Draw a 3-d "grip"
		for (int i = nDOCK_BTN_H + 2; i < (rcClient.bottom - 2); i += 3)
		{
			rcTmp.SetRect(1, i, m_nDragBarWidth - 1, i + 1);
			pdc->FillRect(rcTmp, &brShadow);
			rcTmp.SetRect(1, i + 1, m_nDragBarWidth - 1, i + 2);
			pdc->FillRect(rcTmp, &brHilight);
		}
	}
	else // if vertical
	{
		CRect rcTmp(0, m_nDragBarHeight, rcClient.right, m_nDragBarWidth + 1);
		CBrush brShadow(::GetSysColor(COLOR_BTNSHADOW));
		pdc->FillRect(rcTmp, &brShadow);
		rcTmp.OffsetRect(0, 1);
		CBrush brHilight(::GetSysColor(COLOR_BTNHIGHLIGHT));
		pdc->FillRect(rcTmp, &brHilight);

		// Draw a 3-d "grip"
		for (int i = nDOCK_BTN_W + 2; i < (rcClient.right - 2); i += 3)
		{
			rcTmp.SetRect(i, 1, i + 1, m_nDragBarHeight - 1);
			pdc->FillRect(rcTmp, &brShadow);
			rcTmp.SetRect(i + 1, 1,  i + 2, m_nDragBarHeight - 1);
			pdc->FillRect(rcTmp, &brHilight);
		}



	}
	
} // END OF	FUNCTION CFloatingTaskBar::PaintDragBar()

/****************************************************************************
*
*	CFloatingTaskBar::OnMouseMove
*
*	PARAMETERS:
*		nFlags	- control key flags
*		point	- cursor coordinate
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		We process this message to allow the user to move the floating task
*		bar around, since we're not a normal window and have no title bar.
*
****************************************************************************/

void CFloatingTaskBar::OnMouseMove(UINT nFlags, CPoint point) 
{
	CFloatingTaskBarBase::OnMouseMove(nFlags, point);

	// Are we being dragged?
	if (nFlags & MK_LBUTTON)
	{
		// By the drag bar?
		if (DragBarHitTest(point))
		{
			// Start the drag process
			CRect rc;
			GetWindowRect(&rc);
			CRectTracker Tracker(&rc, CRectTracker::solidLine);
			Tracker.m_sizeMin.cx = rc.Width();
			Tracker.m_sizeMin.cy = rc.Height();
			ClientToScreen(&point);
			if (Tracker.Track(this, point, FALSE, GetDesktopWindow()))
			{
				// User released and position changed
				rc = Tracker.m_rect;
				ClientToScreen(&rc);
				MoveWindow(&rc);
				
				// Inform task bar manager of our new position
				((CNetscapeApp *)AfxGetApp())->GetTaskBarMgr().SetLastFloatPos(
					rc.TopLeft());
			}
		}
	}
		
} // END OF	FUNCTION CFloatingTaskBar::OnMouseMove()

/****************************************************************************
*
*	CFloatingTaskBar::OnLButtonDblClk
*
*	PARAMETERS:
*		nFlags	- control key flags
*		point	- cursor coordinate
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		We process this message so user can dock us by double-clicking on
*		the drag bar.
*
****************************************************************************/

void CFloatingTaskBar::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	CFloatingTaskBarBase::OnLButtonDblClk(nFlags, point);
	
	if (DragBarHitTest(point))
	{
		OnDock();
	}
	
} // END OF	FUNCTION CFloatingTaskBar::OnLButtonDblClk()

/****************************************************************************
*
*	CFloatingTaskBar::OnRButtonUp
*
*	PARAMETERS:
*		nFlags	- control key flags
*		point	- cursor coordinate
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		We process this handler so we can display the right mouse pop-up
*		menu.
*
****************************************************************************/

void CFloatingTaskBar::OnRButtonUp(UINT nFlags, CPoint point) 
{
	CMenu Menu;
	if (Menu.LoadMenu(IDR_TASKBAR_POPUP))
	{
		CMenu * pSubMenu = Menu.GetSubMenu(0);
		SetMenuState(pSubMenu);
		ClientToScreen(&point);
		pSubMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON,
			point.x, point.y, this);
	}
	
	CFloatingTaskBarBase::OnRButtonUp(nFlags, point);
	
} // END OF	FUNCTION CFloatingTaskBar::OnRButtonUp()

void CFloatingTaskBar::OnMove( int x, int y )
{
	CPoint point(x - GetSystemMetrics(SM_CXDLGFRAME),y - GetSystemMetrics(SM_CYCAPTION) -  GetSystemMetrics(SM_CYDLGFRAME));

	// Inform task bar manager of our new position
	((CNetscapeApp *)AfxGetApp())->GetTaskBarMgr().SetLastFloatPos(point);

	CFloatingTaskBarBase::OnMove(x,y);

}

/****************************************************************************
*
*	CFloatingTaskBar::SetMenuState
*
*	PARAMETERS:
*		pMenu	- pointer to popup menu
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Helper function for setting the right mouse popup menu's state.
*
****************************************************************************/

void CFloatingTaskBar::SetMenuState(CMenu * pMenu)
{
	pMenu->CheckMenuItem(ID_TBAR_ALWAYSONTOP,
		m_bOnTop ? MF_CHECKED : MF_UNCHECKED);
	pMenu->ModifyMenu(ID_TBAR_POSITION, MF_BYCOMMAND | MF_STRING, CASTINT(ID_TBAR_POSITION),
                      szLoadString(CASTINT(m_bHorizontal ? IDS_VERTICAL : IDS_HORIZONTAL)) );
	pMenu->ModifyMenu(ID_TBAR_SHOWTEXT, MF_BYCOMMAND | MF_STRING, CASTINT(ID_TBAR_SHOWTEXT),
					  szLoadString(CASTINT(m_bShowText ? IDS_TASKBAR_HIDETEXT : IDS_TASKBAR_SHOWTEXT))); 



} // END OF	FUNCTION CFloatingTaskBar::SetMenuState()

/****************************************************************************
*
*	CFloatingTaskBar::OnAlwaysOnTop
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Command handler for the "Always On Top" menu item.
*
****************************************************************************/

void CFloatingTaskBar::OnAlwaysOnTop() 
{
	if (m_bOnTop)
	{
		m_bOnTop = FALSE;
		SetWindowPos(&wndNoTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}
	else
	{
		m_bOnTop = TRUE;
		SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}
	
	// Inform task bar manager of our new state
	((CNetscapeApp *)AfxGetApp())->GetTaskBarMgr().SetOnTop(m_bOnTop);
					
} // END OF	FUNCTION CFloatingTaskBar::OnAlwaysOnTop()

/****************************************************************************
*
*	CFloatingTaskBar::OnShowText
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Command handler for "Show Text" menu item.
*
****************************************************************************/

void CFloatingTaskBar::OnShowText() 
{
	m_bShowText = !m_bShowText;
	
	// Inform task bar manager of our new state
	((CNetscapeApp *)AfxGetApp())->GetTaskBarMgr().SetSeparateTaskBarStyle(m_bShowText ?  TB_PICTURESANDTEXT : TB_PICTURES);

} // END OF	FUNCTION CFloatingTaskBar::OnShowText()

/****************************************************************************
*
*	CFloatingTaskBar::OnPosition
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Command handler for changing the Horizonta/Vertical menu item.
*
****************************************************************************/

void CFloatingTaskBar::OnPosition() 
{
	m_bHorizontal = !m_bHorizontal;
		
	// We need to set the new button text.
	ChangeButtonText();

	// Inform task bar manager of our new state
	((CNetscapeApp *)AfxGetApp())->GetTaskBarMgr().SetHorizontal(m_bHorizontal);
	
} // END OF	FUNCTION CFloatingTaskBar::OnHorizontal()


void CFloatingTaskBar::OnSysCommand( UINT nID, LPARAM lParam )
{
	if(nID == ID_TBAR_ALWAYSONTOP)
		OnAlwaysOnTop();
	else if(nID == ID_TBAR_POSITION)
		OnPosition();
	else if(nID == ID_TBAR_SHOWTEXT)
		OnShowText();
	else
		CFloatingTaskBarBase::OnSysCommand(nID, lParam);


}

void CFloatingTaskBar::OnInitMenu( CMenu* pMenu )
{

	pMenu->CheckMenuItem(ID_TBAR_ALWAYSONTOP, m_bOnTop ? MF_CHECKED : MF_UNCHECKED);
	pMenu->ModifyMenu(ID_TBAR_POSITION, MF_BYCOMMAND | MF_STRING, CASTUINT(ID_TBAR_POSITION),
                     szLoadString(CASTUINT(m_bHorizontal ? IDS_VERTICAL : IDS_HORIZONTAL)) );
	pMenu->ModifyMenu(ID_TBAR_SHOWTEXT, MF_BYCOMMAND | MF_STRING, CASTUINT(ID_TBAR_SHOWTEXT),
					 szLoadString(CASTUINT(m_bShowText ? IDS_TASKBAR_HIDETEXT : IDS_TASKBAR_SHOWTEXT))); 

}

/****************************************************************************
*
*	Class: CDockedTaskBar
*
*	DESCRIPTION:
*		This derived version of CTaskBar provides a "docked" task bar. It is
*		in the form of a mini child window embedded within its parent
*		(normally a CNetscapeStatusBar).
*
****************************************************************************/

BEGIN_MESSAGE_MAP(CDockedTaskBar, CDockedTaskBarBase)
	//{{AFX_MSG_MAP(CDockedTaskBar)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/****************************************************************************
*
*	CDockedTaskBar::CDockedTaskBar
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		N/A
*
*	DESCRIPTION:
*		Constructor
*
****************************************************************************/

CDockedTaskBar::CDockedTaskBar(int nToolbarStyle)
: CTaskBar(nToolbarStyle)
{
	m_noviceButtonSize.cx = 27;
	m_noviceButtonSize.cy = 25;
	m_advancedButtonSize.cx = 27;
	m_advancedButtonSize.cy = 14;

	m_IconSize.cx = 25;
	m_IconSize.cy = 12;

	m_nDragBarWidth = 11;
	m_nIconSpace = 1;
	m_bShowText = FALSE;

} // END OF	FUNCTION CDockedTaskBar::CDockedTaskBar()

/****************************************************************************
*
*	CDockedTaskBar::~CDockedTaskBar
*
*	PARAMETERS:
*		N/A
*
*	RETURNS:
*		N/A
*
*	DESCRIPTION:
*		Destructor.
*
****************************************************************************/

CDockedTaskBar::~CDockedTaskBar()
{

} // END OF	FUNCTION CDockedTaskBar::~CDockedTaskBar()

/****************************************************************************
*
*	CDockedTaskBar::Create
*
*	PARAMETERS:
*		pParent	- pointer to the window in which the task bar is docked
*
*	RETURNS:
*		TRUE if creation is successful, FALSE if not.
*
*	DESCRIPTION:
*		This function must be called after construction to create the task
*		bar.
*
****************************************************************************/

BOOL CDockedTaskBar::Create(CWnd * pParent)
{
	BOOL bRtn = FALSE;
	
	CBrush brush;

	CString strClass = theApp.NSToolBarClass;
	
	CString strName;
	strName.LoadString(IDS_TBAR_NAME);
	bRtn = CDockedTaskBarBase::CreateEx(0, (const char *)strClass,
		(const char *)strName, WS_CHILD | WS_VISIBLE ,
		0, 0, 0, 0, pParent->GetSafeHwnd(), NULL);
		
	return(bRtn);
	
} // END OF	FUNCTION CDockedTaskBar::Create()

/****************************************************************************
*
*	CDockedTaskBar::DoPaint
*
*	PARAMETERS:
*		dc	- reference to dc for painting
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Virtual handler for the paint message. Here's where we do all our
*		specialized drawing for our task bar.
*
****************************************************************************/

void CDockedTaskBar::DoPaint(CPaintDC & dc)
{
	CRect rcClient;
	GetClientRect(&rcClient);
	
	// Draw a 3-d "grip" for the drag bar
	CRect rcTmp(rcClient);
	CBrush brHilight(::GetSysColor(COLOR_BTNHIGHLIGHT));
	CBrush brShadow(::GetSysColor(COLOR_BTNSHADOW));
	for (int i = 3; i < (rcClient.bottom - 2); i += 3)
	{
		rcTmp.SetRect(2, i, m_nDragBarWidth - 1, i + 1);
		dc.FillRect(rcTmp, &brShadow);
		rcTmp.SetRect(2, i + 1, m_nDragBarWidth - 1, i + 2);
		dc.FillRect(rcTmp, &brHilight);
	}

	// Draw a 3-d border to the right of the drag bar
	rcTmp.SetRect(m_nDragBarWidth, 0, m_nDragBarWidth + 1, rcClient.bottom);
	dc.FillRect(rcTmp, &brShadow);
	rcTmp.OffsetRect(1, 0);
	dc.FillRect(rcTmp, &brHilight);
	
	
	// Now draw our 3-d edge borders
	
	// Left and top edges
	rcTmp.SetRect(rcClient.left, rcClient.top, 1, rcClient.bottom);
	dc.FillRect(rcTmp, &brHilight);
	rcTmp.SetRect(rcClient.left, rcClient.top, rcClient.right, 1);
	dc.FillRect(rcTmp, &brHilight);
	
	// Right and bottom
	rcTmp.SetRect(rcClient.left, rcClient.bottom - 1, rcClient.right,
		rcClient.bottom);
	dc.FillRect(rcTmp, &brShadow);
	rcTmp.SetRect(rcClient.right - 1, rcClient.top, rcClient.right,
		rcClient.bottom);
	dc.FillRect(rcTmp, &brShadow);
	
} // END OF	FUNCTION CDockedTaskBar::DoPaint()

/****************************************************************************
*
*	CDockedTaskBar::OnMouseMove
*
*	PARAMETERS:
*		nFlags	- control key flags
*		point	- cursor coordinate
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		We process this message to allow the user to drag the docked task
*		bar off and "undock" it.
*
****************************************************************************/

void CDockedTaskBar::OnMouseMove(UINT nFlags, CPoint point) 
{
	CDockedTaskBarBase::OnMouseMove(nFlags, point);

	// Are we being dragged?
	if (nFlags & MK_LBUTTON)
	{
		// By the drag bar?
		if (DragBarHitTest(point))
		{
			// Start the drag process
			CRect rc;
			GetWindowRect(&rc);
			CRectTracker Tracker(&rc, CRectTracker::solidLine);
			Tracker.m_sizeMin.cx = rc.Width();
			Tracker.m_sizeMin.cy = rc.Height();
			ClientToScreen(&point);
			if (Tracker.Track(this, point, FALSE, GetDesktopWindow()))
			{
				// User released, so undock.
				rc = Tracker.m_rect;
				ClientToScreen(&rc);
				OnUnDock(rc.TopLeft());
			}
		}
	}
		
} // END OF	FUNCTION CDockedTaskBar::OnMouseMove()

/****************************************************************************
*
*	CDockedTaskBar::OnLButtonUp
*
*	PARAMETERS:
*		nFlags	- control key flags
*		point	- cursor coordinate
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		We process this message so user can un-dock us by clicking on
*		the drag bar.
*
****************************************************************************/

void CDockedTaskBar::OnLButtonUp(UINT nFlags, CPoint point) 
{
	CDockedTaskBarBase::OnLButtonUp(nFlags, point);
	
	if (DragBarHitTest(point))
	{
		OnUnDock();
	}
	
} // END OF	FUNCTION CDockedTaskBar::OnLButtonUp()

/****************************************************************************
*
*	CDockedTaskBar::OnUnDock
*
*	PARAMETERS:
*		ptUL	- upper left corner for new floating task bar, or -1,-1 
*				  for default location.
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This function is called to un-dock the task bar (changed to floating
*		state).
*
****************************************************************************/

void CDockedTaskBar::OnUnDock(CPoint & ptUL /*= (-1,-1)*/)
{
	((CNetscapeApp *)AfxGetApp())->GetTaskBarMgr().OnUnDockTaskBar(ptUL);

} // END OF	FUNCTION CDockedTaskBar::OnUnDock()


/****************************************************************************
*
*	Class: CTaskBarArray
*
*	DESCRIPTION:
*		This is a container class for holding CTaskBar objects.
*
****************************************************************************/

/****************************************************************************
*
*	CTaskBarArray::DeleteAll
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This functions is called to delete all of the CTaskBar objects
*		in the container.
*
****************************************************************************/

void CTaskBarArray::DeleteAll()
{
	for (int i = 0; i < GetSize(); i++)
	{
		CTaskBar * pTaskBar = Get(i);
		if (pTaskBar != NULL)
		{
			pTaskBar->DestroyWindow();
			// no need to call delete - pTaskBar has auto cleanup
		}
	}
	RemoveAll();

} // END OF	FUNCTION CTaskBarArray::DeleteAll()

/****************************************************************************
*
*	CTaskBarArray::Find
*
*	PARAMETERS:
*		pTaskBar	- pointer to object to located
*
*	RETURNS:
*		Index of the object if found, -1 if not.
*
*	DESCRIPTION:
*		This function is called to search the list for a given object.
*
****************************************************************************/

int CTaskBarArray::Find(CTaskBar * pTaskBar)
{
	int nRtn = -1;
	
	BOOL bFound = FALSE;
	for (int i = 0; (i < GetSize()) && !bFound; i++)
	{
		if (Get(i) == pTaskBar)
		{
			nRtn = i;
			bFound = TRUE;
		}
	}
	
	return(nRtn);

} // END OF	FUNCTION CTaskBarArray::Find()


/****************************************************************************
*
*	Class: CTaskBarMgr
*
*	DESCRIPTION:
*		This class provides an object for managing all task bars within
*		the system. It maintains the abstract data for all active task icons
*		and handles the generation and switching between floating, docked or
*		other style task bars. All task bar operations should be piped though
*		this object so it can handle propagation to the appropriate active
*		task bar(s).
*
*		There are also some convenience functions available, for such actions
*		as adding a common set of task icons.
*
****************************************************************************/

/****************************************************************************
*
*	CTaskBarMgr::CTaskBarMgr
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		N/A
*
*	DESCRIPTION:
*		Constructor.
*
****************************************************************************/

CTaskBarMgr::CTaskBarMgr()
{
	m_bInitialized = FALSE;
	m_ptLastFloatPos = CPoint(-1, -1);
	m_dwStateFlags = 0;
	m_bSeparateTaskBarStyle = FALSE;
	m_nReference = 0;

} // END OF	FUNCTION CTaskBarMgr::CTaskBarMgr()

/****************************************************************************
*
*	CTaskBarMgr::~CTaskBarMgr
*
*	PARAMETERS:
*		N/A
*
*	RETURNS:
*		N/A
*
*	DESCRIPTION:
*		Destructor.
*
****************************************************************************/

CTaskBarMgr::~CTaskBarMgr()
{

} // END OF	FUNCTION CTaskBarMgr::~CTaskBarMgr()

/****************************************************************************
*
*	CTaskBarMgr::Init
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		TRUE if initialization is successful
*
*	DESCRIPTION:
*		This function must be called after construction to initialize the
*		task bar manager.
*
****************************************************************************/

BOOL CTaskBarMgr::Init()
{
	// Don't allow multiple initialization
	BOOL bRtn = !m_bInitialized;
	ASSERT(bRtn);
	if (bRtn)
	{
		m_bInitialized = TRUE;
		bRtn = CreateAllTaskBars();
	}
	
	return(bRtn);
	
} // END OF	FUNCTION CTaskBarMgr::Init()

void CTaskBarMgr::LoadPrefs(BOOL bAlwaysDock)
{
	CPoint ptTB;
	int32 x, y, nButtonStyle;
	PREF_GetIntPref("taskbar.x", &x);
	PREF_GetIntPref("taskbar.y", &y);
#ifdef XP_WIN32
	ptTB.x =x;
	ptTB.y =y;
#else
	ptTB.x =CASTINT(x);
	ptTB.y =CASTINT(y);
#endif
	SetLastFloatPos(ptTB);
	BOOL bFloating, bHorizontal, bOnTop;

	if(bAlwaysDock)
		bFloating = FALSE;
	else
		PREF_GetBoolPref("taskbar.floating", &bFloating);

	PREF_GetBoolPref("taskbar.horizontal", &bHorizontal);
	PREF_GetBoolPref("taskbar.ontop", &bOnTop);
    DWORD dwStates = 0;
	dwStates = dwStates | (bFloating ? TBAR_FLOATING : 0) | (bHorizontal ? TBAR_HORIZONTAL : 0) |
		(bOnTop ? TBAR_ONTOP : 0);
	SetStateFlags(dwStates);

	PREF_GetIntPref("taskbar.button_style", &nButtonStyle);
	if(nButtonStyle == -1)
		m_nTaskBarStyle = theApp.m_pToolbarStyle;
	else
	{
		m_nTaskBarStyle = CASTINT(nButtonStyle);
		m_bSeparateTaskBarStyle = TRUE;
	}

	Init();
	AddStandardIcons();
}

void CTaskBarMgr::SavePrefs(void)
{
	DWORD dwState = GetStateFlags();
	PREF_SetBoolPref("taskbar.floating", IsFloating());
	PREF_SetBoolPref("taskbar.horizontal", IsHorizontal());
	PREF_SetBoolPref("taskbar.ontop", IsOnTop());
	CPoint ptTB =GetLastFloatPos();
	PREF_SetIntPref("taskbar.x", ptTB.x);
	PREF_SetIntPref("taskbar.y", ptTB.y);

	if(m_bSeparateTaskBarStyle)
		PREF_SetIntPref("taskbar.button_style", m_nTaskBarStyle);
}

/**********************************************************************************************
*
*	CTaskBarMgr::AllTaskBars
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		TRUE if successful, FALSE if not.
*
*	DESCRIPTION:
*		Protected helper function for generating all task bars (either one
*		floating or any number of docked) from our abstract icon data list.
*
****************************************************************************/

BOOL CTaskBarMgr::CreateAllTaskBars()
{
	BOOL bRtn = FALSE;
	ASSERT(m_bInitialized);
	
	// Should be starting clean, or call DestroyAllTaskBars() first
	ASSERT(m_TaskBarList.GetSize() == 0);
	
	if (IsFloating())
	{
		// Create a single floating task bar
		bRtn = CreateFloatingTaskBar();
	}
	else
	{
		// Create docked task bars for each status bar
		for (int i = 0; i < m_StatBarList.GetSize(); i++)
		{
			CreateDockedTaskBar((CNetscapeStatusBar *)(m_StatBarList.GetAt(i)));
		}
	}
	
	return(bRtn);

} // END OF	FUNCTION CTaskBarMgr::CreateAllTaskBars()

/****************************************************************************
*
*	CTaskBarMgr::DestroyAllTaskBars
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Protected helper function for destroying all active task bars,
*		floating or docked.
*
****************************************************************************/

void CTaskBarMgr::DestroyAllTaskBars()
{
	m_TaskBarList.DeleteAll();

	// Must return the status bar pane to 0 size
	for (int i = 0; i < m_StatBarList.GetSize(); i++)
	{
		CNetscapeStatusBar * pSB =
			(CNetscapeStatusBar *)m_StatBarList.GetAt(i);
        if( !pSB )
        { 
            continue;
        }
    	pSB->SetTaskBarPaneWidth( 0 );
	}
				
	m_StatBarMap.RemoveAll();
	
} // END OF	FUNCTION CTaskBarMgr::DestroyAllTaskBars()

/****************************************************************************
*
*	CTaskBarMgr::RegisterStatusBar
*
*	PARAMETERS:
*		pStatBar	- pointer to the status bar being registered
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This function should be called whenever a new status bar is	created
*		and it is desirable to have task bars docked within it. The new
*		status bar is added to the list maintained by the task bar manager -
*		UnRegisterStatusBar() should be called when the status bar is
*		destroyed or no longer wants to have task bars docked within it.
*
****************************************************************************/

void CTaskBarMgr::RegisterStatusBar(CNetscapeStatusBar * pStatBar)
{

	
	
	m_StatBarList.Add(pStatBar);
	
	// It's perfectly OK for frame windows to register their status bars
	// with us before we're initialized, but we don't create a task bar for
	// it unless we are (and if we're also in the docked state).
	if (m_bInitialized && !IsFloating())
	{
		CreateDockedTaskBar(pStatBar);
	}

} // END OF	FUNCTION CTaskBarMgr::RegisterStatusBar()

/****************************************************************************
*
*	CTaskBarMgr::UnRegisterStatusBar
*
*	PARAMETERS:
*		pStatBar	- pointer to the status bar being unregistered
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This function should be called whenever a status bar is destroyed or
*		no longer wants to have task bars docked within it. It removes it
*		from the list maintained by the task bar manager.
*
****************************************************************************/

void CTaskBarMgr::UnRegisterStatusBar(CNetscapeStatusBar * pStatBar)
{
	// Find the given status bar in our list
	BOOL bDone = FALSE;
	for (int i = 0; (i < m_StatBarList.GetSize()) && !bDone; i++)
	{
		CNetscapeStatusBar * pSB =
			(CNetscapeStatusBar *)m_StatBarList.GetAt(i);
		if (pSB == pStatBar)
		{
			// Found it, so take it out of our list
			bDone = TRUE;
			m_StatBarList.RemoveAt(i);
			
			// If there's a task bar embedded in this status bar, it will
			// be destroyed automatically, so don't worry about deleting
			// the object. However, we do need to eliminate it from our
			// list of active task bars, and break the association in the map.
			CTaskBar * pTB = NULL;
			if (m_StatBarMap.Lookup(pStatBar, (void * &)pTB))
			{
				// Remove map entry
				m_StatBarMap.RemoveKey(pStatBar);
				
				// Now remove the task bar from the list
				int i = m_TaskBarList.Find(pTB);
				if (i > -1)
				{
					m_TaskBarList.RemoveAt(i);
				}
			}
		}
	}

} // END OF	FUNCTION CTaskBarMgr::UnRegisterStatusBar()

/****************************************************************************
*
*	CTaskBarMgr::OnSizeStatusBar
*
*	PARAMETERS:
*		pStatBar	- pointer to the status bar whose size has changed
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This function is called by a registered status bar when it has been
*		resized. Since we have to be in control of all task bars, we're
*		responsible of repositioning any that are docked on the given status
*		bar.
*
****************************************************************************/

void CTaskBarMgr::OnSizeStatusBar(CNetscapeStatusBar * pStatBar)
{
	if (!IsFloating())
	{
		CDockedTaskBar * pTB = NULL;
		if (m_StatBarMap.Lookup(pStatBar, (void * &)pTB))
		{
			AdjustStatusPane(pTB, pStatBar);
			PlaceOnStatusBar(pTB, pStatBar);
		}
	}

} // END OF	FUNCTION CTaskBarMgr::OnSizeStatusBar()

/****************************************************************************
*
*	CTaskBarMgr::CreateDockedTaskBar
*
*	PARAMETERS:
*		pStatBar	- pointer to status bar that houses the task bar
*
*	RETURNS:
*		TRUE if successful, FALSE if not.
*
*	DESCRIPTION:
*		Protected helper function for specifically creating a docked task bar.
*
****************************************************************************/

BOOL CTaskBarMgr::CreateDockedTaskBar(CNetscapeStatusBar * pStatBar)
{
	BOOL bRtn = ((pStatBar != NULL) && ::IsWindow(pStatBar->GetSafeHwnd()));

	if (bRtn)
	{
		CDockedTaskBar * pTB = new CDockedTaskBar(TB_PICTURES);
		ASSERT(pTB != NULL);
		if (pTB != NULL)
		{
			// Create, add to active list and the map
			bRtn = pTB->Create(pStatBar);
			m_TaskBarList.Add(pTB);
			m_StatBarMap.SetAt(pStatBar, pTB);
			
			// Add the icons
			AddIconsToTaskBar(pTB);
			
			// Embed the thing in the status bar. This consists of changing
			// the pane info to the correct width and moving the task bar
			// to that location.
			AdjustStatusPane(pTB, pStatBar);
			PlaceOnStatusBar(pTB, pStatBar);
		}
	}	
	
	return(bRtn);
	
} // END OF	FUNCTION CTaskBarMgr::CreateDockedTaskBar()

/****************************************************************************
*
*	CTaskBarMgr::AdjustStatusPane
*
*	PARAMETERS:
*		pTaskBar	- pointer to task bar
*		pStatBar	- status bar that houses it
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Protected helper function for adjusting the status pane so that
*		the docked task bar will fit within it.
*
****************************************************************************/

void CTaskBarMgr::AdjustStatusPane(CDockedTaskBar * pTaskBar,
	CNetscapeStatusBar * pStatBar)
{
	CSize sizeTB = pTaskBar->CalcDesiredDim();
    pStatBar->SetTaskBarPaneWidth( sizeTB.cx );
} // END OF	FUNCTION CTaskBarMgr::AdjustStatusPane()

/****************************************************************************
*
*	CTaskBarMgr::PlaceOnStatusBar
*
*	PARAMETERS:
*		pTaskBar	- pointer to task bar to be positioned
*		pStatBar	- status bar that houses it
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This protected helper function is called to place a docked task bar
*		in the correct pane of a status bar. AdjustStatusPane() should have
*		been called at some prior time to set the pane to the proper width.
*
****************************************************************************/

void CTaskBarMgr::PlaceOnStatusBar(CDockedTaskBar * pTaskBar,
	CNetscapeStatusBar * pStatBar)
{
	pStatBar->SetTaskBarSize( pTaskBar );
} // END OF	FUNCTION CTaskBarMgr::PlaceOnStatusBar()

/****************************************************************************
*
*	CTaskBarMgr::PositionDockedTaskBars
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Protected helper function called to position all task bars that are
*		docked within status bars. This is needed after icons are added or
*		removed dynamically. First, the status bar pane is set to the desired
*		width, then the task bar is positioned within it.
*
****************************************************************************/

void CTaskBarMgr::PositionDockedTaskBars()
{
	if (!IsFloating())
	{
		for (int i = 0; i < m_StatBarList.GetSize(); i++)
		{
			CNetscapeStatusBar * pSB =
				(CNetscapeStatusBar *)m_StatBarList.GetAt(i);
			CDockedTaskBar * pTB = NULL;
			if (m_StatBarMap.Lookup(pSB, (void * &)pTB))
			{
				AdjustStatusPane(pTB, pSB);
				PlaceOnStatusBar(pTB, pSB);
			}
		}
	}

} // END OF	FUNCTION CTaskBarMgr::PositionDockedTaskBars()

/****************************************************************************
*
*	CTaskBarMgr::PositionFloatingTaskBar
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Protected helper function called to position the floating task bar.
*
****************************************************************************/

void CTaskBarMgr::PositionFloatingTaskBar()
{
	if (IsFloating())
	{
		// First, see how big it wants to be
		CTaskBar * pTB = m_TaskBarList.Get(0);
		ASSERT(pTB != NULL);
		CSize sizeTB = pTB->CalcDesiredDim();
		
		// For the upper left corner, we first try using the last known
		// coordinates, and if that fails, use the top right of the screen.
		int nX = 0, nY = 0;
		if (m_ptLastFloatPos != CPoint(-1, -1))
		{
			nX = m_ptLastFloatPos.x;
			nY = m_ptLastFloatPos.y;
		}
		else
		{
			CGenericFrame * pFrame = (CGenericFrame * )FEU_GetLastActiveFrame();
			ASSERT(pFrame != NULL);
			if (pFrame != NULL)
			{
				RECT rect;
				pFrame->GetWindowRect(&rect);
						
				int nCaptionHeight = ::GetSystemMetrics(SM_CYCAPTION);
	#ifdef WIN32
				int nBorderHeight = ::GetSystemMetrics(SM_CYSIZEFRAME);
	#else
				int nBorderHeight = ::GetSystemMetrics(SM_CYFRAME);
	#endif

				int nCaptionButtonWidth = ::GetSystemMetrics(SM_CXSIZE);
				CSize taskbarButtonSize = pTB->GetButtonDimensions();

				if(IsHorizontal())
				{

					nX = (int) (rect.right - sizeTB.cx - (( 4.5 * nCaptionButtonWidth )));
					nY = rect.top + nCaptionHeight + nBorderHeight + 2 - sizeTB.cy;
				}
				else
				{
					nX = (int) (rect.right + 2);
					nY = rect.top + nCaptionHeight + nBorderHeight;
				}
			}
				
			// DON'T set m_ptLastFloatPos here - leave it set to -1,-1 so
			// we default each time, until the user has moved the task bar
			// and it is set properly.
		}
		
		// make it repaint itself since it might have to repaint the drag bars.
		pTB->Invalidate();
		// Reposition the window
		// make sure it doesn't go off the screen
		int nScreenHeight = ::GetSystemMetrics(SM_CYFULLSCREEN);
		int nScreenWidth = ::GetSystemMetrics(SM_CXFULLSCREEN);

		if(nY > nScreenHeight + sizeTB.cy)
			nY = 0;
		else if(nY + sizeTB.cy > nScreenHeight)
			nY = nScreenHeight - sizeTB.cy;
		else if(nY < 0)
			nY = 0;

		if(nX > nScreenWidth + sizeTB.cx)
			nX = 0;
		else if(nX + sizeTB.cx > nScreenWidth)
			nX = nScreenWidth - sizeTB.cx;
		else if(nX < 0)
			nX = 0;


		pTB->MoveWindow(nX, nY, sizeTB.cx, sizeTB.cy);
	}

} // END OF	FUNCTION CTaskBarMgr::PositionFloatingTaskBar()

/****************************************************************************
*
*	CTaskBarMgr::CreateFloatingTaskBar
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		TRUE if successful, FALSE if not.
*
*	DESCRIPTION:
*		Protected helper function for specifically creating a floating task
*		bar.
*
****************************************************************************/

BOOL CTaskBarMgr::CreateFloatingTaskBar()
{
	BOOL bRtn = FALSE;

	CPoint oldPoint = m_ptLastFloatPos;

	CFloatingTaskBar * pTB = new CFloatingTaskBar(m_nTaskBarStyle, IsOnTop(), IsHorizontal());
	ASSERT(pTB != NULL);
	if (pTB != NULL)
	{
		bRtn = pTB->Create(AfxGetMainWnd());
		
		if (bRtn)
		{
			
			if(m_nReference > 0)
				pTB->ShowWindow(SW_SHOWNA);
			// Put it in our active list and fill it with existing icons
			m_TaskBarList.Add(pTB);
			AddIconsToTaskBar(pTB);
			m_ptLastFloatPos = oldPoint;
			PositionFloatingTaskBar();
		}
	}
	
	return(bRtn);
	
} // END OF	FUNCTION CTaskBarMgr::CreateFloatingTaskBar()

void CTaskBarMgr::ReloadIconBitmaps(CTaskBar *pTaskBar)
{

	HBITMAP hBitmap = NULL;
	int nSize = m_IconList.GetSize();

	int i;

	// First remove all of the bitmaps from the map.
	for(i = 0; i < nSize; i++)
	{
		CTaskIcon * pIcon = m_IconList.Get(i);
		int idBmp = pIcon->GetLargeBmpID();

		WFE_RemoveBitmapFromMap(idBmp);
			
		idBmp = pIcon->GetSmallBmpID();

		WFE_RemoveBitmapFromMap(idBmp);
	}

	// Now replace the bitmaps
	for(i = 0; i < nSize; i++)
	{
		CTaskIcon * pIcon = m_IconList.Get(i);
		int idBmp = IsFloating() ? pIcon->GetLargeBmpID() :
			pIcon->GetSmallBmpID();

		HDC hDC = GetDC(pTaskBar->m_hWnd);
		hBitmap = WFE_LookupLoadAndEnterBitmap(hDC, idBmp, TRUE, 
											WFE_GetUIPalette(pTaskBar->GetParentFrame()), 
											   GetSysColor(COLOR_BTNFACE), RGB(255, 0, 255));
		ReleaseDC(pTaskBar->m_hWnd, hDC);

		pTaskBar->ReplaceButtonBitmap(i, hBitmap);
	}

}

void CTaskBarMgr::Reference(BOOL bAdd)
{
	CFloatingTaskBar *pTB = NULL;

	if(IsFloating())
		pTB = (CFloatingTaskBar*)m_TaskBarList.Get(0);

	if(bAdd)
	{
		if(pTB && m_nReference == 0)
			pTB->ShowWindow(SW_SHOW);
		m_nReference++;
	}
	else
	{
		if(pTB && m_nReference == 1)
			pTB->ShowWindow(SW_HIDE);
		m_nReference--;
	}

}

void CTaskBarMgr::ChangeTaskBarsPalette(HWND hFocus)
{
	if(m_bInitialized)
	{
		int nSize = m_TaskBarList.GetSize();

		for (int i = 0; i < nSize; i++)
		{
			CTaskBar * pTB = m_TaskBarList.Get(i);

			pTB->SendMessage(WM_PALETTECHANGED, (WPARAM)hFocus, 0);

		}
	}
}

/****************************************************************************
*
*	CTaskBarMgr::AddIconsToTaskBar
*
*	PARAMETERS:
*		pTaskBar	- pointer to the task bar to be filled
*
*	RETURNS:
*		TRUE if successful, FALSE if a failure occurs.
*
*	DESCRIPTION:
*		Protected helper function for adding all of the icons in our list
*		to a given (presumably newly created) task bar.
*
****************************************************************************/

BOOL CTaskBarMgr::AddIconsToTaskBar(CTaskBar * pTaskBar)
{

	BOOL bRtn = TRUE;
	HBITMAP hBitmap = NULL;

	for (int i = 0; (i < m_IconList.GetSize()) && bRtn; i++)
	{
		CTaskIcon * pIcon = m_IconList.Get(i);
		int idBmp = IsFloating() ? pIcon->GetLargeBmpID() :
			pIcon->GetSmallBmpID();

		int idTip = IsFloating() ? pIcon->GetFloatingTipID() :
		    pIcon->GetDockedTipID();

		HDC hDC = GetDC(pTaskBar->m_hWnd);
		hBitmap = WFE_LookupLoadAndEnterBitmap(hDC, idBmp, TRUE, WFE_GetUIPalette(pTaskBar->GetParentFrame()), 
											   sysInfo.m_clrBtnFace, RGB(255, 0, 255));
		ReleaseDC(pTaskBar->m_hWnd, hDC);

		int nBitmapIndex = IsFloating() ? pIcon->GetLargeBitmapIndex() :
			pIcon->GetSmallBitmapIndex();
		
		int idText = IsHorizontal() ? pIcon->GetHorizTextID() : pIcon->GetVertTextID();
			
		bRtn = pTaskBar->AddTaskIcon(pIcon->GetTaskID(), pIcon->GetNotifyWnd(),
			pIcon->GetNotifyMessage(), hBitmap, nBitmapIndex, pIcon->GetHorizTextID(), 
			pIcon->GetVertTextID(), idText, idTip, 
			pTaskBar->GetTaskBarStyle());
	}
	
	return(bRtn);

} // END OF	FUNCTION CTaskBarMgr::AddIconsToTaskBar()

/****************************************************************************
*
*	CTaskBarMgr::AddStandardIcons
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		TRUE if successful, FALSE if not.
*
*	DESCRIPTION:
*		This function call be called to add the three default icons (Mail,
*		Web & News) to our icon list, and therefore to all active task bars.
*
****************************************************************************/

BOOL CTaskBarMgr::AddStandardIcons()
{
	BOOL bRtn = TRUE;
	
	bRtn = 	AddTaskIcon(ID_TOOLS_WEB, IDB_TASKBARL, BROWSER_ICON_INDEX, IDB_TASKBARS,
						BROWSER_ICON_INDEX,	IDS_NOTIFY_WEB_TIP, IDS_NOTIFY_WEB_TIP, 
						IDS_NOTIFY_WEB_TIP, IDS_TASKBAR_FLOAT_NAVTIP) &&
			AddTaskIcon(ID_TOOLS_INBOX, IDB_TASKBARL, UNKNOWN_MAIL_ICON_INDEX, IDB_TASKBARS,
						UNKNOWN_MAIL_ICON_INDEX, IDS_NOTIFY_MAIL1_TIP, IDS_NOTIFY_MAIL1_TIP,
						IDS_NOTIFY_MAIL1_TIP, IDS_TASKBAR_FLOAT_MAILTIP) &&
			AddTaskIcon(ID_TOOLS_NEWS, IDB_TASKBARL, NEWS_ICON_INDEX, IDB_TASKBARS,
						NEWS_ICON_INDEX, IDS_TASKBAR_NEWS_HTEXT, IDS_TASKBAR_NEWS_HTEXT, 
						IDS_NOTIFY_NEWS_TIP, IDS_TASKBAR_FLOAT_NEWSTIP) &&
			AddTaskIcon(ID_TOOLS_EDITOR, IDB_TASKBARL, COMPOSE_ICON_INDEX, IDB_TASKBARS,
						COMPOSE_ICON_INDEX, IDS_NOTIFY_COMPOSE_TIP, IDS_NOTIFY_COMPOSE_TIP, 
						IDS_NOTIFY_COMPOSE_TIP, IDS_TASKBAR_FLOAT_COMPOSETIP);
	
	return(bRtn);

} // END OF	FUNCTION CTaskBarMgr::AddStandardIcons

/****************************************************************************
*
*	CTaskBarMgr::AddTaskIcon
*
*	PARAMETERS:
*		idTask		- task identifier
*		idBmpLarge	- bitmap resource ID for large icon (floating)
*		idBmpSmall	- bitmap resource ID for small icon (docked)
*		idTip		- string resource ID of tool tip text
*
*	RETURNS:
*		TRUE if successful, FALSE if not.
*
*	DESCRIPTION:
*		This function is called to add a task icon to the list maintained
*		by the task bar manager. The appropriate icon window will then be
*		created and added to any active task bars, floating or docked.
*
*		Notifications (mouse events) for the new icon are sent to the default
*		window (main frame window) using the default message (MSG_TASK_NOTIFY).
*		To specify a different handler, call the overloaded version of this
*		function that takes	those parameters.
*
****************************************************************************/

BOOL CTaskBarMgr::AddTaskIcon(UINT idTask, UINT idBmpLarge, int nLargeIndex, UINT idBmpSmall,
							  int nSmallIndex,	UINT idHorizText, UINT idVertText,
							  UINT idDockedTip, UINT idFloatingTip)
{
	return(AddTaskIcon(idTask, AfxGetMainWnd(), MSG_TASK_NOTIFY, idBmpLarge, nLargeIndex,
		idBmpSmall, nSmallIndex, idHorizText, idVertText, idDockedTip, idFloatingTip));

} // END OF	FUNCTION CTaskBarMgr::AddTaskIcon()

/****************************************************************************
*
*	CTaskBarMgr::AddTaskIcon
*
*	PARAMETERS:
*		idTask		- task identifier
*		pwndNotify	- pointer to window desiring notification
*		dwMessage	- user-defined callback message
*		idBmpLarge	- bitmap resource ID for large icon (floating)
*		idBmpSmall	- bitmap resource ID for small icon (docked)
*		idHorizText - string resource ID for horizontal text
*		idVertText  - string resource ID for vertical text
*		idTip		- string resource ID of tool tip text
*
*	RETURNS:
*		TRUE if successful, FALSE if not.
*
*	DESCRIPTION:
*		This function is called to add a task icon to the list maintained
*		by the task bar manager. The appropriate icon window will then be
*		created and added to any active task bars, floating or docked.
*
*		Notifications (mouse events) for the new icon are sent to the window
*		identified by pwndNotify, using the message dwMessage. For the default
*		message and handlers, call the overloaded version of this function
*		that omits those parameters.
*
****************************************************************************/

BOOL CTaskBarMgr::AddTaskIcon(UINT idTask, CWnd * pwndNotify, DWORD dwMessage,
	UINT idBmpLarge, int nLargeIndex, UINT idBmpSmall, int nSmallIndex, 
	UINT idHorizText, UINT idVertText, UINT idDockedTip, UINT idFloatingTip)
{
	BOOL bRtn = TRUE;
	HBITMAP hBitmap;

	// First, create a new icon object and add to our list.
	CTaskIcon * pIcon = new CTaskIcon(idTask, pwndNotify, dwMessage,
		idBmpLarge, nLargeIndex,  idBmpSmall, nSmallIndex, idHorizText,
		idVertText, idDockedTip, idFloatingTip);
	ASSERT(pIcon != NULL);		
	m_IconList.Add(pIcon);
	
	// Now add the icon to any existing task bars
	int idBmp = IsFloating() ? idBmpLarge : idBmpSmall;
	int idTip = IsFloating() ? idFloatingTip : idDockedTip;
	int idText = IsHorizontal() ? idHorizText : idVertText;

	int nBitmapIndex = IsFloating() ? nLargeIndex : nSmallIndex;


	for (int i = 0; (i < m_TaskBarList.GetSize()) && bRtn; i++)
	{
		CTaskBar * pTB = m_TaskBarList.Get(i);

		HDC hDC = GetDC(pTB->m_hWnd);
		hBitmap = WFE_LookupLoadAndEnterBitmap(hDC, idBmp, TRUE, WFE_GetUIPalette(pTB->GetParentFrame()), sysInfo.m_clrBtnFace,
											   RGB(255, 0, 255));

		ReleaseDC(pTB->m_hWnd, hDC);
		
		bRtn = pTB->AddTaskIcon(idTask, pwndNotify, dwMessage, hBitmap, nBitmapIndex,
			idHorizText, idVertText, idText, idTip, IsFloating()? pTB->GetTaskBarStyle(): TB_PICTURES);
	}

	// Finally, since the size of the task bar has changed, we must
	// re-position all.
	if (IsFloating())
	{
		PositionFloatingTaskBar();
	}
	else
	{
		PositionDockedTaskBars();
	}
	
	return(bRtn);

} // END OF	FUNCTION CTaskBarMgr::AddTaskIcon()

/****************************************************************************
*
*	CTaskBarMgr::ReplaceTaskIcon
*
*	PARAMETERS:
*		idTask		- task identifier
*		idBmpLarge	- new bitmap resource ID for large icon (floating)
*		idBmpSmall	- new bitmap resource ID for small icon (docked)
*
*	RETURNS:
*		TRUE if the icon was successfully replaced, FALSE if not (such as
*		when no existing icon with the given ID is found).
*
*	DESCRIPTION:
*		This function is called to replace an existing icon in the task bar
*		whenever it is desirable to show a different image, but retain the
*		notification handler and other information for the icon. An example
*		of this would be changing the mail icon to show a different state.
*
****************************************************************************/

BOOL CTaskBarMgr::ReplaceTaskIcon(UINT idTask, UINT idBmpLarge, UINT idBmpSmall, int nIndex)
{
	BOOL bRtn = FALSE;

	if(!m_bInitialized)
		return FALSE;

	// Replace the bmps for the abstract icon object in our list
	int i = m_IconList.FindByID(idTask);
	ASSERT(i > -1);
	if (i > -1)
	{
		bRtn = TRUE;
		
		CTaskIcon * pIcon = m_IconList.Get(i);
		pIcon->SetLargeBmpID(idBmpLarge);
		pIcon->SetSmallBmpID(idBmpSmall);
		//take this out eventually and replace with passed in parameters
		pIcon->SetLargeBitmapIndex(nIndex);
		pIcon->SetSmallBitmapIndex(nIndex);
	
	
		// Now tell all active task bars to replace it
		for (i = 0; i < m_TaskBarList.GetSize(); i++)
		{
			CTaskBar * pTB = m_TaskBarList.Get(i);
			int idBmp = IsFloating() ? idBmpLarge : idBmpSmall;
			BOOL bOk = pTB->ReplaceTaskIcon(idTask, idBmp, nIndex);
			ASSERT(bOk);
		}
	}	
	return(bRtn);

} // END OF	FUNCTION CTaskBarMgr::ReplaceTaskIcon()

/****************************************************************************
*
*	CTaskBarMgr::RemoveTaskIcon
*
*	PARAMETERS:
*		idTask	- task identifier
*
*	RETURNS:
*		TRUE if the icon was successfully removed from the task bar, FALSE
*		if not (such as when no existing icon with the given ID is found).
*
*	DESCRIPTION:
*		This function is called to remove an icon from the task bar when it
*		is no longer desirable to have it displayed. This is the only case
*		where this function is useful, since cleanup is performed
*		automatically when the task bar is destroyed.
*
****************************************************************************/

BOOL CTaskBarMgr::RemoveTaskIcon(UINT idTask)
{
	BOOL bRtn = FALSE;

	// Remove the abstract icon object from our list
	int i = m_IconList.FindByID(idTask);
	ASSERT(i > -1);
	if (i > -1)
	{
		bRtn = TRUE;
		
		CTaskIcon * pIcon = m_IconList.Get(i);
		delete pIcon;
		m_IconList.RemoveAt(i);
	}
	
	// Just tell all active task bars to remove it
	for (i = 0; i < m_TaskBarList.GetSize(); i++)
	{
		CTaskBar * pTB = m_TaskBarList.Get(i);
		BOOL bOk = pTB->RemoveTaskIcon(idTask);
		ASSERT(bOk);
	}
		
	// Re-size/re-position the task bar(s)
	if (IsFloating())
	{
		PositionFloatingTaskBar();
	}
	else
	{
		PositionDockedTaskBars();
	}
	
	return(bRtn);

} // END OF	FUNCTION CTaskBarMgr::RemoveTaskIcon()

/****************************************************************************
*
*	CTaskBarMgr::OnDockTaskBar
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This function is called to dock the task bar.
*
****************************************************************************/

void CTaskBarMgr::OnDockTaskBar()
{
	ASSERT(IsFloating());
	DestroyAllTaskBars();
	SetFloating(FALSE);
	CreateAllTaskBars();

} // END OF	FUNCTION CTaskBarMgr::OnDockTaskBar()

/****************************************************************************
*
*	CTaskBarMgr::OnUnDockTaskBar
*
*	PARAMETERS:
*		ptUL	- upper left corner for new floating task bar, or -1,-1 
*				  for default location.
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		This function is called to un-dock the task bar (move to floating
*		state).
*
****************************************************************************/

void CTaskBarMgr::OnUnDockTaskBar(CPoint & ptUL /*= (-1,-1)*/)
{
	ASSERT(!IsFloating());
	DestroyAllTaskBars();
	SetFloating(TRUE);
	if (ptUL != CPoint(-1, -1))
	{
		m_ptLastFloatPos = ptUL;
	}
	CreateAllTaskBars();

} // END OF	FUNCTION CTaskBarMgr::OnUnDockTaskBar()


void CTaskBarMgr::SetSeparateTaskBarStyle(int nTaskBarStyle)
{
	m_bSeparateTaskBarStyle = TRUE;

	ChangeTaskBarStyle(nTaskBarStyle);

}

void CTaskBarMgr::SetTaskBarStyle(int nTaskBarStyle)
{
	if(!m_bSeparateTaskBarStyle)
	{
		ChangeTaskBarStyle(nTaskBarStyle);
	}
}

void CTaskBarMgr::ChangeTaskBarStyle(int nTaskBarStyle)
{
	m_nTaskBarStyle = nTaskBarStyle;
	if(IsFloating())
	{
		CTaskBar * pTB = m_TaskBarList.Get(0);
		pTB->SetTaskBarStyle(nTaskBarStyle);
		PositionFloatingTaskBar();
	}
}

/****************************************************************************
*
*	Class: CDockButton
*
*	DESCRIPTION:
*		This class represents the docking (minimize) button for the floating
*		task bar.
*
****************************************************************************/

/****************************************************************************
*
*	CONSTANTS
*
****************************************************************************/
static const int nIMG_WIDTH = 9;
static const int nIMG_HEIGHT = 9;

BEGIN_MESSAGE_MAP(CDockButton, CDockButtonBase)
	//{{AFX_MSG_MAP(CDockButton)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/****************************************************************************
*
*	CDockButton::CDockButton
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		N/A
*
*	DESCRIPTION:
*		Constructor.
*
****************************************************************************/

CDockButton::CDockButton()
{

} // END OF	FUNCTION CDockButton::CDockButton()

/****************************************************************************
*
*	CDockButton::Create
*
*	PARAMETERS:
*		rect		- rectangle for initial size/position
*		pwndParent	- pointer to parent window
*		uID			- control identifier
*
*	RETURNS:
*		TRUE if creation is successful, FALSE if an error occurs.
*
*	DESCRIPTION:
*		This function is called to create the button, after construction.
*
****************************************************************************/

BOOL CDockButton::Create(const CRect & rect, CWnd* pwndParent, UINT uID)
{
	BOOL bRtn = CDockButtonBase::Create("", WS_CHILD | WS_VISIBLE |
		BS_OWNERDRAW, rect, pwndParent, uID);
		
	return(bRtn);

} // END OF	FUNCTION CDockButton::Create()

/****************************************************************************
*
*	CDockButton::DrawItem
*
*	PARAMETERS:
*		None
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Overridden to draw the bitmap image on our button.
*
****************************************************************************/

void CDockButton::DrawItem(LPDRAWITEMSTRUCT lpDrawItem)
{
	ASSERT(lpDrawItem != NULL);

	CDC * pDC = CDC::FromHandle(lpDrawItem->hDC);
	CRect rcBtn(&(lpDrawItem->rcItem));
	
	// Since we're owner-draw, we must do everything. First paint the button
	// face.
	CBrush brFace(RGB(192, 192, 192));
	pDC->FillRect(rcBtn, &brFace);
	
	// Now the 3d border
	if (lpDrawItem->itemState & ODS_SELECTED)
	{
		// Draw depressed look
		DrawDownButton(pDC, rcBtn);
	}
	else
	{
		// Draw normal look
		DrawUpButton(pDC, rcBtn);
	}

	// Finally, paint our image. Shift down one pixel if in depressed state.
	if (lpDrawItem->itemState & ODS_SELECTED)
	{
		rcBtn.left += 1;
		rcBtn.top += 1;
	}
	DrawImage(pDC, rcBtn);
	
} // END OF FUNCTION CDockButton::DrawItem()

/****************************************************************************
*
*	CDockButton::DrawUpButton
*
*	PARAMETERS:
*		pDC		- pointer to DC to draw on
*		rect	- client rect to draw in
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Protected helper function for drawing the 3d "up button" look.
*
****************************************************************************/

void CDockButton::DrawUpButton(CDC * pDC, CRect & rect)
{
	// Hilight
	CRect rc(0, 0, rect.right, 1);
	CBrush br(::GetSysColor(COLOR_BTNHIGHLIGHT));
	pDC->FillRect(rc, &br);
	rc.SetRect(0, 0, 1, rect.bottom);
	pDC->FillRect(rc, &br);
	
	// Shadow
	br.DeleteObject();
	br.CreateSolidBrush(::GetSysColor(COLOR_BTNSHADOW));
	rc.SetRect(0, rect.bottom - 1, rect.right, rect.bottom);
	pDC->FillRect(rc, &br);
	rc.SetRect(rect.right - 1, 0, rect.right, rect.bottom);
	pDC->FillRect(rc, &br);

} // END OF	FUNCTION CDockButton::DrawUpButton()

/****************************************************************************
*
*	CDockButton::DrawDownButton
*
*	PARAMETERS:
*		pDC		- pointer to DC to draw on
*		rect	- client rect to draw in
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Protected helper function for drawing the 3d "down button" look.
*
****************************************************************************/

void CDockButton::DrawDownButton(CDC * pDC, CRect & rect)
{
	// Hilight
	CRect rc(0, rect.bottom - 1, rect.right, rect.bottom);
	CBrush br(::GetSysColor(COLOR_BTNHIGHLIGHT));
	pDC->FillRect(rc, &br);
	rc.SetRect(rect.right - 1, 0, rect.right, rect.bottom);
	pDC->FillRect(rc, &br);
	
	// Shadow
	br.DeleteObject();
	br.CreateSolidBrush(::GetSysColor(COLOR_BTNSHADOW));
	rc.SetRect(0, 0, rect.right, 1);
	pDC->FillRect(rc, &br);
	rc.SetRect(0, 0, 1, rect.bottom);
	pDC->FillRect(rc, &br);

} // END OF	FUNCTION CDockButton::DrawDownButton()

/****************************************************************************
*
*	CDockButton::DrawImage
*
*	PARAMETERS:
*		pDC		- pointer to DC to draw on
*		rect	- client rect to draw in
*
*	RETURNS:
*		void
*
*	DESCRIPTION:
*		Protected helper function for drawing our bitmap image on the button.
*
****************************************************************************/

void CDockButton::DrawImage(CDC * pDC, CRect & rect)
{
	// Create a scratch DC and select our bitmap into it.
	CDC * pBmpDC = new CDC;
	pBmpDC->CreateCompatibleDC(pDC);
	CBitmap bmp;
	bmp.LoadBitmap(IDB_DOCK_BTN);
	CBitmap * pOldBmp = pBmpDC->SelectObject(&bmp);
		
	// Center the image within the button	
	CPoint ptDst(rect.left + (((rect.Width() - nIMG_WIDTH) + 1) / 2),
		rect.top + (((rect.Height() - nIMG_HEIGHT) + 1) / 2));
		
	// Call the handy transparent blit function to paint the bitmap over
	// whatever colors exist.	
	::FEU_TransBlt(pBmpDC, pDC, CPoint(0, 0), ptDst, nIMG_WIDTH, nIMG_HEIGHT,WFE_GetUIPalette(GetParentFrame()));
	
	// Cleanup
	pBmpDC->SelectObject(pOldBmp);
	pBmpDC->DeleteDC();
	delete pBmpDC;
	bmp.DeleteObject();
	
} // END OF	FUNCTION CDockButton::DrawImage()
