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
#include "xp.h"
#include "forward.h"
#include "compfile.h"
#include "compfrm.h"
#include "compbar.h"
#include "ole2.h"
#include "abdefn.h"
#include "wfemsg.h"
#include "mailmisc.h"
#include "nsadrlst.h"
#include "addrfrm.h"

extern char* wfe_ConstructFilterString(int type);
extern char* XP_NetToDosFileName(const char * NetName);

#define  BITMAP_WIDTH   16
#define  BITMAP_HEIGHT  16

BEGIN_MESSAGE_MAP(CNSAttachmentList, CListBox)
    ON_WM_CREATE()
    ON_WM_KEYDOWN()
    ON_WM_LBUTTONDOWN()
    ON_WM_DROPFILES()
    ON_WM_DESTROY()
	ON_COMMAND(ID_EDIT_DELETE,OnDelete)
END_MESSAGE_MAP()

CNSAttachmentList::CNSAttachmentList(MSG_Pane * pPane) 
{
    m_pPane = pPane;    
    m_pDropTarget = NULL;
}

CNSAttachmentList::~CNSAttachmentList()
{
   if (m_pDropTarget)
   {
      m_pDropTarget->Revoke();
      delete m_pDropTarget;
   }
}

BOOL CNSAttachmentList::Create(CWnd * pWnd, UINT id)
{
   BOOL bRetVal = CListBox::Create (
      WS_CLIPCHILDREN|WS_CHILD|WS_VISIBLE|WS_VSCROLL|LBS_OWNERDRAWFIXED|
      LBS_HASSTRINGS|LBS_NOTIFY|LBS_WANTKEYBOARDINPUT|LBS_NOINTEGRALHEIGHT,
      CRect(0,0,0,0), pWnd, id);
   return bRetVal;
}

void CNSAttachmentList::OnDelete()
{
	if (GetFocus() == this)
	{
	    int idx = GetCurSel();
		if (idx != LB_ERR)
        {
			DeleteString(idx);
            CComposeBar * pComposeBar = (CComposeBar*)GetParent();
            pComposeBar->UpdateAttachmentInfo(GetCount());
            UpdateAttachments();
        }
	}
}

void CNSAttachmentList::OnUpdateDelete(CCmdUI * pCmdUI)
{
	if (GetFocus() == this)
	    pCmdUI->Enable(GetCount()>0);
}

int CNSAttachmentList::OnCreate( LPCREATESTRUCT lpCreateStruct )
{
   int iRet = CListBox::OnCreate(lpCreateStruct);

    CComposeFrame * pFrame = (CComposeFrame*)GetParentFrame();
    CDC * pdc = GetDC();

    LOGFONT lf;			
    memset(&lf,0,sizeof(LOGFONT));
    lf.lfPitchAndFamily = FF_SWISS;
    if (INTL_DefaultWinCharSetID(0) == CS_LATIN1)
	    strcpy(lf.lfFaceName, "MS Sans Serif");
    else
	    strcpy(lf.lfFaceName, IntlGetUIPropFaceName(0));
    lf.lfCharSet = IntlGetLfCharset(pFrame->m_iCSID);
    lf.lfHeight = -MulDiv(8,pdc->GetDeviceCaps(LOGPIXELSY), 72);
    lf.lfQuality = PROOF_QUALITY;    
	m_cfTextFont = theApp.CreateAppFont( lf );
	::SendMessage(GetSafeHwnd(), WM_SETFONT, (WPARAM)m_cfTextFont, FALSE);

   ReleaseDC(pdc);


   BOOL bDisableButtons = TRUE;
   if(GetMsgPane()) 
   {
        const MSG_AttachmentData * pDataList = MSG_GetAttachmentList(GetMsgPane());
        if(pDataList) 
        {
            bDisableButtons = FALSE;
            for (int i = 0; pDataList[i].url!=NULL; i++) 
            {
				int idx;
                MSG_AttachmentData * pEntry = 
                    (MSG_AttachmentData*)XP_CALLOC(1,sizeof(MSG_AttachmentData));
                ASSERT(pEntry);
                pEntry->url = XP_STRDUP(pDataList[i].url);
				if (pDataList[i].real_type) 
					pEntry->real_type = XP_STRDUP(pDataList[i].real_type);
				if (pDataList[i].description) 
					pEntry->description = XP_STRDUP(pDataList[i].description);
				if (pDataList[i].real_name) 
					pEntry->real_name = XP_STRDUP(pDataList[i].real_name);
				if (pDataList[i].real_name)
					idx = AddString (pEntry->real_name);
				else
					idx = AddString(pEntry->url);
#ifdef DEBUG_bienvenu
				MSG_MessageLine msgLine;
				int ret = MSG_GetMessageLineForURL(WFE_MSGGetMaster(), pEntry->url, &msgLine);
#endif
                ASSERT(idx!=LB_ERR);
                SetItemData(idx,(DWORD)pEntry);
                ASSERT(pEntry->url);
                if (pDataList[i].desired_type) 
                {
                    pEntry->desired_type = XP_STRDUP(pDataList[i].desired_type);
                    ASSERT(pEntry->desired_type);
                }
            }
            SetCurSel(0);
        }
   }

   if(!m_pDropTarget) {
       m_pDropTarget = new CNSAttachDropTarget;
       m_pDropTarget->Register(this);
   }

   DragAcceptFiles();

   return(iRet);
}

void CNSAttachmentList::AttachUrl(char * pUrl)
{
    CWnd * pFocusWnd = GetFocus();
    CLocationDlg LocationDlg(pUrl,this);
    if (LocationDlg.DoModal()==IDOK)
    {
        MSG_AttachmentData * pAttach = 
            (MSG_AttachmentData *)XP_CALLOC(1,sizeof(MSG_AttachmentData));
        ASSERT(pAttach);
        pAttach->url = XP_STRDUP(LocationDlg.m_Location);
        ASSERT(pAttach->url);
        int idx = AddString(pAttach->url);
        ASSERT(idx!=LB_ERR);
        SetItemData(idx,(DWORD)pAttach);
        SetCurSel(idx);
        CComposeBar * pComposeBar = (CComposeBar*)GetParent();
        pComposeBar->UpdateAttachmentInfo(GetCount());
        UpdateAttachments();
    }
    pFocusWnd->SetFocus();
}

void CNSAttachmentList::AddAttachment(char * pName)
{
    if(pName) 
    {
        MSG_AttachmentData * pAttach = 
            (MSG_AttachmentData *)XP_CALLOC(1,sizeof(MSG_AttachmentData));
        CString cs;
        WFE_ConvertFile2Url(cs,(const char *)pName);
        pAttach->url = XP_STRDUP(cs);
        ASSERT(pAttach->url);

        int idx = AddString(pAttach->url);
        CComposeBar * pComposeBar = (CComposeBar*)GetParent();
        pComposeBar->UpdateAttachmentInfo(GetCount());
        ASSERT(idx!=LB_ERR);
        SetItemData(idx,(DWORD)pAttach);
        SetCurSel(idx);
    }
}

void CNSAttachmentList::AttachFile() 
{
    CWnd * pFocusWnd = GetFocus();
    CString cs;
    cs.LoadString(IDS_FILETOATTACH);
    char * pName = GetAttachmentName( (char*)((const char *)cs), ALL, TRUE);
	char * part = NULL;
	char * filename = NULL;
	char * full_path = NULL;
	if (pName) {
		// this assumes that there are space separators between filenames
		// if the dialog getting the filenames changes to explorer style
		// then this should change
		if (part = strstr(pName, " ")) {
			full_path = (char *) XP_ALLOC(_MAX_PATH * sizeof(char));
			if(!full_path){
				XP_FREEIF(pName);
				return;
			}
			*part = NULL;
			part++;
			while (part) {
				XP_STRCPY (full_path, "");
				XP_STRCAT (full_path, pName);
				if (*(full_path + strlen (full_path) - 1) != '\\')
					XP_STRCAT (full_path, "\\");
				filename = strstr(part, " ");
				if (filename) {
					*filename = NULL;
					filename++;
				}
				XP_STRCAT (full_path, part);
				AddAttachment(full_path);
				part = filename;
			}
			XP_FREE(full_path);
		}
		else
			AddAttachment(pName);

	}
    if (pName)
       XP_FREE(pName);
    pFocusWnd->SetFocus();
    UpdateAttachments();
}

void CNSAttachmentList::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	switch (nChar) 
	{
        case VK_INSERT:
            AttachFile();
            break;

        case VK_DELETE:
            RemoveAttachment();
            break;
	}
	CListBox::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CNSAttachmentList::RemoveAttachment() 
{
    int idx = GetCurSel();
    if (idx != LB_ERR) 
    {
        MSG_AttachmentData * pAttach = 
            (MSG_AttachmentData*)GetItemData(idx);
        ASSERT(pAttach);
        if (pAttach == NULL)
            return;
        XP_FREE(pAttach);
        DeleteString(idx);
        CComposeBar * pComposeBar = (CComposeBar*)GetParent();
        pComposeBar->UpdateAttachmentInfo(GetCount());
        if (idx >= GetCount())
            SetCurSel(idx-1);
        UpdateAttachments();
    }
}

void CNSAttachmentList::Cleanup()
{
    int iCount = GetCount();
    for (int i = 0; i<iCount; i++) {
        MSG_AttachmentData * pAttach = 
            (MSG_AttachmentData*)GetItemData(i);
        if (pAttach != NULL)
        {
            if (pAttach->url)
                XP_FREE((void*)pAttach->url);
            if (pAttach->desired_type)
                XP_FREE((void*)pAttach->desired_type);
		    if (pAttach->real_type)
                XP_FREE((void*)pAttach->real_type);
		    if (pAttach->description)
                XP_FREE((void*)pAttach->description);
		    if (pAttach->real_name)
                XP_FREE((void*)pAttach->real_name);
        }
        SetItemData(i,NULL);
    }
}

void CNSAttachmentList::UpdateAttachments()
{
    int iCount = GetCount();
    if (iCount != LB_ERR)
    {
        MSG_AttachmentData * pAttachList = 
            (MSG_AttachmentData *)XP_CALLOC(iCount+1,sizeof(MSG_AttachmentData));
        if (!pAttachList)
            return;
        for (int i = 0; i<iCount; i++) {
            MSG_AttachmentData * pAttach = 
                (MSG_AttachmentData*)GetItemData(i);
            if (pAttach != NULL)
                XP_MEMCPY(&pAttachList[i],pAttach,sizeof(MSG_AttachmentData));
        }
        ASSERT(GetMsgPane());
        MSG_SetAttachmentList(GetMsgPane(),pAttachList);
        XP_FREE(pAttachList);
    }
}

char * CNSAttachmentList::GetAttachmentName(
    char * prompt, int type, XP_Bool bMustExist, BOOL * pOpenIntoEditor)
{

    OPENFILENAME fname;
    char * full_path = NULL;
    char   name[_MAX_FNAME];

    char* filter = wfe_ConstructFilterString(type);

    /* initialize the OPENFILENAME struct */
    
    BOOL result;
    UINT index = (type == HTM_ONLY) ? 1 : type;

    // space for the full path name    
    full_path = (char *) XP_ALLOC(_MAX_PATH * sizeof(char));
    if(!full_path){
        XP_FREE(filter);
        return(NULL);
    }
    name[0]      = '\0';
    full_path[0] = '\0';

    // set up the entries
    fname.lStructSize = sizeof(OPENFILENAME);
    fname.hwndOwner = m_hWnd;
    fname.lpstrFilter = filter;
    fname.lpstrCustomFilter = NULL;
    fname.nFilterIndex = index;
    fname.lpstrFile = full_path;
    fname.nMaxFile = _MAX_PATH;
    fname.lpstrFileTitle = name;
    fname.nMaxFileTitle = _MAX_FNAME;
    fname.lpstrInitialDir = NULL;
    fname.lpstrTitle = prompt;
    fname.Flags = OFN_HIDEREADONLY;
    fname.lpstrDefExt = NULL;

    if(bMustExist)
        fname.Flags |= OFN_FILEMUSTEXIST;

	if (!pOpenIntoEditor)
		fname.Flags |= OFN_ALLOWMULTISELECT;

    result = FEU_GetOpenFileName(&fname);

    XP_FREE(filter);

    // see if the user selects a file or hits cancel    
    if(result) {
        return(full_path);
    } else {
        // user hit cancel
        if(full_path) XP_FREE(full_path);
        return(NULL);
    }
}

UINT CNSAttachmentList::ItemFromPoint(CPoint pt, BOOL& bOutside) const
{
	RECT rect;
	GetClientRect(&rect);

	int iHeight = GetItemHeight(0);
	int iCount = GetCount();
	int iTopIndex = GetTopIndex();

	int iListHeight = iHeight * ( iCount - iTopIndex );
	rect.bottom = rect.bottom < iListHeight ? rect.bottom : iListHeight;

	bOutside = !::PtInRect(&rect, pt);

	if ( bOutside ) {
		return 0;
	} 

	return (pt.y / iHeight) + iTopIndex; 
}

//=============================================================== OnLButtonDown
void CNSAttachmentList::OnLButtonDown(UINT nFlags, CPoint point) 
{
	CListBox::OnLButtonDown(nFlags, point);
	BOOL bOutside;
    if (!GetCount())
    {
        AttachFile();
        return;
    }
	int nNewSelect = ItemFromPoint( point, bOutside );
	CRect rect;
	GetItemRect(nNewSelect,rect);
    if (((point.y+rect.Height())/rect.Height())+GetTopIndex()>GetCount())
    {
        AttachFile();
    }
}

/////////////////////////////////////////////////////////////////////////////
// CLocationDlg dialog


CLocationDlg::CLocationDlg(char * pUrl, CWnd* pParent /*=NULL*/)
	: CDialog(CLocationDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CLocationDlg)
    m_Location = pUrl;
	//}}AFX_DATA_INIT
}


void CLocationDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CLocationDlg)
	DDX_Control(pDX, IDC_EDIT1, m_LocationBox);
	DDX_Text(pDX, IDC_EDIT1, m_Location);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CLocationDlg, CDialog)
	//{{AFX_MSG_MAP(CLocationDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CLocationDlg message handlers


BOOL CLocationDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	// TODO: Add extra initialization here
	
   m_LocationBox.SetFocus();	
   m_LocationBox.SetSel(0,-1);

	return FALSE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL wfe_GetBookmarkData( COleDataObject* pDataObject, char ** ppURL, char ** ppTitle );


BOOL CNSAttachmentList::ProcessDropTarget(COleDataObject * pDataObject)
{
   HGLOBAL hString = NULL;
   char * pString = NULL;
   UINT clipFormat;
    // Get any string data
	if (pDataObject->IsDataAvailable(
		::RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT)))
	{
      char * pURL = NULL, *pTitle = NULL;
      wfe_GetBookmarkData(pDataObject, &pURL, &pTitle);
      if (pURL)
      {
         if (!strnicmp(pURL,"addbook",7))
             return FALSE;
		 if (!strnicmp(pURL,"mailto",6))
			 return FALSE;
	
		 AddAttachment(pURL);
         UpdateAttachments();
      }
      return TRUE;
	}
	else if (pDataObject->IsDataAvailable(
      clipFormat=::RegisterClipboardFormat(NETSCAPE_MESSAGE_FORMAT)))
	{
		HGLOBAL hContent = pDataObject->GetGlobalData (clipFormat);
		MailNewsDragData *pDragData = (MailNewsDragData *) GlobalLock(hContent);

      if (pDragData != NULL)
      {
         for (int i = 0; i< pDragData->m_count; i++)
         {
            ASSERT(pDragData->m_indices);
            MessageKey key = MSG_GetMessageKey(pDragData->m_pane, pDragData->m_indices[i]);
            URL_Struct* pUrl = MSG_ConstructUrlForMessage(pDragData->m_pane, key);
            if (pUrl != NULL)
            {
               if (pUrl->address)
                  AddAttachment(pUrl->address);
   		   	NET_FreeURLStruct(pUrl);
            }
         }
         UpdateAttachments();
      }
      return TRUE;
	}

   return FALSE;
}

void CNSAttachmentList::OnDropFiles( HDROP hDropInfo )
{
   UINT wNumFilesDropped = ::DragQueryFile(hDropInfo,(UINT)-1,NULL,0);
   if (wNumFilesDropped > 0)
   {
      for (UINT x = 0; x < wNumFilesDropped; x++)
      {
         int wPathnameSize = ::DragQueryFile(hDropInfo, x, NULL, 0);
         char * pStr = (char*)XP_CALLOC(1,wPathnameSize+2);
         ASSERT(pStr);
         // Copy the pathname into the buffer & add to listbox
         ::DragQueryFile(hDropInfo, x, pStr, wPathnameSize+1);
         AddAttachment(pStr);
         XP_FREE(pStr);
      }
      UpdateAttachments();
   }
}

void CNSAttachmentList::OnDestroy(void)
{
    Cleanup();
    CListBox::OnDestroy();
}

void CNSAttachmentList::DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct ) 
{
	if (lpDrawItemStruct->itemID != -1) 
	{
		CDC dc;
		dc.Attach(lpDrawItemStruct->hDC);

      HBRUSH hRegBrush = ::CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );
      HPEN hRegPen = ::CreatePen( PS_SOLID, 1, GetSysColor ( COLOR_WINDOW ) );
	   HBRUSH hHighBrush = ::CreateSolidBrush( GetSysColor( COLOR_HIGHLIGHT ) );
	   HPEN hHighPen = ::CreatePen( PS_SOLID, 1, GetSysColor ( COLOR_HIGHLIGHT ) );

      HBRUSH hOldBrush = (HBRUSH) dc.SelectObject ( hRegBrush );
      HPEN hOldPen = (HPEN) dc.SelectObject ( hRegPen );
      COLORREF cOldText = dc.SetTextColor ( GetSysColor ( COLOR_WINDOWTEXT ) );
      COLORREF cOldBk = dc.SetBkColor ( GetSysColor ( COLOR_WINDOW ) );

		CRect rect(lpDrawItemStruct->rcItem);
      BOOL bSelected = lpDrawItemStruct->itemState & ODS_SELECTED;

      if (bSelected && (GetFocus()==this))
      {
         dc.SelectObject ( hHighBrush );
         dc.SelectObject ( hHighPen );
         dc.SetTextColor ( GetSysColor ( COLOR_HIGHLIGHTTEXT ) );
         dc.SetBkColor ( GetSysColor ( COLOR_HIGHLIGHT ) );
      }

		dc.Rectangle(rect);

      MSG_AttachmentData * pAttach = (MSG_AttachmentData *)lpDrawItemStruct->itemData;
      if (pAttach)
      {
		 char* pFilePath = NULL;
         char * pszString = 
            (pAttach->real_name && strlen(pAttach->real_name)) ? pAttach->real_name : pAttach->url;
         int idBitmap = 0;
         if (!strnicmp(pAttach->url, "file:", strlen("file:")))
         {
            idBitmap = IDB_FILEATTACHMENT;
			if (XP_STRCHR(pAttach->url, '#'))
			{
			   char* pTemp = XP_STRCHR(pAttach->url, ':');
			   pFilePath = XP_NetToDosFileName(pTemp + 4); // remove :/// 4 bytes
			}
            else if (pszString == pAttach->url)
			{
			   XP_ConvertUrlToLocalFile(pAttach->url, &pFilePath);
			}
		    if (pFilePath)
		    {
			   char fname[_MAX_FNAME];
			   char ext[_MAX_EXT];
			   _splitpath(pFilePath, NULL, NULL, fname, ext);
			   *pFilePath = '\0';
			   strcat(pFilePath, fname);
			   strcat(pFilePath, ext);

         pszString = pFilePath; // rhp - move this into the conditional - or crash in MAPI
		    }
//			pszString = pFilePath;
         }
         else if (MSG_RequiresNewsWindow(pAttach->url))
               idBitmap = IDB_NEWSARTICLE;
         else if (MSG_RequiresMailWindow(pAttach->url))
               idBitmap = IDB_MAILMESSAGE;
         else if (MSG_RequiresBrowserWindow(pAttach->url))
               idBitmap = IDB_WEBATTACHMENT;
		 else
			idBitmap = IDB_WEBATTACHMENT;

         rect.left += BITMAP_WIDTH + 4;
         dc.DrawText(pszString,strlen(pszString),rect,DT_LEFT|DT_VCENTER);
         rect.left -= BITMAP_WIDTH + 4;

		 if (pFilePath)
			XP_FREE(pFilePath);

         BITMAP bitmap;
         CBitmap cbitmap;
         cbitmap.LoadBitmap(MAKEINTRESOURCE(idBitmap));
         cbitmap.GetObject(sizeof(BITMAP),&bitmap);
         int center_x = 2;
         int center_y = rect.top + (rect.Height()-bitmap.bmHeight)/2;
         DrawTransparentBitmap( 
            dc.GetSafeHdc(), 
            (HBITMAP)cbitmap.GetSafeHandle(), 
            center_x, center_y, 
            RGB(255,0,255));
         cbitmap.DeleteObject();
      }

      if (bSelected)
         dc.DrawFocusRect(rect);

      dc.SetTextColor ( cOldText );
      dc.SetBkColor ( cOldBk );        
      dc.SelectObject ( hOldPen );
      dc.SelectObject ( hOldBrush );

    	dc.Detach();

   	VERIFY(DeleteObject( hRegBrush ));
   	VERIFY(DeleteObject( hRegPen ));
   	VERIFY(DeleteObject( hHighBrush ));
   	VERIFY(DeleteObject( hHighPen ));
	}
}


//=============================================================== MeasureItem
void CNSAttachmentList::MeasureItem( LPMEASUREITEMSTRUCT lpMeasureItemStruct )
{
	lpMeasureItemStruct->itemHeight = BITMAP_HEIGHT + 2;
}


//================================================================ DeleteItem
void CNSAttachmentList::DeleteItem( LPDELETEITEMSTRUCT lpDeleteItemStruct ) 
{
}

//////////////////////////////////////////////////////////////////////////////
// CNSAttachDropTarget

DROPEFFECT CNSAttachDropTarget::OnDragEnter(CWnd *, COleDataObject *, DWORD, CPoint)
{
    DROPEFFECT DropEffect = DROPEFFECT_NONE;
    return(DropEffect);
}

DROPEFFECT CNSAttachDropTarget::OnDragOver(CWnd * pWnd,
	COleDataObject * pDataObject, DWORD dwKeyState, CPoint point)
{
	DROPEFFECT deReturn = DROPEFFECT_NONE;
	// Only interested in bookmarks
	if (pDataObject->IsDataAvailable(
		::RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT)))
	{
		deReturn = DROPEFFECT_COPY;
	}
	else if (pDataObject->IsDataAvailable(
      ::RegisterClipboardFormat(NETSCAPE_MESSAGE_FORMAT)))
	{
		deReturn = DROPEFFECT_COPY;
	}
   else if(pDataObject->IsDataAvailable(
      ::RegisterClipboardFormat(vCardClipboardFormat)) )
	{
		deReturn = DROPEFFECT_COPY;
	}

	return(deReturn);

} 

void CNSAttachDropTarget::OnDragLeave(CWnd *)
{
}

static LPSTR getDataObjectNewsURL(COleDataObject * pDataObject)
{
	if (!pDataObject->IsDataAvailable(::RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT)))
    return FALSE;

  char * szURL = NULL;
  char * szTitle = NULL;
  wfe_GetBookmarkData(pDataObject, &szURL, &szTitle);
  if((strnicmp(szURL, "news", 4) == 0) || (strnicmp(szURL, "snews", 5) == 0))
    return szURL;
  else
    return NULL;
}

BOOL CNSAttachDropTarget::OnDrop(CWnd * pWnd, COleDataObject * pDataObject, DROPEFFECT effect, CPoint point)
{
  CComposeFrame * pFrame = (CComposeFrame*)pWnd->GetParentFrame();
  CComposeBar * pComposeBar = pFrame->GetComposeBar();
#ifdef MOZ_NEWADDR
  if(pDataObject->IsDataAvailable(::RegisterClipboardFormat(ADDRESSBOOK_INDEX_FORMAT)))
  {
	pComposeBar->OnAddressTab();
	pComposeBar->UpdateWindow();
	pComposeBar->ProcessAddressBookIndexFormat(pDataObject,effect, point);

	return TRUE;
  }
  else
#endif
	  if(pDataObject->IsDataAvailable(::RegisterClipboardFormat(vCardClipboardFormat)) )
  {
    pComposeBar->OnAddressTab();
    pComposeBar->UpdateWindow();
    return pComposeBar->ProcessVCardData(pDataObject,point);
  }
  else 
  {
    // do not put news URLs to attachment
    char * szURL = getDataObjectNewsURL(pDataObject);
    if(szURL != NULL)
    {
      pComposeBar->OnAddressTab();
      pComposeBar->UpdateWindow();
      return pComposeBar->AddURLToAddressPane(pDataObject, point, szURL);
    }
    else
    {
      pComposeBar->OnAttachTab();
      pComposeBar->UpdateWindow();
      return pComposeBar->m_pAttachmentList->ProcessDropTarget(pDataObject);
    }
  }
}

