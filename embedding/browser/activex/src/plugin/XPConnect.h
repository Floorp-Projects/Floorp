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
 *
 *   Adam Lock <adamlock@netscape.com>
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
#ifndef XPCONNECT_H
#define XPCONNECT_H

#include <servprov.h>

#ifdef XPC_IDISPATCH_SUPPORT
#include "nsIDispatchSupport.h"
#include "nsIActiveXSecurityPolicy.h"
#endif

#include "nsID.h"
#include "nsCOMPtr.h"
#include "nsIClassInfo.h"
#include "nsIMozAxPlugin.h"
#include "nsIServiceManagerUtils.h"

#include "ControlEventSink.h"

struct PluginInstanceData;

template <class T> class nsIClassInfoImpl : public nsIClassInfo
{
    NS_IMETHODIMP GetFlags(PRUint32 *aFlags)
    {
        *aFlags = nsIClassInfo::PLUGIN_OBJECT | nsIClassInfo::DOM_OBJECT;
        return NS_OK;
    }
    NS_IMETHODIMP GetImplementationLanguage(PRUint32 *aImplementationLanguage)
    {
        *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
        return NS_OK;
    }
    // The rest of the methods can safely return error codes...
    NS_IMETHODIMP GetInterfaces(PRUint32 *count, nsIID * **array)
    { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHODIMP GetHelperForLanguage(PRUint32 language, nsISupports **_retval)
    { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHODIMP GetContractID(char * *aContractID)
    { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHODIMP GetClassDescription(char * *aClassDescription)
    { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHODIMP GetClassID(nsCID * *aClassID)
    { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHODIMP GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
    { return NS_ERROR_NOT_IMPLEMENTED; }
};

class nsScriptablePeerTearOff;

class nsScriptablePeer :
    public nsIClassInfoImpl<nsScriptablePeer>,
    public nsIMozAxPlugin
{
    friend nsScriptablePeerTearOff;
protected:
    virtual ~nsScriptablePeer();

public:
    nsScriptablePeer();

    nsScriptablePeerTearOff *mTearOff;
    PluginInstanceData* mPlugin;

    NS_DECL_ISUPPORTS
    NS_DECL_NSIMOZAXPLUGIN

protected:
    HRESULT GetIDispatch(IDispatch **pdisp);
    HRESULT ConvertVariants(nsIVariant *aIn, VARIANT *aOut);
    HRESULT ConvertVariants(VARIANT *aIn, nsIVariant **aOut);
    nsresult HR2NS(HRESULT hr) const;
    NS_IMETHOD InternalInvoke(const char *aMethod, unsigned int aNumArgs, nsIVariant *aArgs[]);
};

class nsScriptablePeerTearOff :
    public IDispatch
{
public:
    nsScriptablePeerTearOff(nsScriptablePeer *pOwner);
    nsScriptablePeer *mOwner;

// IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE Release( void);

// IDispatch
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT __RPC_FAR *pctinfo);
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo __RPC_FAR *__RPC_FAR *ppTInfo);
    virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID riid, LPOLESTR __RPC_FAR *rgszNames, UINT cNames, LCID lcid, DISPID __RPC_FAR *rgDispId);
    virtual HRESULT STDMETHODCALLTYPE Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS __RPC_FAR *pDispParams, VARIANT __RPC_FAR *pVarResult, EXCEPINFO __RPC_FAR *pExcepInfo, UINT __RPC_FAR *puArgErr);
};

#ifdef XPC_IDISPATCH_SUPPORT
class nsEventSink : public CControlEventSink
{
public:
    PluginInstanceData* mPlugin;

    virtual HRESULT InternalInvoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr);
};

typedef CComObject<nsEventSink> nsEventSinkInstance;
#endif

namespace MozAxPlugin {
    extern void AddRef();
    extern void Release();
    extern CLSID GetCLSIDForType(const char *mimeType);
    extern NPError GetValue(NPP instance, NPPVariable variable, void *value);
    extern nsScriptablePeer *GetPeerForCLSID(const CLSID &clsid);
    extern void GetIIDForCLSID(const CLSID &clsid, nsIID &iid);
    extern HRESULT GetServiceProvider(PluginInstanceData *pData, IServiceProvider **pSP);
#ifdef XPC_IDISPATCH_SUPPORT
    extern PRUint32 PrefGetHostingFlags();
    extern void ReleasePrefObserver();
#endif
}

#endif