/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "stdafx.h"

#include <intshcut.h>
#include <shlobj.h>

#include "DropTarget.h"

#ifndef CFSTR_SHELLURL
#define CFSTR_SHELLURL _T("UniformResourceLocator")
#endif

#ifndef CFSTR_FILENAME
#define CFSTR_FILENAME _T("FileName")
#endif

#ifndef CFSTR_FILENAMEW
#define CFSTR_FILENAMEW _T("FileNameW")
#endif

static const UINT g_cfURL = RegisterClipboardFormat(CFSTR_SHELLURL);
static const UINT g_cfFileName = RegisterClipboardFormat(CFSTR_FILENAME);
static const UINT g_cfFileNameW = RegisterClipboardFormat(CFSTR_FILENAMEW);

CDropTarget::CDropTarget()
{
    m_pOwner = NULL;
}


CDropTarget::~CDropTarget()
{
}


void CDropTarget::SetOwner(CMozillaBrowser *pOwner)
{
    m_pOwner = pOwner;
}


HRESULT CDropTarget::GetURLFromFile(const TCHAR *pszFile, tstring &szURL)
{
    USES_CONVERSION;
    CIPtr(IUniformResourceLocator) spUrl;

    // Let's see if the file is an Internet Shortcut...
    HRESULT hr = CoCreateInstance (CLSID_InternetShortcut, NULL, CLSCTX_INPROC_SERVER, IID_IUniformResourceLocator, (void **) &spUrl);
    if (spUrl == NULL)
    {
        return E_FAIL;
    }

    // Get the IPersistFile interface
    CIPtr(IPersistFile) spFile = spUrl;
    if (spFile == NULL)
    {
        return E_FAIL;
    }

    // Initialise the URL object from the filename
    LPSTR lpTemp = NULL;
    if (FAILED(spFile->Load(T2OLE(pszFile), STGM_READ)) ||
        FAILED(spUrl->GetURL(&lpTemp)))
    {
        return E_FAIL;
    }

    // Free the memory
    CIPtr(IMalloc) spMalloc;
    if (FAILED(SHGetMalloc(&spMalloc)))
    {
        return E_FAIL;
    }
    
    // Copy the URL & cleanup
    szURL = A2T(lpTemp);
    spMalloc->Free(lpTemp);

    return S_OK;
}

///////////////////////////////////////////////////////////////////////////////
// IDropTarget implementation

HRESULT STDMETHODCALLTYPE CDropTarget::DragEnter(/* [unique][in] */ IDataObject __RPC_FAR *pDataObj, /* [in] */ DWORD grfKeyState, /* [in] */ POINTL pt, /* [out][in] */ DWORD __RPC_FAR *pdwEffect)
{
    if (pdwEffect == NULL || pDataObj == NULL)
    {
        NG_ASSERT(0);
        return E_INVALIDARG;
    }

    if (m_spDataObject != NULL)
    {
        NG_ASSERT(0);
        return E_UNEXPECTED;
    }

    // TODO process Internet Shortcuts (*.URL) files
    FORMATETC formatetc;
    memset(&formatetc, 0, sizeof(formatetc));
    formatetc.dwAspect = DVASPECT_CONTENT;
    formatetc.lindex = -1;
    formatetc.tymed = TYMED_HGLOBAL;

    // Test if the data object contains a text URL format
    formatetc.cfFormat = g_cfURL;
    if (pDataObj->QueryGetData(&formatetc) == S_OK)
    {
        m_spDataObject = pDataObj;
        *pdwEffect = DROPEFFECT_LINK;
        return S_OK;
    }

    // Test if the data object contains a file name
    formatetc.cfFormat = g_cfFileName;
    if (pDataObj->QueryGetData(&formatetc) == S_OK)
    {
        m_spDataObject = pDataObj;
        *pdwEffect = DROPEFFECT_LINK;
        return S_OK;
    }
    
    // Test if the data object contains a wide character file name
    formatetc.cfFormat = g_cfFileName;
    if (pDataObj->QueryGetData(&formatetc) == S_OK)
    {
        m_spDataObject = pDataObj;
        *pdwEffect = DROPEFFECT_LINK;
        return S_OK;
    }

    // If we got here, then the format is not supported
    *pdwEffect = DROPEFFECT_NONE;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CDropTarget::DragOver(/* [in] */ DWORD grfKeyState, /* [in] */ POINTL pt, /* [out][in] */ DWORD __RPC_FAR *pdwEffect)
{
    if (pdwEffect == NULL)
    {
        NG_ASSERT(0);
        return E_INVALIDARG;
    }
    *pdwEffect = m_spDataObject ? DROPEFFECT_LINK : DROPEFFECT_NONE;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CDropTarget::DragLeave(void)
{
    if (m_spDataObject)
    {
        m_spDataObject.Release();
    }
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CDropTarget::Drop(/* [unique][in] */ IDataObject __RPC_FAR *pDataObj, /* [in] */ DWORD grfKeyState, /* [in] */ POINTL pt, /* [out][in] */ DWORD __RPC_FAR *pdwEffect)
{
    if (pdwEffect == NULL)
    {
        NG_ASSERT(0);
        return E_INVALIDARG;
    }
    if (m_spDataObject == NULL)
    {
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    *pdwEffect = DROPEFFECT_LINK;

    // Get the URL from the data object
    BSTR bstrURL = NULL;
    FORMATETC formatetc;
    STGMEDIUM stgmedium;
    memset(&formatetc, 0, sizeof(formatetc));
    memset(&stgmedium, 0, sizeof(formatetc));

    formatetc.dwAspect = DVASPECT_CONTENT;
    formatetc.lindex = -1;
    formatetc.tymed = TYMED_HGLOBAL;

    // Does the data object contain a URL?
    formatetc.cfFormat = g_cfURL;
    if (m_spDataObject->GetData(&formatetc, &stgmedium) == S_OK)
    {
        NG_ASSERT(stgmedium.tymed == TYMED_HGLOBAL);
        NG_ASSERT(stgmedium.hGlobal);
        char *pszURL = (char *) GlobalLock(stgmedium.hGlobal);
        NG_TRACE("URL \"%s\" dragged over control\n", pszURL);
        // Browse to the URL
        if (m_pOwner)
        {
            USES_CONVERSION;
            bstrURL = SysAllocString(A2OLE(pszURL));
        }
        GlobalUnlock(stgmedium.hGlobal);
        goto finish;
    }

    // Does the data object point to a file?
    formatetc.cfFormat = g_cfFileName;
    if (m_spDataObject->GetData(&formatetc, &stgmedium) == S_OK)
    {
        USES_CONVERSION;
        NG_ASSERT(stgmedium.tymed == TYMED_HGLOBAL);
        NG_ASSERT(stgmedium.hGlobal);
        tstring szURL;
        char *pszURLFile = (char *) GlobalLock(stgmedium.hGlobal);
        NG_TRACE("File \"%s\" dragged over control\n", pszURLFile);
        if (SUCCEEDED(GetURLFromFile(A2T(pszURLFile), szURL)))
        {
            bstrURL = SysAllocString(T2OLE(szURL.c_str()));
        }
        GlobalUnlock(stgmedium.hGlobal);
        goto finish;
    }
    
    // Does the data object point to a wide character file?
    formatetc.cfFormat = g_cfFileNameW;
    if (m_spDataObject->GetData(&formatetc, &stgmedium) == S_OK)
    {
        USES_CONVERSION;
        NG_ASSERT(stgmedium.tymed == TYMED_HGLOBAL);
        NG_ASSERT(stgmedium.hGlobal);
        tstring szURL;
        WCHAR *pszURLFile = (WCHAR *) GlobalLock(stgmedium.hGlobal);
        NG_TRACE("File \"%s\" dragged over control\n", W2A(pszURLFile));
        if (SUCCEEDED(GetURLFromFile(W2T(pszURLFile), szURL)))
        {
            USES_CONVERSION;
            bstrURL = SysAllocString(T2OLE(szURL.c_str()));
        }
        GlobalUnlock(stgmedium.hGlobal);
        goto finish;
    }

finish:
    // If we got a URL, browse there!
    if (bstrURL)
    {
        m_pOwner->Navigate(bstrURL, NULL, NULL, NULL, NULL);
        SysFreeString(bstrURL);
    }

    ReleaseStgMedium(&stgmedium);
    m_spDataObject.Release();

    return S_OK;
}

