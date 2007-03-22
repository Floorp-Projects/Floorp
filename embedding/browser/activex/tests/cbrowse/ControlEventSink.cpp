// ControlEventSink.cpp : Implementation of CBrowseEventSink
#include "stdafx.h"
#include "Cbrowse.h"
#include "ControlEventSink.h"

/////////////////////////////////////////////////////////////////////////////
// CBrowseEventSink

HRESULT STDMETHODCALLTYPE CBrowseEventSink::GetTypeInfoCount( 
    /* [out] */ UINT __RPC_FAR *pctinfo)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowseEventSink::GetTypeInfo( 
    /* [in] */ UINT iTInfo,
    /* [in] */ LCID lcid,
    /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowseEventSink::GetIDsOfNames( 
    /* [in] */ REFIID riid,
    /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames,
    /* [in] */ UINT cNames,
    /* [in] */ LCID lcid,
    /* [size_is][out] */ DISPID __RPC_FAR *rgDispId)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowseEventSink:: Invoke( 
    /* [in] */ DISPID dispIdMember,
    /* [in] */ REFIID riid,
    /* [in] */ LCID lcid,
    /* [in] */ WORD wFlags,
    /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams,
    /* [out] */ VARIANT __RPC_FAR *pVarResult,
    /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo,
    /* [out] */ UINT __RPC_FAR *puArgErr)
{
    CString szEvent;

    switch (dispIdMember)
    {
    case 0x66:
        {
            USES_CONVERSION;
            CString szText(OLE2T(pDispParams->rgvarg[0].bstrVal));
            szEvent.Format(_T("StatusTextChange: \"%s\""), szText);
            m_pBrowseDlg->m_TabMessages.m_szStatus = szText;
            m_pBrowseDlg->m_TabMessages.UpdateData(FALSE);
        }
        break;
    case 0x6c:
        {
            LONG nProgress = pDispParams->rgvarg[1].lVal;
            LONG nProgressMax = pDispParams->rgvarg[0].lVal;
            szEvent.Format("ProgressChange(%d of %d)", nProgress, nProgressMax);
            CProgressCtrl &pc = m_pBrowseDlg->m_TabMessages.m_pcProgress;
            pc.SetRange(0, nProgressMax);
            pc.SetPos(nProgress);
        }
        break;
    case 0x69:
        {
            BOOL enable = pDispParams->rgvarg[0].boolVal == VARIANT_TRUE ? TRUE : FALSE;
            LONG commandState = pDispParams->rgvarg[1].lVal;
            if (commandState == CSC_NAVIGATEBACK)
            {
                m_pBrowseDlg->m_bCanGoBack = enable;
                m_pBrowseDlg->m_btnBack.EnableWindow(enable);
            }
            else if (commandState == CSC_NAVIGATEFORWARD)
            {
                m_pBrowseDlg->m_bCanGoForward = enable;
                m_pBrowseDlg->m_btnForward.EnableWindow(enable);
            }
            szEvent.Format(_T("CommandStateChange(%ld, %d)"), commandState, enable);
        }
        break;
    case 0x6a:
        szEvent = _T("DownloadBegin");
        m_pBrowseDlg->m_btnStop.EnableWindow(TRUE);
        break;
    case 0x68:
        szEvent = _T("DownloadComplete");
        m_pBrowseDlg->m_btnStop.EnableWindow(FALSE);
        break;
    case 0x71:
        {
            USES_CONVERSION;
            CString szText(OLE2T(pDispParams->rgvarg[0].bstrVal));
            szEvent.Format(_T("TitleChange: \"%s\""), szText);
            CString szTitle;
            szTitle.Format(_T("CBrowse - %s"), szText);
            m_pBrowseDlg->SetWindowText(szTitle);
        }
        break;
    case 0x70:
        szEvent = _T("PropertyChange");
        break;
    case 0xfa:
        szEvent = _T("BeforeNavigate2");
        break;
    case 0xfb:
        {
            szEvent = _T("NewWindow2");
            
            VARIANTARG *pvars = pDispParams->rgvarg;
            CBrowseDlg *pDlg = new CBrowseDlg;
            if (pDlg)
            {
                pDlg->m_clsid = m_pBrowseDlg->m_clsid;
                pDlg->Create(IDD_CBROWSE_DIALOG);
                
                if (pDlg->m_pControlSite)
                {
                    CIUnkPtr spUnkBrowser;
                    pDlg->m_pControlSite->GetControlUnknown(&spUnkBrowser);

                    pvars[0].byref = (void *) VARIANT_FALSE;
                    spUnkBrowser->QueryInterface(IID_IDispatch, (void **) pvars[1].byref);
                }
            }
        }
        break;
    case 0xfc:
        szEvent = _T("NavigateComplete2");
        break;
    case 0x103:
        szEvent = _T("DocumentComplete");
        if (m_pBrowseDlg)
        {
            m_pBrowseDlg->UpdateURL();
        }
        break;
    case 0xfd:
        szEvent = _T("OnQuit");
        break;
    case 0xfe:
        szEvent = _T("OnVisible");
        break;
    case 0xff:
        szEvent = _T("OnToolBar");
        break;
    case 0x100:
        szEvent = _T("OnMenuBar");
        break;
    case 0x101:
        szEvent = _T("OnStatusBar");
        break;
    case 0x102:
        szEvent = _T("OnFullScreen");
        break;
    case 0x104:
        szEvent = _T("OnTheaterMode");
        break;
    default:
        szEvent.Format(_T("%d"), dispIdMember);
    }

    if (m_pBrowseDlg)
    {
        m_pBrowseDlg->OutputString(_T("Event %s"), szEvent);
    }
    return S_OK;
}
