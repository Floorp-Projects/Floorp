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
#include "stdafx.h"

#include <process.h>

#include "Pluginhostctrl.h"
#include "nsPluginHostCtrl.h"
#include "nsURLDataCallback.h"


/////////////////////////////////////////////////////////////////////////////
// nsURLDataCallback

nsURLDataCallback::nsURLDataCallback() :
    m_pOwner(NULL),
    m_pNotifyData(NULL),
    m_szContentType(NULL),
    m_szURL(NULL),
    m_nDataPos(0),
    m_nDataMax(0),
    m_hPostData(NULL),
    m_bSaveToTempFile(FALSE),
    m_bNotifyOnWrite(TRUE),
    m_szTempFileName(NULL),
    m_pTempFile(NULL)
{
    memset(&m_NPStream, 0, sizeof(m_NPStream));
    m_NPStream.ndata = this;
}

nsURLDataCallback::~nsURLDataCallback()
{
    SetPostData(NULL, 0);
    SetURL(NULL);
    SetContentType(NULL);
    if (m_pTempFile)
    {
        fclose(m_pTempFile);
        remove(m_szTempFileName);
    }
    if (m_szTempFileName)
    {
        free(m_szTempFileName);
    }
}

void nsURLDataCallback::SetPostData(const void *pData, unsigned long nSize)
{
    // Copy the post data into an HGLOBAL so it can be given to the 
    // bind object during the call to GetBindInfo
    if (m_hPostData)
    {
        GlobalFree(m_hPostData);
        m_hPostData = NULL;
    }
    if (pData)
    {
        m_hPostData = GlobalAlloc(GHND, nSize);
        if (m_hPostData)
        {
            void *pPostData = GlobalLock(m_hPostData);
            ATLASSERT(pPostData);
            memcpy(pPostData, pData, nSize);
            GlobalUnlock(m_hPostData);
        }
    }
}

HRESULT nsURLDataCallback::OpenURL(nsPluginHostCtrl *pOwner, const TCHAR *szURL, void *pNotifyData, const void *pPostData, unsigned long nPostDataSize)
{
    // Create the callback object
    CComObject<nsURLDataCallback> *pCallback = NULL;
    CComObject<nsURLDataCallback>::CreateInstance(&pCallback);
    if (!pCallback)
    {
        return E_OUTOFMEMORY;
    }
    pCallback->AddRef();

    // Initialise it
    pCallback->SetOwner(pOwner);
    pCallback->SetNotifyData(pNotifyData);
    if (pPostData && nPostDataSize > 0)
    {
        pCallback->SetPostData(pPostData, nPostDataSize);
    }

    USES_CONVERSION;
    pCallback->SetURL(T2CA(szURL));
    
    // Create an object window on this thread that will be sent messages when
    // something happens on the worker thread.
    RECT rcPos = {0, 0, 10, 10};
    pCallback->Create(HWND_DESKTOP, rcPos);    

    // Start the worker thread
    _beginthread(StreamThread, 0, pCallback);

    return S_OK;
}

void __cdecl nsURLDataCallback::StreamThread(void *pData)
{
    HRESULT hr = CoInitialize(NULL);
    ATLASSERT(SUCCEEDED(hr));

    CComObject<nsURLDataCallback> *pThis = (CComObject<nsURLDataCallback> *) pData;

    // Open the URL
    hr = URLOpenStream(NULL, pThis->m_szURL, 0, static_cast<IBindStatusCallback*>(pThis));
    ATLASSERT(SUCCEEDED(hr));

    // Pump messages until WM_QUIT arrives
    BOOL bQuit = FALSE;
    while (!bQuit)
    {
        MSG msg;
        if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
        {
            if (GetMessage(&msg, NULL, 0, 0))
            {
                DispatchMessage(&msg);
            }
            else
            {
                bQuit = TRUE;
            }
        }
    }

    CoUninitialize();
    _endthread();
}

///////////////////////////////////////////////////////////////////////////////
// Windows message handlers

LRESULT nsURLDataCallback::OnNPPNewStream(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    _NewStreamData *pNewStreamData = (_NewStreamData *) lParam;

    // Notify the plugin that a new stream has been created
    if (m_pOwner->m_NPPFuncs.newstream)
    {
        NPError npres = m_pOwner->m_NPPFuncs.newstream(
            pNewStreamData->npp,
            pNewStreamData->contenttype,
            pNewStreamData->stream,
            pNewStreamData->seekable,
            pNewStreamData->stype);

        // How does the plugin want its data?
        switch (*(pNewStreamData->stype))
        {
        case NP_NORMAL:
            m_bSaveToTempFile = FALSE;
            m_bNotifyOnWrite = TRUE;
            break;
        case NP_ASFILEONLY:
            m_bNotifyOnWrite = FALSE;
            m_bSaveToTempFile = TRUE;
            break;
        case NP_ASFILE:
            m_bNotifyOnWrite = TRUE;
            m_bSaveToTempFile = TRUE;
            break;
        case NP_SEEK:
            // TODO!!!
            ATLASSERT(0);
            break;
        }
    }
    return 0;
}

LRESULT nsURLDataCallback::OnNPPDestroyStream(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    _DestroyStreamData *pDestroyStreamData = (_DestroyStreamData *) lParam;

    // Tell the plugin the name of the temporary file containing the data
    if (m_bSaveToTempFile)
    {
        // Close the file
        if (m_pTempFile)
        {
            fclose(m_pTempFile);
        }

        // Determine whether the plugin should be told the name of the temp
        // file depending on whether it completed properly or not.
        char *szTempFileName = NULL;
        if (pDestroyStreamData->reason == NPRES_DONE &&
            m_pTempFile)
        {
            szTempFileName = m_szTempFileName;
        }
        
        // Notify the plugin
        if (m_pOwner->m_NPPFuncs.asfile)
        {
            m_pOwner->m_NPPFuncs.asfile(
                pDestroyStreamData->npp,
                pDestroyStreamData->stream,
                szTempFileName);
        }

        // Remove the file if it wasn't passed into the plugin
        if (!szTempFileName ||
            !m_pOwner->m_NPPFuncs.asfile)
        {
            remove(szTempFileName);
        }

        // Cleanup strings & pointers
        if (m_szTempFileName)
        {
            free(m_szTempFileName);
            m_szTempFileName = NULL;
        }
        m_pTempFile = NULL;
    }

    // Notify the plugin that the stream has been closed
    if (m_pOwner->m_NPPFuncs.destroystream)
    {
        m_pOwner->m_NPPFuncs.destroystream(
            pDestroyStreamData->npp,
            pDestroyStreamData->stream,
            pDestroyStreamData->reason);
    }

    return 0;
}

LRESULT nsURLDataCallback::OnNPPURLNotify(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    _UrlNotifyData *pUrlNotifyData = (_UrlNotifyData *) lParam;

    // Notify the plugin that the url has loaded
    if (m_pNotifyData && m_pOwner->m_NPPFuncs.urlnotify)
    {
        m_pOwner->m_NPPFuncs.urlnotify(
            pUrlNotifyData->npp,
            pUrlNotifyData->url,
            pUrlNotifyData->reason,
            pUrlNotifyData->notifydata);
    }
    return 0;
}

LRESULT nsURLDataCallback::OnNPPWriteReady(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    _WriteReadyData *pWriteReadyData = (_WriteReadyData *) lParam;
    if (m_pOwner->m_NPPFuncs.writeready &&
        m_bNotifyOnWrite)
    {
        pWriteReadyData->result = m_pOwner->m_NPPFuncs.writeready(
            pWriteReadyData->npp,
            pWriteReadyData->stream);
    }

    return 0;
}

LRESULT nsURLDataCallback::OnNPPWrite(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    _WriteData *pWriteData = (_WriteData *) lParam;

#ifdef DUMP_STREAM_DATA_AS_HEX
    // Dumps out the data to the display so you can see it's correct as its
    // fed into the plugin.
    const int kLineBreakAfter = 16;
    const int kSpaceBreakAfter = 8;
    ATLTRACE(_T("nsURLDataCallback::OnNPPWrite()\n"));
    for (int i = 0; i < pWriteData->len; i++)
    {
        TCHAR szLine[100];
        TCHAR szTmp[20];
        if (i % kLineBreakAfter == 0)
        {
            _stprintf(szLine, _T("%04x  "), i);
        }
        unsigned char b = ((unsigned char *) pWriteData->buffer)[i];
        _stprintf(szTmp, _T("%02X "), (unsigned int) b);
        _tcscat(szLine, szTmp);
        
        if (i == pWriteData->len - 1 ||
            i % kLineBreakAfter == kLineBreakAfter - 1)
        {
            _tcscat(szLine, _T("\n"));
            ATLTRACE(szLine);
        }
        else if (i % kSpaceBreakAfter == kSpaceBreakAfter - 1)
        {
            _tcscat(szLine, _T(" "));
        }
    }
    ATLTRACE(_T("--\n"));
#endif

    if (m_bSaveToTempFile)
    {
        if (!m_szTempFileName)
        {
            m_szTempFileName = strdup(_tempnam(NULL, "moz"));
        }
        if (!m_pTempFile)
        {
            m_pTempFile = fopen(m_szTempFileName, "wb");
        }
        ATLASSERT(m_pTempFile);
        if (m_pTempFile)
        {
            fwrite(pWriteData->buffer, 1, pWriteData->len, m_pTempFile);
        }
    }

    if (m_pOwner->m_NPPFuncs.write &&
        m_bNotifyOnWrite)
    {
        int32 nConsumed = m_pOwner->m_NPPFuncs.write(
            pWriteData->npp,
            pWriteData->stream,
            pWriteData->offset,
            pWriteData->len,
            pWriteData->buffer);
        if (nConsumed < 0)
        {
            // TODO destroy the stream!
        }
        ATLASSERT(nConsumed == pWriteData->len);
    }
    return 0;
}

LRESULT nsURLDataCallback::OnClassCreatePluginInstance(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    // Test whether the plugin for this content type exists or not and if not,
    // create it right now.
    if (!m_pOwner->m_bPluginIsAlive &&
        m_pOwner->m_bCreatePluginFromStreamData)
    {
        if (FAILED(m_pOwner->LoadPluginByContentType(A2CT(m_szContentType))) ||
            FAILED(m_pOwner->CreatePluginInstance()))
        {
            return 1;
        }
    }
    return 0;
}

LRESULT nsURLDataCallback::OnClassCleanup(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    DestroyWindow();
    Release();
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
// IBindStatusCallback implementation

HRESULT STDMETHODCALLTYPE nsURLDataCallback::OnStartBinding( 
    /* [in] */ DWORD dwReserved,
    /* [in] */ IBinding __RPC_FAR *pib)
{
    ATLTRACE(_T("nsURLDataCallback::OnStartBinding()\n"));
    m_cpBinding = pib;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE nsURLDataCallback::GetPriority( 
    /* [out] */ LONG __RPC_FAR *pnPriority)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE nsURLDataCallback::OnLowResource( 
    /* [in] */ DWORD reserved)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE nsURLDataCallback::OnProgress( 
    /* [in] */ ULONG ulProgress,
    /* [in] */ ULONG ulProgressMax,
    /* [in] */ ULONG ulStatusCode,
    /* [in] */ LPCWSTR szStatusText)
{
    switch (ulStatusCode)
    {
    case BINDSTATUS_BEGINDOWNLOADDATA:
    case BINDSTATUS_REDIRECTING:
        {
            USES_CONVERSION;
            SetURL(W2A(szStatusText));
        }
        break;

    case BINDSTATUS_MIMETYPEAVAILABLE:
        {
            USES_CONVERSION;
            SetContentType(W2A(szStatusText));
        }
        break;
    }

    m_nDataMax = ulProgressMax;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE nsURLDataCallback::OnStopBinding( 
    /* [in] */ HRESULT hresult,
    /* [unique][in] */ LPCWSTR szError)
{
    ATLTRACE(_T("nsURLDataCallback::OnStopBinding()\n"));
    if (m_pOwner && m_pOwner->m_bPluginIsAlive)
    {
        // TODO check for aborted ops and send NPRES_USER_BREAK
        NPReason reason = SUCCEEDED(hresult) ? NPRES_DONE : NPRES_NETWORK_ERR;

        // Notify the plugin that the stream has been closed
        _DestroyStreamData destroyStreamData;
        destroyStreamData.npp = &m_pOwner->m_NPP;
        destroyStreamData.stream = &m_NPStream;
        destroyStreamData.reason = reason;
        SendMessage(WM_NPP_DESTROYSTREAM, 0, (LPARAM) &destroyStreamData);

        // Notify the plugin that the url has loaded
        _UrlNotifyData urlNotifyData;
        urlNotifyData.npp = &m_pOwner->m_NPP;
        urlNotifyData.url = m_szURL;
        urlNotifyData.reason = reason;
        urlNotifyData.notifydata = m_pNotifyData;
        SendMessage(WM_NPP_URLNOTIFY, 0, (LPARAM) &urlNotifyData);
    }

    m_cpBinding.Release();

    SendMessage(WM_CLASS_CLEANUP);
    PostQuitMessage(0);

    return S_OK;
}

/* [local] */ HRESULT STDMETHODCALLTYPE nsURLDataCallback::GetBindInfo( 
    /* [out] */ DWORD __RPC_FAR *grfBINDF,
    /* [unique][out][in] */ BINDINFO __RPC_FAR *pbindinfo)
{
    *grfBINDF = BINDF_ASYNCHRONOUS |  BINDF_ASYNCSTORAGE |
        BINDF_GETNEWESTVERSION;
    
    ULONG cbSize = pbindinfo->cbSize;
    memset(pbindinfo, 0, cbSize); // zero out structure
    pbindinfo->cbSize = cbSize;
    if (m_hPostData)
    {
        pbindinfo->dwBindVerb = BINDVERB_POST;
        pbindinfo->stgmedData.tymed = TYMED_HGLOBAL;
        pbindinfo->stgmedData.hGlobal = m_hPostData;
    }
    else
    {
		pbindinfo->dwBindVerb = BINDVERB_GET;
    }

    return S_OK ;
}

/* [local] */ HRESULT STDMETHODCALLTYPE nsURLDataCallback::OnDataAvailable( 
    /* [in] */ DWORD grfBSCF,
    /* [in] */ DWORD dwSize,
    /* [in] */ FORMATETC __RPC_FAR *pformatetc,
    /* [in] */ STGMEDIUM __RPC_FAR *pstgmed)
{
    ATLTRACE(_T("nsURLDataCallback::OnDataAvailable()\n"));
    if (pstgmed->tymed != TYMED_ISTREAM ||
        !pstgmed->pstm)
    {
        return S_OK;
    }
    if (!m_pOwner)
    {
        return S_OK;
    }

    // Notify the plugin that a stream has been opened
    if (grfBSCF & BSCF_FIRSTDATANOTIFICATION)
    {
        USES_CONVERSION;

        // Test if there is a plugin yet. If not try and create one for this
        // kind of content.
        if (SendMessage(WM_CLASS_CREATEPLUGININSTANCE))
        {
            m_cpBinding->Abort();
            return S_OK;
        }

        // Tell the plugin that there is a new stream of data
        m_NPStream.url = m_szURL;
        m_NPStream.end = 0;
        m_NPStream.lastmodified = 0;
        m_NPStream.notifyData = m_pNotifyData;

        uint16 stype = NP_NORMAL;
        _NewStreamData newStreamData;
        newStreamData.npp = &m_pOwner->m_NPP;
        newStreamData.contenttype = m_szContentType;
        newStreamData.stream = &m_NPStream;
        newStreamData.seekable = FALSE;
        newStreamData.stype = &stype;
        SendMessage(WM_NPP_NEWSTREAM, 0, (LPARAM) &newStreamData);
    }
    if (!m_pOwner->m_bPluginIsAlive)
    {
        return S_OK;
    }

    m_NPStream.end = m_nDataMax;

    ATLTRACE(_T("Data for stream %s (%d of %d bytes are available)\n"), m_szURL, dwSize, m_NPStream.end);

    // Feed the stream data into the plugin
    HRESULT hr;
    char bData[16384];
    while (m_nDataPos < dwSize)
    {
        ULONG nBytesToRead = dwSize - m_nDataPos;
        ULONG nBytesRead = 0;

        if (nBytesToRead > sizeof(bData))
        {
            nBytesToRead = sizeof(bData);
        }

        // How many bytes can the plugin cope with?
        _WriteReadyData writeReadyData;
        writeReadyData.npp = &m_pOwner->m_NPP;
        writeReadyData.stream = &m_NPStream;
        writeReadyData.result = nBytesToRead;
        SendMessage(WM_NPP_WRITEREADY, 0, (LPARAM) &writeReadyData);
        if (nBytesToRead > writeReadyData.result)
        {
            nBytesToRead = writeReadyData.result;
        }

        // Read 'n' feed
        ATLTRACE(_T("  Reading %d bytes\n"), (int) nBytesToRead);
        hr = pstgmed->pstm->Read(&bData, nBytesToRead, &nBytesRead);

        _WriteData writeData;
        writeData.npp = &m_pOwner->m_NPP;
        writeData.stream = &m_NPStream;
        writeData.offset = m_nDataPos;
        writeData.len = nBytesRead;
        writeData.buffer = bData;
        SendMessage(WM_NPP_WRITE, 0, (LPARAM) &writeData);

        m_nDataPos += nBytesRead;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE nsURLDataCallback::OnObjectAvailable( 
    /* [in] */ REFIID riid,
    /* [iid_is][in] */ IUnknown __RPC_FAR *punk)
{
    return S_OK;
}
