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
    m_hPostData(NULL)
{
    memset(&m_NPStream, 0, sizeof(m_NPStream));
    m_NPStream.ndata = this;
}

nsURLDataCallback::~nsURLDataCallback()
{
    if (m_hPostData)
        GlobalFree(m_hPostData);
    if (m_szURL)
        free(m_szURL);
    if (m_szContentType)
        free(m_szContentType);
}

void nsURLDataCallback::SetPostData(const void *pData, unsigned long nSize)
{
    // Copy the post data into an HGLOBAL so it can be given to the 
    // bind object during the call to GetBindInfo
    if (m_hPostData)
    {
        GlobalFree(m_hPostData);
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

///////////////////////////////////////////////////////////////////////////////
// IBindStatusCallback implementation

HRESULT STDMETHODCALLTYPE nsURLDataCallback::OnStartBinding( 
    /* [in] */ DWORD dwReserved,
    /* [in] */ IBinding __RPC_FAR *pib)
{
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
            if (m_szURL)
                free(m_szURL);
            m_szURL = strdup(W2A(szStatusText));
        }
        break;

    case BINDSTATUS_MIMETYPEAVAILABLE:
        {
            USES_CONVERSION;
            if (m_szContentType)
                free(m_szContentType);
            m_szContentType = strdup(W2A(szStatusText));
        }
        break;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE nsURLDataCallback::OnStopBinding( 
    /* [in] */ HRESULT hresult,
    /* [unique][in] */ LPCWSTR szError)
{
    if (m_pOwner && m_pOwner->m_bPluginIsAlive)
    {
        // TODO check for aborted ops and send NPRES_USER_BREAK
        NPReason reason = SUCCEEDED(hresult) ? NPRES_DONE : NPRES_NETWORK_ERR;

        // Notify the plugin that the stream has been closed
        if (m_pOwner->m_NPPFuncs.destroystream)
        {
            m_pOwner->m_NPPFuncs.destroystream(
                &m_pOwner->m_NPP,
                &m_NPStream,
                reason);
        }

        if (m_pNotifyData && m_pOwner->m_NPPFuncs.urlnotify)
        {
            // Notify the plugin that the url has loaded
            m_pOwner->m_NPPFuncs.urlnotify(
                &m_pOwner->m_NPP,
                m_szURL,
                reason,
                m_pNotifyData);
        }
    }

    m_cpBinding.Release();

    return S_OK;
}

/* [local] */ HRESULT STDMETHODCALLTYPE nsURLDataCallback::GetBindInfo( 
    /* [out] */ DWORD __RPC_FAR *grfBINDF,
    /* [unique][out][in] */ BINDINFO __RPC_FAR *pbindinfo)
{
    *grfBINDF = BINDF_ASYNCHRONOUS;
    if (m_hPostData)
    {
        pbindinfo->dwBindVerb = BINDVERB_POST;
        pbindinfo->stgmedData.tymed = TYMED_HGLOBAL;
        pbindinfo->stgmedData.hGlobal = m_hPostData;
    }
    return S_OK ;
}

/* [local] */ HRESULT STDMETHODCALLTYPE nsURLDataCallback::OnDataAvailable( 
    /* [in] */ DWORD grfBSCF,
    /* [in] */ DWORD dwSize,
    /* [in] */ FORMATETC __RPC_FAR *pformatetc,
    /* [in] */ STGMEDIUM __RPC_FAR *pstgmed)
{
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
        if (!m_pOwner->m_bPluginIsAlive &&
            m_pOwner->m_bCreatePluginFromStreamData)
        {
            if (FAILED(m_pOwner->LoadPluginByContentType(A2CT(m_szContentType))) ||
                FAILED(m_pOwner->CreatePluginInstance()))
            {
                m_cpBinding->Abort();
                return S_OK;
            }
        }

        // Tell the plugin that there is a new stream of data
        m_NPStream.url = m_szURL;
        m_NPStream.end = 0;
        m_NPStream.lastmodified = 0;
        m_NPStream.notifyData = m_pNotifyData;

        if (m_pOwner->m_NPPFuncs.newstream)
        {
            uint16 stype = NP_NORMAL;
            NPError npres = m_pOwner->m_NPPFuncs.newstream(
                &m_pOwner->m_NPP,
                m_szContentType,
                &m_NPStream,
                FALSE,
                &stype);
        }
    }
    if (!m_pOwner->m_bPluginIsAlive)
    {
        return S_OK;
    }

    ATLTRACE(_T("Data for stream %s (%d bytes)\n"), m_szURL, dwSize);

    // Feed the stream data into the plugin
    HRESULT hr;
    char bData[8192];
    while (m_nDataPos < dwSize)
    {
        ULONG nBytesToRead = dwSize - m_nDataPos;
        ULONG nBytesRead = 0;

        if (nBytesToRead > sizeof(bData))
        {
            nBytesToRead = sizeof(bData);
        }

        // How many bytes can the plugin cope with?
        if (m_pOwner->m_NPPFuncs.writeready)
        {
            int32 nPluginMaxBytes = m_pOwner->m_NPPFuncs.writeready(
                &m_pOwner->m_NPP,
                &m_NPStream);
            if (nBytesToRead > nPluginMaxBytes)
            {
                nBytesToRead = nPluginMaxBytes;
            }
        }

        // Read 'n' feed
        hr = pstgmed->pstm->Read(&bData, nBytesToRead, &nBytesRead);
        if (m_pOwner->m_NPPFuncs.write)
        {
            m_pOwner->m_NPPFuncs.write(
                &m_pOwner->m_NPP,
                &m_NPStream,
                m_nDataPos,
                nBytesRead,
                bData);
        }

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
