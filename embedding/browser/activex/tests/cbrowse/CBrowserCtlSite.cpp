// CBrowserCtlSite.cpp : Implementation of CBrowserCtlSite
#include "stdafx.h"
#include "Cbrowse.h"
#include "CBrowserCtlSite.h"

#include <tchar.h>
#include <string.h>

/////////////////////////////////////////////////////////////////////////////
// CBrowserCtlSite

CBrowserCtlSite::CBrowserCtlSite()
{
	m_bUseCustomPopupMenu = TRUE;
	m_bUseCustomDropTarget = FALSE;
}

static void _InsertMenuItem(HMENU hmenu, int nPos, int nID, const TCHAR *szItemText)
{
	MENUITEMINFO mii;
	memset(&mii, 0, sizeof(mii));
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_TYPE;
	mii.fType = MFT_STRING;
	mii.fState = MFS_ENABLED;
	mii.dwTypeData = (LPTSTR) szItemText;
	mii.cch = _tcslen(szItemText);
	InsertMenuItem(hmenu, nPos, TRUE, &mii);
}

static void _InsertMenuSeparator(HMENU hmenu, int nPos)
{
	MENUITEMINFO mii;
	memset(&mii, 0, sizeof(mii));
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_TYPE;
	mii.fType = MFT_SEPARATOR;
	mii.fState = MFS_ENABLED;
	InsertMenuItem(hmenu, nPos, TRUE, &mii);
}

/////////////////////////////////////////////////////////////////////////////
// IDocHostUIHandler

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::ShowContextMenu(/* [in] */ DWORD dwID, /* [in] */ POINT __RPC_FAR *ppt, /* [in] */ IUnknown __RPC_FAR *pcmdtReserved, /* [in] */ IDispatch __RPC_FAR *pdispReserved)
{
	if (m_bUseCustomPopupMenu)
	{
		tstring szMenuText(_T("Unknown context"));
		HMENU hmenu = CreatePopupMenu();
		_InsertMenuItem(hmenu, 0, 1, _T("CBrowse context popup"));
		_InsertMenuSeparator(hmenu, 1);
		switch (dwID)
		{
		case CONTEXT_MENU_DEFAULT:
			szMenuText = _T("Default context");
			break;
		case CONTEXT_MENU_IMAGE:
			szMenuText = _T("Image context");
			break;
		case CONTEXT_MENU_CONTROL:
			szMenuText = _T("Control context");
			break;
		case CONTEXT_MENU_TABLE:
			szMenuText = _T("Table context");
			break;
		case CONTEXT_MENU_TEXTSELECT:
			szMenuText = _T("TextSelect context");
			break;
		case CONTEXT_MENU_ANCHOR:
			szMenuText = _T("Anchor context");
			break;
		case CONTEXT_MENU_UNKNOWN:
			szMenuText = _T("Unknown context");
			break;
		}

		_InsertMenuItem(hmenu, 2, 2, szMenuText.c_str());

		POINT pt;
		GetCursorPos(&pt);
		TrackPopupMenu(hmenu, TPM_RETURNCMD, pt.x, pt.y, 0, AfxGetMainWnd()->GetSafeHwnd(), NULL);
		DestroyMenu(hmenu);
		return S_OK;
	}
	return S_FALSE;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::GetHostInfo(/* [out][in] */ DOCHOSTUIINFO __RPC_FAR *pInfo)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::ShowUI(/* [in] */ DWORD dwID, /* [in] */ IOleInPlaceActiveObject __RPC_FAR *pActiveObject, /* [in] */ IOleCommandTarget __RPC_FAR *pCommandTarget, /* [in] */ IOleInPlaceFrame __RPC_FAR *pFrame, /* [in] */ IOleInPlaceUIWindow __RPC_FAR *pDoc)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::HideUI(void)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::UpdateUI(void)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::EnableModeless(/* [in] */ BOOL fEnable)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::OnDocWindowActivate(/* [in] */ BOOL fActivate)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::OnFrameWindowActivate(/* [in] */ BOOL fActivate)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::ResizeBorder(/* [in] */ LPCRECT prcBorder, /* [in] */ IOleInPlaceUIWindow __RPC_FAR *pUIWindow, /* [in] */ BOOL fRameWindow)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::TranslateAccelerator(/* [in] */ LPMSG lpMsg, /* [in] */ const GUID __RPC_FAR *pguidCmdGroup, /* [in] */ DWORD nCmdID)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::GetOptionKeyPath(/* [out] */ LPOLESTR __RPC_FAR *pchKey, /* [in] */ DWORD dw)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::GetDropTarget(/* [in] */ IDropTarget __RPC_FAR *pDropTarget, /* [out] */ IDropTarget __RPC_FAR *__RPC_FAR *ppDropTarget)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::GetExternal(/* [out] */ IDispatch __RPC_FAR *__RPC_FAR *ppDispatch)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::TranslateUrl(/* [in] */ DWORD dwTranslate, /* [in] */ OLECHAR __RPC_FAR *pchURLIn, /* [out] */ OLECHAR __RPC_FAR *__RPC_FAR *ppchURLOut)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::FilterDataObject(/* [in] */ IDataObject __RPC_FAR *pDO, /* [out] */ IDataObject __RPC_FAR *__RPC_FAR *ppDORet)
{
	return E_NOTIMPL;
}


///////////////////////////////////////////////////////////////////////////////
// IDocHostShowUI

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::ShowMessage(/* [in] */ HWND hwnd, /* [in] */ LPOLESTR lpstrText, /* [in] */ LPOLESTR lpstrCaption, /* [in] */ DWORD dwType, /* [in] */ LPOLESTR lpstrHelpFile, /* [in] */ DWORD dwHelpContext,/* [out] */ LRESULT __RPC_FAR *plResult)
{
	return S_FALSE;
}

HRESULT STDMETHODCALLTYPE CBrowserCtlSite::ShowHelp(/* [in] */ HWND hwnd, /* [in] */ LPOLESTR pszHelpFile, /* [in] */ UINT uCommand, /* [in] */ DWORD dwData, /* [in] */ POINT ptMouse, /* [out] */ IDispatch __RPC_FAR *pDispatchObjectHit)
{
	return S_FALSE;
}

