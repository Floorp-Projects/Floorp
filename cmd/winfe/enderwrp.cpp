#include "stdafx.h"
#include "enderwrp.h"
#include "edview.h"
#include "edt.h"
#include "netsdoc.h"
#include "compstd.h"
#include "compfrm.h"


IMPLEMENT_DYNCREATE(CEnderView, CView)


BEGIN_MESSAGE_MAP(CEnderView, CView)
    //{{AFX_MSG_MAP(CEnderView)
	ON_WM_DESTROY()
    ON_MESSAGE(WM_TOOLCONTROLLER,OnToolController)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()



CEnderView::CEnderView(CAbstractCX *p_pCx)
:m_pEditView(NULL),m_pToolBarController(NULL),m_pCX(p_pCx)
{
}


CEnderView::~CEnderView()
{
    if (m_pEditView)
    {
        m_pEditView->DestroyWindow();
    }
#if 0
    if (m_pToolbar)
        m_pToolbar->DestroyWindow();
#endif //0
}

void CEnderView::OnDestroy()
{
	if (m_pEditView)
	{
		CNetscapeDoc* pDoc = (CNetscapeDoc *)m_pEditView->GetDocument();
		//warning! do not allow the CWinCX to RE-FREE its frame. 
		//we are borrowning the frame from the "browser window or layer"
		//call ClearFrame to "Clear the frame"
		CWinCX *pCX = m_pEditView->GetContext();
		if (pCX)
		{
			pCX->ClearFrame();//NOT WORKING, MUST FIX
			EDT_DestroyEditBuffer(pCX->GetContext());
		}
		m_pEditView->DestroyWindow();
		if (pDoc)
			delete pDoc;
		m_pEditView=NULL;
	}
}

void CEnderView::OnDraw(CDC *pDC)
{
    return;
}

#ifdef MOZ_ENDER_MIME
BOOL 
CEnderView::Create(CWnd *pParent, lo_FormElementHtmlareaData *pData, LO_TextAttr *pTextAttr)
#else
BOOL 
CEnderView::Create(CWnd *pParent, lo_FormElementTextareaData *pData, LO_TextAttr *pTextAttr)
#endif //MOZ_ENDER_MIME
{
    if (!CView::Create(NULL, NULL, WS_CHILD | WS_BORDER | WS_TABSTOP,CRect(0,0,1,1),pParent,ID_ENDER+1,NULL))
        return FALSE;
    if (m_pEditView)
    {
        assert(FALSE);
        return FALSE;
    }
    lo_FormElementTextareaData *t_pData = (lo_FormElementTextareaData *)pData;
	if(m_pCX)	
	{
		if(m_pCX->IsWindowContext() && VOID2CX(m_pCX, CPaneCX)->GetPane())	
		{
			//	Need a widget representation.
			//create a new CPaneCX
			CNetscapeDoc* pDoc = new CNetscapeDoc();
			m_pEditView = new CNetscapeEditView();
			m_pEditView->SetEmbedded(TRUE);
			RECT rect;
			rect.left=0;
			rect.top=0;
			rect.right=1;
			rect.bottom=1;
			if (!m_pEditView->Create(NULL, NULL, 
				WS_CHILD | WS_VSCROLL | ES_LEFT | WS_TABSTOP | ES_MULTILINE //AFX_WS_DEFAULT_VIEW
				, rect, 
				this, ID_ENDER, NULL))
			{
				TRACE("Warning: could not create view for frame.\n");
				m_pEditView=NULL;
				return FALSE;
			}
			CPaneCX* cx= VOID2CX(m_pCX, CPaneCX);
			HWND hwnd= cx->GetPane();
			CWnd *pwnd= CWnd::FromHandle(hwnd);
			CGenericView *genview=NULL;
			if (pwnd->IsKindOf(RUNTIME_CLASS(CGenericView)))
				genview=(CGenericView *)pwnd;
			if (!genview)
				return FALSE;
			CWinCX* pDontCare = new CWinCX((CGenericDoc *)pDoc,
									 genview->GetFrame(), (CGenericView *)m_pEditView);
			pDontCare->GetContext()->is_editor = TRUE;
			m_pEditView->SetContext(pDontCare);
			pDontCare->Initialize(pDontCare->CDCCX::IsOwnDC(), &rect);
			pDontCare->NormalGetUrl(EDT_NEW_DOC_URL);
//ADJUST THE SIZE OF THE WINDOW ACCORDING TO ROWS AND COLS EVEN THOUGH THAT IS NOT ACCURATE
			//	Measure some text.
			CDC *pDC = m_pEditView->GetDC();
            CyaFont *pMyFont;
			if(pDC)	
			{
                CDC t_dc;
                t_dc.CreateCompatibleDC( pDC );
				CDCCX *pDCCX = VOID2CX(m_pCX, CDCCX);
				pDCCX->SelectNetscapeFont( t_dc.GetSafeHdc(), pTextAttr, pMyFont );
				if (pMyFont) 
				{
					//SetWidgetFont(pDC->GetSafeHdc(), m_pEditView->m_hWnd);
					//GetElement()->text_attr->FE_Data = pMyFont;
					//	Default length is 20
					//	Default lines is 1
					int32 lLength = 20;
					int32 lLines = 1;

					//	See if we can measure the default text, and/or
					//		set up the size and size limits.
					if(t_pData)	
					{
						if(t_pData->cols > 0)	{
							//	Use provided size.
							lLength = t_pData->cols;
						}
						if(t_pData->rows > 0)	{
							//	Use provided size.
							lLines = t_pData->rows;
						}
					}

					//	Now figure up the width and height we would like.
	//				int32 lWidgetWidth = (lLength + 1) * tm.tmAveCharWidth + sysInfo.m_iScrollWidth;
	//				int32 lWidgetHeight = (lLines + 1) * tm.tmHeight;
					int32 lWidgetWidth = (lLength + 1) * pMyFont->GetMeanWidth() + sysInfo.m_iScrollWidth;
					int32 lWidgetHeight = (lLines + 1) * pMyFont->GetHeight();

					//	If no word wrapping, account a horizontal scrollbar.
					if(t_pData->auto_wrap == TEXTAREA_WRAP_OFF)	{
						lWidgetHeight += sysInfo.m_iScrollHeight;
					}

					//	Move the window.
					m_pEditView->MoveWindow(0, 32, CASTINT(lWidgetWidth)-5, CASTINT(lWidgetHeight)-4, FALSE);
                    MoveWindow(1, 1, CASTINT(lWidgetWidth), CASTINT(lWidgetHeight+32), FALSE);

                    // Create the HTML edit toolbars.  There are currently two separate
                    // toolbars.. one for formats and another for character operations.
                    m_pToolBarController = new CEditToolBarController(this);
                    if (!m_pToolBarController || !m_pToolBarController->CreateEditBars(m_pCX->GetContext(), m_pEditView, DISPLAY_CHARACTER_TOOLBAR))
                    {
                        TRACE("Bad ComposeBar");
                        if (m_pToolBarController)
                            delete m_pToolBarController;
                        m_pToolBarController = NULL;
                        return FALSE;
                    }
                    
                    CComboToolBar *t_combobar=m_pToolBarController->GetCharacterBar();
                    t_combobar->MoveWindow(0,0,CASTINT(lWidgetWidth),32,FALSE);
                    t_combobar->ShowWindow(SW_SHOW);
					pDCCX->ReleaseNetscapeFont( t_dc.GetSafeHdc(), pMyFont );
	                pDCCX->ReleaseContextDC(t_dc.GetSafeHdc());
					m_pEditView->ReleaseDC(pDC);
				}
				else
				{
					m_pEditView->ReleaseDC(pDC);
					return FALSE;
				}
			}
			else
			{
				return FALSE;
			}
		}
		else if(m_pCX->IsPureDCContext())	
		{
			//	Need a drawn representation.
		}
	}
    return TRUE;
}



LONG 
CEnderView::OnToolController(UINT,LONG)
{
	return (LONG)m_pToolBarController;
}

