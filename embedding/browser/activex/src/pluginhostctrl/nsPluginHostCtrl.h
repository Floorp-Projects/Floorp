/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *
 *   Adam Lock <adamlock@netscape.com> 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

// nsPluginHostCtrl.h : Declaration of the nsPluginHostCtrl

#ifndef __PLUGINHOSTCTRL_H_
#define __PLUGINHOSTCTRL_H_

#include "resource.h"       // main symbols
#include <atlctl.h>

#include "nsPluginWnd.h"

class nsURLDataCallback;

/////////////////////////////////////////////////////////////////////////////
// nsPluginHostCtrl
class ATL_NO_VTABLE nsPluginHostCtrl : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CStockPropImpl<nsPluginHostCtrl, IMozPluginHostCtrl, &IID_IMozPluginHostCtrl, &LIBID_PLUGINHOSTCTRLLib>,
	public CComControl<nsPluginHostCtrl>,
	public IPersistStreamInitImpl<nsPluginHostCtrl>,
	public IOleControlImpl<nsPluginHostCtrl>,
	public IOleObjectImpl<nsPluginHostCtrl>,
	public IOleInPlaceActiveObjectImpl<nsPluginHostCtrl>,
	public IViewObjectExImpl<nsPluginHostCtrl>,
	public IOleInPlaceObjectWindowlessImpl<nsPluginHostCtrl>,
	public ISupportErrorInfo,
	public IConnectionPointContainerImpl<nsPluginHostCtrl>,
	public IPersistStorageImpl<nsPluginHostCtrl>,
    public IPersistPropertyBagImpl<nsPluginHostCtrl>,
	public ISpecifyPropertyPagesImpl<nsPluginHostCtrl>,
	public IQuickActivateImpl<nsPluginHostCtrl>,
	public IDataObjectImpl<nsPluginHostCtrl>,
	public IProvideClassInfo2Impl<&CLSID_MozPluginHostCtrl, &DIID__IMozPluginHostCtrlEvents, &LIBID_PLUGINHOSTCTRLLib>,
	public IPropertyNotifySinkCP<nsPluginHostCtrl>,
	public CComCoClass<nsPluginHostCtrl, &CLSID_MozPluginHostCtrl>
{
protected:
    virtual ~nsPluginHostCtrl();

public:
	nsPluginHostCtrl();

DECLARE_REGISTRY_RESOURCEID(IDR_PLUGINHOSTCTRL)

DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(nsPluginHostCtrl)
	COM_INTERFACE_ENTRY(IMozPluginHostCtrl)
	COM_INTERFACE_ENTRY2(IDispatch, IMozPluginHostCtrl)
	COM_INTERFACE_ENTRY(IViewObjectEx)
	COM_INTERFACE_ENTRY(IViewObject2)
	COM_INTERFACE_ENTRY(IViewObject)
	COM_INTERFACE_ENTRY(IOleInPlaceObjectWindowless)
	COM_INTERFACE_ENTRY(IOleInPlaceObject)
	COM_INTERFACE_ENTRY2(IOleWindow, IOleInPlaceObjectWindowless)
	COM_INTERFACE_ENTRY(IOleInPlaceActiveObject)
	COM_INTERFACE_ENTRY(IOleControl)
	COM_INTERFACE_ENTRY(IOleObject)
	COM_INTERFACE_ENTRY(IPersistStreamInit)
	COM_INTERFACE_ENTRY2(IPersist, IPersistStreamInit)
    COM_INTERFACE_ENTRY(IPersistPropertyBag)
	COM_INTERFACE_ENTRY(ISupportErrorInfo)
	COM_INTERFACE_ENTRY(IConnectionPointContainer)
//	COM_INTERFACE_ENTRY(ISpecifyPropertyPages)
	COM_INTERFACE_ENTRY(IQuickActivate)
	COM_INTERFACE_ENTRY(IPersistStorage)
	COM_INTERFACE_ENTRY(IDataObject)
	COM_INTERFACE_ENTRY(IProvideClassInfo)
	COM_INTERFACE_ENTRY(IProvideClassInfo2)
END_COM_MAP()

BEGIN_PROP_MAP(nsPluginHostCtrl)
	PROP_DATA_ENTRY("_cx", m_sizeExtent.cx, VT_UI4)
	PROP_DATA_ENTRY("_cy", m_sizeExtent.cy, VT_UI4)
	PROP_ENTRY("HWND", DISPID_HWND, CLSID_NULL)
	PROP_ENTRY("Text", DISPID_TEXT, CLSID_NULL)
    // Mozilla plugin host control properties
    PROP_ENTRY("type", 1, CLSID_NULL)
    PROP_ENTRY("src", 2, CLSID_NULL)
    PROP_ENTRY("pluginspage", 3, CLSID_NULL)
	// Example entries
	// PROP_ENTRY("Property Description", dispid, clsid)
	// PROP_PAGE(CLSID_StockColorPage)
END_PROP_MAP()

BEGIN_CONNECTION_POINT_MAP(nsPluginHostCtrl)
	CONNECTION_POINT_ENTRY(IID_IPropertyNotifySink)
END_CONNECTION_POINT_MAP()

BEGIN_MSG_MAP(nsPluginHostCtrl)
	MESSAGE_HANDLER(WM_CREATE, OnCreate)
	MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
	MESSAGE_HANDLER(WM_SIZE, OnSize)
	MESSAGE_HANDLER(WM_PAINT, OnPaint)
	CHAIN_MSG_MAP(CComControl<nsPluginHostCtrl>)
END_MSG_MAP()


// Handler prototypes:
//  LRESULT MessageHandler(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
//  LRESULT CommandHandler(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
//  LRESULT NotifyHandler(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);



// ISupportsErrorInfo
	STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid)
	{
		static const IID* arr[] = 
		{
			&IID_IMozPluginHostCtrl,
		};
		for (int i=0; i<sizeof(arr)/sizeof(arr[0]); i++)
		{
			if (InlineIsEqualGUID(*arr[i], riid))
				return S_OK;
		}
		return S_FALSE;
	}

// IViewObjectEx
	DECLARE_VIEW_STATUS(0)

// IPersistPropertyBag override
	STDMETHOD(Load)(LPPROPERTYBAG pPropBag, LPERRORLOG pErrorLog);

// IMozPluginHostCtrl
public:
	STDMETHOD(get_PluginSource)(/*[out, retval]*/ BSTR *pVal);
	STDMETHOD(put_PluginSource)(/*[in]*/ BSTR newVal);
	STDMETHOD(get_PluginContentType)(/*[out, retval]*/ BSTR *pVal);
	STDMETHOD(put_PluginContentType)(/*[in]*/ BSTR newVal);
	STDMETHOD(get_PluginsPage)(/*[out, retval]*/ BSTR *pVal);
	STDMETHOD(put_PluginsPage)(/*[in]*/ BSTR newVal);

// Windows message handlers
public:
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

public:
	CComBSTR m_bstrText;

protected:
    CComBSTR m_bstrContentType;
    CComBSTR m_bstrSource;
    CComBSTR m_bstrPluginsPage;

    // Array of plugins
    struct PluginInfo
    {
        TCHAR *szPluginPath;
        TCHAR *szPluginName;
        TCHAR *szMIMEType;
    };
    PluginInfo  **m_pPlugins;
    unsigned long m_nPlugins;
    unsigned long m_nPluginsMax;

    // Array of names and values to pass to the new plugin
    unsigned long m_nArgs;
    unsigned long m_nArgsMax;
    char **m_pszArgNames;
    char **m_pszArgValues;

    // Array of loaded plugins which is shared amongst instances of this control
    struct LoadedPluginInfo
    {
        TCHAR *szFullPath; // Path + plugin name
        HINSTANCE hInstance;
        DWORD nRefCount;
        NPPluginFuncs NPPFuncs;
    };
    static LoadedPluginInfo **m_pLoadedPlugins;
    static unsigned long m_nLoadedPlugins;
    static unsigned long m_nLoadedPluginsMax;

    LoadedPluginInfo *m_pLoadedPlugin;

    nsPluginWnd m_wndPlugin;
    NPWindow m_NPWindow;

    static NPNetscapeFuncs g_NPNFuncs;
    static HRESULT InitPluginCallbacks();

    HRESULT CreatePluginList();
    HRESULT CleanupPluginList();
    HRESULT GetPluginInfo(const TCHAR * pszPluginPath, PluginInfo *pInfo);

public:
    NPP_t m_NPP;
    BOOL  m_bPluginIsAlive;
    BOOL  m_bCreatePluginFromStreamData;

    // Struct holding pointers to the functions within the plugin
    NPPluginFuncs m_NPPFuncs;

    HRESULT GetWebBrowserApp(IWebBrowserApp **pBrowser);
    HRESULT GetBaseURL(TCHAR **szBaseURL);
    
    HRESULT AddPluginParam(const char *szName, const char *szValue);
    HRESULT LoadPluginByContentType(const TCHAR *pszContentType);
	HRESULT LoadPlugin(const TCHAR *pszPluginPath);
    HRESULT FindPluginPathByContentType(const TCHAR *pszContentType, TCHAR **ppszPluginPath);
    HRESULT UnloadPlugin();

    HRESULT OpenURLStream(const TCHAR *szURL, void *pNotifyData, const void *pPostData, unsigned long nDataLength);

    HRESULT CreatePluginInstance();
    HRESULT DestroyPluginInstance();
    HRESULT SizeToFitPluginInstance();
};

#endif //__PLUGINHOSTCTRL_H_
