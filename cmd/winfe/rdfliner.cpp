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

// RDFLiner Implementation.  Created by Dave Hyatt.

#include "stdafx.h"
#include "mainfrm.h"
#include "hiddenfr.h"
#include "rdf.h"
#include "htrdf.h"
#include "rdfliner.h"
#include "shcut.h"
#include "shcutdlg.h"
#include "prefapi.h"
#include "netsvw.h"
#include "cxsave.h"
#include "fegui.h"
#include "nethelp.h"
#include "time.h"

CMapStringToPtr CHTFEData::m_LocalFileCache(40);
CMapStringToPtr CHTFEData::m_CustomURLCache(20);

extern "C" {
#include "xpgetstr.h"
extern int XP_BKMKS_LESS_THAN_ONE_HOUR_AGO;
extern int XP_BKMKS_DAYS_AGO;
extern int XP_BKMKS_HOURS_AGO;
};

// Some tooltip stuff
#define TIP_WAITING	1
#define TIP_SHOWING	2
#define TIP_SHOWN	3
#define TIP_HEARTBEAT	100
#define TIP_DELAY		250

// Focus/timer stuff for edit wnds
#define IDT_EDITFOCUS 16412
#define IDT_SPRINGLOAD 16413
#define IDT_DRAGRECT 16414

#define EDIT_DELAY 500
#define SPRINGLOAD_DELAY 300
#define RDF_DRAG_HEARTBEAT 125 // Faster than COutliner's DRAG_HEARTBEAT

// Click location stuff
#define CLICKED_BACKGROUND 0
#define CLICKED_TRIGGER 1
#define CLICKED_LINE 2

// Used in painting
#define COL_LEFT_MARGIN	((m_cxChar+1)/2)
#define OUTLINE_TEXT_OFFSET 8

#define HTFE_USE_CUSTOM_IMAGE -1
#define HTFE_LOCAL_FILE -2

#define HT_NO_SORT -1
#define HT_SORT_ASCENDING 1
#define HT_SORT_DESCENDING 0

// Function to make a pretty date string
#define SECONDS_PER_DAY		86400L
void FormatDateTime( CTime &tmDate, CString &csFormatDate )
{
    //
	// Get locale info
    //
	static char cName[] = "intl" ;
	TCHAR szSepDate[2], szSepTime[2];    
	GetProfileString( cName, "sDate", "/", szSepDate, sizeof(szSepDate) );
	GetProfileString( cName, "sTime", ":", szSepTime, sizeof(szSepTime) );    

    TCHAR szAM[9], szPM[9];
    GetProfileString( cName, "s1159", "AM", szAM, sizeof(szAM) );
    GetProfileString( cName, "s2359", "PM", szPM, sizeof(szPM) );    

	int iDate = GetProfileInt( cName, "iDate", 0 );
    int iTime = GetProfileInt( cName, "iTime", 0 );
  
    TCHAR szBuffer[64];
    switch( iDate )
    {
        default:
        case 0: // Month-Day-Year 
        {
            wsprintf( szBuffer, "%d%s%d%s%d", tmDate.GetMonth(), szSepDate, tmDate.GetDay(), szSepDate, tmDate.GetYear() );
            break;
        }
        
        case 1: // Day-Month-Year 
        {
            wsprintf( szBuffer, "%d%s%d%s%d", tmDate.GetDay(), szSepDate, tmDate.GetMonth(), szSepDate, tmDate.GetYear() );
            break;
        }

        case 2: // Year-Month-Day     
        {
            wsprintf( szBuffer, "%d%s%d%s%d", tmDate.GetYear(), szSepDate, tmDate.GetMonth(), szSepDate, tmDate.GetDay() );
            break;
        }
    }
    
    _tcscat( szBuffer, " " );
    
    TCHAR *pszTime = &szBuffer[_tcslen( szBuffer )];
    
    switch( iTime )
    {
        default:
        case 0: // AM/PM 12-hour
        {
            BOOL bPM = FALSE;
            int iHour = tmDate.GetHour();
            if( iHour > 11 )
            {
                bPM = TRUE;
                iHour -= 12;
            }
            
            iHour = iHour ? iHour : 12;
            
            wsprintf( pszTime, "%d%s%.2d %s", iHour, szSepTime, tmDate.GetMinute(), (TCHAR *)(bPM ? szPM : szAM) );
            break;
        }
        
        case 1: // 24-hour
        {
            wsprintf( pszTime, "%d%s%.2d", tmDate.GetHour(), szSepTime, tmDate.GetMinute() );
            break;
        }
    }
    
    csFormatDate = (const char *)szBuffer;
}


void HTFE_MakePrettyDate(char* buffer, time_t lastVisited)
{
  buffer[0] = 0;
  time_t today = XP_TIME();
  int elapsed = today - lastVisited;

  if (elapsed < SECONDS_PER_DAY) 
  {
    int32 hours = (elapsed + 1800L) / 3600L;
    if (hours < 1) 
	{
	  strcpy(buffer, XP_GetString(XP_BKMKS_LESS_THAN_ONE_HOUR_AGO));
	}
    sprintf(buffer, XP_GetString(XP_BKMKS_HOURS_AGO), hours);
  } 
  else if (elapsed < (SECONDS_PER_DAY * 31)) 
  {
	  sprintf(buffer, XP_GetString(XP_BKMKS_DAYS_AGO),
			  (elapsed + (SECONDS_PER_DAY / 2)) / SECONDS_PER_DAY);
  } 
  else 
  {
      CString csFormatDate;
      CTime ctDate( lastVisited );
      FormatDateTime( ctDate, csFormatDate );
      _tcscpy( buffer, (const char *)csFormatDate );
  }
}

BEGIN_MESSAGE_MAP(CRDFOutliner, COutliner)
	//{{AFX_MSG_MAP(CMainFrame)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	ON_WM_PAINT()
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_CREATE()
	ON_WM_TIMER()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CRDFOutliner::CRDFOutliner (HT_Pane thePane, HT_View theView, CRDFOutlinerParent* theParent)
:m_pAncestor(NULL), m_Pane(thePane), m_View(theView), m_Parent(theParent), m_EditField(NULL),
m_nSortType(HT_NO_SORT), m_nSortColumn(HT_NO_SORT), m_hEditTimer(0), m_hDragRectTimer(0),
m_bNeedToClear(FALSE), m_nSelectedColumn(0), m_bDoubleClick(FALSE), m_Node(NULL), 
m_bDataSourceInWindow(FALSE), m_NavMenuBar(NULL)
{
    ApiApiPtr(api);
    m_pUnkUserImage = api->CreateClassInstance(APICLASS_IMAGEMAP,NULL,(APISIGNATURE)IDB_BOOKMARKS);
    if (m_pUnkUserImage) {
        m_pUnkUserImage->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIUserImage);
        ASSERT(m_pIUserImage);
		if (!m_pIUserImage->GetResourceID())
			m_pIUserImage->Initialize(IDB_BOOKMARKS,16,16);
    }
}

CRDFOutliner::~CRDFOutliner ()
{
    if (m_pUnkUserImage) {
        if (m_pIUserImage)
            m_pUnkUserImage->Release();
    }

	delete m_pAncestor;
}

int CRDFOutliner::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    int iRetVal = COutliner::OnCreate(lpCreateStruct);
	DragAcceptFiles(FALSE);
    return iRetVal;
}

void CRDFOutliner::HandleEvent(HT_Notification ns, HT_Resource n, HT_Event whatHappened) 
{
	if (whatHappened == HT_EVENT_NODE_OPENCLOSE_CHANGED)
	{
		FinishExpansion(HT_GetNodeIndex(m_View, n));
	}
	else if (whatHappened == HT_EVENT_NODE_SELECTION_CHANGED)
	{
		int i = HT_GetNodeIndex(m_View, n);
		InvalidateLine(i);
	}
	else if (whatHappened == HT_EVENT_NODE_ADDED)
	{
		HT_Resource par = HT_GetParent(n);
		int toggle = -1;

		if (par != NULL)
			toggle = HT_GetNodeIndex(m_View, par);

		FinishExpansion(toggle);
	}
	else if (whatHappened == HT_EVENT_VIEW_SORTING_CHANGED)
	{
		if (m_Node)
		{		
			int index =	HT_GetNodeIndex(m_View, m_Node);
			m_iFocus = index;
		}
		Invalidate();
	}
	else if (whatHappened == HT_EVENT_NODE_EDIT)
	{
		m_Node = n;
		m_iSelection = m_iFocus = HT_GetNodeIndex(m_View, n);
		m_nSelectedColumn = GetColumnAtPos(0);

		AddTextEdit();

	}
	else if (whatHappened == HT_EVENT_NODE_DELETED_DATA ||
			 whatHappened == HT_EVENT_NODE_DELETED_NODATA)
	{
		m_iFocus = -1;
	}
	else if (whatHappened == HT_EVENT_VIEW_REFRESH)
	{
		SetTotalLines(HT_GetItemListCount(m_View));
		Invalidate();
		UpdateWindow();
	}
	else if (whatHappened == HT_EVENT_NODE_VPROP_CHANGED)
	{
		// Invalidate a single line
		if (n)
		{		
			int index =	HT_GetNodeIndex(m_View, n);
			InvalidateLine(index);
		}
	}
}


LPCTSTR CRDFOutliner::GetColumnText ( UINT iColumn, void * pLineData )
{
	void* nodeData;
	time_t dateVal;
	struct tm* time;
	
	HT_Resource pEntry = (HT_Resource)pLineData;
	CRDFCommandMap& map = m_Parent->GetColumnCommandMap(); 
	CRDFColumn* theColumn = (CRDFColumn*)(map.GetCommand(iColumn));
		
	if (pEntry && HT_GetNodeData(pEntry, theColumn->GetToken(), theColumn->GetDataType(), &nodeData)
		&& nodeData)
	{
		char* buffer = theColumn->GetStorageBuffer();
		switch (theColumn->GetDataType())
		{
			case HT_COLUMN_DATE_STRING:
				if ((dateVal = (time_t)atol((char *)nodeData)) == 0)	break;
				if ((time = localtime(&dateVal)) == NULL)	break;
				HTFE_MakePrettyDate(buffer, dateVal);
				break;
			case HT_COLUMN_DATE_INT:
				if ((time = localtime((time_t *) &nodeData)) == NULL)	break;
				HTFE_MakePrettyDate(buffer, (time_t)nodeData);
				break;
			case HT_COLUMN_INT:
				sprintf(buffer,"%d",(int)nodeData);
				break;
			case HT_COLUMN_STRING:
				strcpy(buffer, (char*)nodeData);
				break;
		}

		return buffer;
	}
    return NULL;
}

void * CRDFOutliner::AcquireLineData ( int iLine )
{ 
	// Grab this resource
	if (GetTotalLines() <= iLine)
		return NULL;

	HT_Resource r = HT_GetNthItem(m_View, iLine);

	if (r != NULL)
	{
		int indent = HT_GetItemIndentation(r);
		BOOL hasPrev = HT_ItemHasBackwardSibling(r);
		BOOL hasNext = HT_ItemHasForwardSibling(r);

		if (m_bPaintingFirstObject || indent > m_nSizeOfAncestorArray)
		{
			OutlinerAncestorInfo* oldInfo = m_pAncestor;
			
			if (m_bPaintingFirstObject)
			{
				m_nSizeOfAncestorArray = indent;
				oldInfo = NULL;
			}

			m_pAncestor = new OutlinerAncestorInfo[indent];
			
			// Fill in the ancestor values.
			copyAncestorValues(r, m_nSizeOfAncestorArray, oldInfo, m_pAncestor);
			m_nSizeOfAncestorArray = indent;

			delete []oldInfo;
		}
		
		
		m_pAncestor[indent-1].has_next = hasNext;
		m_pAncestor[indent-1].has_prev = hasPrev;
	}		

	return r;
}

void CRDFOutliner::ReleaseLineData ( void * pLineData )
{
}

void CRDFOutliner::GetTreeInfo( int iLine, uint32 * pFlags, int * iDepth, 
									 OutlinerAncestorInfo ** pAncestor )
{
	HT_Resource r = HT_GetNthItem(m_View, iLine);
	if (r == NULL)
		return;

	if (pFlags)
        *pFlags = (uint32)HT_IsContainer(r);
    if (iDepth)
        *iDepth = HT_GetItemIndentation(r)-1;
    
	if (pAncestor)
		*pAncestor = m_pAncestor;
}

void CRDFOutliner::copyAncestorValues(HT_Resource r, int numItems, OutlinerAncestorInfo* oldInfo, OutlinerAncestorInfo* newInfo)
{
	if (oldInfo != NULL)
	{
		for (int i = 0; i < numItems; i++)
		{
			newInfo[i].has_next = oldInfo[i].has_next;
			newInfo[i].has_prev = oldInfo[i].has_prev;
		}
	}
	else
	{
		// Have to acquire ALL the data.
		HT_Resource currNode = r;
		for (int i = numItems-1; i >= 0; i--)
		{
			newInfo[i].has_next = HT_ItemHasForwardSibling(currNode);
			newInfo[i].has_prev = HT_ItemHasBackwardSibling(currNode);
			currNode = HT_GetParent(currNode);
		}
	}
}

BOOL CRDFOutliner::IsSelected(int iLine)
{
	HT_Resource r = HT_GetNthItem(m_View, iLine);
	return (r != NULL) && (HT_IsSelected(r));
}

HFONT CRDFOutliner::GetLineFont ( void * pLineData )
{
	// Will need to hack this for aliases/shortcuts? (italics!)
	/*HT_Resource r = (HT_Resource)pLineData;
	char* url = HT_GetNodeURL(r);

	if (url && !IsLocalFile(url))
		return m_hItalFont;
	*/

	return m_hRegFont;
}

int CRDFOutliner::TranslateIcon ( void * pData )
{
   HT_Resource data = (HT_Resource)pData;
   char* pURL = HT_GetNodeSmallIconURL(data);

   //TRACE("%s\n", pURL);
	
   if (strncmp("icon", pURL, 4) == 0)
   {
	   // Icon URL
		if ((strcmp("icon/small:folder/closed", pURL) == 0) ||
			(strcmp("icon/small:folder,history/closed", pURL) == 0))
			return IDX_BOOKMARKCLOSED;

		if ((strcmp("icon/small:folder/open", pURL) == 0) ||
			(strcmp("icon/small:folder,history/open", pURL) == 0))
			return IDX_BOOKMARKOPEN;

		if (strcmp("icon/small:url", pURL) == 0)
			return IDX_BOOKMARK;

		if ((strcmp("icon/small:file/local", pURL) == 0) ||
		    (strcmp("icon/small:folder/local,closed", pURL) == 0) ||
			(strcmp("icon/small:folder/local,open", pURL) == 0))
			return HTFE_LOCAL_FILE;

		// Catchall code for unknown ICON urls
		if (HT_IsContainer(data))
		{
			if (HT_IsContainerOpen(data))
				return IDX_BOOKMARKOPEN;
			else return IDX_BOOKMARKCLOSED;
		}

		return IDX_BOOKMARK;
   }

   // Handle the arbitrary URL case
   return HTFE_USE_CUSTOM_IMAGE;   
}

int CRDFOutliner::TranslateIconFolder( void *pData )
{
   HT_Resource data = (HT_Resource)pData;

   if (data) 
   {
	   if (HT_IsContainer(data))
	   {
		   if (HT_IsContainerOpen(data))
		   {
			   return OUTLINER_OPENFOLDER;
		   }
		   else
		   {
			   return OUTLINER_CLOSEDFOLDER;
		   }
	   }
   }
    
   return OUTLINER_ITEM;
}

void CRDFOutliner::LoadComplete(HT_Resource pResource)
{
	InvalidateLine(HT_GetNodeIndex(m_View, pResource));
}

// Functions used to draw custom images (either local file system icons or arbitrary image URLs)
HICON DrawLocalFileIcon(HT_Resource r, int left, int top, HDC hDC) 
{
	char* pLocalName = NULL;
	HICON hIcon = FetchLocalFileIcon(r);
	
	if (hIcon)
	{
		// Now we draw this bad boy.
		DrawIconEx(hDC, left, top, hIcon, 0, 0, 0, NULL, DI_NORMAL);	
	}

	return hIcon;
}

NSNavCenterImage* DrawArbitraryURL(HT_Resource r, int left, int top, int imageWidth, int imageHeight, HDC hDC, COLORREF bkColor, 
					  CCustomImageObject* pObject, BOOL largeIcon)
{
	NSNavCenterImage* pImage = FetchCustomIcon(r, pObject, largeIcon);
	if (pImage && pImage->CompletelyLoaded()) 
	{
		// Now we draw this bad boy.
		if (pImage->m_BadImage) 
		{ 
			// display broken icon.
			HDC tempDC = ::CreateCompatibleDC(hDC);
			HBITMAP hOldBmp = (HBITMAP)::SelectObject(tempDC,  NSNavCenterImage::m_hBadImageBitmap);
			::StretchBlt(hDC, 
					 left, top,
					 imageWidth, imageHeight, 
					 tempDC, 0, 0, 
					 imageWidth, imageHeight, SRCCOPY);
			::SelectObject(tempDC, hOldBmp);
			::DeleteDC(tempDC);
		}
		else if (pImage->bits ) 
		{
			// Center the image. 
			long width = pImage->bmpInfo->bmiHeader.biWidth;
			long height = pImage->bmpInfo->bmiHeader.biHeight;
			int xoffset = (imageWidth-width)/2;
			int yoffset = (imageHeight-height)/2;
			if (xoffset < 0) xoffset = 0;
			if (yoffset < 0) yoffset = 0;
			if (width > imageWidth) width = imageWidth;
			if (height > imageHeight) height = imageHeight;

			HPALETTE hPal = WFE_GetUIPalette(NULL);
			HPALETTE hOldPal = ::SelectPalette(hDC, hPal, TRUE);

			::RealizePalette(hDC);
			
			if (pImage->maskbits) 
			{
				WFE_StretchDIBitsWithMask(hDC, TRUE, NULL,
					left+xoffset, top+xoffset,
					width, height,
					0, 0, width, height,
					pImage->bits, pImage->bmpInfo,
					pImage->maskbits, FALSE, bkColor);
			}
			else 
			{
				::StretchDIBits(hDC,
					left+xoffset, top+xoffset,
					width, height,
					0, 0, width, height, pImage->bits, pImage->bmpInfo, DIB_RGB_COLORS,
					SRCCOPY);
			}

		::SelectPalette(hDC, hOldPal, TRUE);
		}
	}

	return pImage;
}

BOOL IsLocalFile(const char* pURL)
{
	return strncmp(pURL, "file:", 5) == 0;
}

BOOL IsExecutableURL(const char* pURL)
{
	char* pLocalName = NULL;
	if (!XP_ConvertUrlToLocalFile(pURL, &pLocalName))
		return FALSE;
	
	pLocalName = NET_UnEscape(pLocalName);

	// Do a FindExecutable.  If the filenames match, we win.
	char execName[_MAX_PATH];
	if (FEU_FindExecutable(pLocalName, execName, FALSE))
	{
		// Well, there's at least SOMETHING.
		// See if it matches us.
		char answerString[_MAX_PATH];
		strcpy(answerString, pLocalName);
		char execString[_MAX_PATH];
		strcpy(execString, execName);
		
#ifdef _WIN32
		GetShortPathName(pLocalName, answerString, _MAX_PATH);
		GetShortPathName(execName, execString, _MAX_PATH);
#endif // _WIN32

		if (!stricmp(answerString, execString))
		  return TRUE;
	}

	if (pLocalName)
		XP_FREE(pLocalName);

	return FALSE;
}

// The expansion handler
int CRDFOutliner::ToggleExpansion (int iLine)
{
	PRBool openp;
	HT_Resource pEntry = HT_GetNthItem(m_View, iLine);
	if (pEntry && HT_IsContainer(pEntry))
	{
		HT_GetOpenState(pEntry, &openp);
		HT_SetOpenState(pEntry, (PRBool)(!openp));
	}
	return 0;
}

HICON FetchLocalFileIcon(HT_Resource r)
{
	HICON hIcon = NULL;
	CString hashString(HT_GetNodeURL(r));
		
	if (hashString.GetLength() < 12) // Hack.  It's a disk volume.
	{
	    // Hash disk drives based on URL.
	}
	else if (HT_IsContainer(r))
	{
		// We're a directory.
		if (HT_IsContainerOpen(r))
			hashString = "open folder";
		else hashString = "closed folder";
	}
	else
	{
		// We're a file.
		int index = hashString.ReverseFind('.');
		if (index == -1)
			hashString = "no extension";
		else hashString = hashString.Right(hashString.GetLength() - index);
	}
		
	void* hashPtr;
	char* pLocalName = NULL;
	if (CHTFEData::m_LocalFileCache.Lookup(hashString, hashPtr))
	{
		// We found an icon in the icon cache.
		hIcon = (HICON)hashPtr;
	}
	else 
	{
		XP_ConvertUrlToLocalFile(HT_GetNodeURL(r), &pLocalName);
		pLocalName = NET_UnEscape(pLocalName);

		SHFILEINFO shInfo;
		shInfo.hIcon = NULL;

		if (HT_IsContainer(r) && HT_IsContainerOpen(r))
			SHGetFileInfo(pLocalName, 0, &shInfo, sizeof(shInfo), SHGFI_OPENICON | SHGFI_ICON | SHGFI_SMALLICON);
		else SHGetFileInfo(pLocalName, 0, &shInfo, sizeof(shInfo), SHGFI_ICON | SHGFI_SMALLICON);
		
		if(shInfo.hIcon)
		{
			hIcon = shInfo.hIcon;

			// Add to the local file cache.
			CHTFEData::m_LocalFileCache.SetAt(hashString, (void*)hIcon);
		}

		if (pLocalName)
			XP_FREE(pLocalName);
	}

	return hIcon;
}

NSNavCenterImage* FetchCustomIcon(HT_Resource r, CCustomImageObject* imageObject, BOOL largeIcon)
{
	NSNavCenterImage* pImage = NULL;
	CString hashString;
	if (largeIcon)
		hashString = HT_GetNodeLargeIconURL(r);
	else hashString = HT_GetNodeSmallIconURL(r);
	
	if (strncmp("icon/large:workspace", hashString, 20) == 0)
	{
		char name[8];
		int ret = sscanf(hashString, "icon/large:workspace,%s", name);
		if (ret != 0) 
			hashString = CString("about:") + name + ".gif";
	}
	else if (strncmp("icon/large:folder", hashString, 17) == 0)
	{
		hashString = "about:personal.gif";
	}

	// Find the image.
	void* pData;
	if (CHTFEData::m_CustomURLCache.Lookup(hashString, pData))
	{
		pImage = (NSNavCenterImage*)pData;

		// Add ourselves to the callback list if the image hasn't completely loaded.
		if (!pImage->CompletelyLoaded())
		{
			// The image is currently loading.  Register ourselves with the image so that we will get called
			// when the image finishes loading.
			CIconCallbackInfo* iconCallbackInfo = new CIconCallbackInfo(imageObject, r);
			pImage->resourceList.AddHead(iconCallbackInfo);  
		}
	}
	else
	{
		// Create a new NavCenter image.
		CIconCallbackInfo* iconCallbackInfo = new CIconCallbackInfo(imageObject, r);
		pImage = new NSNavCenterImage((char*)(const char*)hashString, iconCallbackInfo);
		CHTFEData::m_CustomURLCache.SetAt(hashString, pImage);
	}

	return pImage;
}


IconType DetermineIconType(HT_Resource pNode, BOOL largeIcon)
{ 
   IconType returnValue;
   if (pNode != NULL)
   {
	    char* pURL = largeIcon? HT_GetNodeLargeIconURL(pNode) : HT_GetNodeSmallIconURL(pNode);
		TRACE("%s\n", pURL);
		if (strncmp("icon", pURL, 4) != 0)
		{
			// Time to load a custom image.
			returnValue = ARBITRARY_URL;
		}
		else if ((strncmp("icon/large:workspace", pURL, 20) == 0) ||
				 (strncmp("icon/large:folder", pURL, 17) == 0))
		{
			returnValue = ARBITRARY_URL;
		}
		else if (IsLocalFile(HT_GetNodeURL(pNode)))
		{
			returnValue = LOCAL_FILE;
		}
		else returnValue = BUILTIN_BITMAP;
   }
   else returnValue = BUILTIN_BITMAP;

   return returnValue;
}

// The callback for finishing the expansion
void CRDFOutliner::FinishExpansion(int toggleIndex)
{
	int iCount = GetTotalLines();
	int newCount = HT_GetItemListCount(m_View);
	SetTotalLines(newCount);  // Update the outliner's visible lines
	
	int iDelta = newCount - iCount;
	int iSel = GetFocusLine();
	if (iSel > toggleIndex) {
		if (iDelta > 0) {
			iSel += iDelta;
		} else {
			if (iSel < (toggleIndex - iDelta)) {
				iSel = toggleIndex;
			} else {
				iSel += iDelta;
			}
		}
		if (iSel != GetFocusLine()) 
		{
			m_iFocus = m_iSelection = iSel;
			InvalidateLine(m_iFocus);
		}
	}
	
	if ( iDelta > 0 ) 
	{
		iDelta = iDelta > m_iPaintLines - 2 ? m_iPaintLines - 2 : iDelta;
		
		if (!m_bDataSourceInWindow) // Springloading folders go haywire if we shift lines around, so don't do it
			EnsureVisible( toggleIndex + iDelta );
	}

	Invalidate();
}

void CRDFOutliner::SelectItem(int iSel,int mode,UINT flags)
{
	int oldColumn = GetSelectedColumn();
	
	if (m_iColHit == -1)
		return;

	UINT command = m_pColumn[ m_iColHit ]->iCommand;

	RemoveTextEdit();

	m_Node = HT_GetNthItem(m_View, iSel);

    if (!m_Node)
	{
		HT_SetSelectionAll(m_View, (PRBool)FALSE);
		
		if (mode != OUTLINER_RBUTTONDOWN)
			return;

		m_Node = HT_TopNode(m_View);
		iSel = -1;
		
	}

    switch (mode) 
	{
		case OUTLINER_RETURN:
		case OUTLINER_LBUTTONDBLCLK:
			OnSelDblClk(iSel);
			m_bDoubleClick = TRUE;
			if (m_hEditTimer)
			{
				KillTimer(m_hEditTimer);
				m_hEditTimer = 0;
			}
			break;
		case OUTLINER_LBUTTONUP:
			if (m_bNeedToClear)
			{
				HT_SetSelection(m_Node);
				if (m_bNeedToEdit && !m_bDoubleClick)
				{

					CRDFCommandMap& map = m_Parent->GetColumnCommandMap(); 
					CRDFColumn* theColumn = (CRDFColumn*)(map.GetCommand(m_nSelectedColumn));
		
					BOOL isEditable = HT_IsNodeDataEditable(m_Node, theColumn->GetToken(), 
															theColumn->GetDataType());

					// Get down.... get down... time for a column edit.
					if (isEditable)
						m_hEditTimer = SetTimer(IDT_EDITFOCUS, EDIT_DELAY, NULL);
				}
			}
			m_bNeedToClear = FALSE;
			m_bNeedToEdit = FALSE;
			m_bDoubleClick = FALSE;
			break;	
		case OUTLINER_LBUTTONDOWN:
		case OUTLINER_KEYDOWN:
		case OUTLINER_SET:
 			m_iFocus = iSel;
			m_bNeedToClear = FALSE;
			m_bNeedToEdit = FALSE;
			SetSelectedColumn((int)command);
			
			if (flags & MK_CONTROL) 
			{
				// Causes a selection toggle.
				HT_ToggleSelection(m_Node);	
			}
			else if (flags & MK_SHIFT) 
			{
				// A range of items is being selected.
				HT_Resource node = HT_GetNthItem(m_View, m_iSelection);
				if (node != NULL)
				{
					HT_SetSelectionRange(node, m_Node);
				}
			}
			else 
			{
				m_iSelection = iSel;

				if (!IsSelected(m_iFocus))
				{
					HT_SetSelection(m_Node);
					
					//if (!HT_IsContainer(m_Node) && !HT_IsSeparator(m_Node) && IsDocked())
					//	DisplayURL();  For now do double-click behavior
				}
				else 
				{
					m_bNeedToClear = TRUE;
					if (oldColumn == m_nSelectedColumn)
						m_bNeedToEdit = TRUE;
				}
			}	
			break;

		case OUTLINER_RBUTTONDOWN:
 			m_iFocus = iSel;
			SetSelectedColumn((int)command);	
			m_iSelection = iSel;

			if (m_iFocus >= 0 && !IsSelected(m_iFocus))
			{
				HT_SetSelection(m_Node);
			}	
			break;
		default:
			break;
	}
}

BOOL CRDFOutliner::DeleteItem(int iSel)
{
	HT_DoMenuCmd(m_Pane, HT_CMD_CUT);
	return TRUE;
}

BOOL CRDFOutliner::IsDocked()
{
	CGenericFrame* theFrame = (CGenericFrame*)FEU_GetLastActiveFrame();
	
	if (theFrame == NULL)
	  return FALSE;

	return theFrame->IsChild(this);
}

void CRDFOutliner::OnSelDblClk(int iLine)
{
	if (m_Node)
	{
		if (HT_IsContainer(m_Node))
		{
			ToggleExpansion(iLine);
		}
		else if (!HT_IsSeparator(m_Node))// && !IsDocked()) For now always do double-click behavior
		{
			DisplayURL();
		}
	}
}

void CRDFOutliner::DisplayURL()
{
	char* url = HT_GetNodeURL(m_Node);
	if (IsExecutableURL(url))
	{
		// Shell Execute
#ifdef _WIN32
		char* pLocalName = NULL;
		XP_ConvertUrlToLocalFile(url, &pLocalName);
		pLocalName = NET_UnEscape(pLocalName);

		SHELLEXECUTEINFO    sei;
	  
		// Use ShellExecuteEx to launch
		sei.cbSize = sizeof(sei);
		sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
		sei.hwnd = NULL;
		sei.lpVerb = NULL;  // default to Open
		sei.lpFile = pLocalName;
		sei.lpParameters = NULL;
		sei.lpDirectory = NULL;
		sei.nShow = SW_SHOW;
		ShellExecuteEx(&sei);
		int uSpawn = (UINT)sei.hInstApp;
		if(uSpawn <= 32)	
		{
			char szMsg[80];
			switch(uSpawn) 
			{
				case 0:
				case 8:
					sprintf(szMsg, szLoadString(IDS_WINEXEC_0_8));
					break;
				case 2:                                      
				case 3:
					sprintf(szMsg, szLoadString(IDS_WINEXEC_2_3));
					break;
				case 10:
				case 11:
				case 12:
				case 13:
				case 14:
				case 15:
					sprintf(szMsg, szLoadString(IDS_WINEXEC_10_THRU_15));
					break;
				case 16:
					sprintf(szMsg, szLoadString(IDS_WINEXEC_16));
					break;
				case 21:
					sprintf(szMsg, szLoadString(IDS_WINEXEC_21));
					break;
				default:
					sprintf(szMsg, szLoadString(IDS_WINEXEC_XX), uSpawn);
					break;
			}        
		
			CString s;
			if (s.LoadString( IDS_BOOKMARK_ADDRESSPROPERTIES ))
			{
				::MessageBox(GetParentFrame()->m_hWnd, szMsg, s, MB_OK | MB_APPLMODAL);
			}
		}
		if (pLocalName)
			XP_FREE(pLocalName);
#endif // _WIN32
	}
	else
	{
		CAbstractCX * pCX = FEU_GetLastActiveFrameContext();
		ASSERT(pCX != NULL);
		if (pCX != NULL)
		{
			if (!strncmp(url, "nes:", 4)) 
			  pCX->NormalGetUrl((LPTSTR)&url[4]);
			else 
			  pCX->NormalGetUrl((LPTSTR) url);
		}
	}
}

BOOL CRDFOutliner::TestRowCol(POINT point, int &iRow, int &iCol)
{
	RECT rcClient;
	GetClientRect(&rcClient);
	BOOL answer = TRUE;

	if (::PtInRect(&rcClient, point)) 
	{
		void *pLineData = NULL;
		int iSel = point.y  / m_itemHeight;
    	int iNewSel = m_iTopLine + iSel;
	    if ( ( pLineData = AcquireLineData ( iNewSel ) ) != NULL ) 
		{
			ReleaseLineData( pLineData );
			iRow = iNewSel;
		}
		else answer = FALSE;

		int i, offset;
		int y = iSel * m_itemHeight;

	    for ( i = 0, offset = 0; i < m_iVisColumns; i++ )
	    {
	       CRect rect ( offset, y,
						offset + m_pColumn[ i ]->iCol, y + m_itemHeight );

		   if ( rect.PtInRect(point) ) 
		   {
				iCol = i;
				m_rcHit = rect;
				return answer;
		   }
	        	
		   offset += m_pColumn[ i ]->iCol;
	    }
	}
	return FALSE;
}

int CRDFOutliner::DetermineClickLocation(CPoint point)
{
	// TestRowCol returns FALSE if the row clicked on contains no data. We must have clicked on the background
	// in this case.
	int iRow, iCol;

	if ( !TestRowCol( point, iRow, iCol ) )
		return CLICKED_BACKGROUND;

	// We clicked on a row/column with data.  Update our member variables to reflect our hit.
	m_iRowHit = iRow;
	m_iColHit = iCol;

	// Get the text rectangle corresponding to the hit column.
	CRect textRect = GetColumnRect(iRow, (int)(m_pColumn[iCol]->iCommand));

	// Double-check and make sure we have line data.  (We should, but let's be safe.)
	void * pLineData;
	if ( ( pLineData = AcquireLineData ( iRow ) ) == NULL)
		return CLICKED_BACKGROUND;
	int iDepth;
	GetTreeInfo ( iRow, NULL, &iDepth, NULL );
	HT_Resource r = (HT_Resource)pLineData;
	ReleaseLineData ( pLineData );
			
	// If the user clicked on the image column, they might have struck the trigger.  Check for this.
	if ( m_pColumn[ iCol ]->iCommand == m_idImageCol ) 
	{
		int iImageWidth = m_pIImage->GetImageWidth ( );
		RECT rcToggle = m_rcHit;
		rcToggle.left += iDepth * iImageWidth;
		rcToggle.right = rcToggle.left + iImageWidth;

		// If the data is a container and if the point is inside the toggle rect, the user clicked the
		// trigger.
		if ( ::PtInRect( &rcToggle, point ) && HT_IsContainer(r)) 
			return CLICKED_TRIGGER;
			
		// If the user clicked to the left of the trigger on a container, then treat as a background click.
		// If the user clicked where the trigger would have been (or to the left of the trigger) on a non-
		// container, then treat that as a background click also.
		if ((point.x < rcToggle.left && HT_IsContainer(r)) ||
			(point.x < rcToggle.right && !HT_IsContainer(r)))
		  return CLICKED_BACKGROUND;
	}

	// See if the user clicked on the column text. 
	const char* text = GetColumnText(m_pColumn[ iCol ]->iCommand, r);
	if (!text)
		return CLICKED_BACKGROUND;  // Column has no text. User has to have clicked on the background.

	// All this code just determines our text rectangle.
	CClientDC dc(this);
	CString theString(text);
	CRect bgRect;
	dc.SelectObject( GetLineFont( r ) );
	UINT dwDTFormat = DT_NOCLIP | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER;
	switch (m_pColumn[ iCol ]->alignment) 
	{
		case AlignCenter:
			dwDTFormat |= DT_CENTER;
			break;
		case AlignRight:
			dwDTFormat |= DT_RIGHT;
			break;
		case AlignLeft:
		default:
			dwDTFormat |= DT_LEFT;
	}
		
	dc.DrawText(theString, theString.GetLength(), &bgRect, DT_CALCRECT | dwDTFormat);
	int w = bgRect.Width() + 2*COL_LEFT_MARGIN;
	if (w > textRect.Width())
		w = textRect.Width();
	bgRect.left = textRect.left;
	bgRect.top = textRect.top;
	bgRect.right = bgRect.left + w;
	bgRect.bottom = textRect.bottom;
		
	if ( m_pColumn[ iCol ]->iCommand == m_idImageCol ) 
	  bgRect.left -= (m_pIUserImage->GetImageWidth() + OUTLINE_TEXT_OFFSET);

	// Now that we've set up the rect, see if the user clicked in it.  If they didn't, it's a background
	// click.
	if (!bgRect.PtInRect(point))
		return CLICKED_BACKGROUND;
		
	// User jumped through all our hoops.  Must have clicked on some actual data.  
	return CLICKED_LINE;
}

void CRDFOutliner::OnLButtonDown ( UINT nFlags, CPoint point )
{
// Reset drag rect info
	m_bCheckForDragRect = FALSE;
	m_LastDragRect = CRect(0,0,0,0);
	m_nRectYDisplacement = 0;

	Default();
	TipHide();

	SetFocus();

    m_ptHit = point;
    
	m_iRowHit = -1;
	m_iColHit = -1;
	
	int clickResult = DetermineClickLocation(point);
	
	if (clickResult == CLICKED_TRIGGER)
		DoToggleExpansion(m_iRowHit);
	else if (clickResult == CLICKED_LINE)				
	{
		if ( ColumnCommand( m_pColumn[ m_iColHit ]->iCommand, m_iRowHit) )
			return;

		SetCapture();
		
		SelectItem( m_iRowHit, OUTLINER_LBUTTONDOWN, nFlags );
	}
	else
	{
		// user clicked on the background
		m_bCheckForDragRect = TRUE;
		SetCapture();
		m_iFocus = -1;
		if (m_iColHit != -1)
		{
			SetSelectedColumn(m_pColumn[ m_iColHit ]->iCommand);
		}
		HT_SetSelectionAll(m_View, (PRBool)FALSE);
	}
}

void CRDFOutliner::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (GetCapture() == this) {
		ReleaseCapture();
		if (m_LastDragRect != CRect(0,0,0,0))
		{
			// Dragging happened
			CClientDC dc(this);
			EraseDragRect(&dc, m_LastDragRect);

			if (m_hDragRectTimer)
			{
				KillTimer(m_hDragRectTimer);
				m_hDragRectTimer = 0;
			}
		}
		else SelectItem( m_iSelection, OUTLINER_LBUTTONUP, nFlags );
	}
}

void CRDFOutliner::OnRButtonDown ( UINT nFlags, CPoint point )
{
	// Update our column hit, since we draw selected columns.
	m_iRowHit = -1;
	m_iColHit = -1;

	int clickResult = DetermineClickLocation(point);
	
	if (clickResult == CLICKED_LINE)				
	{
		// user clicked on the item.  We will go ahead and select.
		SelectItem( m_iRowHit, OUTLINER_RBUTTONDOWN, nFlags );
	}
	else
	{
		// user clicked on the background... deselect everything.		
		m_iFocus = -1;
		HT_SetSelectionAll(m_View, (PRBool)FALSE);
	}

/*
	int iRow, iCol;
	if ( TestRowCol( point, iRow, iCol ) ){
		HT_Resource pEntry = HT_GetNthItem(m_View, iRow);
		CExportRDF exportDlg;

		if (exportDlg.DoModal() == IDOK) {
			FILE* mcfStr;
			mcfStr = fopen(exportDlg.m_csAns, "w");
			outputMCFTree(RDF_GetNavCenterDB(), stdPrint, mcfStr, HT_GetRDFResource(pEntry));
			fclose(mcfStr);  
		}
	}
	*/
}

void CRDFOutliner::OnRButtonUp( UINT nFlags, CPoint point )
{
    m_ptHit = point;
	int iSel = m_iTopLine + (point.y  / m_itemHeight);
    
	int clickResult = DetermineClickLocation(point);
	PropertyMenu( iSel, clickResult );
}

void CRDFOutliner::DragRectScroll( BOOL bBackwards )
{
	int oldLine = m_iTopLine;
    OnVScroll(bBackwards ? SB_LINEUP : SB_LINEDOWN, 0, 0);
	m_nRectYDisplacement += (oldLine - m_iTopLine)*m_itemHeight;
	if (m_nRectYDisplacement != 0)
	{
		OnMouseMove(0, m_MovePoint);
	}
}

CRect CRDFOutliner::ConstructDragRect(const CPoint& pt1, const CPoint& pt2)
{
	int left = pt1.x < pt2.x ? pt1.x : pt2.x;
	int right = pt1.x > pt2.x ? pt1.x : pt2.x;

	int top = pt1.y < pt2.y ? pt1.y : pt2.y;
	int bottom = pt1.y > pt2.y ? pt1.y : pt2.y;

	return CRect(left, top, right, bottom);
}

void CRDFOutliner::EraseDragRect(CDC* pDC, CRect rect)
{
#ifdef XP_WIN32
	CRgn rgnLast, rgnOutside, rgnInside;

	// set up regions and rects for drag rect with border of size (1,1)
	rgnLast.CreateRectRgn(0, 0, 0, 0);
	rgnOutside.CreateRectRgnIndirect(&rect);
	CRect rect2 = rect;
	rect.InflateRect(-1, -1);
	rect.IntersectRect(rect, rect2);
	rgnInside.CreateRectRgnIndirect(&rect);
	rgnLast.CombineRgn(&rgnOutside, &rgnInside, RGN_XOR);

	// Do the erase
	pDC->SelectClipRgn(&rgnLast);
	pDC->GetClipBox(&rect);
	CBrush* pBrushLast = CDC::GetHalftoneBrush();
	CBrush* pBrushOld = pDC->SelectObject(pBrushLast);
	pDC->PatBlt(rect.left, rect.top, rect.Width(), rect.Height(), PATINVERT);
	pDC->SelectObject(pBrushOld);
	pDC->SelectClipRgn(NULL);
#endif
}

void CRDFOutliner::OnMouseMove(UINT nFlags, CPoint point)
{
    if (GetCapture() == this) 
	{
#ifdef XP_WIN32
		if (m_bCheckForDragRect)
		{
			CRect rect;
			GetClientRect(&rect);

			// We're dragging a rectangle, baby.
			if (point.x < 0) // HACK so hit test works
				point.x = 0;

			if (point.y < 0)
				point.y = 0;

			if (point.y > rect.bottom)
				point.y = rect.bottom;
			
			m_MovePoint = point;

			if ( point.y < m_itemHeight ) 
			{
				m_hDragRectTimer = SetTimer(IDT_DRAGRECT, GetDragHeartbeat(), NULL);
			} 
			else if ( point.y > (rect.bottom - m_itemHeight) ) 
			{
				m_hDragRectTimer = SetTimer(IDT_DRAGRECT, GetDragHeartbeat(), NULL);
			}
			else if (m_hDragRectTimer)
			{
				KillTimer(m_hDragRectTimer);
				m_hDragRectTimer = 0;
			}
			
			CClientDC dc(this);
			CRect newRect = ConstructDragRect(m_ptHit, point);

			// Determine the new selection state
			if (m_LastDragRect == CRect(0,0,0,0))
				dc.DrawDragRect(&newRect, CSize(1,1), NULL, CSize(1,1));
			else 
			{
				// Displace the rects to account for scrolling
				m_LastDragRect.OffsetRect(0, m_nRectYDisplacement);
				newRect.OffsetRect(0, m_nRectYDisplacement);

				if (m_nRectYDisplacement > 0)
					newRect.top += m_nRectYDisplacement;
				else newRect.bottom -= m_nRectYDisplacement;

				// Reset the displacement for future scroll events
				m_nRectYDisplacement = 0;
				
				// Potential to add selection.
				CPoint testPoint1 = m_LastDragRect.TopLeft();
				CPoint testPoint2 = newRect.TopLeft();
				if (m_LastDragRect.bottom != newRect.bottom)
				{
					testPoint1 = CPoint(m_LastDragRect.left, 
										m_LastDragRect.bottom);
					testPoint2 = CPoint(newRect.left, newRect.bottom);
				}
				int iRow = -1;
				int iCol = -1;
				int otherRow = -1;
				int otherCol = -1;
				TestRowCol(testPoint1, iRow, iCol);
				TestRowCol(testPoint2, otherRow, otherCol);

				// Both over selections.
				if (iRow != otherRow)
				{
					// Erase the old drag rect
					EraseDragRect(&dc, m_LastDragRect);
					UpdateWindow();

					// Add or remove selection...
					int start;
					int end;
					BOOL select;

					if (iRow == -1)
						iRow = otherRow+1;
					else if (otherRow == -1)
						otherRow = iRow+1;

					if (m_LastDragRect.bottom < newRect.bottom)
					{
						start = iRow+1;
						end = otherRow+1;
						select = TRUE;
					}
					else if (m_LastDragRect.bottom > newRect.bottom)
					{
						start = otherRow+1;
						end = iRow+1;
						select = FALSE;
					}
					else if (m_LastDragRect.top > newRect.top)
					{
						start = otherRow;
						end = iRow;
						select = TRUE;
					}
					else if (m_LastDragRect.top < newRect.top)
					{
						start = iRow;
						end = otherRow;
						select = FALSE;
					}

					// Going from start to end, performing selection
					for (int i = start; i < end; i++)
					{
						HT_Resource r = HT_GetNthItem(m_View, i);
					    if (r)
					  	  HT_SetSelectedState(r, (PRBool)select);
					}

					UpdateWindow();

					// Draw the new rect
					dc.DrawDragRect(&newRect, CSize(1,1), NULL, CSize(1,1));
			
				}
				else 
					dc.DrawDragRect(&newRect, CSize(1,1), &m_LastDragRect, CSize(1,1));
				
			}
			m_LastDragRect = newRect;
		}
		else
#endif // XP_WIN32

        // See if the mouse has moved far enough to start
        // a drag operation
        if ((abs(point.x - m_ptHit.x) > 3)
        || (abs(point.y - m_ptHit.y) > 3)) 
		{
			// release the mouse capture

            ReleaseCapture();
            InitiateDragDrop();

			m_bClearOnRelease = FALSE;
			m_bSelectOnRelease = FALSE;
			SelectItem( m_iSelection, OUTLINER_LBUTTONUP, nFlags );
        }
    }
	if (m_iTipState != TIP_SHOWING)
		m_iTipState = TIP_WAITING;
	HandleMouseMove( point );
}


void CRDFOutliner::PropertyMenu(int iLine, UINT flags)
{
// Grab the resource
	BOOL backgroundCommands = (flags == CLICKED_BACKGROUND);

	CMenu popup;
	if (popup.CreatePopupMenu() != 0)
	{
		// We have a node and a menu. Fetch those commands.
		// Remove the elements from the last property menu we pulled up.
		m_MenuCommandMap.Clear();
		HT_Cursor theCursor = HT_NewContextualMenuCursor(m_View, (PRBool)FALSE, (PRBool)backgroundCommands);
		if (theCursor != NULL)
		{
			// We have a cursor. Attempt to iterate
			HT_MenuCmd theCommand; 
			while (HT_NextContextMenuItem(theCursor, &theCommand))
			{
				char* menuName = HT_GetMenuCmdName(theCommand);
				if (theCommand == HT_CMD_SEPARATOR)
					popup.AppendMenu(MF_SEPARATOR);
				else
				{
					// Add the command to our command map
					CRDFMenuCommand* rdfCommand = new CRDFMenuCommand(menuName, theCommand);
					int index = m_MenuCommandMap.AddCommand(rdfCommand);
					popup.AppendMenu(MF_ENABLED, index+FIRST_HT_MENU_ID, menuName);
				}
			}
			HT_DeleteCursor(theCursor);
		}

		//	Track the popup now.
		POINT pt = m_ptHit;
		ClientToScreen(&pt);
		popup.TrackPopupMenu( TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this, NULL );

		//  Cleanup
		popup.DestroyMenu();
	}
}

BOOL CRDFOutliner::OnCommand(UINT wParam, LONG lParam)
{
	if (wParam >= FIRST_HT_MENU_ID && wParam <= LAST_HT_MENU_ID)
	{
		// A selection was made from the context menu.
		// Use the menu map to get the HT command value
		CRDFMenuCommand* theCommand = (CRDFMenuCommand*)(m_MenuCommandMap.GetCommand((int)wParam-FIRST_HT_MENU_ID));
		if (theCommand)
		{
			HT_MenuCmd htCommand = theCommand->GetHTCommand();
			HT_DoMenuCmd(m_Pane, htCommand);
		}
	}

	return TRUE;
}

void CRDFOutliner::OnSetFocus ( CWnd * pOldWnd )
{
	// Update the context's stored RDF Info.  Hack for the back end.  Needs to go away eventually.
    theApp.m_pRDFCX->TrackRDFWindow(this);
	
	Default();

	if (m_iSelection >= 0)
        for (int i =0; i < (m_iTopLine+m_iPaintLines); i++)
            if (IsSelected(i))
	            InvalidateLine(i);
	
	FocusCheck(pOldWnd, TRUE);
}
	
void CRDFOutliner::OnKillFocus ( CWnd * pNewWnd )
{
	CWnd::OnKillFocus ( pNewWnd );
	if (m_iSelection >= 0)
        for (int i =0; i < (m_iTopLine+m_iPaintLines); i++)
            if (IsSelected(i))
	            InvalidateLine(i);

	((COutlinerParent*)GetParent())->UpdateFocusFrame();

	FocusCheck(pNewWnd, FALSE);
}
	
void CRDFOutliner::FocusCheck(CWnd* pWnd, BOOL gotFocus)
{
	if (m_NavMenuBar)
	{
		if (gotFocus)
			m_NavMenuBar->NotifyFocus(TRUE);
		else
		{
			CFrameWnd* pFrame = GetParentFrame();
			if (pFrame && !pFrame->IsChild(pWnd))
			{
				// Invalidate for a redraw
				m_NavMenuBar->NotifyFocus(FALSE);
			}
		}
	}
}

COLORREF Darken(COLORREF r)
{
	COLORREF ref = GetSysColor(COLOR_WINDOW);
	float x = GetRValue(ref);
	float y = GetGValue(ref);
	float z = GetBValue(ref);
	
	x = 0.75f*(x+1);
	y = 0.75f*(y+1);
	z = 0.75f*(z+1);

	BYTE newR = (BYTE)(int)x;
	BYTE newG = (BYTE)(int)y;
	BYTE newB = (BYTE)(int)z;

	return RGB(newR, newG, newB);
}

COLORREF Brighten(COLORREF r)
{
	COLORREF ref = GetSysColor(COLOR_WINDOW);
	float x = GetRValue(ref);
	float y = GetGValue(ref);
	float z = GetBValue(ref);
	
	x = x + 0.25f*(256 - x);
	y = y + 0.25f*(256 - y);
	z = z + 0.25f*(256 - z);

	if (x > 255.0f) x = 255.0f;
	if (y > 255.0f) y = 255.0f;
	if (z > 255.0f) z = 255.0f;

	BYTE newR = (BYTE)(int)x;
	BYTE newG = (BYTE)(int)y;
	BYTE newB = (BYTE)(int)z;

	return RGB(newR, newG, newB);
}

void CRDFOutliner::OnPaint()
{
	m_bPaintingFirstObject = TRUE;

	RECT rcClient;
	GetClientRect(&rcClient);
    CPaintDC pdc ( this );

	COLORREF bgColor = GetSysColor(COLOR_WINDOW);
	COLORREF textColor = GetSysColor(COLOR_WINDOWTEXT);
	COLORREF sortColor;
	if (GetRValue(bgColor) == 255 && GetGValue(bgColor) == 255 && GetBValue(bgColor) == 255 
		&& sysInfo.m_iBitsPerPixel > 8)
	{
			// Translate white to gray (HACK TO SATISFY THE MIGHTY GUHA)
			sortColor = RGB(192,192,192);
			bgColor = RGB(224,224,224);
			textColor = RGB(0,0,0);
	}
	else
	{
		if (((GetRValue(textColor) + GetGValue(textColor) + GetBValue(textColor))/3) > 128)
			sortColor = Darken(bgColor);
		else sortColor = Brighten(bgColor);
	}

	HPALETTE hPalette = WFE_GetUIPalette(GetParentFrame());
	HPALETTE pOldPalette = ::SelectPalette(pdc.m_hDC, hPalette, TRUE);	
/*
	if (sysInfo.m_iBitsPerPixel <= 8)
	{
		PALETTEENTRY paletteColors[1];
			
		UINT index;

		index = ::GetNearestPaletteIndex(hPalette, sortColor);
		::GetPaletteEntries(hPalette, index, 1, paletteColors);
		sortColor = RGB(paletteColors[0].peRed, paletteColors[0].peGreen, paletteColors[0].peBlue);

		index = ::GetNearestPaletteIndex(hPalette, bgColor);
		::GetPaletteEntries(hPalette, index, 1, paletteColors);
		bgColor = RGB(paletteColors[0].peRed, paletteColors[0].peGreen, paletteColors[0].peBlue);

		index = ::GetNearestPaletteIndex(hPalette, textColor);
		::GetPaletteEntries(hPalette, index, 1, paletteColors);
		textColor = RGB(paletteColors[0].peRed, paletteColors[0].peGreen, paletteColors[0].peBlue);
	}
*/
	HBRUSH hRegBrush = (HBRUSH) ::CreateSolidBrush(bgColor); 
    HPEN hRegPen = (HPEN)::CreatePen( PS_SOLID, 0, bgColor);

	HBRUSH hHighBrush = ::CreateSolidBrush( GetSysColor( COLOR_HIGHLIGHT ) );
	HPEN hHighPen = ::CreatePen( PS_SOLID, 0, GetSysColor ( COLOR_HIGHLIGHT ) );

    HBRUSH hOldBrush = (HBRUSH) pdc.SelectObject ( hRegBrush );
    HPEN hOldPen = (HPEN) pdc.SelectObject ( hRegPen );
    COLORREF cOldText = pdc.SetTextColor ( textColor );
    COLORREF cOldBk = pdc.SetBkColor ( bgColor );

    int i;            
    for ( i = pdc.m_ps.rcPaint.top / m_itemHeight; 
        i < ( pdc.m_ps.rcPaint.bottom / m_itemHeight ) + 1; i++ ) 
	{

		int index = i + m_iTopLine;

		PaintLine ( i, pdc.m_hDC, &pdc.m_ps.rcPaint, sortColor, hHighBrush, hHighPen );

		if (m_bPaintingFirstObject)
			m_bPaintingFirstObject = FALSE;
    }

    pdc.SetTextColor ( cOldText );
    pdc.SetBkColor ( cOldBk );        
    pdc.SelectObject ( hOldPen );
    pdc.SelectObject ( hOldBrush );
	
	::SelectPalette(pdc.m_hDC, pOldPalette, FALSE);

	VERIFY(DeleteObject( hRegBrush ));
	VERIFY(DeleteObject( hRegPen ));
	VERIFY(DeleteObject( hHighBrush ));
	VERIFY(DeleteObject( hHighPen ));
}

void CRDFOutliner::PaintLine ( int iLineNo, HDC hdc, LPRECT lpPaintRect, COLORREF sortColor,
							   HBRUSH hHighlightBrush,
						       HPEN hHighlightPen )
{
    void * pLineData;
    int iImageWidth = m_pIImage->GetImageWidth ( );
    CRect WinRect;
    GetClientRect(&WinRect);

    int y = m_itemHeight * iLineNo;

    int iColumn, offset;
	CRect rectColumn, rectInter;

	rectColumn.top = y;
	rectColumn.bottom = y + m_itemHeight;

	if ( !( pLineData = AcquireLineData( iLineNo + m_iTopLine )) ) 
	{
		EraseLine ( iLineNo, hdc, lpPaintRect );
		if (ViewerHasFocus() && HasFocus(iLineNo + m_iTopLine)) 
		{
			rectColumn.left = WinRect.left;
			rectColumn.right = WinRect.right;
			DrawFocusRect ( hdc, &rectColumn );
		}

		// Draw the column rect
		for ( int iColumn = offset = 0; iColumn < m_iVisColumns; iColumn++ )
		{
			rectColumn.left = offset;
			rectColumn.right = offset + m_pColumn[ iColumn ]->iCol;

			if (iColumn == GetSortColumn())
			{
				// Draw the sort rect (Yes, I know this is inefficient.)
				HBRUSH hSortBrush = (HBRUSH)::CreateSolidBrush(sortColor);
				HBRUSH pOldBrush = (HBRUSH) ::SelectObject(hdc, hSortBrush);
				::FillRect(hdc, &rectColumn, (HBRUSH) GetCurrentObject(hdc, OBJ_BRUSH));
				::SelectObject(hdc, pOldBrush);
				VERIFY(DeleteObject(hSortBrush));
			}

			offset += m_pColumn[ iColumn ]->iCol;
		}
		return;
	}

    HFONT hOldFont =(HFONT) ::SelectObject ( hdc, GetLineFont ( pLineData ) );

    for ( iColumn = offset = 0; iColumn < m_iVisColumns; iColumn++ )
    {
		rectColumn.left = offset;
		rectColumn.right = offset + m_pColumn[ iColumn ]->iCol;

        if ( rectInter.IntersectRect ( &rectColumn, lpPaintRect ) ) 
		{
			HBRUSH pOldBrush; 
			COLORREF cOldBk;
			HPEN pOldPen;
			HPEN hRegPen;
			HBRUSH hSortBrush;
			if (iColumn == GetSortColumn())
			{
				hSortBrush = (HBRUSH)::CreateSolidBrush(sortColor);
				pOldBrush = (HBRUSH) ::SelectObject(hdc, hSortBrush);
				cOldBk = ::SetBkColor ( hdc, sortColor );
				hRegPen = (HPEN)::CreatePen( PS_SOLID, 0, sortColor);
				pOldPen = (HPEN)::SelectObject(hdc, hRegPen);
			}

			::FillRect(hdc, &rectColumn, (HBRUSH) GetCurrentObject(hdc, OBJ_BRUSH));
			
			if ( m_pColumn[ iColumn ]->iCommand == m_idImageCol ) 
			{
				rectColumn.left = DrawPipes ( iLineNo, iColumn, offset, hdc, pLineData );
				rectColumn.left += OUTLINE_TEXT_OFFSET;
			}
            PaintColumn ( iLineNo, iColumn, rectColumn, hdc, pLineData, hHighlightBrush,
						  hHighlightPen );

			if (iColumn == GetSortColumn())
			{
				::SelectObject(hdc, pOldBrush);
				::SetBkColor(hdc, cOldBk);
				::SelectObject(hdc, pOldPen);
				VERIFY(DeleteObject(hRegPen));
				VERIFY(DeleteObject(hSortBrush));
			}
		}

        offset += m_pColumn[ iColumn ]->iCol;
    }

    rectColumn.left = offset;
	rectColumn.right = WinRect.right;

	::FillRect(hdc, &rectColumn, (HBRUSH) GetCurrentObject(hdc, OBJ_BRUSH));
    PaintColumn ( iLineNo, iColumn, rectColumn, hdc, pLineData, hHighlightBrush, hHighlightPen );

	rectColumn.left = WinRect.left;
	rectColumn.right = WinRect.right;

	// if we are dragging we and we don't highlight we need to draw the drag line
	if(m_iDragSelection == m_iTopLine + iLineNo && !HighlightIfDragging() && GetSortColumn() == -1
		&& m_iCurrentDropAction != DROPEFFECT_NONE)
	   PaintDragLine(hdc, rectColumn);

    ::SelectObject ( hdc, hOldFont );
    ReleaseLineData ( pLineData );
}

void CRDFOutliner::PaintColumn(int iLineNo, int iColumn, LPRECT lpColumnRect, 
							   HDC hdc, void * pLineData, HBRUSH hHighlightBrush,
							   HPEN hHighlightPen)
{
    if (iColumn < m_iVisColumns) 
	{
		const char* lpsz = GetColumnText (m_pColumn[ iColumn ]->iCommand,pLineData);
		HT_Resource theNode = (HT_Resource)pLineData;
		if (theNode && HT_IsSeparator(theNode))
		{
			// Alter the column text name.
			lpsz = NULL;
		}

		if ( !RenderData( m_pColumn[iColumn]->iCommand, CRect(lpColumnRect), 
						  *CDC::FromHandle( hdc ), lpsz) ) 
		{
			BOOL hasFocus = FALSE;
		
			if (theNode && HT_IsSeparator(theNode))
			{
				// Draw the horizontal line.
				CPen separatorPen;
				separatorPen.CreatePen(PS_SOLID, 1, GetSysColor ( COLOR_WINDOWTEXT ));
				
				HPEN sepPen = (HPEN)separatorPen.GetSafeHandle();
				
				HPEN usePen = IsSelected(iLineNo) ? hHighlightPen : sepPen;

				HPEN pOldPen = (HPEN)(::SelectObject(hdc, usePen)); //separatorPen.GetSafeHandle()));
				::MoveToEx(hdc, lpColumnRect->left, lpColumnRect->top+m_itemHeight/2, NULL);
				::LineTo(hdc, lpColumnRect->right, lpColumnRect->top+m_itemHeight/2);
				::SelectObject(hdc, pOldPen);
			}

			if (lpsz) 
			{
				if ((IsSelected(iLineNo + m_iTopLine) || 
					((m_iDragSelection == m_iTopLine + iLineNo) && HighlightIfDragging())) &&
					IsSelectedColumn(iColumn))
				{
					hasFocus = HasFocus(iLineNo + m_iTopLine);
					
					if (ViewerHasFocus())
					{
						HBRUSH pOldBrush = (HBRUSH)(::SelectObject ( hdc, hHighlightBrush ));
						HPEN pOldPen = (HPEN)(::SelectObject ( hdc, hHighlightPen ));
						COLORREF cOldText = ::SetTextColor ( hdc, GetSysColor ( COLOR_HIGHLIGHTTEXT ) );
						COLORREF oldColor = ::SetBkColor ( hdc, GetSysColor ( COLOR_HIGHLIGHT ) );
						DrawColumn ( hdc, lpColumnRect, lpsz, 
								 m_pColumn[ iColumn ]->cropping, 
								 m_pColumn[ iColumn ]->alignment, hHighlightBrush, hasFocus );
						::SelectObject(hdc, pOldBrush);
						::SelectObject(hdc, pOldPen);
						::SetTextColor ( hdc, cOldText );
						::SetBkColor ( hdc, oldColor );

					}
					else DrawColumn ( hdc, lpColumnRect, lpsz, 
								 m_pColumn[ iColumn ]->cropping, 
								 m_pColumn[ iColumn ]->alignment, hHighlightBrush, hasFocus );
				}
				else DrawColumn ( hdc, lpColumnRect, lpsz, 
								 m_pColumn[ iColumn ]->cropping, 
								 m_pColumn[ iColumn ]->alignment);

			}
		}
	}
}

void CRDFOutliner::DrawColumn(HDC hdc, LPRECT lpColumnRect, LPCTSTR lpszString,
							  CropType_t cropping, AlignType_t alignment, HBRUSH theBrush,
							  BOOL hasFocus)
{
	if (!(lpColumnRect->right - lpColumnRect->left))
		return;
	ASSERT(lpszString);
    int iLength = _tcslen(lpszString);
	if (!iLength)
		return;

	UINT dwDTFormat = DT_NOCLIP | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER;
	switch (alignment) {
	case AlignCenter:
		dwDTFormat |= DT_CENTER;
		break;
	case AlignRight:
		dwDTFormat |= DT_RIGHT;
		break;
	case AlignLeft:
	default:
		dwDTFormat |= DT_LEFT;
	}

	UINT dwMoreFormat = 0;
	switch (cropping) {
	case CropCenter:
		dwMoreFormat |= WFE_DT_CROPCENTER;
		break;
	case CropLeft:
		dwMoreFormat |= WFE_DT_CROPLEFT;
		break;
	case CropRight:
	default:
		dwMoreFormat |= WFE_DT_CROPRIGHT;
		break;
	}

	CRect textRect(*lpColumnRect);
	CRect bgRect;
	CString theString(lpszString);

	// Compute background rectangle
	DrawText(hdc, theString, theString.GetLength(), &bgRect, DT_CALCRECT | dwDTFormat);
	int w = bgRect.Width() + 2*COL_LEFT_MARGIN;
	if (w > textRect.Width())
		w = textRect.Width();
	bgRect.left = textRect.left;
	bgRect.top = textRect.top;
	bgRect.right = bgRect.left + w;
	bgRect.bottom = textRect.bottom;

	// Fill the background with the current brush (or frame if we don't have focus)
	if (ViewerHasFocus())
		::FillRect(hdc, &bgRect, (HBRUSH) GetCurrentObject(hdc, OBJ_BRUSH));
	else if (theBrush)
		::FrameRect( hdc, &bgRect, theBrush);
	
	// Adjust the text rectangle for the left and right margins
	textRect.left += COL_LEFT_MARGIN;
	textRect.right -= COL_LEFT_MARGIN;

	WFE_DrawTextEx( m_iCSID, hdc, (LPTSTR) lpszString, iLength, &textRect, dwDTFormat, dwMoreFormat );

	if (hasFocus)
	{
		DrawFocusRect ( hdc, &bgRect );
	}
}

int CRDFOutliner::DrawPipes ( int iLineNo, int iColNo, int offset, HDC hdc, void * pLineData )
{
	int iImageWidth = m_pIImage->GetImageWidth ( );
	int iMaxX = offset + m_pColumn[ iColNo ]->iCol;
	int idx;

	int iDepth;
	uint32 flags;
	OutlinerAncestorInfo * pAncestor;
	GetTreeInfo ( m_iTopLine + iLineNo, &flags, &iDepth, &pAncestor );

	RECT rect;
	rect.left = offset;
	rect.right = rect.left + iImageWidth;
	rect.top = iLineNo * m_itemHeight;
	rect.bottom = rect.top + m_itemHeight;

	if ( m_bHasPipes ) 
	{
		for ( int i = 0; i < iDepth; i++ ) 
		{
			if ( rect.right <= iMaxX ) 
			{
				if ( pAncestor && pAncestor[ i ].has_next ) 
				{
					m_pIImage->DrawTransImage( IDX_VERTPIPE, rect.left, rect.top, hdc );
				} 
				else 
				{
					::FillRect(hdc, &rect, (HBRUSH) GetCurrentObject(hdc, OBJ_BRUSH));
				}
			}
			rect.left += iImageWidth;
			rect.right += iImageWidth;
		}
    
		if ( rect.right <= iMaxX ) {
			idx = 0;
			if ( pAncestor ) {
				idx = GetPipeIndex ( pLineData, iDepth, &pAncestor[iDepth] );
			}
			if ( idx ) { 
				m_pIImage->DrawTransImage( idx, rect.left, rect.top, hdc );
			} else {
				::FillRect(hdc, &rect, (HBRUSH) GetCurrentObject(hdc, OBJ_BRUSH));
			}
		}
		rect.left += iImageWidth;
		rect.right += iImageWidth;
	}
    if ( rect.right <= iMaxX ) {
        idx = TranslateIcon (pLineData);
		HT_Resource r = (HT_Resource)pLineData;
		if (idx == HTFE_USE_CUSTOM_IMAGE)
		  DrawArbitraryURL(r, rect.left, rect.top, 16, 16, hdc, ::GetBkColor(hdc), this, FALSE);
		else if (idx == HTFE_LOCAL_FILE)
		  DrawLocalFileIcon(r, rect.left, rect.top, hdc);
		else m_pIUserImage->DrawTransImage ( idx, rect.left, rect.top, hdc );
    }
	return rect.right;
}


void CRDFOutliner::ColumnsSwapped()
{
	SetImageColumn(m_pColumn[0]->iCommand);
}

CRect CRDFOutliner::GetColumnRect(int iLine, int column)
{
	CRect rectColumn;
	rectColumn.top = (iLine-m_iTopLine) * m_itemHeight;
	rectColumn.bottom = rectColumn.top + m_itemHeight;
	rectColumn.left = rectColumn.right = 0;

	int iColumn, offset;
	int iDepth;
	uint32 flags;
	OutlinerAncestorInfo * pAncestor;
	GetTreeInfo ( iLine, &flags, &iDepth, &pAncestor );

	for (iColumn = offset = 0; iColumn < m_iVisColumns; iColumn++)
    {
		if (m_pColumn[iColumn]->iCommand == m_idImageCol) 
		{
			// Handle the image column (where the indentation is)
			int iImageWidth = m_pIImage->GetImageWidth ( );
			rectColumn.left = (iImageWidth * (iDepth+1)) + OUTLINE_TEXT_OFFSET + m_pIUserImage->GetImageWidth();
        }
		else rectColumn.left = offset;

		rectColumn.right = offset + m_pColumn[ iColumn ]->iCol;

		if (m_pColumn[iColumn]->iCommand == (UINT)column)
			break;

        offset += m_pColumn[ iColumn ]->iCol;
    }

    return rectColumn;
}

char * CRDFOutliner::GetTextEditText(void)
{
	char str[1000];

	if(m_EditField)
	{
		int size = m_EditField->GetWindowText(str, 1000);
		if(size > 0)
		{
			char *newStr = new char[size + 1];
			strcpy(newStr, str);
			return newStr;
		}
	}

	return NULL;

}

// Edit window routines
void CRDFOutliner::EditTextChanged(char* text)
{
	if (text != NULL)
	{
		// Do stuff

		CRDFCommandMap& map = m_Parent->GetColumnCommandMap(); 
		CRDFColumn* theColumn = (CRDFColumn*)(map.GetCommand(m_nSelectedColumn));
		
		HT_SetNodeData(m_Node, theColumn->GetToken(), theColumn->GetDataType(), text);

		delete []text;
	}
}

void CRDFOutliner::AddTextEdit()
{
	BOOL bRtn = TRUE;
	if(!m_EditField)
	{
		m_EditField = new CRDFEditWnd(this);
		bRtn = m_EditField->Create(WS_BORDER | ES_AUTOHSCROLL, CRect(0,0,0,0), this, 0);
	}

	CRect rect;
	HDC hDC = ::GetDC(m_hWnd);
	HFONT hFont = WFE_GetUIFont(hDC);
	HFONT hOldFont = (HFONT)::SelectObject(hDC, hFont);
	TEXTMETRIC tm;

	GetTextMetrics(hDC, &tm);
	int height = tm.tmHeight;
	::SelectObject(hDC, hOldFont);
	::ReleaseDC(m_hWnd, hDC);

	rect = GetColumnRect(m_iSelection, m_nSelectedColumn);
	rect.bottom+=2; // Adjustment, since the default is a little too small

	if (rect.left == rect.right)
		bRtn = FALSE;

	if(!bRtn)
	{
		delete m_EditField;
		m_EditField = NULL;
	}
	else
	{
		m_EditField->MoveWindow(rect);
		m_EditField->ShowWindow(SW_SHOW);
		HDC hDC = ::GetDC(m_hWnd);
		HFONT hFont = WFE_GetUIFont(hDC);
		CFont *font = (CFont*) CFont::FromHandle(hFont);
		m_EditField->SetFont(font);
		::ReleaseDC(m_hWnd, hDC);
		m_EditField->SetFocus();

		char* pText = (char*)GetColumnText(m_nSelectedColumn, HT_GetNthItem(m_View, m_iSelection));

		if (pText)
		{
			m_EditField->ReplaceSel(pText);
			m_EditField->SetSel(0, strlen(pText), TRUE);
		}
	}
}

void CRDFOutliner::RemoveTextEdit()
{
	if(m_EditField)
	{
		m_EditField->ShowWindow(SW_HIDE);
	}
}

void CRDFOutliner::MoveToNextColumn()
{
	int oldPos = GetColumnPos((UINT)m_nSelectedColumn);
	oldPos++;
	if (oldPos < (int)GetVisibleColumns())
	{
		m_nSelectedColumn = GetColumnAtPos(oldPos);
		RemoveTextEdit();
	}
	else 
	{
		m_nSelectedColumn = GetColumnAtPos(0);
		RemoveTextEdit();
		HT_Resource r = HT_GetNthItem(m_View, m_iSelection);
		if (r)
			HT_SetSelectedState(r, (PRBool)FALSE);
		m_iSelection++;
		m_iFocus = m_iSelection;
		HT_Resource r2 = HT_GetNthItem(m_View, m_iSelection);
		if (!r2)
		{
			m_iFocus = -1;
			m_iSelection = -1;
			m_Node = NULL;
			return;
		}

		CRDFCommandMap& map = m_Parent->GetColumnCommandMap(); 
		CRDFColumn* theColumn = (CRDFColumn*)(map.GetCommand(m_nSelectedColumn));
		
		BOOL isEditable = HT_IsNodeDataEditable(r2, theColumn->GetToken(), 
													theColumn->GetDataType());
		if (!isEditable)
		{
			m_iFocus = -1;
			m_iSelection = -1;
			m_Node = NULL;
			return;
		}

		HT_SetSelectedState(r2, (PRBool)TRUE);

		m_Node = r2;
	}
	
	AddTextEdit();
}


void CRDFOutliner::OnTimer(UINT nID)
{
	COutliner::OnTimer(nID);
	if (nID == IDT_EDITFOCUS) 
	{
		if (m_hEditTimer != 0)
		{
			KillTimer(m_hEditTimer);
			m_hEditTimer = 0;
		}	
		AddTextEdit();		
	}
	else if (nID == IDT_SPRINGLOAD)
	{
		if (m_hSpringloadTimer != 0) 
		{
			KillTimer(m_hSpringloadTimer);
			m_hSpringloadTimer = 0;
		
			if (m_iDragSelection == m_iSpringloadSelection)
			{
				// Open the folder
				HT_Resource r = HT_GetNthItem(m_View, m_iDragSelection);
				if (r != NULL)
				{
					// Pop it open
					HT_SetOpenState(r, (PRBool)TRUE);

					// Add this folder to our list of springloaded folders
					m_SpringloadStack.AddHead((void*)r);
				}
			}
		}
	}
	else if (nID == IDT_DRAGRECT)
	{
		if (m_hDragRectTimer != 0)
		{
			KillTimer(m_hDragRectTimer);
			m_hDragRectTimer = 0;

			CRect rect;
			GetClientRect(&rect);

			if (m_MovePoint.y < m_itemHeight)
			{
				DragRectScroll(TRUE);
				m_hDragRectTimer = SetTimer(IDT_DRAGRECT, GetDragHeartbeat(), NULL);
			}

			else if (m_MovePoint.y > rect.bottom - m_itemHeight)
			{
				DragRectScroll(FALSE);
				m_hDragRectTimer = SetTimer(IDT_DRAGRECT, GetDragHeartbeat(), NULL);
			}
		}
	}
}

// DRAG AND DROP ROUTINES
int CRDFOutliner::GetDragHeartbeat()
{
	return RDF_DRAG_HEARTBEAT;
}

COleDataSource * CRDFOutliner::GetDataSource()
{
	m_bNeedToClear = FALSE; // Hack. Just leave it here, and don't worry about it.

    COleDataSource * pDataSource = new COleDataSource;  

    HANDLE hString = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE,1);
    pDataSource->CacheGlobalData(m_cfHTNode, hString);

	// The view is sufficient
	RDFGLOBAL_BeginDrag(pDataSource, m_View);
	
    return pDataSource;
}

void RDFGLOBAL_BeginDrag(COleDataSource* pDataSource, HT_View pView)
{
	CLIPFORMAT cfBookmark = (CLIPFORMAT) RegisterClipboardFormat( NETSCAPE_HTNODE_FORMAT );
    HGLOBAL hBookmark = GlobalAlloc( GMEM_DDESHARE | GMEM_ZEROINIT, sizeof(HT_View) );
    if( hBookmark ) 
	{
        HT_View* pBookmark = (HT_View*) GlobalLock( hBookmark );
		*pBookmark = pView;
        GlobalUnlock( hBookmark);
        pDataSource->CacheGlobalData( cfBookmark, hBookmark );
    }

	// Get the selected items and set up internet shortcut
	// drag if the item is not a container or separator.
	int selCount = 0;
	HT_Resource r = HT_GetNextSelection(pView, NULL);
	while (r != NULL)
	{
		if (!HT_IsContainer(r) && !HT_IsSeparator(r))
			selCount++;
		r = HT_GetNextSelection(pView, r);
	}

	CString* titleArray = new CString[selCount];
	CString* urlArray = new CString[selCount];
	int i = -1;
	r = HT_GetNextSelection(pView, NULL);
	while (r != NULL)
	{
		if (!HT_IsContainer(r) && !HT_IsSeparator(r))
		{
			i++; // Found one
			titleArray[i] = HT_GetNodeName(r);
			urlArray[i] = HT_GetNodeURL(r);
		}
		r = HT_GetNextSelection(pView, r);
	}
	DragMultipleShortcuts(pDataSource, titleArray, urlArray, selCount);
	delete []titleArray;
	delete []urlArray;
}

void RDFGLOBAL_DragTitleAndURL( COleDataSource *pDataSource, LPCSTR title, LPCSTR url )
{
	CLIPFORMAT cfBookmark = (CLIPFORMAT) RegisterClipboardFormat( NETSCAPE_BOOKMARK_FORMAT );
    HGLOBAL hBookmark = GlobalAlloc( GMEM_DDESHARE | GMEM_ZEROINIT, sizeof(BOOKMARKITEM) );
    if( hBookmark ) {
        LPBOOKMARKITEM pBookmark = (LPBOOKMARKITEM) GlobalLock( hBookmark );
        PR_snprintf( pBookmark->szAnchor, sizeof(pBookmark->szAnchor),"%s", url );
        PR_snprintf( pBookmark->szText, sizeof(pBookmark->szText),"%s", title );
        GlobalUnlock( hBookmark);

        pDataSource->CacheGlobalData( cfBookmark, hBookmark );
    }

	HGLOBAL hString = GlobalAlloc( GMEM_MOVEABLE|GMEM_DDESHARE, strlen( url ) + 1 );
	if ( hString ) {
		LPSTR lpszString = (LPSTR) GlobalLock( hString );
		strcpy( lpszString, url );
		GlobalUnlock( hString );

		pDataSource->CacheGlobalData( CF_TEXT, hString );
    }

	// Do an internet shortcut drag
	DragInternetShortcut(pDataSource, title, url);
}


COutlinerDropTarget* CRDFOutliner::CreateDropTarget()
{
	return new CRDFDropTarget(this);
}

BOOL CRDFOutliner::HighlightIfDragging(void)
{
	if(m_iDragSelection != -1)
	{
		HT_Resource r = HT_GetNthItem(m_View, m_iDragSelection);
		if (r != NULL && HT_IsContainer(r) && (m_iDragSelectionLineThird == 2 || (GetSortColumn() != -1)))
			return TRUE;
	}
	return FALSE;
}

void CRDFOutliner::PaintDragLine(HDC hdc, CRect &rectColumn)
{
	HPEN pen = ::CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));
	HPEN pOldPen = (HPEN)::SelectObject(hdc, pen);

	if(m_iDragSelectionLineThird == 1) {
#ifdef XP_WIN32
		MoveToEx(hdc, rectColumn.left, rectColumn.top, NULL);
#else
		MoveTo(hdc, rectColumn.left, rectColumn.top);
#endif
		LineTo(hdc, rectColumn.right, rectColumn.top);
	}
	else if(m_iDragSelectionLineThird == 3){
#ifdef XP_WIN32
		MoveToEx(hdc, rectColumn.left, rectColumn.bottom -1, NULL);
#else
		MoveTo(hdc, rectColumn.left, rectColumn.bottom - 1);
#endif
		LineTo(hdc, rectColumn.right, rectColumn.bottom - 1);
	}

	::SelectObject(hdc, pOldPen);
	::DeleteObject(pen);
}

void CRDFOutliner::AcceptDrop( int iLineNo, COleDataObject *pDataObject, DROPEFFECT dropEffect )
{
	HT_Resource theNode = NULL;
	if (iLineNo >= 0)
		theNode = HT_GetNthItem(m_View, iLineNo);
	if (theNode == NULL)
		theNode = HT_TopNode(m_View);

	RDFGLOBAL_PerformDrop(pDataObject, theNode, m_iDragSelectionLineThird);

	m_SpringloadStack.RemoveAll();
}

void RDFGLOBAL_PerformDrop(COleDataObject* pDataObject, HT_Resource theNode, int dragFraction)
{
    if(!pDataObject)
        return;

	CLIPFORMAT cfHTNode = (CLIPFORMAT)RegisterClipboardFormat(NETSCAPE_HTNODE_FORMAT);
	CLIPFORMAT cfBookmark = (CLIPFORMAT)RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT);
	
    if(!pDataObject->IsDataAvailable(CF_TEXT) &&
	   !pDataObject->IsDataAvailable(cfHTNode) &&
	   !pDataObject->IsDataAvailable(cfBookmark) &&
	   !pDataObject->IsDataAvailable(CF_HDROP))
	   return;

    if(pDataObject->IsDataAvailable(cfHTNode)) 
	{
		HT_View pView = NULL;

        HGLOBAL hBookmark = pDataObject->GetGlobalData(cfHTNode);
        HT_View* pBookmark = (HT_View*)GlobalLock(hBookmark);
        ASSERT(pBookmark);
		pView = *pBookmark;
		GlobalUnlock(hBookmark);

		DropPosition dropPosition = RDFGLOBAL_TranslateDropPosition(theNode, dragFraction);

		HT_Resource node = NULL;
		while (node = (HT_GetNextSelection(pView, node)))
		{
			if (dropPosition == DROP_BEFORE)
				HT_DropHTRAtPos(theNode, node, (PRBool)TRUE);
			else if (dropPosition == DROP_AFTER)
				HT_DropHTRAtPos(theNode, node, (PRBool)FALSE);
			else if (dropPosition == DROP_ON)
				HT_DropHTROn(theNode, node);
			else HT_DropHTROn(HT_GetParent(theNode), node);
		}
	}
	else if(pDataObject->IsDataAvailable(cfBookmark)) 
	{
        HGLOBAL hBookmark = pDataObject->GetGlobalData(cfBookmark);
        LPBOOKMARKITEM pBookmark = (LPBOOKMARKITEM)GlobalLock(hBookmark);
        ASSERT(pBookmark);
		CString title(pBookmark->szText);
		CString url(pBookmark->szAnchor);

		GlobalUnlock(hBookmark);

		DropPosition dropPosition = RDFGLOBAL_TranslateDropPosition(theNode, dragFraction);
		if (dropPosition == DROP_BEFORE)
			HT_DropURLAndTitleAtPos(theNode, (char*)(const char*)url, (char*)(const char*)title, (PRBool)TRUE);
		else if (dropPosition == DROP_AFTER)
			HT_DropURLAndTitleAtPos(theNode, (char*)(const char*)url, (char*)(const char*)title, (PRBool)FALSE);
		else if (dropPosition == DROP_ON)
			HT_DropURLAndTitleOn(theNode, (char*)(const char*)url, (char*)(const char*)title);
		else HT_DropURLAndTitleOn(HT_GetParent(theNode), (char*)(const char*)url, (char*)(const char*)title);
    }
	else if (pDataObject->IsDataAvailable(CF_HDROP))
	{
		HGLOBAL hGlobal = pDataObject->GetGlobalData(CF_HDROP);
        HDROP hDropInfo = (HDROP)hGlobal;
		
		// Get the number of files dropped
		int i_num = ::DragQueryFile(hDropInfo, (UINT)-1, NULL, 0);

		char ca_drop[_MAX_PATH];
		CString csUnescapedUrl, csEscapedUrl;

		int i_loop;
		for(i_loop = 0; i_loop < i_num; i_loop++)  
		{
			::DragQueryFile(hDropInfo, i_loop, ca_drop, _MAX_PATH);

			//  Convert to an acceptable URL.
			WFE_ConvertFile2Url(csUnescapedUrl, ca_drop);
			csEscapedUrl = WFE_EscapeURL(csUnescapedUrl);

			// Tell HT about the drop
			DropPosition dropPosition = RDFGLOBAL_TranslateDropPosition(theNode, dragFraction);
			if (dropPosition == DROP_BEFORE)
				HT_DropURLAtPos(theNode, (char*)(const char*)csEscapedUrl, (PRBool)TRUE);
			else if (dropPosition == DROP_AFTER)
				HT_DropURLAtPos(theNode, (char*)(const char*)csEscapedUrl, (PRBool)FALSE);
			else if (dropPosition == DROP_ON)
				HT_DropURLOn(theNode, (char*)(const char*)csEscapedUrl);
			else HT_DropURLOn(HT_GetParent(theNode), (char*)(const char*)csEscapedUrl);
		}
		GlobalUnlock(hGlobal);
	}
    else if (pDataObject->IsDataAvailable(CF_TEXT))
	{
        HGLOBAL hString = pDataObject->GetGlobalData(CF_TEXT);
        char * pAddress = (char*)GlobalLock(hString);
        CString url(pAddress);
		GlobalUnlock(hString);

		DropPosition dropPosition = RDFGLOBAL_TranslateDropPosition(theNode, dragFraction);
		if (dropPosition == DROP_BEFORE)
			HT_DropURLAtPos(theNode, (char*)(const char*)url, (PRBool)TRUE);
		else if (dropPosition == DROP_AFTER)
			HT_DropURLAtPos(theNode, (char*)(const char*)url, (PRBool)FALSE);
		else if (dropPosition == DROP_ON)
			HT_DropURLOn(theNode, (char*)(const char*)url);
		else HT_DropURLOn(HT_GetParent(theNode), (char*)(const char*)url);
	}
	
}

void CRDFOutliner::InitializeClipFormats(void)
{
    m_cfHTNode = (CLIPFORMAT)RegisterClipboardFormat(NETSCAPE_HTNODE_FORMAT);
	m_cfBookmark = (CLIPFORMAT)RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT);
    
	m_clipFormatArray[0] = m_cfHTNode;
	m_clipFormatArray[1] = m_cfBookmark;
	m_clipFormatArray[2] = CF_HDROP;
	m_clipFormatArray[3] = CF_TEXT;
	m_clipFormatArray[4] = 0;
}

CLIPFORMAT * CRDFOutliner::GetClipFormatList(void)
{
    return m_clipFormatArray;
}

DROPEFFECT CRDFOutliner::DropSelect(int iLineNo, COleDataObject *object)
{
	HT_Resource r = NULL;
	if (iLineNo >= 0)
		r = HT_GetNthItem(m_View, iLineNo);
	if (r == NULL)
		r = HT_TopNode(m_View);

	DROPEFFECT answer = RDFGLOBAL_TranslateDropAction(r, object, 
		HT_IsContainer(r) ? m_iDragSelectionLineThird : m_iDragSelectionLineHalf);
	
	m_iCurrentDropAction = answer;

    if (iLineNo == m_iDragSelection && !m_bDragSectionChanged)
        return answer;

    if (m_iDragSelection != -1)
        InvalidateLine (m_iDragSelection);

    m_iDragSelection = iLineNo;
    InvalidateLine (m_iDragSelection);
    
	// Start the hover timer for folder springloading
	
	if (m_iDragSelection != -1)
	{
		if (r != NULL && HT_IsContainer(r) && !HT_IsContainerOpen(r) && m_iCurrentDropAction != DROPEFFECT_NONE)
		{
	 		m_iSpringloadSelection = m_iDragSelection;
			m_hSpringloadTimer = SetTimer(IDT_SPRINGLOAD, SPRINGLOAD_DELAY, NULL);
		}
	}

	// Check for springload closing time
	if (!m_SpringloadStack.IsEmpty())
	{
		// There are some springloaded folders that might need to be closed.
		POSITION p = m_SpringloadStack.GetHeadPosition();
		HT_Resource r = (HT_Resource)m_SpringloadStack.GetNext(p);

		int count = (int)HT_GetCountVisibleChildren(r);
		int index = (int)HT_GetNodeIndex(m_View, r);
		
		if (m_iDragSelection < index ||
			m_iDragSelection == -1 ||
			m_iDragSelection > index + count)
		{
			m_SpringloadStack.RemoveHead();
			HT_SetOpenState(r, (PRBool)FALSE);
		
			if (m_iDragSelection > index + count)
				m_iDragSelection -= count;
		}
	}
	return answer;
}

DropPosition RDFGLOBAL_TranslateDropPosition(HT_Resource dropTarget, int position)
{
	HT_Resource targetParent = HT_GetParent(dropTarget);

	BOOL sortImposed = !HT_ContainerSupportsNaturalOrderSort(targetParent); // Is a sort imposed on the parent?

	DropPosition res;
	if (targetParent == NULL) // Top Node of the view.  Can't drop before or after.
		res = DROP_ON;
	else if (!sortImposed) // Can only drop/before or after if sort is not imposed on the view.
	{
		if (position == 1)
		{
			// Drop before
			res = DROP_BEFORE; // In upper half of selection (for non-containers) or bottom third 
							   // of selection (for containers).  Drop before.
		}
		else if (position == 3 || (position == 2 && !HT_IsContainer(dropTarget)))
		{
			// Drop after if we're in the bottom half (for non-containers) or bottom third (for containers)
			res = DROP_AFTER;  // In lower third of selection.  Drop after.
		}
		else res = DROP_ON;  // We're a container and right in the middle of selection.  Drop on.
	}
	else if (HT_IsContainer(dropTarget))
	{
		res = DROP_ON; // If a sort is imposed and we're over a container, drop into that.
	}
	else res = DROP_ON_PARENT;  // Otherwise, we'll drop into the parent container and let the sort
								// put it where it's supposed to go.

	return res;
}

DROPEFFECT RDFGLOBAL_TranslateDropAction(HT_Resource dropTarget, COleDataObject* pDataObject, 
										 int position)
{
	HT_DropAction res;
	CLIPFORMAT cfHTNode = (CLIPFORMAT)RegisterClipboardFormat(NETSCAPE_HTNODE_FORMAT);
	CLIPFORMAT cfBookmark = (CLIPFORMAT)RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT);

	if (dropTarget == NULL)
		return DROPEFFECT_NONE;

	DropPosition dropPosition = RDFGLOBAL_TranslateDropPosition(dropTarget, position);
		
	if(pDataObject->IsDataAvailable(cfHTNode)) 
	{
		HT_View pView = NULL;

        HGLOBAL hBookmark = pDataObject->GetGlobalData(cfHTNode);
        HT_View* pBookmark = (HT_View*)GlobalLock(hBookmark);
        ASSERT(pBookmark);
		pView = *pBookmark;
		GlobalUnlock(hBookmark);

		HT_Resource node = HT_GetNextSelection(pView, NULL); 		
		while (node != NULL)
		{
			HT_DropAction res2;
			if (dropPosition == DROP_BEFORE)
				res2 = HT_CanDropHTRAtPos(dropTarget, node, (PRBool)TRUE); 
			else if (dropPosition == DROP_AFTER)
				res2 = HT_CanDropHTRAtPos(dropTarget, node, (PRBool)FALSE);
			else if (dropPosition == DROP_ON)
				res2 = HT_CanDropHTROn(dropTarget, node);
			else res2 = HT_CanDropHTROn(HT_GetParent(dropTarget), node);

			if (res2 == DROP_NOT_ALLOWED)
			{
				res = DROP_NOT_ALLOWED;
				break;
			}
			
			res = res2;

			node = HT_GetNextSelection(pView, node);
		}
	}
	else if (pDataObject->IsDataAvailable(cfBookmark))
	{
		HGLOBAL hBookmark = pDataObject->GetGlobalData(cfBookmark);
        LPBOOKMARKITEM pBookmark = (LPBOOKMARKITEM)GlobalLock(hBookmark);
        ASSERT(pBookmark);
		CString title(pBookmark->szText);
		CString url(pBookmark->szAnchor);

		GlobalUnlock(hBookmark);
		
		if (dropPosition == DROP_BEFORE)
		  res = HT_CanDropURLAtPos(dropTarget, (char*)(const char*)url, (PRBool)TRUE); 
		else if (dropPosition == DROP_AFTER)
		  res = HT_CanDropURLAtPos(dropTarget, (char*)(const char*)url, (PRBool)FALSE);
		else if (dropPosition == DROP_ON)
		  res = HT_CanDropURLOn(dropTarget, (char*)(const char*)url);
		else res = HT_CanDropURLOn(HT_GetParent(dropTarget), (char*)(const char*)url);
	}
	else if (pDataObject->IsDataAvailable(CF_HDROP))
	{
		HGLOBAL hGlobal = pDataObject->GetGlobalData(CF_HDROP);
        HDROP hDropInfo = (HDROP)hGlobal;
		
		// Get the number of files dropped
		int i_num = ::DragQueryFile(hDropInfo, (UINT)-1, NULL, 0);

		char ca_drop[_MAX_PATH];
		CString csUnescapedUrl, csEscapedUrl;

		int i_loop;
		HT_DropAction res2;
		for(i_loop = 0; i_loop < i_num; i_loop++)  
		{
			::DragQueryFile(hDropInfo, i_loop, ca_drop, _MAX_PATH);

			//  Convert to an acceptable URL.
			WFE_ConvertFile2Url(csUnescapedUrl, ca_drop);
			csEscapedUrl = WFE_EscapeURL(csUnescapedUrl);

			// Tell HT about the drop
			if (dropPosition == DROP_BEFORE)
				res2 = HT_CanDropURLAtPos(dropTarget, (char*)(const char*)csEscapedUrl, (PRBool)TRUE);
			else if (dropPosition == DROP_AFTER)
				res2 = HT_CanDropURLAtPos(dropTarget, (char*)(const char*)csEscapedUrl, (PRBool)FALSE);
			else if (dropPosition == DROP_ON)
				res2 = HT_CanDropURLOn(dropTarget, (char*)(const char*)csEscapedUrl);
			else res2 = HT_CanDropURLOn(HT_GetParent(dropTarget), (char*)(const char*)csEscapedUrl);

			if (res2 == DROP_NOT_ALLOWED)
			{
				res = DROP_NOT_ALLOWED;
				break;
			}
			
			res = res2;
		}
		GlobalUnlock(hGlobal);
	}
	else if (pDataObject->IsDataAvailable(CF_TEXT))
	{
        HGLOBAL hString = pDataObject->GetGlobalData(CF_TEXT);
        char * pAddress = (char*)GlobalLock(hString);
        CString url(pAddress);
		GlobalUnlock(hString);

		if (dropPosition == DROP_BEFORE)
		  res = HT_CanDropURLAtPos(dropTarget, (char*)(const char*)url, (PRBool)TRUE); 
		else if (dropPosition == DROP_AFTER)
		  res = HT_CanDropURLAtPos(dropTarget, (char*)(const char*)url, (PRBool)FALSE);
		else if (dropPosition == DROP_ON)
		  res = HT_CanDropURLOn(dropTarget, (char*)(const char*)url);
		else res = HT_CanDropURLOn(HT_GetParent(dropTarget), (char*)(const char*)url);
	}

	switch (res)
	{
		case DROP_NOT_ALLOWED:
			return DROPEFFECT_NONE;
		case COPY_MOVE_CONTENT:
		case COPY_MOVE_LINK:
			return DROPEFFECT_MOVE;
		default:
			return DROPEFFECT_COPY;
	}
}

// RDFDropTarget Overridden Funcs

BOOL CRDFDropTarget::OnDrop(
    CWnd* pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point)
{
	m_dwOldTicks = 0;

	CRDFOutliner* pOutliner = (CRDFOutliner*)m_pOutliner;

    if(!pDataObject || !pOutliner)
        return(FALSE);

    if (!pOutliner->RecognizedFormat(pDataObject))
        return FALSE;

	pOutliner->m_ptHit = point;

    pOutliner->AcceptDrop( point.y < 0 ? -1 : pOutliner->GetDropLine(), pDataObject, dropEffect);
    pOutliner->EndDropSelect();

    return(TRUE);
}

DROPEFFECT CRDFDropTarget::OnDragEnter(
    CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
{
	DROPEFFECT res = OnDragOver(pWnd, pDataObject, dwKeyState, point);

	CRDFOutliner* pOutliner = (CRDFOutliner*)m_pOutliner;
	pOutliner->m_bDataSourceInWindow = TRUE;

	if ( res != DROPEFFECT_NONE && point.y >= 0) {
		m_pOutliner->Invalidate();
		m_pOutliner->UpdateWindow();
	}

	return res;
}

DROPEFFECT CRDFDropTarget::OnDragOver(
    CWnd* pWnd, COleDataObject* pDataObject, DWORD dwKeyState, CPoint point)
{
    DROPEFFECT effect = DROPEFFECT_NONE;
	CRDFOutliner* pOutliner = (CRDFOutliner*)m_pOutliner;

    if (pOutliner->RecognizedFormat(pDataObject)) {
		RECT rect;
		pWnd->GetClientRect(&rect);

		if ( point.y < pOutliner->m_itemHeight && point.y >= 0) {
			DragScroll(TRUE);
			effect |= DROPEFFECT_SCROLL;
		} else if ( point.y > ( rect.bottom - pOutliner->m_itemHeight ) ) {
			DragScroll(FALSE);
			effect |= DROPEFFECT_SCROLL;
		} else {
			m_dwOldTicks = 0;
		}

		pOutliner->m_ptHit = point;

		effect |= pOutliner->DropSelect(point.y < 0 ? -1 : pOutliner->LineFromPoint(point), pDataObject);

		// Default is to move
		if ( ( effect & DROPEFFECT_MOVE) && (dwKeyState & MK_CONTROL) )
			effect = (effect & ~DROPEFFECT_MOVE) | DROPEFFECT_COPY;
	}
    return effect;
}

void CRDFDropTarget::OnDragLeave(CWnd* pWnd)
{
	CRDFOutliner* pOutliner = (CRDFOutliner*)m_pOutliner;
	for (POSITION p = pOutliner->m_SpringloadStack.GetHeadPosition(); p != NULL; )
	{
		HT_Resource r = (HT_Resource)(pOutliner->m_SpringloadStack.GetNext(p));
		HT_SetOpenState(r, (PRBool)FALSE);
	}
			 
	pOutliner->m_SpringloadStack.RemoveAll();

	pOutliner->m_bDataSourceInWindow = FALSE;

	pOutliner->m_iDragSelection = -1;

	pOutliner->Invalidate();
	pOutliner->UpdateWindow();
}

// ========================================================================

BEGIN_MESSAGE_MAP(CRDFOutlinerParent,COutlinerParent)
    ON_WM_DESTROY()
END_MESSAGE_MAP()

CRDFOutlinerParent::CRDFOutlinerParent(HT_Pane thePane, HT_View theView)
{
	CRDFOutliner* theOutliner = new CRDFOutliner(thePane, theView, this);
	m_pOutliner = theOutliner;
}

BOOL CRDFOutlinerParent::PreCreateWindow(CREATESTRUCT& cs)
{
    BOOL bRet = CWnd::PreCreateWindow(cs);

//#ifdef XP_WIN32
//    cs.dwExStyle |= WS_EX_CLIENTEDGE;
//#endif
    return bRet;
}

void CRDFOutlinerParent::OnDestroy()
{
	// Save prefs might happen here...
	COutlinerParent::OnDestroy();
}

COutliner* CRDFOutlinerParent::GetOutliner ( )
{
	return m_pOutliner;
}

void CRDFOutlinerParent::CreateColumns ( void )
{
	// Retrieve the columns from RDF
	CRDFOutliner* theOutliner = (CRDFOutliner*)m_pOutliner;
	HT_View theView = theOutliner->GetHTView();
	
	HT_Cursor columnCursor = HT_NewColumnCursor(theView);
	char* columnName;
	uint32 columnWidth;
	void* token;
	uint32 tokenType;
	UINT index = 0;
	while (HT_GetNextColumn(columnCursor, &columnName, &columnWidth, &token, &tokenType))
	{
		// We have retrieved a new column.  Contruct a front end column object
		CRDFColumn* newColumn = new CRDFColumn(columnName, columnWidth, token, tokenType);
		index = (UINT)columnMap.AddCommand(newColumn);
		m_pOutliner->AddColumn(columnName, index, 5000, 10000, ColumnVariable, 3000);
	}
	HT_DeleteColumnCursor(columnCursor);
	m_pOutliner->SetVisibleColumns(1); // For now... TODO: Get visible/invisible info!
	m_pOutliner->SetImageColumn(0);
}

BOOL CRDFOutlinerParent::RenderData( int iColumn, CRect & rect, CDC &dc, LPCTSTR text )
{
	CRDFOutliner *pOutliner = (CRDFOutliner *)m_pOutliner;
    
    if( !pOutliner )
    {
       return FALSE;
    }
     
	if( pOutliner->GetSortType() == HT_NO_SORT)
    {
       return FALSE;
    }
    
	MSG_COMMAND_CHECK_STATE sortType = (iColumn == pOutliner->GetSortColumn()) ? MSG_Checked : MSG_Unchecked;
    
    if( sortType != MSG_Checked )
    {
       return FALSE;
    }

	int idxImage = (pOutliner->GetSortType() % 2) ? IDX_SORTINDICATORUP : IDX_SORTINDICATORDOWN;

	UINT dwDTFormat = DT_NOCLIP | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER;
	RECT rectText = rect;
	rectText.left += 4;
	rectText.right -= 5;

	rectText.right -= 22;
    m_pIImage->DrawTransImage( idxImage,
							   rectText.right + 4,
							   (rect.top + rect.bottom) / 2 - 4,
							   &dc );

	WFE_DrawTextEx( 0, dc.m_hDC, (LPTSTR) text, -1, 
					&rectText, dwDTFormat, WFE_DT_CROPRIGHT );

    return TRUE;
}

BOOL CRDFOutlinerParent::ColumnCommand( int iColumn )
{	
	CRDFOutliner *pOutliner = (CRDFOutliner *)m_pOutliner;
    
	int enSortType = pOutliner->GetSortType();
	if (pOutliner->GetSortColumn() != iColumn)
	{
		// May need to resort in back end (not sure)
		enSortType = HT_NO_SORT;
	}

    if (enSortType == HT_SORT_ASCENDING)
		enSortType = HT_SORT_DESCENDING;
	else if (enSortType == HT_SORT_DESCENDING)
		enSortType = HT_NO_SORT;
	else enSortType = HT_SORT_ASCENDING;

    pOutliner->SetSortType( enSortType );
	
	if (enSortType == HT_NO_SORT)
		pOutliner->SetSortColumn(-1);
	else pOutliner->SetSortColumn( iColumn );
     
	CRDFColumn* theColumn = (CRDFColumn*)(columnMap.GetCommand(iColumn));
		
	BOOL sort = TRUE;
	if (enSortType == HT_SORT_ASCENDING)
		sort = FALSE;

	pOutliner->SetSelectedColumn(pOutliner->m_pColumn[ iColumn ]->iCommand);

	Invalidate();

	HT_SetSortColumn(pOutliner->GetHTView(), 
		             enSortType == HT_NO_SORT ? NULL : theColumn->GetToken(), 
					 theColumn->GetDataType(), (PRBool)sort);
    
	return TRUE;
}

void CRDFOutlinerParent::Initialize()
{
	CreateColumns ( );

	CRDFOutliner* theOutliner = (CRDFOutliner*)m_pOutliner;
	m_pOutliner->SetTotalLines( HT_GetItemListCount(theOutliner->GetHTView()) );
}

// =======================================================================
// RDFEditWnd - Class that controls the in-place editing of column values
// =======================================================================

BOOL CRDFEditWnd::PreTranslateMessage ( MSG * msg )
{
   if ( msg->message == WM_KEYDOWN)
   {
      switch(msg->wParam)
      {
         case VK_RETURN:
         {
			m_pOwner->EditTextChanged(m_pOwner->GetTextEditText());
			m_pOwner->RemoveTextEdit();
            return TRUE;

         }
		 case VK_ESCAPE:
			 m_pOwner->RemoveTextEdit();
			 break;
		 case VK_TAB:
			 m_pOwner->EditTextChanged(m_pOwner->GetTextEditText());
			 m_pOwner->MoveToNextColumn();
			 return TRUE;
	  }
   }
   
   return CEdit::PreTranslateMessage ( msg );
}

//////////////////////////////////////////////////////////////////////////
//					Messages for CRDFEditWnd
//////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CRDFEditWnd, CEdit)
	//{{AFX_MSG_MAP(CWnd)
 	ON_WM_DESTROY()
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CRDFEditWnd::OnDestroy( )
{
	delete this;

}
  
void CRDFEditWnd::OnSetFocus(CWnd* pOldWnd)
{
	CEdit::OnSetFocus(pOldWnd);
}

void CRDFEditWnd::OnKillFocus( CWnd* pNewWnd )
{
	CEdit::OnKillFocus(pNewWnd);
	m_pOwner->FocusCheck(pNewWnd, FALSE);
	m_pOwner->RemoveTextEdit(); 
}

// =========================================================================
// RDF Content View
IMPLEMENT_DYNAMIC(CRDFContentView, CContentView)
BEGIN_MESSAGE_MAP(CRDFContentView, CContentView)
	//{{AFX_MSG_MAP(CMainFrame)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	ON_MESSAGE(WM_NAVCENTER_QUERYPOSITION, OnNavCenterQueryPosition)
	ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_SETFOCUS()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

LRESULT CRDFContentView::OnNavCenterQueryPosition(WPARAM wParam, LPARAM lParam)
{
    NAVCENTPOS *pPos = (NAVCENTPOS *)lParam;
    
    //  We like being in the middle.
    pPos->m_iYDisposition = 0;
    
    //  We like being this many units in size.
    pPos->m_iYVector = 200;
    
    //  Handled.
    return(NULL);
}

BOOL CRDFContentView::PreCreateWindow(CREATESTRUCT& cs)
{
	cs.style |= WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

	return CContentView::PreCreateWindow(cs);
}

void CRDFContentView::OnDraw ( CDC * pDC )
{
}

int CRDFContentView::OnCreate ( LPCREATESTRUCT lpCreateStruct )
{
    int iRetVal = CContentView::OnCreate ( lpCreateStruct );
	LPCTSTR lpszClass = AfxRegisterWndClass( CS_VREDRAW, ::LoadCursor(NULL, IDC_ARROW));
    m_pOutlinerParent->Create( lpszClass, _T("NSOutlinerParent"), 
							   WS_VISIBLE|WS_CHILD|WS_CLIPCHILDREN,
							   CRect(0,0,100,100), this, 101 );
    return iRetVal;
}

void CRDFContentView::OnSize ( UINT nType, int cx, int cy )
{
	CView::OnSize ( nType, cx, cy );
	if (IsWindow(m_pOutlinerParent->m_hWnd)) 
	{
		m_pOutlinerParent->MoveWindow ( 0, 0, cx, cy );
	}
}

void CRDFContentView::InvalidateOutlinerParent()
{
//	COutliner* outliner = m_pOutlinerParent->GetOutliner();
//	outliner->SqueezeColumns(-1, 0, TRUE);
}

void CRDFContentView::OnSetFocus ( CWnd * pOldWnd )
{
   m_pOutlinerParent->SetFocus ( );
}

// Functionality for the RDF Tree Embedded in HTML (Added 3/10/98 by Dave Hyatt).

// The event handler.  Only cares about tree events, so we'll just pass everything to the
// tree view.
void embeddedTreeNotifyProcedure (HT_Notification ns, HT_Resource n, HT_Event whatHappened) 
{
	CRDFOutliner* theOutliner = (CRDFOutliner*)HT_GetViewFEData(HT_GetView(n));
	if (theOutliner)
		theOutliner->HandleEvent(ns, n, whatHappened);
}

void CRDFContentView::DisplayRDFTree(CWnd* pParent, int width, int height, RDF_Resource rdfResource)
{
	HT_Notification ns = new HT_NotificationStruct;
	ns->notifyProc = embeddedTreeNotifyProcedure;
	ns->data = NULL;
	
	// Construct the pane and give it our notification struct
	HT_Pane thePane = HT_PaneFromResource(rdfResource, ns, PR_FALSE);
	
	// Build our FE windows.
	CRDFOutlinerParent* newParent = new CRDFOutlinerParent(thePane, HT_GetSelectedView(thePane));
	CRDFContentView* newView = new CRDFContentView(newParent);

	// Create the windows
	CRect rClient(0, 0, width, height);
	newView->Create( NULL, "", WS_CHILD | WS_VISIBLE, rClient, pParent, NC_IDW_OUTLINER);

	// Initialize the columns, etc.
	newParent->Initialize();

	// Set our FE data to be the outliner.
	HT_SetViewFEData(HT_GetSelectedView(thePane), newParent->GetOutliner());
}

// Function to grab an RDF context

extern "C" MWContext* FE_GetRDFContext(void) 
{
    MWContext *pRetval = NULL;
    
    if(theApp.m_pRDFCX) 
	{
        pRetval = theApp.m_pRDFCX->GetContext();
    }
    
    return(pRetval);
}


// The context's GetDialogOwner() function.
CWnd* CRDFCX::GetDialogOwner() const
{
	if (m_pCurrentRDFWindow == NULL)
		return CWnd::GetDesktopWindow();
	return m_pCurrentRDFWindow->GetTopLevelFrame();
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CDialogPRMT dialog

void stdPrint (void* data, char* str) {
  fprintf((FILE*)data, str);
}

CExportRDF::CExportRDF(CWnd* pParent /*=NULL*/)
    : CDialog(CExportRDF::IDD, pParent)
{
    m_csTitle = _T("");
    
    //{{AFX_DATA_INIT(CDialogPRMT)
    m_csAns = "";
    //}}AFX_DATA_INIT
}

void CExportRDF::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CDialogPRMT)
    DDX_Text(pDX, IDC_PROMPT_ANS, m_csAns);
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CExportRDF, CDialog)
    //{{AFX_MSG_MAP(CDialogPRMT)
        // NOTE: the ClassWizard will add message map macros here
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()
