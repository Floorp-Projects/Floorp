#include "stdafx.h"
#include "embdlist.h"
#include "genframe.h"

#define  BITMAP_WIDTH   16
#define  BITMAP_HEIGHT  16

BEGIN_MESSAGE_MAP(CEmbeddedAttachList, CListBox)
    ON_WM_CREATE()
    ON_WM_KEYDOWN()
    ON_WM_LBUTTONDOWN()
    ON_WM_DROPFILES()
    ON_WM_DESTROY()
	ON_COMMAND(ID_EDIT_DELETE,OnDelete)
END_MESSAGE_MAP()

IMPLEMENT_DYNCREATE(CEmbeddedAttachList, CListBox)

void DrawTransBitmap(HDC hdc, HBITMAP hBitmap, short xStart, short yStart, COLORREF cTransparentColor );

void DrawTransBitmap(HDC hdc, HBITMAP hBitmap, short xStart, short yStart, COLORREF cTransparentColor )
{
	HDC hSrcDC = CreateCompatibleDC(hdc);
	SelectObject(hSrcDC, hBitmap);

	BITMAP bm;
  GetObject(hBitmap, sizeof(BITMAP), (LPSTR)&bm);

  POINT ptSize;
	ptSize.x = bm.bmWidth;
	ptSize.y = bm.bmHeight;
	DPtoLP(hSrcDC, &ptSize, 1);

  HPALETTE hPalette = (HPALETTE)GetCurrentObject(hdc, OBJ_PAL);

  FEU_TransBlt(hdc, xStart, yStart, ptSize.x, ptSize.y, hSrcDC, 0, 0, hPalette, cTransparentColor);

  DeleteDC(hSrcDC);
}

CEmbeddedAttachList::CEmbeddedAttachList()
:m_numattachments(0),m_attachmentlist(NULL)
{
}



CEmbeddedAttachList::~CEmbeddedAttachList()
{
    for (int i = 0; i< m_numattachments; i++)
        XP_FREE(m_attachmentlist[i]);
    XP_FREE(m_attachmentlist);
}



BOOL
CEmbeddedAttachList::Create(CWnd *pWnd, UINT id)
{
   BOOL bRetVal = CListBox::Create (
      WS_CLIPCHILDREN|WS_CHILD|WS_BORDER|WS_VISIBLE|WS_VSCROLL|LBS_OWNERDRAWFIXED|
      LBS_HASSTRINGS|LBS_NOTIFY|LBS_WANTKEYBOARDINPUT|LBS_NOINTEGRALHEIGHT,
      CRect(0,0,0,0), pWnd, id);
   return bRetVal;
}



void
CEmbeddedAttachList::OnDelete()
{
    if (GetFocus() == this)
	{
	    int idx = GetCurSel();
		if (idx != LB_ERR)
        {
			DeleteString(idx);
        }
    }
}



int 
CEmbeddedAttachList::OnCreate( LPCREATESTRUCT lpCreateStruct )
{
   int iRet = CListBox::OnCreate(lpCreateStruct);

    CGenericFrame * pFrame = (CGenericFrame*)GetParentFrame();
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


#if 0
   const MSG_AttachmentData * pDataList = MSG_GetAttachmentList(GetMsgPane());
#endif //0

   DragAcceptFiles();

   return(iRet);
}



void
CEmbeddedAttachList::OnDestroy()
{
    CListBox::OnDestroy();
}



void
CEmbeddedAttachList::OnDropFiles(HDROP hDropInfo)
{
    CListBox::OnDropFiles(hDropInfo);
    UINT wNumFilesDropped = ::DragQueryFile(hDropInfo,(UINT)-1,NULL,0);
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

}



void CEmbeddedAttachList::OnLButtonDown(UINT nFlags, CPoint point) 
{
	CListBox::OnLButtonDown(nFlags, point);
}



void CEmbeddedAttachList::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
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



void CEmbeddedAttachList::RemoveAttachment() 
{
    int idx = GetCurSel();
    if (idx != LB_ERR) 
    {
        DeleteString(idx);
        if (idx >= GetCount())
            SetCurSel(idx-1);
    }
}


void CEmbeddedAttachList::DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct ) 
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

      char *t_pFile = m_attachmentlist[lpDrawItemStruct->itemID];
      char *pszString = NULL;
      if (t_pFile)
      {
/*		 char* pFilePath = NULL;
         char * pszString = 
            (pAttach->real_name && strlen(pAttach->real_name)) ? pAttach->real_name : pAttach->url;*/
         int idBitmap = 0;
//         if (!strnicmp(pAttach->url, "file:", strlen("file:")))
         {
            idBitmap = IDB_FILEATTACHMENT;
/*			if (XP_STRCHR(pAttach->url, '#'))
			{
			   char* pTemp = XP_STRCHR(pAttach->url, ':');
			   pFilePath = XP_NetToDosFileName(pTemp + 4); // remove :/// 4 bytes
			}
            else if (pszString == pAttach->url)
			{
			   XP_ConvertUrlToLocalFile(pAttach->url, &pFilePath);
			}*/

         pszString = t_pFile; // rhp - move this into the conditional - or crash in MAPI
//			pszString = pFilePath;
         }
//		 else
//			idBitmap = IDB_WEBATTACHMENT;

         rect.left += BITMAP_WIDTH + 4;
         dc.DrawText(pszString,strlen(pszString),rect,DT_LEFT|DT_VCENTER);
         rect.left -= BITMAP_WIDTH + 4;

         BITMAP bitmap;
         CBitmap cbitmap;
         cbitmap.LoadBitmap(MAKEINTRESOURCE(idBitmap));
         cbitmap.GetObject(sizeof(BITMAP),&bitmap);
         int center_x = 2;
         int center_y = rect.top + (rect.Height()-bitmap.bmHeight)/2;
         DrawTransBitmap( 
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
void CEmbeddedAttachList::MeasureItem( LPMEASUREITEMSTRUCT lpMeasureItemStruct )
{
	lpMeasureItemStruct->itemHeight = BITMAP_HEIGHT + 2;
}



//================================================================ DeleteItem
void CEmbeddedAttachList::DeleteItem( LPDELETEITEMSTRUCT lpDeleteItemStruct ) 
{
}



void CEmbeddedAttachList::AttachFile()
{
}



void CEmbeddedAttachList::AttachUrl(char *pUrl /*= NULL*/)
{
    if (pUrl)
        AddString(pUrl);
}



void CEmbeddedAttachList::AddAttachment(char * pName)
{
    if (pName)
    {
        AddString(pName);
        char **t_list = m_attachmentlist;
        m_attachmentlist = new char *[m_numattachments+1];
        if (t_list)
            XP_MEMCPY(m_attachmentlist,t_list,m_numattachments*sizeof(char *));
        m_attachmentlist[m_numattachments++]=XP_STRDUP(pName);
        UpdateWindow();
    }
}
