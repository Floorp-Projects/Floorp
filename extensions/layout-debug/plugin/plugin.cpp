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

#include "plugin.h"
#include "nsIServiceManager.h"
#include "nsIMemory.h"
#include "nsIDebugObject.h"
#include "nsXPIDLString.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsISimpleEnumerator.h"
#include "nsMemory.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsIPref.h"

// service manager which will give the access to all public browser services
// we will use memory service as an illustration
nsIServiceManager * gServiceManager = NULL;

//static NS_DEFINE_IID(kMemoryCID, NS_MEMORY_CID);

// Unix needs this
#ifdef XP_UNIX
#define MIME_TYPES_HANDLED  "application/debug-plugin"
#define PLUGIN_NAME         "Layout Debug Plugin"
#define PLUGIN_DESCRIPTION  "Layout Debug Plugin"

char* NPP_GetMIMEDescription(void)
{
    return(MIME_TYPES_HANDLED);
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
  
  NPN_GetValue(NULL, NPNVserviceManager, &sm);

  // Mozilla returns nsIServiceManagerObsolete which can be queried for nsIServiceManager
  if(sm) {
    sm->QueryInterface(NS_GET_IID(nsIServiceManager), (void**)&gServiceManager);
    NS_RELEASE(sm);
  }
  
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
  if(!aCreateDataStruct)
    return NULL;

  nsPluginInstance * plugin = new nsPluginInstance(aCreateDataStruct->instance);
  if (plugin) {
    plugin->init(nsnull);
  }
  return plugin;
}

void NS_DestroyPluginInstance(nsPluginInstanceBase * aPlugin)
{
  if(aPlugin)
    delete aPlugin;
}

////////////////////////////////////////
//
// nsPluginInstance class implementation
//
nsPluginInstance::nsPluginInstance(NPP aInstance) : nsPluginInstanceBase(),
  mInstance(aInstance),
  mInitialized(FALSE),
  mScriptablePeer(NULL)
{

}

nsPluginInstance::~nsPluginInstance()
{
  NS_IF_RELEASE(mScriptablePeer);
}

NPBool nsPluginInstance::init(NPWindow* aWindow)
{

  if(aWindow == NULL)
    return FALSE;

  mInitialized = TRUE;

  return TRUE;
}

void nsPluginInstance::shut()
{
  mScriptablePeer->PluginShutdown();

  mInitialized = FALSE;
}

NPBool nsPluginInstance::isInitialized()
{
  return mInitialized;
}


//-----------------------------------------------------


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
      nsIDebugPlugin * scriptablePeer = getScriptablePeer();
      if (scriptablePeer) {
        *(nsISupports **)aValue = scriptablePeer;
      } else
        rv = NPERR_OUT_OF_MEMORY_ERROR;
    }
    break;

    case NPPVpluginScriptableIID: {
      static nsIID scriptableIID = NS_IDEBUGPLUGIN_IID;
      nsIID* ptr = (nsIID *)NPN_MemAlloc(sizeof(nsIID));
      if (ptr) {
          *ptr = scriptableIID;
          *(nsIID **)aValue = ptr;
      } else
        rv = NPERR_OUT_OF_MEMORY_ERROR;
    }
    break;

#ifdef XP_UNIX
    // Unix needs some additional cases
    case NPPVpluginNameString:
      *((char **)aValue) = PLUGIN_NAME;
      break;
    case NPPVpluginDescriptionString:
      *((char **)aValue) = PLUGIN_DESCRIPTION;
      break;
#endif //XP_UNIX

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
nsIDebugPlugin* nsPluginInstance::getScriptablePeer()
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

  return NULL;
}


