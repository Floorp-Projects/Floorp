/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "nppluglet.h"

#include "PlugletLoader.h"
#include "iPlugletEngine.h"
#include "nsIPlugin.h"

#include "nsIServiceManager.h"
#include "nsIMemory.h"
#include "nsISupportsUtils.h" // this is where some useful macros defined
#include "ns4xPluginStreamListener.h"
#include "nsIPluginStreamInfo.h"

#include "nsServiceManagerUtils.h"

#include "plstr.h"

#include "npAPInsIInputStreamShim.h"

#include "prlog.h"

static PRLogModuleInfo* log = NULL;

// service manager which will give the access to all public browser services
// we will use memory service as an illustration
nsIServiceManager * gServiceManager = NULL;

// Unix needs this
#ifdef XP_UNIX
#define MIME_TYPES_HANDLED  "*"
#define PLUGIN_NAME         "Java Plugins for Mozilla"
#define MIME_TYPES_DESCRIPTION  MIME_TYPES_HANDLED"::"PLUGIN_NAME
#define PLUGIN_DESCRIPTION  PLUGIN_NAME " Pluglets" 

char* NPP_GetMIMEDescription(void)
{
    return(MIME_TYPES_DESCRIPTION);
}

// get values per plugin
NPError NS_PluginGetValue(NPPVariable aVariable, void *aValue)
{
  NPError err = NPERR_NO_ERROR;
  switch (aVariable) {
    case NPPVpluginNameString:
      *((char **)aValue) = PLUGIN_NAME;
      break;
    case NPPVpluginDescriptionString:
      *((char **)aValue) = PLUGIN_DESCRIPTION;
      break;
    default:
      err = NPERR_INVALID_PARAM;
      break;
  }
  return err;
}
#endif //XP_UNIX

//////////////////////////////////////
//
// general initialization and shutdown
//
NPError NS_PluginInitialize()
{
  // this is probably a good place to get the service manager
  // note that Mozilla will add reference, so do not forget to release
  nsISupports * sm = NULL;

  log = PR_NewLogModule("nppluglet");

  PR_LOG(log, PR_LOG_DEBUG,
         ("nppluglet NS_PluginInitialize\n"));
  
  NPN_GetValue(NULL, NPNVserviceManager, &sm);

  // Mozilla returns nsIServiceManager so we can use it directly; doing QI on
  // nsISupports here can still be more appropriate in case something is changed 
  // in the future so we don't need to do casting of any sort.
  if(sm) {
    sm->QueryInterface(NS_GET_IID(nsIServiceManager), (void**)&gServiceManager);
    NS_RELEASE(sm);
  }

  PlugletLoader::Initialize();

  return NPERR_NO_ERROR;
}

void NS_PluginShutdown()
{

  // we should release the service manager
  NS_IF_RELEASE(gServiceManager);
  gServiceManager = NULL;
}

/////////////////////////////////////////////////////////////
//
// construction and destruction of our plugin instance object
//
nsPluginInstanceBase * NS_NewPluginInstance(nsPluginCreateData * aCreateDataStruct)
{
    nsPluginInstance * result = nsnull;
    nsresult rv = NS_ERROR_FAILURE;
    if(aCreateDataStruct) {
        result = new nsPluginInstance(aCreateDataStruct);
        PRBool isSupported = PR_FALSE;
        rv = result->HasPlugletForMimeType(aCreateDataStruct->type, 
                                           &isSupported);
        if (NS_SUCCEEDED(rv)) {
            if (!isSupported) {
                result = nsnull;
            }
        }
        else {
            result = nsnull;
        }
    }
    
    return result;
}

void NS_DestroyPluginInstance(nsPluginInstanceBase * aPlugin)
{
  if(aPlugin)
    delete (nsPluginInstance *)aPlugin;
}

////////////////////////////////////////
//
// nsPluginInstance class implementation
//
nsPluginInstance::nsPluginInstance(nsPluginCreateData * aCreateDataStruct) : nsPluginInstanceBase(),
  mInitialized(PR_FALSE),
  mScriptablePeer(nsnull),
  mPluglet(nsnull)
{
  mInstance = aCreateDataStruct->instance;

  mCreateDataStruct.instance = aCreateDataStruct->instance;
  mCreateDataStruct.type = aCreateDataStruct->type;
  mCreateDataStruct.mode = aCreateDataStruct->mode;
  mCreateDataStruct.argc = aCreateDataStruct->argc;
  mCreateDataStruct.argn = aCreateDataStruct->argn;
  mCreateDataStruct.saved = aCreateDataStruct->saved;

  mCreateDataStruct.instance->pdata = this;
  mInstance->pdata = this;
  mString[0] = '\0';
}

nsPluginInstance::~nsPluginInstance()
{

    if (mPluglet) {
        mPluglet->Destroy();
        mPluglet = nsnull;

    }
    mInitialized = PR_FALSE;


    if (mScriptablePeer) {
        // mScriptablePeer may be also held by the browser 
        // so releasing it here does not guarantee that it is over
        // we should take precaution in case it will be called later
        // and zero its mPlugin member
        mScriptablePeer->SetInstance(nsnull);
        
        NS_IF_RELEASE(mScriptablePeer);
    }
}

NPBool nsPluginInstance::init(NPWindow* aWindow)
{
    nsresult rv = NS_ERROR_FAILURE;
    nsPluginWindow pluginWindow;

    if (mInitialized) {
        return PR_TRUE;
    }

    if(nsnull == aWindow || nsnull == mPluglet) {
        return mInitialized;
    }
    
    nsCOMPtr<nsIPluginInstance> ns4xPluginInst = 
        do_QueryInterface((nsISupports*)mInstance->ndata, &rv);
    if (NS_FAILED(rv)) {
        return mInitialized;
    }
    nsCOMPtr<nsIPluginInstancePeer> peer = nsnull;
    rv = ns4xPluginInst->GetPeer(getter_AddRefs(peer));
    if(NS_FAILED(rv)) {
        return mInitialized;
    }
    rv = mPluglet->Initialize(peer);
    if (NS_SUCCEEDED(rv)) {
        rv = this->CopyNPWindowToNsPluginWindow(aWindow, &pluginWindow);
        if (NS_SUCCEEDED(rv)) {
            rv = mPluglet->Start();
            if (NS_SUCCEEDED(rv)) {
                rv = mPluglet->SetWindow(&pluginWindow);
                if (NS_SUCCEEDED(rv)) {
                    mInitialized = PR_TRUE;
                }
            }
        }
    }
    
    return mInitialized; 
}

void nsPluginInstance::shut()
{
    if (mInitialized) {
        if (mPluglet) {
            mPluglet->Stop();
        }
    }
}

NPBool nsPluginInstance::isInitialized()
{
  return mInitialized;
}

NPError nsPluginInstance::SetWindow(NPWindow* pNPWindow)
{
    nsresult rv = NS_ERROR_FAILURE;
    nsPluginWindow pluginWindow;

    if (!this->isInitialized()) {
        return rv;
    }

    rv = this->CopyNPWindowToNsPluginWindow(pNPWindow, &pluginWindow);
    if (NS_SUCCEEDED(rv)) {
        rv = mPluglet->SetWindow(&pluginWindow);
    }

    return (NPError) rv;
}

NPError nsPluginInstance::NewStream(NPMIMEType type, NPStream* stream, 
                                    NPBool seekable, uint16* stype)
{
    nsresult rv = NS_ERROR_FAILURE;

    if (!this->isInitialized()) {
        return rv;
    }

    nsCOMPtr<nsIPluginStreamListener> listener = nsnull;
    rv = mPluglet->NewStream(getter_AddRefs(listener));
    if (NS_SUCCEEDED(rv)) {
        ns4xPluginStreamListener * hostListener = 
            (ns4xPluginStreamListener *) stream->ndata;
        if (hostListener) {
            nsCOMPtr<nsIPluginStreamInfo> hostStreamInfo = 
                hostListener->mStreamInfo;
            if (hostStreamInfo) {
                rv = listener->OnStartBinding(hostStreamInfo);
                if (NS_SUCCEEDED(rv)) {
                    npAPInsIInputStreamShim *shim = 
                        new npAPInsIInputStreamShim(listener,
                                                    hostStreamInfo);
                    shim->AddRef();
                    stream->pdata = (void *) shim;
                }
            }
        }
    }

    return rv;
}

NPError nsPluginInstance::DestroyStream(NPStream *stream, NPError reason) 
{
    nsresult rv = NS_ERROR_FAILURE;

    if (!this->isInitialized()) {
        return rv;
    }

    npAPInsIInputStreamShim *shim = 
        (npAPInsIInputStreamShim *) stream->pdata;
    shim->Close();
    shim->DoClose();
    shim->Release();
    stream->pdata = nsnull;

    return NS_OK;
}

int32 nsPluginInstance::WriteReady(NPStream *stream) {
    int32 result = 65536;
    return result;
}

int32 nsPluginInstance::Write(NPStream *stream, int32 offset, 
                              int32 len, void *buffer)
{
    int32 result = len;
    npAPInsIInputStreamShim *shim = 
        (npAPInsIInputStreamShim *) stream->pdata;
    nsresult rv = NS_ERROR_NULL_POINTER;

    rv = shim->AllowStreamToReadFromBuffer(len, buffer, &result);
    
    return result;
}

NS_IMETHODIMP nsPluginInstance::HasPlugletForMimeType(const char *aMimeType, 
                                                      PRBool *outResult)
{
    nsresult rv = NS_ERROR_FAILURE;
    *outResult = PR_FALSE;
    nsCOMPtr<nsIPlugin> plugletEngine = nsnull;
    nsIID scriptableIID = NS_ISIMPLEPLUGIN_IID;    

    if (!mPluglet) {
        plugletEngine = do_GetService(PLUGLETENGINE_ContractID, &rv);
    
        if (NS_SUCCEEDED(rv)) {
            rv = plugletEngine->CreatePluginInstance(nsnull, scriptableIID, 
                                                     aMimeType, 
                                                     getter_AddRefs(mPluglet));
            if (NS_SUCCEEDED(rv) && mPluglet) {
                *outResult = PR_TRUE;
            }
        }
    }
    else {
        if (0 == PL_strcmp(aMimeType, mCreateDataStruct.type)) {
            *outResult = PR_TRUE;
        }
		rv = NS_OK;
    }

    return rv;
}

nsresult nsPluginInstance::CopyNPWindowToNsPluginWindow(NPWindow *aWindow,
                                                        nsPluginWindow *pluginWindow)
{
    nsresult rv = NS_ERROR_NULL_POINTER;

    if (nsnull == aWindow || nsnull == pluginWindow) {
        return rv;
    }

#if defined(XP_MAC) || defined(XP_MACOSX)
#elif defined(XP_WIN) || defined(XP_OS2)
    pluginWindow->window = (nsPluginPort *) aWindow->window;
#elif defined(XP_UNIX) && defined(MOZ_X11)
#else
#endif
    pluginWindow->x = aWindow->x;
    pluginWindow->y = aWindow->y;
    pluginWindow->width = aWindow->width;
    pluginWindow->height = aWindow->height;
    pluginWindow->clipRect.top = aWindow->clipRect.top;
    pluginWindow->clipRect.left = aWindow->clipRect.left;
    pluginWindow->clipRect.bottom = aWindow->clipRect.bottom;
    pluginWindow->clipRect.right = aWindow->clipRect.right;
#if defined(XP_UNIX) && !defined(XP_MACOSX)
    pluginWindow->ws_info = aWindow->ws_info;
#endif
    pluginWindow->type = aWindow->type == NPWindowTypeWindow ?
        nsPluginWindowType_Window : nsPluginWindowType_Drawable;
    rv = NS_OK;

    return rv;
}



// ==============================
// ! Scriptability related code !
// ==============================
//
// here the plugin is asked by Mozilla to tell if it is scriptable
// we should return a valid interface id and a pointer to 
// nsScriptablePeer interface which we should have implemented
// and which should be defined in the corressponding *.xpt file
// in the bin/components folder
NPError	nsPluginInstance::GetValue(NPPVariable aVariable, void *aValue)
{
  NPError rv = NPERR_NO_ERROR;

  switch (aVariable) {
    case NPPVpluginScriptableInstance: {
      // addref happens in getter, so we don't addref here
      nsISimplePlugin * scriptablePeer = getScriptablePeer();
      if (scriptablePeer) {
        *(nsISupports **)aValue = scriptablePeer;
      } else
        rv = NPERR_OUT_OF_MEMORY_ERROR;
    }
    break;

    case NPPVpluginScriptableIID: {
      static nsIID scriptableIID = NS_ISIMPLEPLUGIN_IID;
      nsIID* ptr = (nsIID *)NPN_MemAlloc(sizeof(nsIID));
      if (ptr) {
          *ptr = scriptableIID;
          *(nsIID **)aValue = ptr;
      } else
        rv = NPERR_OUT_OF_MEMORY_ERROR;
    }
    break;

    default:
      break;
  }

  return rv;
}

// ==============================
// ! Scriptability related code !
// ==============================
//
// this method will return the scriptable object (and create it if necessary)
nsScriptablePeer* nsPluginInstance::getScriptablePeer()
{
  if (!mScriptablePeer) {
    mScriptablePeer = new nsScriptablePeer(this);
    if(!mScriptablePeer)
      return NULL;

    NS_ADDREF(mScriptablePeer);
  }

  // add reference for the caller requesting the object
  NS_ADDREF(mScriptablePeer);
  return mScriptablePeer;
}
