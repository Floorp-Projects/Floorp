/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Adam Lock <adamlock@eircom.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
// MozillaBrowser.h : Declaration of the CMozillaBrowser

#ifndef __MOZILLABROWSER_H_
#define __MOZILLABROWSER_H_

#include "IWebBrowserImpl.h"

// Commands sent via WM_COMMAND
enum {
    ID_PRINT = 1,
    ID_PAGESETUP,
    ID_VIEWSOURCE,
    ID_SAVEAS,
    ID_PROPERTIES,
    ID_CUT,
    ID_COPY,
    ID_PASTE,
    ID_SELECTALL
};

// Command group and IDs exposed through IOleCommandTarget
extern GUID CGID_IWebBrowser_Moz;
extern GUID CGID_MSHTML_Moz;

enum {
    HTMLID_FIND = 1,
    HTMLID_VIEWSOURCE,
    HTMLID_OPTIONS
};

// A list of objects
typedef CComPtr<IUnknown> CComUnkPtr;

class CWebBrowserContainer;
class CPromptService;

/////////////////////////////////////////////////////////////////////////////
// CMozillaBrowser
class ATL_NO_VTABLE CMozillaBrowser : 
    public CComObjectRootEx<CComMultiThreadModel>,
    public CComCoClass<CMozillaBrowser, &CLSID_MozillaBrowser>,
    public CComControl<CMozillaBrowser>,
    public IPropertyNotifySinkCP<CMozillaBrowser>,
    public IWebBrowserImpl<CMozillaBrowser, &CLSID_MozillaBrowser, &LIBID_MOZILLACONTROLLib>,
    public IProvideClassInfo2Impl<&CLSID_MozillaBrowser, &DIID_DWebBrowserEvents2, &LIBID_MOZILLACONTROLLib>,
    public IPersistStreamInitImpl<CMozillaBrowser>,
    public IPersistStorageImpl<CMozillaBrowser>,
    public IQuickActivateImpl<CMozillaBrowser>,
    public IOleControlImpl<CMozillaBrowser>,
    public IOleObjectImpl<CMozillaBrowser>,
    public IOleInPlaceActiveObjectImpl<CMozillaBrowser>,
    public IViewObjectExImpl<CMozillaBrowser>,
    public IOleInPlaceObjectWindowlessImpl<CMozillaBrowser>,
    public IDataObjectImpl<CMozillaBrowser>,
    public ISupportErrorInfo,
    public IOleCommandTargetImpl<CMozillaBrowser>,
    public IConnectionPointContainerImpl<CMozillaBrowser>,
    public ISpecifyPropertyPagesImpl<CMozillaBrowser>,
    public IMozControlBridge
{
    friend CWebBrowserContainer;
    friend CPromptService;

public:
    CMozillaBrowser();
    virtual ~CMozillaBrowser();

DECLARE_REGISTRY_RESOURCEID(IDR_MOZILLABROWSER)

BEGIN_COM_MAP(CMozillaBrowser)
    // Mozilla control interfaces
    COM_INTERFACE_ENTRY(IMozControlBridge)
    // IE web browser interface
    COM_INTERFACE_ENTRY(IWebBrowser2)
    COM_INTERFACE_ENTRY_IID(IID_IDispatch, IWebBrowser2)
    COM_INTERFACE_ENTRY_IID(IID_IWebBrowser, IWebBrowser2)
    COM_INTERFACE_ENTRY_IID(IID_IWebBrowserApp, IWebBrowser2)
    // Outgoing IE event interfaces
    COM_INTERFACE_ENTRY_IID(DIID_DWebBrowserEvents,
            CProxyDWebBrowserEvents<CMozillaBrowser>)
    COM_INTERFACE_ENTRY_IID(DIID_DWebBrowserEvents2,
            CProxyDWebBrowserEvents2<CMozillaBrowser>)
    // Other ActiveX/OLE interfaces
    COM_INTERFACE_ENTRY_IMPL(IViewObjectEx)
    COM_INTERFACE_ENTRY_IMPL_IID(IID_IViewObject2, IViewObjectEx)
    COM_INTERFACE_ENTRY_IMPL_IID(IID_IViewObject, IViewObjectEx)
    COM_INTERFACE_ENTRY_IMPL(IOleInPlaceObjectWindowless)
    COM_INTERFACE_ENTRY_IMPL_IID(IID_IOleInPlaceObject, IOleInPlaceObjectWindowless)
    COM_INTERFACE_ENTRY_IMPL(IOleInPlaceActiveObject)
    COM_INTERFACE_ENTRY_IMPL(IOleControl)
    COM_INTERFACE_ENTRY_IMPL(IOleObject)
    COM_INTERFACE_ENTRY_IMPL(IQuickActivate) // This causes size assertion in ATL
    COM_INTERFACE_ENTRY_IMPL(IPersistStorage)
    COM_INTERFACE_ENTRY_IMPL(IPersistStreamInit)
    COM_INTERFACE_ENTRY_IMPL(ISpecifyPropertyPages)
    COM_INTERFACE_ENTRY_IMPL(IDataObject)
    COM_INTERFACE_ENTRY(IOleCommandTarget)
    COM_INTERFACE_ENTRY(IProvideClassInfo)
    COM_INTERFACE_ENTRY(IProvideClassInfo2)
    COM_INTERFACE_ENTRY(ISupportErrorInfo)
    COM_INTERFACE_ENTRY_IMPL(IConnectionPointContainer)
END_COM_MAP()

// Properties supported by the control that map onto property
// pages that the user may be able to configure from tools like VB.

BEGIN_PROPERTY_MAP(CMozillaBrowser)
    // Example entries
    // PROP_ENTRY("Property Description", dispid, clsid)
    PROP_PAGE(CLSID_StockColorPage)
END_PROPERTY_MAP()

// Table of outgoing connection points. Anyone subscribing
// to events from the control should do so through one of these
// connect points.

BEGIN_CONNECTION_POINT_MAP(CMozillaBrowser)
    CONNECTION_POINT_ENTRY(IID_IPropertyNotifySink)
    // Fires IE events
    CONNECTION_POINT_ENTRY(DIID_DWebBrowserEvents2)
    CONNECTION_POINT_ENTRY(DIID_DWebBrowserEvents)
END_CONNECTION_POINT_MAP()

// Table of window messages and their associate handlers

BEGIN_MSG_MAP(CMozillaBrowser)
    MESSAGE_HANDLER(WM_CREATE, OnCreate)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    MESSAGE_HANDLER(WM_SIZE, OnSize)
    MESSAGE_HANDLER(WM_PAINT, OnPaint)
    MESSAGE_HANDLER(WM_SETFOCUS, OnSetFocus)
    MESSAGE_HANDLER(WM_KILLFOCUS, OnKillFocus)
    MESSAGE_HANDLER(WM_GETDLGCODE, OnGetDlgCode)
    MESSAGE_HANDLER(WM_MOUSEACTIVATE, OnMouseActivate)
    COMMAND_ID_HANDLER(ID_PRINT, OnPrint)
    COMMAND_ID_HANDLER(ID_PAGESETUP, OnPageSetup)
    COMMAND_ID_HANDLER(ID_SAVEAS, OnSaveAs)
    COMMAND_ID_HANDLER(ID_PROPERTIES, OnProperties)
    COMMAND_ID_HANDLER(ID_CUT, OnCut)
    COMMAND_ID_HANDLER(ID_COPY, OnCopy)
    COMMAND_ID_HANDLER(ID_PASTE, OnPaste)
    COMMAND_ID_HANDLER(ID_SELECTALL, OnSelectAll)
    COMMAND_ID_HANDLER(ID_DOCUMENT_BACK, OnDocumentBack)
    COMMAND_ID_HANDLER(ID_DOCUMENT_FORWARD, OnDocumentForward)
    COMMAND_ID_HANDLER(ID_DOCUMENT_PRINT, OnDocumentPrint)
    COMMAND_ID_HANDLER(ID_DOCUMENT_REFRESH, OnDocumentRefresh)
    COMMAND_ID_HANDLER(ID_DOCUMENT_PROPERTIES, OnDocumentProperties)
    COMMAND_ID_HANDLER(ID_DOCUMENT_VIEWSOURCE, OnViewSource)
    COMMAND_ID_HANDLER(ID_LINK_OPEN, OnLinkOpen)
    COMMAND_ID_HANDLER(ID_LINK_OPENINNEWWINDOW, OnLinkOpenInNewWindow)
    COMMAND_ID_HANDLER(ID_LINK_COPYSHORTCUT, OnLinkCopyShortcut)
    COMMAND_ID_HANDLER(ID_LINK_PROPERTIES, OnLinkProperties)
END_MSG_MAP()

    static HRESULT _stdcall PrintHandler(CMozillaBrowser *pThis, const GUID *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut);
    static HRESULT _stdcall EditModeHandler(CMozillaBrowser *pThis, const GUID *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut);
    static HRESULT _stdcall EditCommandHandler(CMozillaBrowser *pThis, const GUID *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut);

// Table of OLE commands (invoked through IOleCommandTarget and their
// associated command groups and command handlers.

BEGIN_OLECOMMAND_TABLE()
    // Standard "common" commands
    OLECOMMAND_HANDLER(OLECMDID_PRINT, NULL, PrintHandler, L"Print", L"Print the page")
    OLECOMMAND_MESSAGE(OLECMDID_PAGESETUP, NULL, ID_PAGESETUP, L"Page Setup", L"Page Setup")
    OLECOMMAND_MESSAGE(OLECMDID_UNDO, NULL, 0, L"Undo", L"Undo")
    OLECOMMAND_MESSAGE(OLECMDID_REDO, NULL, 0, L"Redo", L"Redo")
    OLECOMMAND_MESSAGE(OLECMDID_REFRESH, NULL, 0, L"Refresh", L"Refresh")
    OLECOMMAND_MESSAGE(OLECMDID_STOP, NULL, 0, L"Stop", L"Stop")
    OLECOMMAND_MESSAGE(OLECMDID_ONUNLOAD, NULL, 0, L"OnUnload", L"OnUnload")
    OLECOMMAND_MESSAGE(OLECMDID_SAVEAS, NULL, ID_SAVEAS, L"SaveAs", L"Save the page")
    OLECOMMAND_MESSAGE(OLECMDID_CUT, NULL, ID_CUT, L"Cut", L"Cut selection")
    OLECOMMAND_MESSAGE(OLECMDID_COPY, NULL, ID_COPY, L"Copy", L"Copy selection")
    OLECOMMAND_MESSAGE(OLECMDID_PASTE, NULL, ID_PASTE, L"Paste", L"Paste as selection")
    OLECOMMAND_MESSAGE(OLECMDID_SELECTALL, NULL, ID_SELECTALL, L"SelectAll", L"Select all")
    OLECOMMAND_MESSAGE(OLECMDID_PROPERTIES, NULL, ID_PROPERTIES, L"Properties", L"Show page properties")
    // Unsupported IE 4.x command group
    OLECOMMAND_MESSAGE(HTMLID_FIND, &CGID_IWebBrowser_Moz, 0, L"Find", L"Find")
    OLECOMMAND_MESSAGE(HTMLID_VIEWSOURCE, &CGID_IWebBrowser_Moz, ID_VIEWSOURCE, L"ViewSource", L"View Source")
    OLECOMMAND_MESSAGE(HTMLID_OPTIONS, &CGID_IWebBrowser_Moz, 0, L"Options", L"Options")
    // DHTML editor command group
    OLECOMMAND_HANDLER(IDM_EDITMODE, &CGID_MSHTML_Moz, EditModeHandler, L"EditMode", L"Switch to edit mode")
    OLECOMMAND_HANDLER(IDM_BROWSEMODE, &CGID_MSHTML_Moz, EditModeHandler, L"UserMode", L"Switch to user mode")
    OLECOMMAND_HANDLER(IDM_BOLD, &CGID_MSHTML_Moz, EditCommandHandler, L"Bold", L"Toggle Bold")
    OLECOMMAND_HANDLER(IDM_ITALIC, &CGID_MSHTML_Moz, EditCommandHandler, L"Italic", L"Toggle Italic")
    OLECOMMAND_HANDLER(IDM_UNDERLINE, &CGID_MSHTML_Moz, EditCommandHandler, L"Underline", L"Toggle Underline")
    OLECOMMAND_HANDLER(IDM_UNKNOWN, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_ALIGNBOTTOM, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_ALIGNHORIZONTALCENTERS, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_ALIGNLEFT, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_ALIGNRIGHT, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_ALIGNTOGRID, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_ALIGNTOP, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_ALIGNVERTICALCENTERS, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_ARRANGEBOTTOM, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_ARRANGERIGHT, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_BRINGFORWARD, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_BRINGTOFRONT, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_CENTERHORIZONTALLY, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_CENTERVERTICALLY, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_CODE, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_DELETE, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_FONTNAME, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_FONTSIZE, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_GROUP, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_HORIZSPACECONCATENATE, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_HORIZSPACEDECREASE, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_HORIZSPACEINCREASE, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_HORIZSPACEMAKEEQUAL, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_INSERTOBJECT, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_MULTILEVELREDO, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_SENDBACKWARD, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_SENDTOBACK, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_SHOWTABLE, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_SIZETOCONTROL, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_SIZETOCONTROLHEIGHT, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_SIZETOCONTROLWIDTH, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_SIZETOFIT, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_SIZETOGRID, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_SNAPTOGRID, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_TABORDER, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_TOOLBOX, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_MULTILEVELUNDO, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_UNGROUP, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_VERTSPACECONCATENATE, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_VERTSPACEDECREASE, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_VERTSPACEINCREASE, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_VERTSPACEMAKEEQUAL, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_JUSTIFYFULL, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_BACKCOLOR, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_BORDERCOLOR, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_FLAT, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_FORECOLOR, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_JUSTIFYCENTER, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_JUSTIFYGENERAL, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_JUSTIFYLEFT, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_JUSTIFYRIGHT, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_RAISED, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_SUNKEN, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_CHISELED, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_ETCHED, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_SHADOWED, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_FIND, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_SHOWGRID, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_OBJECTVERBLIST0, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_OBJECTVERBLIST1, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_OBJECTVERBLIST2, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_OBJECTVERBLIST3, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_OBJECTVERBLIST4, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_OBJECTVERBLIST5, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_OBJECTVERBLIST6, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_OBJECTVERBLIST7, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_OBJECTVERBLIST8, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_OBJECTVERBLIST9, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_OBJECTVERBLISTLAST, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_CONVERTOBJECT, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_CUSTOMCONTROL, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_CUSTOMIZEITEM, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_RENAME, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_IMPORT, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_NEWPAGE, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_MOVE, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_CANCEL, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_FONT, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_STRIKETHROUGH, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_DELETEWORD, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_EXECPRINT, &CGID_MSHTML_Moz, NULL, L"", L"")
    OLECOMMAND_HANDLER(IDM_JUSTIFYNONE, &CGID_MSHTML_Moz, NULL, L"", L"")
END_OLECOMMAND_TABLE()

    HWND GetCommandTargetWindow() const
    {
        return m_hWnd;
    }

// Windows message handlers
    LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnSetFocus(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnKillFocus(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnGetDlgCode(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnMouseActivate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

    LRESULT OnPrint(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnPageSetup(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnViewSource(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

    LRESULT OnSaveAs(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnProperties(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnCut(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnCopy(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnPaste(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnSelectAll(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

    LRESULT OnDocumentBack(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnDocumentForward(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnDocumentPrint(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnDocumentRefresh(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnDocumentProperties(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    
    LRESULT OnLinkOpen(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnLinkOpenInNewWindow(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnLinkCopyShortcut(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
    LRESULT OnLinkProperties(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

// ISupportsErrorInfo
    STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid);

// IViewObjectEx
    STDMETHOD(GetViewStatus)(DWORD* pdwStatus)
    {
        ATLTRACE(_T("IViewObjectExImpl::GetViewStatus\n"));
        *pdwStatus = VIEWSTATUS_SOLIDBKGND | VIEWSTATUS_OPAQUE;
        return S_OK;
    }

// Protected members
protected:

    // List of browsers
    static nsVoidArray sBrowserList;

    // Name of profile to use
    nsString mProfileName;

    // Pointer to web browser manager
    CWebBrowserContainer    * mWebBrowserContainer;
    // CComObject to IHTMLDocument implementer
    CIEHtmlDocumentInstance * mIERootDocument;

    // Mozilla interfaces
    nsCOMPtr<nsIWebBrowser> mWebBrowser;
    nsCOMPtr<nsIBaseWindow> mWebBrowserAsWin;
    nsCOMPtr<nsIEditingSession> mEditingSession;
    nsCOMPtr<nsICommandManager> mCommandManager;

    // Context menu
    nsCOMPtr<nsIDOMNode>    mContextNode;
    
    // Prefs service
    nsCOMPtr<nsIPrefBranch> mPrefBranch;

    // Flag to indicate if browser is created or not
    BOOL                    mValidBrowserFlag;
    // Flag to indicate if browser is in edit mode or not
    BOOL                    mEditModeFlag;
    // Flag to indicate if the browser has a drop target
    BOOL                    mHaveDropTargetFlag;
    // Contains an error message if startup went wrong
    tstring                 mStartupErrorMessage;
    // List of registered browser helper objects
    CComUnkPtr             *mBrowserHelperList;
    ULONG                   mBrowserHelperListCount;

    virtual HRESULT Initialize();
    virtual HRESULT Terminate();
    virtual HRESULT CreateBrowser();
    virtual HRESULT DestroyBrowser();
    virtual HRESULT SetStartupErrorMessage(UINT nStringID);
    virtual HRESULT GetDOMDocument(nsIDOMDocument **pDocument);
    virtual HRESULT SetEditorMode(BOOL bEnabled);
    virtual HRESULT OnEditorCommand(DWORD nCmdID);
    virtual HRESULT PrintDocument(BOOL promptUser);

    virtual HRESULT LoadBrowserHelpers();
    virtual HRESULT UnloadBrowserHelpers();

    // User interface methods
    virtual int MessageBox(LPCTSTR lpszText, LPCTSTR lpszCaption = _T(""), UINT nType = MB_OK);
    virtual void ShowContextMenu(PRUint32 aContextFlags, nsIDOMEvent *aEvent, nsIDOMNode *aNode);
    virtual void ShowURIPropertyDlg(const nsAString &aURI, const nsAString &aContentType);
    virtual void NextDlgControl();
    virtual void PrevDlgControl();

public:
// IOleObjectImpl overrides
    HRESULT InPlaceActivate(LONG iVerb, const RECT* prcPosRect);

// IOleObject overrides
    virtual HRESULT STDMETHODCALLTYPE CMozillaBrowser::GetClientSite(IOleClientSite **ppClientSite);

// IMozControlBridge implementation
    virtual HRESULT STDMETHODCALLTYPE GetWebBrowser(/* [out] */ void __RPC_FAR *__RPC_FAR *aBrowser);

// IWebBrowserImpl overrides
    virtual nsresult GetWebNavigation(nsIWebNavigation **aWebNav);
    virtual nsresult GetDOMWindow(nsIDOMWindow **aDOMWindow);
    virtual nsresult GetPrefs(nsIPrefBranch **aPrefBranch);
    virtual PRBool BrowserIsValid();
    // IWebBrowser
    virtual HRESULT STDMETHODCALLTYPE get_Parent(IDispatch __RPC_FAR *__RPC_FAR *ppDisp);
    virtual HRESULT STDMETHODCALLTYPE get_Document(IDispatch __RPC_FAR *__RPC_FAR *ppDisp);
    virtual HRESULT STDMETHODCALLTYPE get_RegisterAsDropTarget(VARIANT_BOOL __RPC_FAR *pbRegister);
    virtual HRESULT STDMETHODCALLTYPE put_RegisterAsDropTarget(VARIANT_BOOL bRegister);

public:
    HRESULT OnDraw(ATL_DRAWINFO& di);

};

#endif //__MOZILLABROWSER_H_
