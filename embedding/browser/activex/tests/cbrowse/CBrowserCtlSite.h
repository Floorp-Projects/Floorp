// CBrowserCtlSite.h : Declaration of the CBrowserCtlSite

#ifndef __CBROWSERCTLSITE_H_
#define __CBROWSERCTLSITE_H_

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CBrowserCtlSite
class ATL_NO_VTABLE CBrowserCtlSite : 
	public CControlSite,
	public IDocHostUIHandler,
	public IDocHostShowUI
{
public:
	CBrowserCtlSite();

DECLARE_REGISTRY_RESOURCEID(IDR_CBROWSERCTLSITE)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(CBrowserCtlSite)
	CCONTROLSITE_INTERFACES()
	COM_INTERFACE_ENTRY(IDocHostUIHandler)
	COM_INTERFACE_ENTRY(IDocHostShowUI)
END_COM_MAP()

	BOOL m_bUseCustomPopupMenu;
	BOOL m_bUseCustomDropTarget;

public:
// IDocHostUIHandler
	virtual HRESULT STDMETHODCALLTYPE ShowContextMenu(/* [in] */ DWORD dwID, /* [in] */ POINT __RPC_FAR *ppt, /* [in] */ IUnknown __RPC_FAR *pcmdtReserved, /* [in] */ IDispatch __RPC_FAR *pdispReserved);
	virtual HRESULT STDMETHODCALLTYPE GetHostInfo(/* [out][in] */ DOCHOSTUIINFO __RPC_FAR *pInfo);
	virtual HRESULT STDMETHODCALLTYPE ShowUI(/* [in] */ DWORD dwID, /* [in] */ IOleInPlaceActiveObject __RPC_FAR *pActiveObject, /* [in] */ IOleCommandTarget __RPC_FAR *pCommandTarget, /* [in] */ IOleInPlaceFrame __RPC_FAR *pFrame, /* [in] */ IOleInPlaceUIWindow __RPC_FAR *pDoc);
	virtual HRESULT STDMETHODCALLTYPE HideUI(void);
	virtual HRESULT STDMETHODCALLTYPE UpdateUI(void);
	virtual HRESULT STDMETHODCALLTYPE EnableModeless(/* [in] */ BOOL fEnable);
	virtual HRESULT STDMETHODCALLTYPE OnDocWindowActivate(/* [in] */ BOOL fActivate);
	virtual HRESULT STDMETHODCALLTYPE OnFrameWindowActivate(/* [in] */ BOOL fActivate);
	virtual HRESULT STDMETHODCALLTYPE ResizeBorder(/* [in] */ LPCRECT prcBorder, /* [in] */ IOleInPlaceUIWindow __RPC_FAR *pUIWindow, /* [in] */ BOOL fRameWindow);
	virtual HRESULT STDMETHODCALLTYPE TranslateAccelerator(/* [in] */ LPMSG lpMsg, /* [in] */ const GUID __RPC_FAR *pguidCmdGroup, /* [in] */ DWORD nCmdID);
	virtual HRESULT STDMETHODCALLTYPE GetOptionKeyPath(/* [out] */ LPOLESTR __RPC_FAR *pchKey, /* [in] */ DWORD dw);
	virtual HRESULT STDMETHODCALLTYPE GetDropTarget(/* [in] */ IDropTarget __RPC_FAR *pDropTarget, /* [out] */ IDropTarget __RPC_FAR *__RPC_FAR *ppDropTarget);
	virtual HRESULT STDMETHODCALLTYPE GetExternal(/* [out] */ IDispatch __RPC_FAR *__RPC_FAR *ppDispatch);
	virtual HRESULT STDMETHODCALLTYPE TranslateUrl(/* [in] */ DWORD dwTranslate, /* [in] */ OLECHAR __RPC_FAR *pchURLIn, /* [out] */ OLECHAR __RPC_FAR *__RPC_FAR *ppchURLOut);
	virtual HRESULT STDMETHODCALLTYPE FilterDataObject(/* [in] */ IDataObject __RPC_FAR *pDO, /* [out] */ IDataObject __RPC_FAR *__RPC_FAR *ppDORet);

// IDocHostShowUI
	virtual HRESULT STDMETHODCALLTYPE ShowMessage(/* [in] */ HWND hwnd, /* [in] */ LPOLESTR lpstrText, /* [in] */ LPOLESTR lpstrCaption, /* [in] */ DWORD dwType, /* [in] */ LPOLESTR lpstrHelpFile, /* [in] */ DWORD dwHelpContext,/* [out] */ LRESULT __RPC_FAR *plResult);
	virtual HRESULT STDMETHODCALLTYPE ShowHelp(/* [in] */ HWND hwnd, /* [in] */ LPOLESTR pszHelpFile, /* [in] */ UINT uCommand, /* [in] */ DWORD dwData, /* [in] */ POINT ptMouse, /* [out] */ IDispatch __RPC_FAR *pDispatchObjectHit);
};

typedef CComObject<CBrowserCtlSite> CBrowserCtlSiteInstance;

#endif //__CBROWSERCTLSITE_H_
