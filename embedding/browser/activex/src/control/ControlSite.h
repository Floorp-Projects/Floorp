/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef CONTROLSITE_H
#define CONTROLSITE_H

#include "IOleCommandTargetImpl.h"

// If you created a class derived from CControlSite, use the following macro
// in the interface map of the derived class to include all the necessary
// interfaces.
#define CCONTROLSITE_INTERFACES() \
    COM_INTERFACE_ENTRY(IOleWindow) \
    COM_INTERFACE_ENTRY(IOleClientSite) \
    COM_INTERFACE_ENTRY(IOleInPlaceSite) \
    COM_INTERFACE_ENTRY_IID(IID_IOleInPlaceSite, IOleInPlaceSiteWindowless) \
    COM_INTERFACE_ENTRY_IID(IID_IOleInPlaceSiteEx, IOleInPlaceSiteWindowless) \
    COM_INTERFACE_ENTRY_IID(IID_IOleInPlaceSiteWindowless, IOleInPlaceSiteWindowless) \
    COM_INTERFACE_ENTRY(IOleControlSite) \
    COM_INTERFACE_ENTRY(IDispatch) \
    COM_INTERFACE_ENTRY_IID(IID_IAdviseSink, IAdviseSinkEx) \
    COM_INTERFACE_ENTRY_IID(IID_IAdviseSink2, IAdviseSinkEx) \
    COM_INTERFACE_ENTRY_IID(IID_IAdviseSinkEx, IAdviseSinkEx) \
    COM_INTERFACE_ENTRY(IOleCommandTarget)


//
// Class for hosting an ActiveX control
//
// This class supports both windowed and windowless classes. The normal
// steps to hosting a control are this:
//
//   CControlSiteInstance *pSite = NULL;
//   CControlSiteInstance::CreateInstance(&pSite);
//   pSite->AddRef();
//   pSite->Create(clsidControlToCreate);
//   pSite->Attach(hwndParentWindow, rcPosition);
//
// Where propertyList is a named list of values to initialise the new object
// with, hwndParentWindow is the window in which the control is being created,
// and rcPosition is the position in window coordinates where the control will
// be rendered.
//
// Destruction is this:
//
//   pSite->Detach();
//   pSite->Release();
//   pSite = NULL;

class CControlSite :    public CComObjectRootEx<CComSingleThreadModel>,
                        public IOleClientSite,
                        public IOleInPlaceSiteWindowless,
                        public IOleControlSite,
                        public IAdviseSinkEx,
                        public IDispatch,
                        public IOleCommandTargetImpl<CControlSite>
{
protected:
// Site management values
    // Handle to parent window
    HWND m_hWndParent;
    // Position of the site and the contained object
    RECT m_rcObjectPos;
    // Flag indicating if client site should be set early or late
    unsigned m_bSetClientSiteFirst:1;
    // Flag indicating whether control is visible or not
    unsigned m_bVisibleAtRuntime:1;
    // Flag indicating if control is in-place active
    unsigned m_bInPlaceActive:1;
    // Flag indicating if control is UI active
    unsigned m_bUIActive:1;
    // Flag indicating if control is in-place locked and cannot be deactivated
    unsigned m_bInPlaceLocked:1;
    // Flag indicating if the site allows windowless controls
    unsigned m_bSupportWindowlessActivation:1;
    // Flag indicating if control is windowless
    unsigned m_bWindowless:1;
    // Flag indicating if only safely scriptable controls are allowed
    unsigned m_bSafeForScriptingObjectsOnly:1;

// Pointers to object interfaces
    // Raw pointer to the object
    CComPtr<IUnknown> m_spObject;
    // Pointer to objects IViewObject interface
    CComQIPtr<IViewObject, &IID_IViewObject> m_spIViewObject;
    // Pointer to object's IOleObject interface
    CComQIPtr<IOleObject, &IID_IOleObject> m_spIOleObject;
    // Pointer to object's IOleInPlaceObject interface
    CComQIPtr<IOleInPlaceObject, &IID_IOleInPlaceObject> m_spIOleInPlaceObject;
    // Pointer to object's IOleInPlaceObjectWindowless interface
    CComQIPtr<IOleInPlaceObjectWindowless, &IID_IOleInPlaceObjectWindowless> m_spIOleInPlaceObjectWindowless;
    // Name of this control
    tstring m_szName;
    // CLSID of the control
    CLSID m_clsid;
    // Parameter list
    PropertyList m_ParameterList;

// Double buffer drawing variables used for windowless controls
    // Area of buffer
    RECT m_rcBuffer;
    // Bitmap to buffer
    HBITMAP m_hBMBuffer;
    // Bitmap to buffer
    HBITMAP m_hBMBufferOld;
    // Device context
    HDC m_hDCBuffer;
    // Clipping area of site
    HRGN m_hRgnBuffer;
    // Flags indicating how the buffer was painted
    DWORD m_dwBufferFlags;

// Ambient properties
    // Locale ID
    LCID m_nAmbientLocale;
    // Foreground colour
    COLORREF m_clrAmbientForeColor;
    // Background colour
    COLORREF m_clrAmbientBackColor;
    // Flag indicating if control should hatch itself
    bool m_bAmbientShowHatching:1;
    // Flag indicating if control should have grab handles
    bool m_bAmbientShowGrabHandles:1;
    // Flag indicating if control is in edit/user mode
    bool m_bAmbientUserMode:1;
    // Flag indicating if control has a 3d border or not
    bool m_bAmbientAppearance:1;

protected:
    // Notifies the attached control of a change to an ambient property
    virtual void FireAmbientPropertyChange(DISPID id);

public:
// Construction and destruction
    // Constructor
    CControlSite();
    // Destructor
    virtual ~CControlSite();

BEGIN_COM_MAP(CControlSite)
    CCONTROLSITE_INTERFACES()
END_COM_MAP()

BEGIN_OLECOMMAND_TABLE()
END_OLECOMMAND_TABLE()

    // List of controls
    static std::list<CControlSite *> m_cControlList;

    // Helper method
    static HRESULT ClassImplementsCategory(const CLSID &clsid, const CATID &catid);

    // Returns the window used when processing ole commands
    HWND GetCommandTargetWindow()
    {
        return NULL; // TODO
    }

// Object creation and management functions
    // Creates and initialises an object
    virtual HRESULT Create(REFCLSID clsid, PropertyList &pl = PropertyList(), const tstring szName = _T(""));
    // Attaches the object to the site
    virtual HRESULT Attach(HWND hwndParent, const RECT &rcPos, IUnknown *pInitStream = NULL);
    // Detaches the object from the site
    virtual HRESULT Detach();
    // Returns the IUnknown pointer for the object
    virtual HRESULT GetControlUnknown(IUnknown **ppObject);
    // Sets the bounding rectangle for the object
    virtual HRESULT SetPosition(const RECT &rcPos);
    // Draws the object using the provided DC
    virtual HRESULT Draw(HDC hdc);
    // Performs the specified action on the object
    virtual HRESULT DoVerb(LONG nVerb, LPMSG lpMsg = NULL);
    // Sets an advise sink up for changes to the object
    virtual HRESULT Advise(IUnknown *pIUnkSink, const IID &iid, DWORD *pdwCookie);
    // Removes an advise sink
    virtual HRESULT Unadvise(const IID &iid, DWORD dwCookie);

// Methods to set ambient properties
    virtual void SetAmbientUserMode(BOOL bUser);

// Inline helper methods
    // Returns the object's CLSID
    virtual const CLSID &GetObjectCLSID() const
    {
        return m_clsid;
    }
    // Returns the name of the object
    virtual const tstring &GetObjectName() const
    {
        return m_szName;
    }
    // Tests if the object is valid or not
    virtual BOOL IsObjectValid() const
    {
        return (m_spObject) ? TRUE : FALSE;
    }
    // Returns the parent window to this one
    virtual HWND GetParentWindow() const
    {
        return m_hWndParent;
    }
    // Returns the inplace active state of the object
    virtual BOOL IsInPlaceActive() const
    {
        return m_bInPlaceActive;
    }

// IDispatch
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(/* [out] */ UINT __RPC_FAR *pctinfo);
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(/* [in] */ UINT iTInfo, /* [in] */ LCID lcid, /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo);
    virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(/* [in] */ REFIID riid, /* [size_is][in] */ LPOLESTR __RPC_FAR *rgszNames, /* [in] */ UINT cNames, /* [in] */ LCID lcid, /* [size_is][out] */ DISPID __RPC_FAR *rgDispId);
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Invoke(/* [in] */ DISPID dispIdMember, /* [in] */ REFIID riid, /* [in] */ LCID lcid, /* [in] */ WORD wFlags, /* [out][in] */ DISPPARAMS __RPC_FAR *pDispParams, /* [out] */ VARIANT __RPC_FAR *pVarResult, /* [out] */ EXCEPINFO __RPC_FAR *pExcepInfo, /* [out] */ UINT __RPC_FAR *puArgErr);

// IAdviseSink implementation
    virtual /* [local] */ void STDMETHODCALLTYPE OnDataChange(/* [unique][in] */ FORMATETC __RPC_FAR *pFormatetc, /* [unique][in] */ STGMEDIUM __RPC_FAR *pStgmed);
    virtual /* [local] */ void STDMETHODCALLTYPE OnViewChange(/* [in] */ DWORD dwAspect, /* [in] */ LONG lindex);
    virtual /* [local] */ void STDMETHODCALLTYPE OnRename(/* [in] */ IMoniker __RPC_FAR *pmk);
    virtual /* [local] */ void STDMETHODCALLTYPE OnSave(void);
    virtual /* [local] */ void STDMETHODCALLTYPE OnClose(void);

// IAdviseSink2
    virtual /* [local] */ void STDMETHODCALLTYPE OnLinkSrcChange(/* [unique][in] */ IMoniker __RPC_FAR *pmk);

// IAdviseSinkEx implementation
    virtual /* [local] */ void STDMETHODCALLTYPE OnViewStatusChange(/* [in] */ DWORD dwViewStatus);

// IOleWindow implementation
    virtual /* [input_sync] */ HRESULT STDMETHODCALLTYPE GetWindow(/* [out] */ HWND __RPC_FAR *phwnd);
    virtual HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(/* [in] */ BOOL fEnterMode);

// IOleClientSite implementation
    virtual HRESULT STDMETHODCALLTYPE SaveObject(void);
    virtual HRESULT STDMETHODCALLTYPE GetMoniker(/* [in] */ DWORD dwAssign, /* [in] */ DWORD dwWhichMoniker, /* [out] */ IMoniker __RPC_FAR *__RPC_FAR *ppmk);
    virtual HRESULT STDMETHODCALLTYPE GetContainer(/* [out] */ IOleContainer __RPC_FAR *__RPC_FAR *ppContainer);
    virtual HRESULT STDMETHODCALLTYPE ShowObject(void);
    virtual HRESULT STDMETHODCALLTYPE OnShowWindow(/* [in] */ BOOL fShow);
    virtual HRESULT STDMETHODCALLTYPE RequestNewObjectLayout(void);

// IOleInPlaceSite implementation
    virtual HRESULT STDMETHODCALLTYPE CanInPlaceActivate(void);
    virtual HRESULT STDMETHODCALLTYPE OnInPlaceActivate(void);
    virtual HRESULT STDMETHODCALLTYPE OnUIActivate(void);
    virtual HRESULT STDMETHODCALLTYPE GetWindowContext(/* [out] */ IOleInPlaceFrame __RPC_FAR *__RPC_FAR *ppFrame, /* [out] */ IOleInPlaceUIWindow __RPC_FAR *__RPC_FAR *ppDoc, /* [out] */ LPRECT lprcPosRect, /* [out] */ LPRECT lprcClipRect, /* [out][in] */ LPOLEINPLACEFRAMEINFO lpFrameInfo);
    virtual HRESULT STDMETHODCALLTYPE Scroll(/* [in] */ SIZE scrollExtant);
    virtual HRESULT STDMETHODCALLTYPE OnUIDeactivate(/* [in] */ BOOL fUndoable);
    virtual HRESULT STDMETHODCALLTYPE OnInPlaceDeactivate(void);
    virtual HRESULT STDMETHODCALLTYPE DiscardUndoState(void);
    virtual HRESULT STDMETHODCALLTYPE DeactivateAndUndo(void);
    virtual HRESULT STDMETHODCALLTYPE OnPosRectChange(/* [in] */ LPCRECT lprcPosRect);

// IOleInPlaceSiteEx implementation
    virtual HRESULT STDMETHODCALLTYPE OnInPlaceActivateEx(/* [out] */ BOOL __RPC_FAR *pfNoRedraw, /* [in] */ DWORD dwFlags);
    virtual HRESULT STDMETHODCALLTYPE OnInPlaceDeactivateEx(/* [in] */ BOOL fNoRedraw);
    virtual HRESULT STDMETHODCALLTYPE RequestUIActivate(void);

// IOleInPlaceSiteWindowless implementation
    virtual HRESULT STDMETHODCALLTYPE CanWindowlessActivate(void);
    virtual HRESULT STDMETHODCALLTYPE GetCapture(void);
    virtual HRESULT STDMETHODCALLTYPE SetCapture(/* [in] */ BOOL fCapture);
    virtual HRESULT STDMETHODCALLTYPE GetFocus(void);
    virtual HRESULT STDMETHODCALLTYPE SetFocus(/* [in] */ BOOL fFocus);
    virtual HRESULT STDMETHODCALLTYPE GetDC(/* [in] */ LPCRECT pRect, /* [in] */ DWORD grfFlags, /* [out] */ HDC __RPC_FAR *phDC);
    virtual HRESULT STDMETHODCALLTYPE ReleaseDC(/* [in] */ HDC hDC);
    virtual HRESULT STDMETHODCALLTYPE InvalidateRect(/* [in] */ LPCRECT pRect, /* [in] */ BOOL fErase);
    virtual HRESULT STDMETHODCALLTYPE InvalidateRgn(/* [in] */ HRGN hRGN, /* [in] */ BOOL fErase);
    virtual HRESULT STDMETHODCALLTYPE ScrollRect(/* [in] */ INT dx, /* [in] */ INT dy, /* [in] */ LPCRECT pRectScroll, /* [in] */ LPCRECT pRectClip);
    virtual HRESULT STDMETHODCALLTYPE AdjustRect(/* [out][in] */ LPRECT prc);
    virtual HRESULT STDMETHODCALLTYPE OnDefWindowMessage(/* [in] */ UINT msg, /* [in] */ WPARAM wParam, /* [in] */ LPARAM lParam, /* [out] */ LRESULT __RPC_FAR *plResult);

// IOleControlSite implementation
    virtual HRESULT STDMETHODCALLTYPE OnControlInfoChanged(void);
    virtual HRESULT STDMETHODCALLTYPE LockInPlaceActive(/* [in] */ BOOL fLock);
    virtual HRESULT STDMETHODCALLTYPE GetExtendedControl(/* [out] */ IDispatch __RPC_FAR *__RPC_FAR *ppDisp);
    virtual HRESULT STDMETHODCALLTYPE TransformCoords(/* [out][in] */ POINTL __RPC_FAR *pPtlHimetric, /* [out][in] */ POINTF __RPC_FAR *pPtfContainer, /* [in] */ DWORD dwFlags);
    virtual HRESULT STDMETHODCALLTYPE TranslateAccelerator(/* [in] */ MSG __RPC_FAR *pMsg, /* [in] */ DWORD grfModifiers);
    virtual HRESULT STDMETHODCALLTYPE OnFocus(/* [in] */ BOOL fGotFocus);
    virtual HRESULT STDMETHODCALLTYPE ShowPropertyFrame( void);
};

typedef CComObject<CControlSite> CControlSiteInstance;



#endif