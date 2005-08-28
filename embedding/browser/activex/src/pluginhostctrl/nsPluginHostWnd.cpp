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
 *   Adam Lock <adamlock@eircom.net> 
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
#include "stdafx.h"

#include "nsPluginHostWnd.h"

#include "nsURLDataCallback.h"

#include "npn.h"

#define NS_4XPLUGIN_CALLBACK(_type, _name) _type (__stdcall * _name)

typedef NS_4XPLUGIN_CALLBACK(NPError, NP_GETENTRYPOINTS) (NPPluginFuncs* pCallbacks);
typedef NS_4XPLUGIN_CALLBACK(NPError, NP_PLUGININIT) (const NPNetscapeFuncs* pCallbacks);
typedef NS_4XPLUGIN_CALLBACK(NPError, NP_PLUGINSHUTDOWN) (void);

const kArraySizeIncrement = 10;

nsSimpleArray<nsPluginHostWnd::LoadedPluginInfo *> nsPluginHostWnd::m_LoadedPlugins;

nsPluginHostWnd::nsPluginHostWnd() :
    m_bPluginIsAlive(false),
    m_bCreatePluginFromStreamData(false),
    m_bPluginIsWindowless(false),
    m_bPluginIsTransparent(false),
    m_pLoadedPlugin(NULL),
    m_nArgs(0),
    m_nArgsMax(0),
    m_pszArgNames(NULL),
    m_pszArgValues(NULL)
{
    InitPluginCallbacks();
    memset(&m_NPPFuncs, 0, sizeof(m_NPPFuncs));
}

nsPluginHostWnd::~nsPluginHostWnd()
{
}

LRESULT nsPluginHostWnd::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    SetWindowLong(GWL_STYLE, GetWindowLong(GWL_STYLE) | WS_CLIPCHILDREN);

    // Load a list of plugins
    CreatePluginList(
        PLUGINS_FROM_IE | PLUGINS_FROM_NS4X |
        PLUGINS_FROM_FIREFOX | PLUGINS_FROM_MOZILLA);

    HRESULT hr = E_FAIL;
    if (m_bstrContentType.Length() == 0 &&
        m_bstrSource.Length() != 0)
    {
        USES_CONVERSION;
        // Do a late instantiation of the plugin based on the content type of
        // the stream data.
        m_bCreatePluginFromStreamData = true;
        hr = OpenURLStream(OLE2T(m_bstrSource), NULL, NULL, 0);
    }
    else
    {
        // Create a plugin based upon the specified content type property
        USES_CONVERSION;
        hr = LoadPluginByContentType(OLE2T(m_bstrContentType));
        if (SUCCEEDED(hr))
        {
            hr = CreatePluginInstance();
            if (m_bstrSource.Length())
            {
                OpenURLStream(OLE2T(m_bstrSource), NULL, NULL, 0);
            }
        }
    }

	return SUCCEEDED(hr) ? 0 : -1;
}

LRESULT nsPluginHostWnd::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    DestroyPluginInstance();
    UnloadPlugin();
    CleanupPluginList();
    return 0;
}

LRESULT nsPluginHostWnd::OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    SizeToFitPluginInstance();
    return 0;
}

LRESULT nsPluginHostWnd::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(&ps);

    RECT rc;
    GetClientRect(&rc);

    if (m_bPluginIsWindowless && m_NPPFuncs.event)
    {
        if (this->m_bPluginIsTransparent)
        {
            int x = 0;
            const int inc = 20;
            for (int i = rc.left; i < rc.right; i += inc)
            {
                const COLORREF c1 = RGB(255, 120, 120);
                const COLORREF c2 = RGB(120, 120, 255);
                RECT rcStrip = rc;
                HBRUSH hbr = CreateSolidBrush(x % 2 ? c1 : c2);
                rcStrip.left = i;
                rcStrip.right = i + inc;
                FillRect(hdc, &rcStrip, hbr);
                DeleteObject(hbr);
                x++;
            }
        }
        else
        {
            FillRect(hdc, &rc, (HBRUSH) GetStockObject(GRAY_BRUSH));
        }

        m_NPWindow.type = NPWindowTypeDrawable;
        m_NPWindow.window = hdc;
        m_NPWindow.x = 0;
        m_NPWindow.y = 0;
        m_NPWindow.width = rc.right - rc.left;
        m_NPWindow.height = rc.bottom - rc.top;
        m_NPWindow.clipRect.left = 0;
        m_NPWindow.clipRect.top = 0;
        m_NPWindow.clipRect.right = m_NPWindow.width;
        m_NPWindow.clipRect.bottom = m_NPWindow.height;

        if (m_NPPFuncs.setwindow)
        {
            NPError npres = m_NPPFuncs.setwindow(&m_NPP, &m_NPWindow);
        }

        NPRect paintRect;
        paintRect.left = rc.left;
        paintRect.top = rc.top;
        paintRect.right = rc.right;
        paintRect.bottom = rc.bottom;

        NPEvent evt;
        evt.event = WM_PAINT;
        evt.wParam = wParam;
        evt.lParam = (LPARAM) &paintRect;
        m_NPPFuncs.event(&m_NPP, &evt);
    }
    else
    {
        FillRect(hdc, &rc, (HBRUSH) GetStockObject(GRAY_BRUSH));
    }

    EndPaint(&ps);

    return 0;
}

LRESULT nsPluginHostWnd::OnMouseMove(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
    if (m_bPluginIsWindowless && m_NPPFuncs.event)
    {
        NPEvent evt;
        evt.event = uMsg;
        evt.wParam = wParam;
        evt.lParam = lParam;
        m_NPPFuncs.event(&m_NPP, &evt);
    }
    return 0;
}

LRESULT nsPluginHostWnd::OnLButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
    if (m_bPluginIsWindowless && m_NPPFuncs.event)
    {
        NPEvent evt;
        evt.event = uMsg;
        evt.wParam = wParam;
        evt.lParam = lParam;
        m_NPPFuncs.event(&m_NPP, &evt);
    }
    return 0;
}

LRESULT nsPluginHostWnd::OnLButtonUp(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
    if (m_bPluginIsWindowless && m_NPPFuncs.event)
    {
        NPEvent evt;
        evt.event = uMsg;
        evt.wParam = wParam;
        evt.lParam = lParam;
        m_NPPFuncs.event(&m_NPP, &evt);
    }
    return 0;
}


///////////////////////////////////////////////////////////////////////////////

NPNetscapeFuncs nsPluginHostWnd::g_NPNFuncs;

HRESULT nsPluginHostWnd::InitPluginCallbacks()
{
    static BOOL gCallbacksSet = FALSE;
    if (gCallbacksSet)
    {
        return S_OK;
    }

    gCallbacksSet = TRUE;

    memset(&g_NPNFuncs, 0, sizeof(g_NPNFuncs));
    g_NPNFuncs.size             = sizeof(g_NPNFuncs);
    g_NPNFuncs.version          = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;

    g_NPNFuncs.geturl           = NewNPN_GetURLProc(NPN_GetURL);
    g_NPNFuncs.posturl          = NewNPN_PostURLProc(NPN_PostURL);
    g_NPNFuncs.requestread      = NewNPN_RequestReadProc(NPN_RequestRead);
    g_NPNFuncs.newstream        = NewNPN_NewStreamProc(NPN_NewStream);
    g_NPNFuncs.write            = NewNPN_WriteProc(NPN_Write);
    g_NPNFuncs.destroystream    = NewNPN_DestroyStreamProc(NPN_DestroyStream);
    g_NPNFuncs.status           = NewNPN_StatusProc(NPN_Status);
    g_NPNFuncs.uagent           = NewNPN_UserAgentProc(NPN_UserAgent);
    g_NPNFuncs.memalloc         = NewNPN_MemAllocProc(NPN_MemAlloc);
    g_NPNFuncs.memfree          = NewNPN_MemFreeProc(NPN_MemFree);
    g_NPNFuncs.memflush         = NewNPN_MemFlushProc(NPN_MemFlush);
    g_NPNFuncs.reloadplugins    = NewNPN_ReloadPluginsProc(NPN_ReloadPlugins);
    g_NPNFuncs.getJavaEnv       = NewNPN_GetJavaEnvProc(NPN_GetJavaEnv);
    g_NPNFuncs.getJavaPeer      = NewNPN_GetJavaPeerProc(NPN_GetJavaPeer);
    g_NPNFuncs.geturlnotify     = NewNPN_GetURLNotifyProc(NPN_GetURLNotify);
    g_NPNFuncs.posturlnotify    = NewNPN_PostURLNotifyProc(NPN_PostURLNotify);
    g_NPNFuncs.getvalue         = NewNPN_GetValueProc(NPN_GetValue);
    g_NPNFuncs.setvalue         = NewNPN_SetValueProc(NPN_SetValue);
    g_NPNFuncs.invalidaterect   = NewNPN_InvalidateRectProc(NPN_InvalidateRect);
    g_NPNFuncs.invalidateregion = NewNPN_InvalidateRegionProc(NPN_InvalidateRegion);
    g_NPNFuncs.forceredraw      = NewNPN_ForceRedrawProc(NPN_ForceRedraw);

    return S_OK;
}

HRESULT nsPluginHostWnd::GetWebBrowserApp(IWebBrowserApp **pBrowser)
{
    // Override this method if there is a way to get this iface
    ATLASSERT(pBrowser);
    if (!pBrowser)
    {
        return E_INVALIDARG;
    }
    *pBrowser = NULL;
    return S_OK;
}

void nsPluginHostWnd::SetPluginWindowless(bool bWindowless)
{
    m_bPluginIsWindowless = bWindowless;
}

void nsPluginHostWnd::SetPluginTransparent(bool bTransparent)
{
    m_bPluginIsTransparent = bTransparent;
}

HRESULT nsPluginHostWnd::GetBaseURL(TCHAR **ppszBaseURL)
{
    ATLASSERT(ppszBaseURL);
    *ppszBaseURL = NULL;

    CComPtr<IWebBrowserApp> cpWebBrowser;
    GetWebBrowserApp(&cpWebBrowser);
    if (!cpWebBrowser)
    {
        return E_FAIL;
    }

    USES_CONVERSION;
    CComBSTR bstrURL;
    cpWebBrowser->get_LocationURL(&bstrURL);
    
    DWORD cbBaseURL = (bstrURL.Length() + 1) * sizeof(WCHAR);
    DWORD cbBaseURLUsed = 0;
    WCHAR *pszBaseURL = (WCHAR *) malloc(cbBaseURL);
    ATLASSERT(pszBaseURL);

    CoInternetParseUrl(
        bstrURL.m_str,
        PARSE_ROOTDOCUMENT,
        0,
        pszBaseURL,
        cbBaseURL,
        &cbBaseURLUsed,
        0);

    *ppszBaseURL = _tcsdup(W2T(pszBaseURL));
    free(pszBaseURL);

    return S_OK;
}

HRESULT nsPluginHostWnd::LoadPluginByContentType(const TCHAR *pszContentType)
{
    TCHAR * pszPluginPath = NULL;

    // Set the content type
    USES_CONVERSION;
    SetPluginContentType(T2OLE(pszContentType));

    // Search for a plugin that can handle this content
    HRESULT hr = FindPluginPathByContentType(pszContentType, &pszPluginPath);
    if (FAILED(hr))
    {
        // Try the default 'catch-all' plugin
        hr = FindPluginPathByContentType(_T("*"), &pszPluginPath);
        if (FAILED(hr))
        {
            return hr;
        }
    }

    hr = LoadPlugin(pszPluginPath);
    free(pszPluginPath);

    return hr;
}



HRESULT nsPluginHostWnd::GetPluginContentType(BSTR *pVal)
{
    if (!pVal)
    {
        return E_INVALIDARG;
    }
    *pVal = m_bstrContentType.Copy();
	return S_OK;
}

HRESULT nsPluginHostWnd::SetPluginContentType(BSTR newVal)
{
    // Security. Copying the source BSTR this way ensures that embedded NULL
    // characters do not end up in the destination BSTR. SysAllocString will
    // create a copy truncated at the first NULL char.
    m_bstrContentType.Empty();
    m_bstrContentType.Attach(SysAllocString(newVal));
	return S_OK;
}

HRESULT nsPluginHostWnd::GetPluginSource(BSTR *pVal)
{
    if (!pVal)
    {
        return E_INVALIDARG;
    }
    *pVal = m_bstrSource.Copy();
	return S_OK;
}

HRESULT nsPluginHostWnd::SetPluginSource(BSTR newVal)
{
    // Security. Copying the source BSTR this way ensures that embedded NULL
    // characters do not end up in the destination BSTR. SysAllocString will
    // create a copy truncated at the first NULL char.
    m_bstrSource.Empty();
    m_bstrSource.Attach(SysAllocString(newVal));
	return S_OK;
}

HRESULT nsPluginHostWnd::GetPluginsPage(BSTR *pVal)
{
    if (!pVal)
    {
        return E_INVALIDARG;
    }
    *pVal = m_bstrPluginsPage.Copy();
	return S_OK;
}

HRESULT nsPluginHostWnd::SetPluginsPage(BSTR newVal)
{
    // Security. Copying the source BSTR this way ensures that embedded NULL
    // characters do not end up in the destination BSTR. SysAllocString will
    // create a copy truncated at the first NULL char.
    m_bstrPluginsPage.Empty();
    m_bstrPluginsPage.Attach(SysAllocString(newVal));
	return S_OK;
}

HRESULT nsPluginHostWnd::ReadPluginsFromGeckoAppPath(const TCHAR *szAppName)
{
    const TCHAR szGeneralKey[] = _T("SOFTWARE\\Mozilla\\");
    const size_t nGeneralKeyLen = sizeof(szGeneralKey) / sizeof(TCHAR);

    const size_t nMainKeyLen = nGeneralKeyLen + _tcslen(szAppName);

    TCHAR *szMainKey = new TCHAR[nMainKeyLen];
    memset(szMainKey, 0, nMainKeyLen * sizeof(TCHAR));
    _tcscpy(szMainKey, szGeneralKey);
    _tcscat(szMainKey, szAppName);

    CRegKey keyGeneral;
    if (keyGeneral.Open(HKEY_LOCAL_MACHINE, szMainKey, KEY_READ) == ERROR_SUCCESS)
    {
        // First find the Current Version
        TCHAR szVersion[64];
        const size_t nVersionLen = sizeof(szVersion) / sizeof(szVersion[0]);
        memset(szVersion, 0, sizeof(szVersion));

        DWORD nVersion = nVersionLen;
        keyGeneral.QueryStringValue(_T("CurrentVersion"), szVersion, &nVersion);
        if (nVersion > 0)
        {
            TCHAR *szBracket = _tcschr(szVersion, TCHAR('('));
            if (szBracket)
            {
                while (szBracket >= szVersion)
                {
                    if (*szBracket == TCHAR(' ') || *szBracket == TCHAR('('))
                    {
                        *szBracket = TCHAR('\0');
                        szBracket--;
                    }
                    else
                        break;
                }
            }
            nVersion = _tcslen(szVersion);

            TCHAR *szKey = new TCHAR[nMainKeyLen + nVersion + 32];
            _tcscpy(szKey, szMainKey);
            _tcscat(szKey, _T(" "));
            _tcscat(szKey, szVersion);
            _tcscat(szKey, _T("\\Extensions"));

            CRegKey key;
            if (key.Open(HKEY_LOCAL_MACHINE, szKey, KEY_READ) == ERROR_SUCCESS)
            {
                TCHAR szPluginsDir[_MAX_PATH];
                memset(szPluginsDir, 0, sizeof(szPluginsDir));
                DWORD nPluginsDir = sizeof(szPluginsDir) / sizeof(szPluginsDir[0]);
                key.QueryStringValue(_T("Plugins"), szPluginsDir, &nPluginsDir);
                if (szPluginsDir[0])
                {
                    CreatePluginListFrom(szPluginsDir);
                }
            }
            delete []szKey;
        }
    }
    delete []szMainKey;
    return S_OK;
}

HRESULT nsPluginHostWnd::CreatePluginList(unsigned long ulFlags)
{
    // This function trawls through the plugin directory and builds a list
    // of plugins and what MIME types each plugin handles.

    CleanupPluginList();

    // Firefox
    if (ulFlags & PLUGINS_FROM_FIREFOX)
    {
        ReadPluginsFromGeckoAppPath(_T("Mozilla Firefox"));
    }

    // Mozilla
    if (ulFlags & PLUGINS_FROM_MOZILLA)
    {
        ReadPluginsFromGeckoAppPath(_T("Mozilla"));
    }

    // Try and obtain a path to the plugins in Netscape 4.x
    if (ulFlags & PLUGINS_FROM_NS4X)
    {
        TCHAR szPluginsDir[_MAX_PATH];
        memset(szPluginsDir, 0, sizeof(szPluginsDir));
        
        CRegKey keyNS;
        const TCHAR *kNav4xKey = _T("Software\\Netscape\\Netscape Navigator");
        if (keyNS.Open(HKEY_LOCAL_MACHINE, kNav4xKey, KEY_READ) == ERROR_SUCCESS)
        {
            TCHAR szVersion[10];
            DWORD nVersion = sizeof(szVersion) / sizeof(szVersion[0]);
            keyNS.QueryValue(szVersion, _T("CurrentVersion"), &nVersion);
        
            CRegKey keyVersion;
            if (keyVersion.Open(keyNS, szVersion, KEY_READ) == ERROR_SUCCESS)
            {
                CRegKey keyMain;
                if (keyMain.Open(keyVersion, _T("Main"), KEY_READ) == ERROR_SUCCESS)
                {
                    DWORD nPluginsDir = sizeof(szPluginsDir) / sizeof(szPluginsDir[0]);
                    keyMain.QueryValue(szPluginsDir, _T("Plugins Directory"), &nPluginsDir);
                    keyMain.Close();
                }
                keyVersion.Close();
            }
            keyNS.Close();
        }
        if (szPluginsDir[0])
        {
            CreatePluginListFrom(szPluginsDir);
        }
    }

    // Try and obtain a path to the plugins in Internet Explorer
    if (ulFlags & PLUGINS_FROM_IE)
    {
        TCHAR szPluginsDir[_MAX_PATH];
        memset(szPluginsDir, 0, sizeof(szPluginsDir));

        CRegKey keyIE;
        const TCHAR *kIEKey = _T("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\IEXPLORE.EXE");
        if (keyIE.Open(HKEY_LOCAL_MACHINE, kIEKey, KEY_READ) == ERROR_SUCCESS)
        {
            DWORD nPluginsDir = sizeof(szPluginsDir) / sizeof(szPluginsDir[0]);
            keyIE.QueryValue(szPluginsDir, _T("Path"), &nPluginsDir);

            TCHAR *szSemiColon = _tcschr(szPluginsDir, _TCHAR(';'));
            if (szSemiColon)
            {
                *szSemiColon = _TCHAR('\0');
            }

            ULONG nLen = _tcslen(szPluginsDir);
            if (nLen > 0 && szPluginsDir[nLen - 1] == _TCHAR('\\'))
            {
                szPluginsDir[nLen - 1] = _TCHAR('\0');
            }
            _tcscat(szPluginsDir, _T("\\Plugins"));

            keyIE.Close();
        }
        if (szPluginsDir[0])
        {
            CreatePluginListFrom(szPluginsDir);
        }
    }

    return S_OK;
}

HRESULT nsPluginHostWnd::CreatePluginListFrom(const TCHAR *szPluginsDir)
{
    HANDLE hFind;
    WIN32_FIND_DATA finddata;

    // Change to the plugin directory
    TCHAR szCurrentDir[MAX_PATH + 1];
    GetCurrentDirectory(sizeof(szCurrentDir) / sizeof(szCurrentDir[0]), szCurrentDir);
    SetCurrentDirectory(szPluginsDir);

    // Search for files matching the "np*dll" pattern
    hFind = FindFirstFile(_T("np*dll"), &finddata);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do {
            PluginInfo *pInfo = new PluginInfo;
            if (!pInfo)
            {
                CleanupPluginList();
                SetCurrentDirectory(szCurrentDir);
                return E_OUTOFMEMORY;
            }
            if (SUCCEEDED(GetPluginInfo(finddata.cFileName, pInfo)))
            {
                pInfo->szPluginName = _tcsdup(finddata.cFileName);
                pInfo->szPluginPath = _tcsdup(szPluginsDir);
                m_Plugins.AppendElement(pInfo);
            }
            else
            {
                ATLTRACE(_T("Error: Cannot plugin info for \"%s\".\n"), finddata.cFileName);
                delete pInfo;
            }
        } while (FindNextFile(hFind, &finddata));
        FindClose(hFind);
    }

    SetCurrentDirectory(szCurrentDir);

    return S_OK;
}


HRESULT nsPluginHostWnd::CleanupPluginList()
{
    // Free the memory used by the plugin info list
    for (unsigned long i = 0; i < m_Plugins.Count(); i++)
    {
        PluginInfo *pI = m_Plugins[i];
        if (pI->szMIMEType)
            free(pI->szMIMEType);
        if (pI->szPluginName)
            free(pI->szPluginName);
        if (pI->szPluginPath)
            free(pI->szPluginPath);
        free(pI);
    }
    m_Plugins.Empty();
    return S_OK;
}


HRESULT nsPluginHostWnd::GetPluginInfo(const TCHAR *pszPluginPath, PluginInfo *pInfo)
{
    // Get the version info from the plugin
    USES_CONVERSION;
    DWORD nVersionInfoSize;
    DWORD nZero = 0;
    void *pVersionInfo = NULL;
    nVersionInfoSize = GetFileVersionInfoSize((TCHAR *)pszPluginPath, &nZero);
    if (nVersionInfoSize)
    {
        pVersionInfo = malloc(nVersionInfoSize);
    }
    if (!pVersionInfo)
    {
        return E_OUTOFMEMORY;
    }

    GetFileVersionInfo((TCHAR *)pszPluginPath, NULL, nVersionInfoSize, pVersionInfo);

    // Extract the MIMEType info
    TCHAR *szValue = NULL;
    UINT nValueLength = 0;
    if (!VerQueryValue(pVersionInfo,
        _T("\\StringFileInfo\\040904E4\\MIMEType"),
        (void **) &szValue, &nValueLength))
    {
        return E_FAIL;
    }

    if (pInfo)
    {
        pInfo->szMIMEType = _tcsdup(szValue);
    }

    free(pVersionInfo);

    return S_OK;
}

HRESULT nsPluginHostWnd::FindPluginPathByContentType(const TCHAR *pszContentType, TCHAR **ppszPluginPath)
{
    *ppszPluginPath = NULL;

    if (pszContentType == NULL)
    {
        return E_FAIL;
    }

    // Search the list of plugins for one that will handle the content type
    TCHAR szPluginPath[_MAX_PATH + 1];
    unsigned long nContentType = _tcslen(pszContentType);
    for (unsigned long i = 0; i < m_Plugins.Count(); i++)
    {
        PluginInfo *pI = m_Plugins[i];
        if (pI->szMIMEType)
        {
            TCHAR *pszMIMEType = pI->szMIMEType;
            do {
                if (_tcsncmp(pszContentType, pszMIMEType, nContentType) == 0)
                {
                    // Found a match
                    _tmakepath(szPluginPath, NULL,
                        pI->szPluginPath, pI->szPluginName, NULL);
                    *ppszPluginPath = _tcsdup(szPluginPath);
                    return S_OK;
                }
                // Check the other types the plugin handles
                pszMIMEType = _tcschr(pszMIMEType, TCHAR('|'));
                if (pszMIMEType)
                {
                    pszMIMEType++;
                }
            } while (pszMIMEType && *pszMIMEType);
        }
    }

    return E_FAIL;
}

HRESULT nsPluginHostWnd::LoadPlugin(const TCHAR *szPluginPath)
{
    ATLASSERT(m_pLoadedPlugin == NULL);
    if (m_pLoadedPlugin)
    {
        return E_UNEXPECTED;
    }

    // TODO critical section

    // Test if the plugin has already been loaded
    for (unsigned long i = 0; i < m_LoadedPlugins.Count(); i++)
    {
        if (_tcscmp(m_LoadedPlugins[i]->szFullPath, szPluginPath) == 0)
        {
            m_pLoadedPlugin = m_LoadedPlugins[i];
            memcpy(&m_NPPFuncs, &m_pLoadedPlugin->NPPFuncs, sizeof(m_NPPFuncs));
            m_pLoadedPlugin->nRefCount++;
            return S_OK;
        }
    }

    // Plugin library is being loaded for the first time so initialise it
    // and store an entry in the loaded plugins array.

    HINSTANCE hInstance = LoadLibrary(szPluginPath);
    if (!hInstance)
    {
        return E_FAIL;
    }

    m_pLoadedPlugin = new LoadedPluginInfo;
    if (!m_pLoadedPlugin)
    {
        ATLASSERT(m_pLoadedPlugin);
        return E_OUTOFMEMORY;
    }

    // Get the plugin function entry points
    NP_GETENTRYPOINTS pfnGetEntryPoints =
        (NP_GETENTRYPOINTS) GetProcAddress(hInstance, "NP_GetEntryPoints");
    if (pfnGetEntryPoints)
    {
        pfnGetEntryPoints(&m_NPPFuncs);
    }

    // Tell the plugin to initialize itself
    NP_PLUGININIT pfnInitialize = (NP_PLUGININIT)
        GetProcAddress(hInstance, "NP_Initialize");
    if (!pfnInitialize)
    {
        pfnInitialize = (NP_PLUGININIT)
            GetProcAddress(hInstance, "NP_PluginInit");
    }
    if (pfnInitialize)
    {
        pfnInitialize(&g_NPNFuncs);
    }

    // Create a new entry for the plugin
    m_pLoadedPlugin->szFullPath = _tcsdup(szPluginPath);
    m_pLoadedPlugin->nRefCount = 1;
    m_pLoadedPlugin->hInstance = hInstance;
    memcpy(&m_pLoadedPlugin->NPPFuncs, &m_NPPFuncs, sizeof(m_NPPFuncs));

    // Add it to the array
    m_LoadedPlugins.AppendElement(m_pLoadedPlugin);

    return S_OK;
}

HRESULT nsPluginHostWnd::UnloadPlugin()
{
    if (!m_pLoadedPlugin)
    {
        return E_FAIL;
    }

    // TODO critical section

    ATLASSERT(m_pLoadedPlugin->nRefCount > 0);
    if (m_pLoadedPlugin->nRefCount == 1)
    {
        NP_PLUGINSHUTDOWN pfnShutdown = (NP_PLUGINSHUTDOWN)
            GetProcAddress(
                m_pLoadedPlugin->hInstance,
                "NP_Shutdown");
        if (pfnShutdown)
        {
            pfnShutdown();
        }
        FreeLibrary(m_pLoadedPlugin->hInstance);

        // Delete the entry from the array
        m_LoadedPlugins.RemoveElement(m_pLoadedPlugin);
        free(m_pLoadedPlugin->szFullPath);
        delete m_pLoadedPlugin;
    }
    else
    {
        m_pLoadedPlugin->nRefCount--;
    }

    m_pLoadedPlugin = NULL;

    return S_OK;
}


HRESULT nsPluginHostWnd::AddPluginParam(const char *szName, const char *szValue)
{
    ATLASSERT(szName);
    ATLASSERT(szValue);
    if (!szName || !szValue)
    {
        return E_INVALIDARG;
    }

    // Skip params that already there
    for (unsigned long i = 0; i < m_nArgs; i++)
    {
        if (stricmp(szName, m_pszArgNames[i]) == 0)
        {
            return S_OK;
        }
    }

    // Add the value
    if (!m_pszArgNames)
    {
        ATLASSERT(!m_pszArgValues);
        m_nArgsMax = kArraySizeIncrement;
        m_pszArgNames = (char **) malloc(sizeof(char *) * m_nArgsMax);
        m_pszArgValues = (char **) malloc(sizeof(char *) * m_nArgsMax);
    }
    else if (m_nArgs == m_nArgsMax)
    {
        m_nArgsMax += kArraySizeIncrement;
        m_pszArgNames = (char **) realloc(m_pszArgNames, sizeof(char *) * m_nArgsMax);
        m_pszArgValues = (char **) realloc(m_pszArgValues, sizeof(char *) * m_nArgsMax);
    }
    if (!m_pszArgNames || !m_pszArgValues)
    {
        return E_OUTOFMEMORY;
    }

    m_pszArgNames[m_nArgs] = strdup(szName);
    m_pszArgValues[m_nArgs] = strdup(szValue);

    m_nArgs++;
    
    return S_OK;
}


HRESULT nsPluginHostWnd::CreatePluginInstance()
{
    m_NPP.pdata = NULL;
    m_NPP.ndata = this;

    USES_CONVERSION;
    char *szContentType = strdup(OLE2A(m_bstrContentType.m_str));

    // Create a child window to house the plugin
    RECT rc;
    GetClientRect(&rc);
//    m_wndPlugin.Create(m_hWnd, rc, NULL, WS_CHILD | WS_VISIBLE);

//    m_NPWindow.window = (void *) m_wndPlugin.m_hWnd;
    m_NPWindow.window = (void *) m_hWnd;
    m_NPWindow.type = NPWindowTypeWindow;

    if (m_NPPFuncs.newp)
    {
        // Create the arguments to be fed into the plugin
        if (m_bstrSource.m_str)
        {
            AddPluginParam("SRC", OLE2A(m_bstrSource.m_str));
        }
        if (m_bstrContentType.m_str)
        {
            AddPluginParam("TYPE", OLE2A(m_bstrContentType.m_str));
        }
        if (m_bstrPluginsPage.m_str)
        {
            AddPluginParam("PLUGINSPAGE", OLE2A(m_bstrPluginsPage.m_str));
        }
        char szTmp[50];
        sprintf(szTmp, "%d", (int) (rc.right - rc.left));
        AddPluginParam("WIDTH", szTmp);
        sprintf(szTmp, "%d", (int) (rc.bottom - rc.top));
        AddPluginParam("HEIGHT", szTmp);

        NPSavedData *pSaved = NULL;

        // Create the plugin instance
        NPError npres = m_NPPFuncs.newp(szContentType, &m_NPP, NP_EMBED,
            (short) m_nArgs, m_pszArgNames, m_pszArgValues, pSaved);

        if (npres != NPERR_NO_ERROR)
        {
            return E_FAIL;
        }
    }

    m_bPluginIsAlive = true;

    SizeToFitPluginInstance();

    return S_OK;
}

HRESULT nsPluginHostWnd::DestroyPluginInstance()
{
    if (!m_bPluginIsAlive)
    {
        return S_OK;
    }

    // Destroy the plugin
    if (m_NPPFuncs.destroy)
    {
        NPSavedData *pSavedData = NULL;
        NPError npres = m_NPPFuncs.destroy(&m_NPP, &pSavedData);

        // TODO could store saved data instead of just deleting it.
        if (pSavedData && pSavedData->buf)
        {
            NPN_MemFree(pSavedData->buf);
        }
    }

    // Destroy the arguments
    if (m_pszArgNames)
    {
        for (unsigned long i = 0; i < m_nArgs; i++)
        {
            free(m_pszArgNames[i]);
        }
        free(m_pszArgNames);
        m_pszArgNames = NULL;
    }
    if (m_pszArgValues)
    {
        for (unsigned long i = 0; i < m_nArgs; i++)
        {
            free(m_pszArgValues[i]);
        }
        free(m_pszArgValues);
        m_pszArgValues = NULL;
    }

    //m_wndPlugin.DestroyWindow();

    m_bPluginIsAlive = false;

    return S_OK;
}

HRESULT nsPluginHostWnd::SizeToFitPluginInstance()
{
    if (!m_bPluginIsAlive)
    {
        return S_OK;
    }

    // Resize the plugin to fit the window

    RECT rc;
    GetClientRect(&rc);

    //m_wndPlugin.SetWindowPos(HWND_TOP,
    //    rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
    //    SWP_NOZORDER);

    m_NPWindow.x = 0;
    m_NPWindow.y = 0;
    m_NPWindow.width = rc.right - rc.left;
    m_NPWindow.height = rc.bottom - rc.top;
    m_NPWindow.clipRect.left = 0;
    m_NPWindow.clipRect.top = 0;
    m_NPWindow.clipRect.right = m_NPWindow.width;
    m_NPWindow.clipRect.bottom = m_NPWindow.height;

    if (m_NPPFuncs.setwindow)
    {
       NPError npres = m_NPPFuncs.setwindow(&m_NPP, &m_NPWindow);
    }

    return S_OK;
}

HRESULT nsPluginHostWnd::OpenURLStream(const TCHAR *szURL, void *pNotifyData, const void *pPostData, unsigned long nPostDataLength)
{
    nsURLDataCallback::OpenURL(this, szURL, pNotifyData, pPostData, nPostDataLength);
    return S_OK;
}
