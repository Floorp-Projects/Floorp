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
 * Paul Oswald <paul.oswald@isinet.com>
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

#include "jni.h"
#include "npapi.h"
#include "nsISupports.h"

#ifdef MOZ_ACTIVEX_PLUGIN_LIVECONNECT
#define IMPLEMENT_MozAxPlugin
#include "_gen/MozAxPlugin.h"
//#include "_gen/netscape_plugin_Plugin.h"
#endif

#ifdef MOZ_ACTIVEX_PLUGIN_XPCONNECT
#include "nsMemory.h"
#endif /* MOZ_ACTIVEX_PLUGIN_XPCONNECT */


// Plugin types supported
enum PluginInstanceType
{
    itScript,
    itControl
};

// Data associated with a plugin instance
struct PluginInstanceData {
    PluginInstanceType nType;
    union
    {
        CActiveScriptSiteInstance *pScriptSite;
        CControlSiteInstance *pControlSite;
    };
#ifdef MOZ_ACTIVEX_PLUGIN_XPCONNECT
    nsISupports *pScriptingPeer;
#endif
};


// NPP_Initialize
//
//    Initialize the plugin library. Your DLL global initialization
//    should be here
//
NPError NPP_Initialize(void)
{
    NG_TRACE_METHOD(NPP_Initialize);
    _Module.Lock();
    return NPERR_NO_ERROR;
}


// NPP_Shutdown
//
//    shutdown the plugin library. Revert initializition
//
void NPP_Shutdown(void)
{
    NG_TRACE_METHOD(NPP_Shutdown);
#ifdef MOZ_ACTIVEX_PLUGIN_LIVECONNECT
    JRIEnv* env = NPN_GetJavaEnv();
	if (env) {
		// unuse_MozAxPlugin(env);
        unregister_MozAxPlugin(env);
	}
#endif

    _Module.Unlock();
}


// npp_GetJavaClass
//
//    Return the Java class representing this plugin
//
jref NPP_GetJavaClass(void)
{
    NG_TRACE_METHOD(NPP_GetJavaClass);
#ifdef MOZ_ACTIVEX_PLUGIN_LIVECONNECT
    JRIEnv* env = NPN_GetJavaEnv();
    if (env) {
	    return (jref) register_MozAxPlugin(env);
        // return (jref) use_MozAxPlugin(env);
    }
#endif
    return NULL;
}


#define MIME_OLEOBJECT1   "application/x-oleobject"
#define MIME_OLEOBJECT2   "application/oleobject"
#define MIME_ACTIVESCRIPT "text/x-activescript"


NPError NewScript(const char *pluginType,
                    PluginInstanceData *pData,
                    uint16 mode,
                    int16 argc,
                    char *argn[],
                    char *argv[])
{
    CActiveScriptSiteInstance *pScriptSite = NULL;
    CActiveScriptSiteInstance::CreateInstance(&pScriptSite);

    // TODO support ActiveScript
    MessageBox(NULL, _T("ActiveScript not supported yet!"), NULL, MB_OK);
    return NPERR_GENERIC_ERROR;
}

NPError NewControl(const char *pluginType,
                    PluginInstanceData *pData,
                    uint16 mode,
                    int16 argc,
                    char *argn[],
                    char *argv[])
{
    // Read the parameters
    CLSID clsid = CLSID_NULL;
    tstring szName;
    tstring szCodebase;
    PropertyList pl;

    for (int16 i = 0; i < argc; i++)
    {
        if (stricmp(argn[i], "CLSID") == 0 ||
            stricmp(argn[i], "CLASSID") == 0)
        {
            // Accept CLSIDs specified in various ways
            // e.g:
            //   "CLSID:C16DF970-D1BA-11d2-A252-000000000000"
            //   "C16DF970-D1BA-11d2-A252-000000000000"
            //   "{C16DF970-D1BA-11d2-A252-000000000000}"
            //
            // The first example is the proper way

            char szCLSID[256];
            if (strlen(argv[i]) < sizeof(szCLSID))
            {
                if (strnicmp(argv[i], "CLSID:", 6) == 0)
                {
                    sprintf(szCLSID, "{%s}", argv[i]+6);
                }
                else if(argv[i][0] != '{')
                {
                    sprintf(szCLSID, "{%s}", argv[i]);
                }
                else
                {
                    strncpy(szCLSID, argv[i], sizeof(szCLSID));
                }
                USES_CONVERSION;
                CLSIDFromString(A2OLE(szCLSID), &clsid);
            }
        }
        else if (stricmp(argn[i], "NAME") == 0)
        {
            USES_CONVERSION;
            szName = tstring(A2T(argv[i]));
        }
        else if (stricmp(argn[i], "CODEBASE") == 0)
        {
            szCodebase = tstring(A2T(argv[i]));
        }
        else 
        {
            USES_CONVERSION;

            std::wstring szName;

            if (strnicmp(argn[i], "PARAM_", 6) == 0)
            {
                szName = A2W(argn[i]+6);
            }
            else if (stricmp(argn[i], "PARAM") == 0)
            {
                // The next argn and argv values after this symbol
                // will be <param> tag names and values
                continue;
            }
            else
            {
                szName = A2W(argn[i]);
            }

            // Empty parameters are ignored
            if (szName.empty())
            {
                continue;
            }

            std::wstring szParam(A2W(argv[i]));

            // Check for existing params with the same name
            BOOL bFound = FALSE;
            for (PropertyList::const_iterator i = pl.begin(); i != pl.end(); i++)
            {
                if (wcscmp((BSTR) (*i).szName, szName.c_str()) == 0)
                {
                    bFound = TRUE;
                    break;
                }
            }
            // If the parameter already exists, don't add it to the
            // list again.
            if (bFound)
            {
                continue;
            }

            CComVariant vsValue(szParam.c_str());
            CComVariant vIValue; // Value converted to int
            CComVariant vRValue; // Value converted to real
            CComVariant vBValue; // Value converted to bool
            CComVariant &vValue = vsValue;

            // See if the variant can be converted to a number or boolean
            if (VariantChangeType(&vIValue, &vsValue, 0, VT_I4) == S_OK)
            {
                vValue = vIValue;
            }
            else if (VariantChangeType(&vRValue, &vsValue, 0, VT_R8) == S_OK)
            {
                vValue = vRValue;
            }
            else if (VariantChangeType(&vBValue, &vsValue, 0, VT_BOOL) == S_OK)
            {
                vValue = vBValue;
            }


            // Add named parameter to list
            Property p;
            p.szName = szName.c_str();
            p.vValue = vValue;
            pl.push_back(p);
        }
    }

    // Make sure we got a CLSID
    if (memcmp(&clsid, &CLSID_NULL, sizeof(CLSID)) == 0)
    {
        return NPERR_GENERIC_ERROR;
    }

    // Create the control site
    CControlSiteInstance *pSite = NULL;
    CControlSiteInstance::CreateInstance(&pSite);
    if (pSite == NULL)
    {
        return NPERR_GENERIC_ERROR;
    }
    pSite->AddRef();

    // TODO check the object is installed and at least as recent as
    //      that specified in szCodebase

    // Create the object
    if (FAILED(pSite->Create(clsid, pl, szName)))
    {
        USES_CONVERSION;
        LPOLESTR szClsid;
        StringFromCLSID(clsid, &szClsid);
        TCHAR szBuffer[256];
        _stprintf(szBuffer, _T("Could not create the control %s. Check that it has been installed on your computer and that this page correctly references it."), OLE2T(szClsid));
        MessageBox(NULL, szBuffer, _T("ActiveX Error"), MB_OK | MB_ICONWARNING);
        CoTaskMemFree(szClsid);

        pSite->Release();
        return NPERR_GENERIC_ERROR;
    }

    pData->nType = itControl;
    pData->pControlSite = pSite;

    return NPERR_NO_ERROR;
}


// NPP_New
//
//    create a new plugin instance 
//    handle any instance specific code initialization here
//
NPError NP_LOADDS NPP_New(NPMIMEType pluginType,
                NPP instance,
                uint16 mode,
                int16 argc,
                char* argn[],
                char* argv[],
                NPSavedData* saved)
{
    NG_TRACE_METHOD(NPP_New);

    // trap duff args
    if (instance == NULL)
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    PluginInstanceData *pData = new PluginInstanceData;
    if (pData == NULL)
    {
        return NPERR_GENERIC_ERROR;
    }
#ifdef MOZ_ACTIVEX_PLUGIN_XPCONNECT
    pData->pScriptingPeer = NULL;
#endif

    // Create a plugin according to the mime type

    NPError rv = NPERR_GENERIC_ERROR;
    if (strcmp(pluginType, MIME_ACTIVESCRIPT) == 0)
    {
        rv = NewScript(pluginType, pData, mode, argc, argn, argv);
    }
    else if (strcmp(pluginType, MIME_OLEOBJECT1) == 0 ||
             strcmp(pluginType, MIME_OLEOBJECT2) == 0)
    {
        rv = NewControl(pluginType, pData, mode, argc, argn, argv);
    }
    else
    {
        // Unknown MIME type
    }

    // Test if plugin creation has succeeded and cleanup if it hasn't
    if (rv != NPERR_NO_ERROR)
    {
        delete pData;
        return rv;
    }

    instance->pdata = pData;

    return NPERR_NO_ERROR;
}


// NPP_Destroy
//
//    Deletes a plug-in instance and releases all of its resources.
//
NPError NP_LOADDS
NPP_Destroy(NPP instance, NPSavedData** save)
{
    NG_TRACE_METHOD(NPP_Destroy);

    PluginInstanceData *pData = (PluginInstanceData *) instance->pdata;
    if (pData == NULL)
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    if (pData->nType == itControl)
    {
        // Destroy the site
        CControlSiteInstance *pSite = pData->pControlSite;
        if (pSite)
        {
            pSite->Detach();
            pSite->Release();
        }
#ifdef MOZ_ACTIVEX_PLUGIN_XPCONNECT
        if (pData->pScriptingPeer)
        {
            pData->pScriptingPeer->Release();
        }
#endif
    }
    else if (pData->nType == itScript)
    {
        // TODO
    }

    delete pData;

    instance->pdata = 0;

    return NPERR_NO_ERROR;

}


// NPP_SetWindow
//
//    Associates a platform specific window handle with a plug-in instance.
//        Called multiple times while, e.g., scrolling.  Can be called for three
//        reasons:
//
//            1.  A new window has been created
//            2.  A window has been moved or resized
//            3.  A window has been destroyed
//
//    There is also the degenerate case;  that it was called spuriously, and
//  the window handle and or coords may have or have not changed, or
//  the window handle and or coords may be ZERO.  State information
//  must be maintained by the plug-in to correctly handle the degenerate
//  case.
//
NPError NP_LOADDS
NPP_SetWindow(NPP instance, NPWindow* window)
{
    NG_TRACE_METHOD(NPP_SetWindow);

    // Reject silly parameters
    if (!window)
    {
        return NPERR_GENERIC_ERROR;
    }

    PluginInstanceData *pData = (PluginInstanceData *) instance->pdata;
    if (pData == NULL)
    {
        return  NPERR_INVALID_INSTANCE_ERROR;
    }

    if (pData->nType == itControl)
    {
        CControlSiteInstance *pSite = pData->pControlSite;
        if (pSite == NULL)
        {
            return NPERR_GENERIC_ERROR;
        }

        HWND hwndParent = (HWND) window->window;
        if (hwndParent)
        {
            RECT rcPos;
            GetClientRect(hwndParent, &rcPos);

            if (pSite->GetParentWindow() == NULL)
            {
                pSite->Attach(hwndParent, rcPos, NULL);
            }
            else
            {
                pSite->SetPosition(rcPos);
            }
        }
}

    return NPERR_NO_ERROR;
}


// NPP_NewStream
//
//    Notifies the plugin of a new data stream.
//  The data type of the stream (a MIME name) is provided.
//  The stream object indicates whether it is seekable.
//  The plugin specifies how it wants to handle the stream.
//
//  In this case, I set the streamtype to be NPAsFile.  This tells the Navigator
//  that the plugin doesn't handle streaming and can only deal with the object as
//  a complete disk file.  It will still call the write functions but it will also
//  pass the filename of the cached file in a later NPE_StreamAsFile call when it
//  is done transfering the file.
//
//  If a plugin handles the data in a streaming manner, it should set streamtype to
//  NPNormal  (e.g. *streamtype = NPNormal)...the NPE_StreamAsFile function will
//  never be called in this case
//
NPError NP_LOADDS
NPP_NewStream(NPP instance,
              NPMIMEType type,
              NPStream *stream, 
              NPBool seekable,
              uint16 *stype)
{
    NG_TRACE_METHOD(NPP_NewStream);

    if(!instance)
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    // save the plugin instance object in the stream instance
    stream->pdata = instance->pdata;
    *stype = NP_ASFILE;
    return NPERR_NO_ERROR;
}


// NPP_StreamAsFile
//
//    The stream is done transferring and here is a pointer to the file in the cache
//    This function is only called if the streamtype was set to NPAsFile.
//
void NP_LOADDS
NPP_StreamAsFile(NPP instance, NPStream *stream, const char* fname)
{
    NG_TRACE_METHOD(NPP_StreamAsFile);

    if(fname == NULL || fname[0] == NULL)
    {
        return;
    }
}


//
//        These next 2 functions are really only directly relevant 
//        in a plug-in which handles the data in a streaming manner.  
//        For a NPAsFile stream, they are still called but can safely 
//        be ignored.
//
//        In a streaming plugin, all data handling would take place here...
//
////\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//.
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\.

int32 STREAMBUFSIZE = 0X0FFFFFFF;   // we are reading from a file in NPAsFile mode
                                    // so we can take any size stream in our write
                                    // call (since we ignore it)
                                

// NPP_WriteReady
//
//    The number of bytes that a plug-in is willing to accept in a subsequent
//    NPO_Write call.
//
int32 NP_LOADDS
NPP_WriteReady(NPP instance, NPStream *stream)
{
    return STREAMBUFSIZE;  
}


// NPP_Write
//
//    Provides len bytes of data.
//
int32 NP_LOADDS
NPP_Write(NPP instance, NPStream *stream, int32 offset, int32 len, void *buffer)
{   
    return len;
}


// NPP_DestroyStream
//
//    Closes a stream object.  
//    reason indicates why the stream was closed.  Possible reasons are
//    that it was complete, because there was some error, or because 
//    the user aborted it.
//
NPError NP_LOADDS
NPP_DestroyStream(NPP instance, NPStream *stream, NPError reason)
{
    // because I am handling the stream as a file, I don't do anything here...
    // If I was streaming, I would know that I was done and do anything appropriate
    // to the end of the stream...   
    return NPERR_NO_ERROR;
}


// NPP_Print
//
//    Printing the plugin (to be continued...)
//
void NP_LOADDS
NPP_Print(NPP instance, NPPrint* printInfo)
{
    if(printInfo == NULL)   // trap invalid parm
    {
        return;
    }

//    if (instance != NULL) {
//        CPluginWindow* pluginData = (CPluginWindow*) instance->pdata;
//        pluginData->Print(printInfo);
//    }
}

/*******************************************************************************
// NPP_URLNotify:
// Notifies the instance of the completion of a URL request. 
// 
// NPP_URLNotify is called when Netscape completes a NPN_GetURLNotify or
// NPN_PostURLNotify request, to inform the plug-in that the request,
// identified by url, has completed for the reason specified by reason. The most
// common reason code is NPRES_DONE, indicating simply that the request
// completed normally. Other possible reason codes are NPRES_USER_BREAK,
// indicating that the request was halted due to a user action (for example,
// clicking the "Stop" button), and NPRES_NETWORK_ERR, indicating that the
// request could not be completed (for example, because the URL could not be
// found). The complete list of reason codes is found in npapi.h. 
// 
// The parameter notifyData is the same plug-in-private value passed as an
// argument to the corresponding NPN_GetURLNotify or NPN_PostURLNotify
// call, and can be used by your plug-in to uniquely identify the request. 
 ******************************************************************************/

void
NPP_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData)
{
}

#ifdef MOZ_ACTIVEX_PLUGIN_XPCONNECT

#include "nsIMozAxPlugin.h"
#include "nsIClassInfo.h"

// We must implement nsIClassInfo because it signals the
// Mozilla Security Manager to allow calls from JavaScript.

// helper class to implement all necessary nsIClassInfo method stubs
// and to set flags used by the security system
class nsClassInfoMozAxPlugin : public nsIClassInfo
{
  // These flags are used by the DOM and security systems to signal that 
  // JavaScript callers are allowed to call this object's scritable methods.
  NS_IMETHOD GetFlags(PRUint32 *aFlags)
    {*aFlags = nsIClassInfo::PLUGIN_OBJECT | nsIClassInfo::DOM_OBJECT;
     return NS_OK;}
  NS_IMETHOD GetImplementationLanguage(PRUint32 *aImplementationLanguage)
    {*aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
     return NS_OK;}

  // The rest of the methods can safely return error codes...
  NS_IMETHOD GetInterfaces(PRUint32 *count, nsIID * **array)
    {return NS_ERROR_NOT_IMPLEMENTED;}
  NS_IMETHOD GetHelperForLanguage(PRUint32 language, nsISupports **_retval)
    {return NS_ERROR_NOT_IMPLEMENTED;}
  NS_IMETHOD GetContractID(char * *aContractID)
    {return NS_ERROR_NOT_IMPLEMENTED;}
  NS_IMETHOD GetClassDescription(char * *aClassDescription)
    {return NS_ERROR_NOT_IMPLEMENTED;}
  NS_IMETHOD GetClassID(nsCID * *aClassID)
    {return NS_ERROR_NOT_IMPLEMENTED;}
  NS_IMETHOD GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
    {return NS_ERROR_NOT_IMPLEMENTED;}
};

// Defines to be used as interface names by nsScriptablePeer
static NS_DEFINE_IID(kIMoxAxPluginIID, NS_IMOZAXPLUGIN_IID);
static NS_DEFINE_IID(kIClassInfoIID, NS_ICLASSINFO_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

class nsScriptablePeer : public nsIMozAxPlugin,
                         public nsClassInfoMozAxPlugin
{
    long mRef;
    PluginInstanceData* mPlugin;

protected:
    virtual ~nsScriptablePeer();

public:
    nsScriptablePeer();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIMOZAXPLUGIN
};

nsScriptablePeer::nsScriptablePeer()
{
    mRef = 0;
}

nsScriptablePeer::~nsScriptablePeer()
{
}

///////////////////////////////////////////////////////////////////////////////
// nsISupports

// NOTE: We're not using the helpful NS_IMPL_ISUPPORTS macro because they drag
//       in dependencies to xpcom that a plugin like ourselves is better off
//       without.

/* void QueryInterface (in nsIIDRef uuid, [iid_is (uuid), retval] out nsQIResult result); */
NS_IMETHODIMP nsScriptablePeer::QueryInterface(const nsIID & aIID, void * *aInstancePtr)
{
    if (aIID.Equals(NS_GET_IID(nsISupports)))
    {
        *aInstancePtr = NS_STATIC_CAST(void *, this);
        AddRef();
        return NS_OK;
    }
    else if (aIID.Equals(NS_GET_IID(nsIMozAxPlugin)))
    {
        *aInstancePtr = NS_STATIC_CAST(void *, this);
        AddRef();
        return NS_OK;
    }
    else if (aIID.Equals(kIClassInfoIID))
    {
        *aInstancePtr = static_cast<nsIClassInfo*>(this); 
        AddRef();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}

/* [noscript, notxpcom] nsrefcnt AddRef (); */
NS_IMETHODIMP_(nsrefcnt) nsScriptablePeer::AddRef()
{
    mRef++;
    return NS_OK;
}

/* [noscript, notxpcom] nsrefcnt Release (); */
NS_IMETHODIMP_(nsrefcnt) nsScriptablePeer::Release()
{
    mRef--;
    if (mRef <= 0)
    {
        delete this;
    }
    return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// nsIMozAxPlugin

// the following method will be callable from JavaScript

NS_IMETHODIMP 
nsScriptablePeer::Invoke(const char *str)
{
/*    HRESULT hr;
    DISPID dispid;
    IDispatch FAR* pdisp = (IDispatch FAR*)NULL;
    // call the requested function
    const char* funcName = str; //_T("Update");
    PluginInstanceData *pData = mPlugin;
    if (pData == NULL) {
        return NPERR_INVALID_INSTANCE_ERROR;
    }
    IUnknown FAR* punk;
    hr = pData->pControlSite->GetControlUnknown(&punk);
    if (FAILED(hr)) {
        return NPERR_GENERIC_ERROR; 
    }
    punk->AddRef();
    hr = punk->QueryInterface(IID_IDispatch,(void FAR* FAR*)&pdisp);
    if (FAILED(hr)) { 
        punk->Release();
        return NPERR_GENERIC_ERROR; 
    }
    USES_CONVERSION;
    OLECHAR FAR* szMember = A2OLE(funcName);
    hr = pdisp->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) { 
        punk->Release();
        return NPERR_GENERIC_ERROR; 
    }
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    hr = pdisp->Invoke(
        dispid,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_METHOD,
        &dispparamsNoArgs, NULL, NULL, NULL);
    if (FAILED(hr)) { 
        return NPERR_GENERIC_ERROR; 
    }
    punk->Release(); */
    return NS_OK;
}


NS_IMETHODIMP 
nsScriptablePeer::GetProperty(const char *propertyName, char **_retval)
{
/*    HRESULT hr;
    DISPID dispid;
    //VARIANT VarResult;
    _variant_t VarResult;
    //char* propertyValue;
    IDispatch FAR* pdisp = (IDispatch FAR*)NULL;
    const char* property = propertyName;
    PluginInstanceData *pData = mPlugin;
    if (pData == NULL) { 
        return NPERR_INVALID_INSTANCE_ERROR;
    }
    IUnknown FAR* punk;
    hr = pData->pControlSite->GetControlUnknown(&punk);
    if (FAILED(hr)) { return NULL; }
    punk->AddRef();
    hr = punk->QueryInterface(IID_IDispatch,(void FAR* FAR*)&pdisp);
    if (FAILED(hr)) { 
        punk->Release();
        return NPERR_GENERIC_ERROR; 
    }
    USES_CONVERSION;
    OLECHAR FAR* szMember = A2OLE(property);
    hr = pdisp->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) { 
        punk->Release();
        return NPERR_GENERIC_ERROR;
    }
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    hr = pdisp->Invoke(
        dispid,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYGET,
        &dispparamsNoArgs, &VarResult, NULL, NULL);
    if (FAILED(hr)) { 
        return NPERR_GENERIC_ERROR;
    }
	punk->Release();
    
    char* tempStr;
    switch(VarResult.vt & VT_TYPEMASK) {
    case VT_BSTR:    
        tempStr = OLE2A(VarResult.bstrVal);
        if(!_retval) return NS_ERROR_NULL_POINTER;
        *_retval = (char*) nsMemory::Alloc(strlen(tempStr) + 1);
        if (! *_retval) return NS_ERROR_NULL_POINTER;
        if (VarResult.bstrVal == NULL) {
            *_retval = NULL;
        } else {
            strcpy(*_retval, tempStr);
        }
        break;
//  case VT_I2:
    default:
        VarResult.ChangeType(VT_BSTR);
        tempStr = OLE2A(VarResult.bstrVal);
        if(!_retval) return NS_ERROR_NULL_POINTER;
        *_retval = (char*) nsMemory::Alloc(strlen(tempStr) + 1);
        if (! *_retval) return NS_ERROR_NULL_POINTER;
        if (VarResult.bstrVal == NULL) {
            *_retval = NULL;
        } else {
            strcpy(*_retval, tempStr);
        }
        break;
    } */

    // caller will be responsible for any memory allocated.
	return NS_OK;
}

NS_IMETHODIMP nsScriptablePeer::GetNProperty(const char *propertyName, PRInt16 *_retval)
{
/*    HRESULT hr;
    DISPID dispid;
    VARIANT VarResult;
    IDispatch FAR* pdisp = (IDispatch FAR*)NULL;
    const char* property = propertyName;
    PluginInstanceData *pData = mPlugin;
    if (pData == NULL) { 
        return NPERR_INVALID_INSTANCE_ERROR;
    }
    IUnknown FAR* punk;
    hr = pData->pControlSite->GetControlUnknown(&punk);
    if (FAILED(hr)) { return NULL; }
    punk->AddRef();
    hr = punk->QueryInterface(IID_IDispatch,(void FAR* FAR*)&pdisp);
    if (FAILED(hr)) { 
        punk->Release();
        return NPERR_GENERIC_ERROR; 
    }
    USES_CONVERSION;
    OLECHAR FAR* szMember = A2OLE(property);
    hr = pdisp->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) { 
        punk->Release();
        return NPERR_GENERIC_ERROR;
    }
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    hr = pdisp->Invoke(
        dispid,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYGET,
        &dispparamsNoArgs, &VarResult, NULL, NULL);
    if (FAILED(hr)) { 
        return NPERR_GENERIC_ERROR;
    }
    punk->Release();
    if(!_retval) return NS_ERROR_NULL_POINTER;
    // make sure we are dealing with an int
    if ((VarResult.vt & VT_TYPEMASK) != VT_I2) {
        *_retval = NULL;
        return NPERR_GENERIC_ERROR;
    }
    *_retval = VarResult.iVal; */

	// caller will be responsible for any memory allocated.
	return NS_OK;
}

/* void setProperty (in string propertyName, in string propertyValue); */
NS_IMETHODIMP nsScriptablePeer::SetProperty(const char *propertyName, const char *propertyValue)
{
    HRESULT hr;
    DISPID dispid;
    VARIANT VarResult;
    IDispatch FAR* pdisp = (IDispatch FAR*)NULL;
    const char* property = propertyName;
    PluginInstanceData *pData = mPlugin;
    if (pData == NULL) { 
        return NPERR_INVALID_INSTANCE_ERROR;
    }
    IUnknown FAR* punk;
    hr = pData->pControlSite->GetControlUnknown(&punk);
    if (FAILED(hr)) { return NULL; }
    punk->AddRef();
    hr = punk->QueryInterface(IID_IDispatch,(void FAR* FAR*)&pdisp);
    if (FAILED(hr)) { 
        punk->Release();
        return NPERR_GENERIC_ERROR;
    }
    USES_CONVERSION;
    OLECHAR FAR* szMember = A2OLE(property);
    hr = pdisp->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) { 
        punk->Release();
        return NPERR_GENERIC_ERROR;
    }
    VARIANT *pvars = new VARIANT[1];
    VariantInit(&pvars[0]);
    pvars->vt = VT_BSTR;
    pvars->bstrVal = A2OLE(propertyValue);
    DISPID dispIdPut = DISPID_PROPERTYPUT;
    DISPPARAMS functionArgs;
    functionArgs.rgdispidNamedArgs = &dispIdPut;
    functionArgs.rgvarg = pvars;
    functionArgs.cArgs = 1;
    functionArgs.cNamedArgs = 1;

    hr = pdisp->Invoke(
        dispid,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYPUT,
        &functionArgs, &VarResult, NULL, NULL);
    delete []pvars;
    if (FAILED(hr)) { 
        return NPERR_GENERIC_ERROR;
    }
    punk->Release();
    return NS_OK;
}

/* void setNProperty (in string propertyName, in string propertyValue); */
NS_IMETHODIMP nsScriptablePeer::SetNProperty(const char *propertyName, PRInt16 propertyValue)
{
    HRESULT hr;
    DISPID dispid;
    VARIANT VarResult;
    IDispatch FAR* pdisp = (IDispatch FAR*)NULL;
    const char* property = propertyName;
    PluginInstanceData *pData = mPlugin;
    if (pData == NULL) { 
        return NPERR_INVALID_INSTANCE_ERROR;
    }
    IUnknown FAR* punk;
    hr = pData->pControlSite->GetControlUnknown(&punk);
    if (FAILED(hr)) { return NULL; }
    punk->AddRef();
    hr = punk->QueryInterface(IID_IDispatch,(void FAR* FAR*)&pdisp);
    if (FAILED(hr)) { 
        punk->Release();
        return NPERR_GENERIC_ERROR;
    }
    USES_CONVERSION;
    OLECHAR FAR* szMember = A2OLE(property);
    hr = pdisp->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) { 
        punk->Release();
        return NPERR_GENERIC_ERROR;
    }
   
    VARIANT *pvars = new VARIANT[1];
    VariantInit(&pvars[0]);
    pvars->vt = VT_I2;
    pvars->iVal = propertyValue;

    DISPID dispIdPut = DISPID_PROPERTYPUT;
    DISPPARAMS functionArgs;
    functionArgs.rgdispidNamedArgs = &dispIdPut;
    functionArgs.rgvarg = pvars;
    functionArgs.cArgs = 1;
    functionArgs.cNamedArgs = 1;

    hr = pdisp->Invoke(
        dispid,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYPUT,
        &functionArgs, &VarResult, NULL, NULL);
    delete []pvars;
    if (FAILED(hr)) { 
        return NPERR_GENERIC_ERROR;
    }
    punk->Release();
    return NS_OK;
}

// Happy happy fun fun - redefine some NPPVariable values that we might
// be asked for but not defined by every PluginSDK 

const int kVarScriptableInstance = 10; // NPPVpluginScriptableInstance
const int kVarScriptableIID = 11; // NPPVpluginScriptableIID

NPError	NP_LOADDS
NPP_GetValue(NPP instance, NPPVariable variable, void *value)
{
    if (instance == NULL)
    {
        return NPERR_INVALID_INSTANCE_ERROR;
    }

    if (variable == kVarScriptableInstance)
    {
        PluginInstanceData *pData = (PluginInstanceData *) instance->pdata;
        if (!pData->pScriptingPeer)
        {
            nsScriptablePeer *peer  = new nsScriptablePeer();
            peer->AddRef();
            pData->pScriptingPeer = (nsIMozAxPlugin *) peer;
        }
        if (pData->pScriptingPeer)
        {
            pData->pScriptingPeer->AddRef();
            *((nsISupports **) value)= pData->pScriptingPeer;
            return NPERR_NO_ERROR;
        }
    }
    else if (variable == kVarScriptableIID)
    {
	    static nsIID kIMozAxPluginIID = NS_IMOZAXPLUGIN_IID;
        nsIID *piid = (nsIID *) NPN_MemAlloc(sizeof(nsIID));
        *piid = kIMozAxPluginIID;
        *((nsIID **) value) = piid;
        return NPERR_NO_ERROR;
    }

    return NPERR_GENERIC_ERROR;
}

NPError	NP_LOADDS
NPP_SetValue(NPP instance, NPNVariable variable, void *value)
{
    return NPERR_GENERIC_ERROR;
}
#endif

#ifdef MOZ_ACTIVEX_PLUGIN_LIVECONNECT

// The following will be callable from Javascript through LiveConnect
HRESULT
_GetIDispatchFromJRI(JRIEnv *env, struct MozAxPlugin* self, IDispatch **pdisp)
{
    *pdisp = NULL;

    NPP npp = (NPP) self->getPeer(env);
    PluginInstanceData *pData = (PluginInstanceData *) npp->pdata;
    if (pData == NULL)
    { 
        return E_FAIL;
    }

    IUnknownPtr unk;
    HRESULT hr = pData->pControlSite->GetControlUnknown(&unk);
    if (unk.GetInterfacePtr() == NULL)
    {
		return E_FAIL; 
	}

    IDispatchPtr disp = unk;
    if (!disp.GetInterfacePtr() == NULL)
    { 
        return E_FAIL; 
    }

    *pdisp = disp.GetInterfacePtr();
    (*pdisp)->AddRef();

    return S_OK;
}


HRESULT
_JRIObjectToVariant(java_lang_Object *o, VARIANT *v)
{
    // TODO
    return S_OK;
}

struct java_lang_Object *
_InvokeFromJRI(JRIEnv *env, struct MozAxPlugin* self, struct java_lang_String *func, int nargs, java_lang_Object *args[])
{
	HRESULT hr;
    DISPID dispid = 0;
    
	// call the requested function
    const char* funcName = JRI_GetStringUTFChars(env, func);

    IDispatchPtr disp;
    if (FAILED(_GetIDispatchFromJRI(env, self, &disp)))
    {
        return NULL;
    }

    USES_CONVERSION;
    OLECHAR FAR* szMember = A2OLE(funcName);
	hr = disp->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr))
    { 
        return NULL; 
    }

    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};

    hr = disp->Invoke(
        dispid,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_METHOD,
        &dispparamsNoArgs, NULL, NULL, NULL);
    
    if (FAILED(hr))
    { 
        return NULL; 
    }

	return NULL;

}

/*******************************************************************************
 * Native Methods: 
 * These are the native methods which we are implementing.
 ******************************************************************************/

//*** native Invoke (Ljava/lang/String;)Ljava/lang/Object; ***
JRI_PUBLIC_API(struct java_lang_Object *)
native_MozAxPlugin_invoke(JRIEnv* env, struct MozAxPlugin* self, struct java_lang_String *a)
{
    return _InvokeFromJRI(env, self, a, 0, NULL);
}

//*** native Invoke (Ljava/lang/String;Ljava/lang/Object;)Ljava/lang/Object; ***
JRI_PUBLIC_API(struct java_lang_Object *)
native_MozAxPlugin_invoke_1(JRIEnv* env, struct MozAxPlugin* self, struct java_lang_String *a, struct java_lang_Object *b)
{
    java_lang_Object *args[1];
    args[0] = b;
    return _InvokeFromJRI(env, self, a, sizeof(args) / sizeof(args[0]), args);
}

//*** native Invoke (Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object; ***
JRI_PUBLIC_API(struct java_lang_Object *)
native_MozAxPlugin_invoke_2(JRIEnv* env, struct MozAxPlugin* self, struct java_lang_String *a, struct java_lang_Object *b, struct java_lang_Object *c)
{
    java_lang_Object *args[2];
    args[0] = b;
    args[1] = c;
    return _InvokeFromJRI(env, self, a, sizeof(args) / sizeof(args[0]), args);
}

//*** native Invoke (Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object; ***
JRI_PUBLIC_API(struct java_lang_Object *)
native_MozAxPlugin_invoke_3(JRIEnv* env, struct MozAxPlugin* self, struct java_lang_String *a, struct java_lang_Object *b, struct java_lang_Object *c, struct java_lang_Object *d)
{
    java_lang_Object *args[3];
    args[0] = b;
    args[1] = c;
    args[2] = d;
    return _InvokeFromJRI(env, self, a, sizeof(args) / sizeof(args[0]), args);
}

//*** native Invoke (Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object; ***
JRI_PUBLIC_API(struct java_lang_Object *)
native_MozAxPlugin_invoke_4(JRIEnv* env, struct MozAxPlugin* self, struct java_lang_String *a, struct java_lang_Object *b, struct java_lang_Object *c, struct java_lang_Object *d, struct java_lang_Object *e)
{
    java_lang_Object *args[4];
    args[0] = b;
    args[1] = c;
    args[2] = d;
    args[3] = e;
    return _InvokeFromJRI(env, self, a, sizeof(args) / sizeof(args[0]), args);
}


//*** native GetProperty (Ljava/lang/String;)Ljava/lang/Object; ***
JRI_PUBLIC_API(struct java_lang_Object *)
native_MozAxPlugin_getProperty(JRIEnv* env, struct MozAxPlugin* self, struct java_lang_String *a)
{
    HRESULT hr;
    DISPID dispid;
    _variant_t VarResult;

    IDispatchPtr disp;
    if (FAILED(_GetIDispatchFromJRI(env, self, &disp)))
    {
        return NULL;
    }

    // return the requested property to the Java peer
    USES_CONVERSION;
    OLECHAR FAR* szMember = A2OLE(JRI_GetStringUTFChars(env, a));
    hr = disp->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr))
    { 
        return NULL; 
    }
    DISPPARAMS dispparamsNoArgs = {NULL, NULL, 0, 0};
    hr = disp->Invoke(
        dispid,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYGET,
        &dispparamsNoArgs, &VarResult, NULL, NULL);
    if (FAILED(hr))
    { 
        return NULL; 
    }

    _bstr_t strVal;
    switch(VarResult.vt & VT_TYPEMASK)
    {
    case VT_BSTR:
        strVal = _bstr_t(VarResult.bstrVal);
        return (java_lang_Object*)JRI_NewStringUTF( env, strVal, strVal.length() );
        break;
    case VT_I2:
        VarResult.ChangeType(VT_BSTR);
        strVal = _bstr_t(VarResult.bstrVal);
        return (java_lang_Object*)JRI_NewStringUTF( env, strVal, strVal.length() );
        break;
	}
	// caller will be responsible for any memory allocated by JRI_NewStringUTF

	return NULL;
}

/*** native setProperty (Ljava/lang/String;Ljava/lang/Object;)V ***/
JRI_PUBLIC_API(void)
native_MozAxPlugin_setProperty(JRIEnv* env, struct MozAxPlugin* self, struct java_lang_String *a, struct java_lang_Object *b)

{
    // TODO
}

//*** public native SetProperty (Ljava/lang/String;Ljava/lang/String;)V ***
JRI_PUBLIC_API(void)
native_MozAxPlugin_setProperty_1(JRIEnv* env, struct MozAxPlugin* self, struct java_lang_String *a, struct java_lang_String *b)
{
    HRESULT hr;
    DISPID dispid;
    VARIANT VarResult;

    IDispatchPtr disp;
    if (FAILED(_GetIDispatchFromJRI(env, self, &disp)))
    {
        return;
    }

    USES_CONVERSION;
    OLECHAR FAR* szMember = A2OLE(JRI_GetStringUTFChars(env, a));
    hr = disp->GetIDsOfNames(IID_NULL, &szMember, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr))
    { 
        return;
    }

    VARIANT *pvars = new VARIANT[1];
    VariantInit(&pvars[0]);
    pvars->vt = VT_BSTR;
    pvars->bstrVal = A2OLE(JRI_GetStringUTFChars(env, b));

    DISPID dispIdPut = DISPID_PROPERTYPUT;

    DISPPARAMS functionArgs;
    functionArgs.rgdispidNamedArgs = &dispIdPut;
    functionArgs.rgvarg = pvars;
    functionArgs.cArgs = 1;
    functionArgs.cNamedArgs = 1;

    hr = disp->Invoke(
        dispid,
        IID_NULL,
        LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYPUT,
        &functionArgs, &VarResult, NULL, NULL);

    delete []pvars;
    
    if (FAILED(hr))
    {
        return;
    }
}


#define NO_JDK
#include "_stubs/MozAxPlugin.c"

#define UNUSED_use_netscape_plugin_Plugin
#include "_stubs/netscape_plugin_Plugin.c"

#endif
