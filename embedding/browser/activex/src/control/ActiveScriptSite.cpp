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
#include <sys/types.h>
#include <sys/stat.h>
#include "ActiveScriptSite.h"


CActiveScriptSite::CActiveScriptSite()
{
    m_ssScriptState = SCRIPTSTATE_UNINITIALIZED;
}


CActiveScriptSite::~CActiveScriptSite()
{
    Detach();
}


HRESULT CActiveScriptSite::Attach(CLSID clsidScriptEngine)
{
    // Detach to anything already attached to
    Detach();

    // Create the new script engine
    HRESULT hr = m_spIActiveScript.CoCreateInstance(clsidScriptEngine);
    if (FAILED(hr))
    {
        return hr;
    }

    // Attach the script engine to this site
    m_spIActiveScript->SetScriptSite(this);

    // Initialise the script engine
    CIPtr(IActiveScriptParse) spActiveScriptParse = m_spIActiveScript;
    if (spActiveScriptParse)
    {
        spActiveScriptParse->InitNew();
    }
    else
    {
    }

    return S_OK;
}


HRESULT CActiveScriptSite::Detach()
{
    if (m_spIActiveScript)
    {
        StopScript();
        m_spIActiveScript->Close();
        m_spIActiveScript.Release();
    }

    return S_OK;
}


HRESULT CActiveScriptSite::AttachVBScript()
{
    static const CLSID CLSID_VBScript =
    { 0xB54F3741, 0x5B07, 0x11CF, { 0xA4, 0xB0, 0x00, 0xAA, 0x00, 0x4A, 0x55, 0xE8} };
    
    return Attach(CLSID_VBScript);
}


HRESULT CActiveScriptSite::AttachJScript()
{
    static const CLSID CLSID_JScript =
    { 0xF414C260, 0x6AC0, 0x11CF, { 0xB6, 0xD1, 0x00, 0xAA, 0x00, 0xBB, 0xBB, 0x58} };

    return Attach(CLSID_JScript);
}


HRESULT CActiveScriptSite::AddNamedObject(const TCHAR *szName, IUnknown *pObject, BOOL bGlobalMembers)
{
    if (m_spIActiveScript == NULL)
    {
        return E_UNEXPECTED;
    }

    if (pObject == NULL || szName == NULL)
    {
        return E_INVALIDARG;
    }

    // Check for objects of the same name already
    CNamedObjectList::iterator i = m_cObjectList.find(szName);
    if (i != m_cObjectList.end())
    {
        return E_FAIL;
    }

    // Add object to the list
    m_cObjectList.insert(CNamedObjectList::value_type(szName, pObject));

    // Tell the script engine about the object
    HRESULT hr;
    USES_CONVERSION;
    DWORD dwFlags = SCRIPTITEM_ISSOURCE | SCRIPTITEM_ISVISIBLE;
    if (bGlobalMembers)
    {
        dwFlags |= SCRIPTITEM_GLOBALMEMBERS;
    }

    hr = m_spIActiveScript->AddNamedItem(T2OLE(szName), dwFlags);

    if (FAILED(hr))
    {
        m_cObjectList.erase(szName);
        return hr;
    }

    return S_OK;
}


HRESULT CActiveScriptSite::ParseScriptFile(const TCHAR *szFile)
{
    USES_CONVERSION;
    const char *pszFileName = T2CA(szFile);
    
    // Stat the file and get its length;
    struct _stat cStat;
    _stat(pszFileName, &cStat);

    // Allocate a buffer
    size_t nBufSize = cStat.st_size + 1;
    char *pBuffer = (char *) malloc(nBufSize);
    if (pBuffer == NULL)
    {
        return E_OUTOFMEMORY;
    }
    memset(pBuffer, 0, nBufSize);

    // Read the script into the buffer and parse it
    HRESULT hr = E_FAIL;
    FILE *f = fopen(pszFileName, "rb");
    if (f)
    {
        fread(pBuffer, 1, nBufSize - 1, f);
        hr = ParseScriptText(A2T(pBuffer));
        fclose(f);
    }

    free(pBuffer);

    return hr;
}


HRESULT CActiveScriptSite::ParseScriptText(const TCHAR *szScript)
{
    if (m_spIActiveScript == NULL)
    {
        return E_UNEXPECTED;
    }

    CIPtr(IActiveScriptParse) spIActiveScriptParse = m_spIActiveScript;
    if (spIActiveScriptParse)
    {
        USES_CONVERSION;

        CComVariant vResult;
        DWORD dwCookie = 0; // TODO
        DWORD dwFlags = 0;
        EXCEPINFO cExcepInfo;
        HRESULT hr;

        hr = spIActiveScriptParse->ParseScriptText(
                    T2OLE(szScript),
                    NULL, NULL, NULL, dwCookie, 0, dwFlags,
                    &vResult, &cExcepInfo);

        if (FAILED(hr))
        {
            return E_FAIL;
        }
    }
    else
    {
        CIPtr(IPersistStream) spPersistStream = m_spIActiveScript;
        CIPtr(IStream) spStream;

        // Load text into the stream IPersistStream
        if (spPersistStream &&
            SUCCEEDED(CreateStreamOnHGlobal(NULL, TRUE, &spStream)))
        {
            USES_CONVERSION;
            LARGE_INTEGER cPos = { 0, 0 };
            LPOLESTR szText = T2OLE(szScript);
            spStream->Write(szText, wcslen(szText) * sizeof(WCHAR), NULL);
            spStream->Seek(cPos, STREAM_SEEK_SET, NULL);
            spPersistStream->Load(spStream);
        }
        else
        {
            return E_UNEXPECTED;
        }
    }

    return S_OK;
}


HRESULT CActiveScriptSite::PlayScript()
{
    if (m_spIActiveScript == NULL)
    {
        return E_UNEXPECTED;
    }

    m_spIActiveScript->SetScriptState(SCRIPTSTATE_CONNECTED);

    return S_OK;
}


HRESULT CActiveScriptSite::StopScript()
{
    if (m_spIActiveScript == NULL)
    {
        return E_UNEXPECTED;
    }

    m_spIActiveScript->SetScriptState(SCRIPTSTATE_DISCONNECTED);

    return S_OK;
}


///////////////////////////////////////////////////////////////////////////////
// IActiveScriptSite implementation

HRESULT STDMETHODCALLTYPE CActiveScriptSite::GetLCID(/* [out] */ LCID __RPC_FAR *plcid)
{
    // Use the system defined locale
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CActiveScriptSite::GetItemInfo(/* [in] */ LPCOLESTR pstrName, /* [in] */ DWORD dwReturnMask, /* [out] */ IUnknown __RPC_FAR *__RPC_FAR *ppiunkItem, /* [out] */ ITypeInfo __RPC_FAR *__RPC_FAR *ppti)
{
    if (pstrName == NULL)
    {
        return E_INVALIDARG;
    }
    
    if (ppiunkItem)
    {
        *ppiunkItem = NULL;
    }

    if (ppti)
    {
        *ppti = NULL;
    }

    USES_CONVERSION;

    // Find object in list
    CIUnkPtr spUnkObject;
    CNamedObjectList::iterator i = m_cObjectList.find(OLE2T(pstrName));
    if (i != m_cObjectList.end())
    {
        spUnkObject = (*i).second;
    }

    // Fill in the output values
    if (spUnkObject == NULL) 
    {
        return TYPE_E_ELEMENTNOTFOUND;
    }
    if (dwReturnMask & SCRIPTINFO_IUNKNOWN) 
    {
        spUnkObject->QueryInterface(IID_IUnknown, (void **) ppiunkItem);
    }
    if (dwReturnMask & SCRIPTINFO_ITYPEINFO) 
    {
        // Return the typeinfo in ptti
        CIPtr(IDispatch) spIDispatch = spUnkObject;
        if (spIDispatch)
        {
            HRESULT hr;
            hr = spIDispatch->GetTypeInfo(0, GetSystemDefaultLCID(), ppti);
            if (FAILED(hr))
            {
                *ppti = NULL;
            }
        }
    }

    return S_OK;
}


HRESULT STDMETHODCALLTYPE CActiveScriptSite::GetDocVersionString(/* [out] */ BSTR __RPC_FAR *pbstrVersion)
{
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE CActiveScriptSite::OnScriptTerminate(/* [in] */ const VARIANT __RPC_FAR *pvarResult, /* [in] */ const EXCEPINFO __RPC_FAR *pexcepinfo)
{
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CActiveScriptSite::OnStateChange(/* [in] */ SCRIPTSTATE ssScriptState)
{
    m_ssScriptState = ssScriptState;
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CActiveScriptSite::OnScriptError(/* [in] */ IActiveScriptError __RPC_FAR *pscripterror)
{
    BSTR bstrSourceLineText = NULL;
    DWORD dwSourceContext = 0;
    ULONG pulLineNumber = 0;
    LONG  ichCharPosition = 0;
    EXCEPINFO cExcepInfo;

    memset(&cExcepInfo, 0, sizeof(cExcepInfo));

    // Get error information
    pscripterror->GetSourcePosition(&dwSourceContext, &pulLineNumber, &ichCharPosition);
    pscripterror->GetSourceLineText(&bstrSourceLineText);
    pscripterror->GetExceptionInfo(&cExcepInfo);

    tstring szDescription(_T("(No description)"));
    if (cExcepInfo.bstrDescription)
    {
        // Dump info
        USES_CONVERSION;
        szDescription = OLE2T(cExcepInfo.bstrDescription);
    }

    ATLTRACE(_T("Script Error: %s, code=0x%08x\n"), szDescription.c_str(), cExcepInfo.scode);

    SysFreeString(bstrSourceLineText);

    return S_OK;
}


HRESULT STDMETHODCALLTYPE CActiveScriptSite::OnEnterScript(void)
{
    return S_OK;
}


HRESULT STDMETHODCALLTYPE CActiveScriptSite::OnLeaveScript(void)
{
    return S_OK;
}


