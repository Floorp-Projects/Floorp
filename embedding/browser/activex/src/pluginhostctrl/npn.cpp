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
 */
#include "stdafx.h"

#include "npn.h"
#include "Pluginhostctrl.h"

#include "nsPluginHostCtrl.h"

static NPError
_OpenURL(NPP npp, const char *szURL, const char *szTarget, void *pNotifyData, const char *pPostData, uint32 len, NPBool isFile)
{
    if (!npp)
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    nsPluginHostCtrl *pCtrl = (nsPluginHostCtrl *) npp->ndata;
    ATLASSERT(pCtrl);

    // Other window targets
    if (szTarget)
    {
        CComPtr<IWebBrowserApp> cpBrowser;
        pCtrl->GetWebBrowserApp(&cpBrowser);
        if (!cpBrowser)
        {
            return NPERR_GENERIC_ERROR;
        }

        CComBSTR url(szURL);
        CComVariant vFlags;
        CComVariant vTarget(szTarget);
        CComVariant vPostData;
        CComVariant vHeaders;

        cpBrowser->Navigate(url, &vFlags, &vTarget, &vPostData, &vHeaders);
        // TODO listen to navigation & send a URL notify to plugin when completed
        return NPERR_NO_ERROR;
    }

    void *pData = NULL;
    unsigned long nPostDataLen;
    
    // TODO handle file postdata
    if (pPostData && !isFile)
    {
        pPostData = pPostData;
        nPostDataLen = len;
    }

    USES_CONVERSION;
    HRESULT hr = pCtrl->OpenURLStream(A2CT(szURL), pNotifyData, pData, nPostDataLen);
    return SUCCEEDED(hr) ? NPERR_NO_ERROR : NPERR_GENERIC_ERROR;
}


NPError NP_EXPORT
NPN_GetURL(NPP npp, const char* relativeURL, const char* target)
{
    return _OpenURL(npp, relativeURL, target, NULL, NULL, 0, FALSE);
}

////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
NPN_GetURLNotify(NPP         npp, 
              const char* relativeURL, 
              const char* target, 
              void*       notifyData)
{
    return _OpenURL(npp, relativeURL, target, notifyData, NULL, 0, FALSE);
}


////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
NPN_PostURLNotify(NPP         npp,
               const char *relativeURL,
               const char *target,
               uint32     len,
               const char *buf,
               NPBool     file,
               void       *notifyData)
{
    return _OpenURL(npp, relativeURL, target, notifyData, buf, len, file);
}


////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
NPN_PostURL(NPP npp,
         const char *relativeURL,
         const char *target,
         uint32     len,
         const char *buf,
         NPBool     file)
{
    return _OpenURL(npp, relativeURL, target, NULL, buf, len, file);
}


////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
NPN_NewStream(NPP npp, NPMIMEType type, const char* window, NPStream* *result)
{
    if (!npp)
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    return NPERR_GENERIC_ERROR;
}


////////////////////////////////////////////////////////////////////////
int32 NP_EXPORT
NPN_Write(NPP npp, NPStream *pstream, int32 len, void *buffer)
{
    if (!npp)
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    return NPERR_GENERIC_ERROR;
}


////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
NPN_DestroyStream(NPP npp, NPStream *pstream, NPError reason)
{
    if (!npp)
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    return NPERR_GENERIC_ERROR;
}


////////////////////////////////////////////////////////////////////////
void NP_EXPORT
NPN_Status(NPP npp, const char *message)
{
    if (!npp)
    {
        return;
    }

    nsPluginHostCtrl *pCtrl = (nsPluginHostCtrl *) npp->ndata;
    ATLASSERT(pCtrl);

    // Update the status bar in the browser
    CComPtr<IWebBrowserApp> cpBrowser;
    pCtrl->GetWebBrowserApp(&cpBrowser);
    if (cpBrowser)
    {
        USES_CONVERSION;
        cpBrowser->put_StatusText(A2OLE(message));
    }
}


////////////////////////////////////////////////////////////////////////
void * NP_EXPORT
NPN_MemAlloc (uint32 size)
{
    return malloc(size);
}


////////////////////////////////////////////////////////////////////////
void NP_EXPORT
NPN_MemFree (void *ptr)
{
    if (ptr)
    {
        free(ptr);
    }
}


////////////////////////////////////////////////////////////////////////
uint32 NP_EXPORT
NPN_MemFlush(uint32 size)
{
    return 0;
}


////////////////////////////////////////////////////////////////////////
void NP_EXPORT
NPN_ReloadPlugins(NPBool reloadPages)
{
}


////////////////////////////////////////////////////////////////////////
void NP_EXPORT
NPN_InvalidateRect(NPP npp, NPRect *invalidRect)
{
    if (!npp)
    {
        return;
    }

    // TODO - windowless plugins
}


////////////////////////////////////////////////////////////////////////
void NP_EXPORT
NPN_InvalidateRegion(NPP npp, NPRegion invalidRegion)
{
    if (!npp)
    {
        return;
    }
    // TODO - windowless plugins
}


////////////////////////////////////////////////////////////////////////
void NP_EXPORT
NPN_ForceRedraw(NPP npp)
{
    if (!npp)
    {
        return;
    }
    // TODO - windowless plugins
}

////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
NPN_GetValue(NPP npp, NPNVariable variable, void *result)
{
    if (!npp)
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    if (!result)
    {
        return NPERR_INVALID_PARAM;
    }

    nsPluginHostCtrl *pCtrl = (nsPluginHostCtrl *) npp->ndata;
    ATLASSERT(pCtrl);

    CComPtr<IWebBrowserApp> cpBrowser;
    pCtrl->GetWebBrowserApp(&cpBrowser);

    // Test the variable
    if (variable == NPNVnetscapeWindow)
    {
        *((HWND *) result) = pCtrl->m_wndPlugin.m_hWnd;
    }
    else if (variable == NPNVjavascriptEnabledBool)
    {
        // TODO
        *((NPBool *) result) = TRUE;
    }
    else if (variable == NPNVasdEnabledBool) // Smart update
    {
        *((NPBool *) result) = FALSE;
    }
    else if (variable == NPNVisOfflineBool)
    {
        *((NPBool *) result) = FALSE;
        if (cpBrowser)
        {
            CComQIPtr<IWebBrowser2> cpBrowser2 = cpBrowser;
            if (cpBrowser2)
            {
                VARIANT_BOOL bOffline = VARIANT_FALSE;
                cpBrowser2->get_Offline(&bOffline);
                *((NPBool *) result) = (bOffline == VARIANT_TRUE) ? TRUE : FALSE;
            }
        }
    }
    else
    {
        return NPERR_GENERIC_ERROR;
    }

    return NPERR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
NPN_SetValue(NPP npp, NPPVariable variable, void *result)
{
    if (!npp)
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    // TODO windowless
    // NPPVpluginWindowBool
    // NPPVpluginTransparentBool

    return NPERR_GENERIC_ERROR;
}


////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
NPN_RequestRead(NPStream *pstream, NPByteRange *rangeList)
{
    if (!pstream || !rangeList || !pstream->ndata)
    {
        return NPERR_INVALID_PARAM;
    }

    return NPERR_GENERIC_ERROR;
}

////////////////////////////////////////////////////////////////////////
JRIEnv* NP_EXPORT
NPN_GetJavaEnv(void)
{
    return NULL;
}


////////////////////////////////////////////////////////////////////////
const char * NP_EXPORT
NPN_UserAgent(NPP npp)
{
    return NULL;
}


////////////////////////////////////////////////////////////////////////
java_lang_Class* NP_EXPORT
NPN_GetJavaClass(void* handle)
{
    return NULL;
}


////////////////////////////////////////////////////////////////////////
jref NP_EXPORT
NPN_GetJavaPeer(NPP npp)
{
    return NULL;
}


