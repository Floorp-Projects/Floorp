/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   Ben Turner <bent.mozilla@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
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

#include "mozilla/plugins/PluginModuleChild.h"

#ifdef OS_LINUX
#include <gtk/gtk.h>
#endif

#include "nsILocalFile.h"

#include "nsDebug.h"
#include "nsCOMPtr.h"
#include "nsPluginsDir.h"

#include "mozilla/plugins/PluginInstanceChild.h"
#include "mozilla/plugins/StreamNotifyChild.h"
#include "mozilla/plugins/BrowserStreamChild.h"

using mozilla::ipc::NPRemoteIdentifier;

using namespace mozilla::plugins;

namespace {
    PluginModuleChild* gInstance = nsnull;
}

PluginModuleChild::PluginModuleChild() :
    mLibrary(0),
    mInitializeFunc(0),
    mShutdownFunc(0)
#ifdef OS_WIN
  , mGetEntryPointsFunc(0)
#endif
//  ,mNextInstanceId(0)
{
    NS_ASSERTION(!gInstance, "Bad news bears!");
    memset(&mFunctions, 0, sizeof(mFunctions));
    memset(&mSavedData, 0, sizeof(mSavedData));
    gInstance = this;
}

PluginModuleChild::~PluginModuleChild()
{
    NS_ASSERTION(gInstance == this, "Bad news bears!");
    if (mLibrary) {
        PR_UnloadLibrary(mLibrary);
    }
    gInstance = nsnull;
}

bool
PluginModuleChild::Init(const std::string& aPluginFilename,
                        MessageLoop* aIOLoop,
                        IPC::Channel* aChannel)
{
    _MOZ_LOG(__FUNCTION__);

    NS_ASSERTION(aChannel, "need a channel");

    if (!InitGraphics())
        return false;

    nsCString filename;
    filename = aPluginFilename.c_str();
    nsCOMPtr<nsILocalFile> pluginFile;
    NS_NewNativeLocalFile(filename,
                          PR_TRUE,
                          getter_AddRefs(pluginFile));

    PRBool exists;
    pluginFile->Exists(&exists);
    NS_ASSERTION(exists, "plugin file ain't there");

    nsCOMPtr<nsIFile> pluginIfile;
    pluginIfile = do_QueryInterface(pluginFile);

    nsPluginFile lib(pluginIfile);

    nsresult rv = lib.LoadPlugin(mLibrary);
    NS_ASSERTION(NS_OK == rv, "trouble with mPluginFile");
    NS_ASSERTION(mLibrary, "couldn't open shared object");

    if (!Open(aChannel, aIOLoop))
        return false;

    memset((void*) &mFunctions, 0, sizeof(mFunctions));
    mFunctions.size = sizeof(mFunctions);


#if defined(OS_LINUX)
    mShutdownFunc =
        (NP_PLUGINSHUTDOWN) PR_FindFunctionSymbol(mLibrary, "NP_Shutdown");

    // create the new plugin handler

    mInitializeFunc =
        (NP_PLUGINUNIXINIT) PR_FindFunctionSymbol(mLibrary, "NP_Initialize");
    NS_ASSERTION(mInitializeFunc, "couldn't find NP_Initialize()");

#elif defined(OS_WIN)
    mShutdownFunc =
        (NP_PLUGINSHUTDOWN)PR_FindFunctionSymbol(mLibrary, "NP_Shutdown");

    mGetEntryPointsFunc =
        (NP_GETENTRYPOINTS)PR_FindSymbol(mLibrary, "NP_GetEntryPoints");
    NS_ENSURE_TRUE(mGetEntryPointsFunc, false);

    mInitializeFunc =
        (NP_PLUGININIT)PR_FindFunctionSymbol(mLibrary, "NP_Initialize");
    NS_ENSURE_TRUE(mInitializeFunc, false);
#else

#error Please copy the initialization code from nsNPAPIPlugin.cpp

#endif
    return true;
}

bool
PluginModuleChild::InitGraphics()
{
    // FIXME/cjones: is this the place for this?
#if defined(OS_LINUX)
    gtk_init(0, 0);
#else
    // may not be necessary on all platforms
#endif

    return true;
}

void
PluginModuleChild::CleanUp()
{
    // FIXME/cjones: destroy all instances
}


//-----------------------------------------------------------------------------
// FIXME/cjones: just getting this out of the way for the moment ...

// FIXME
typedef void (*PluginThreadCallback)(void*);

PR_BEGIN_EXTERN_C

static NPError NP_CALLBACK
_requestread(NPStream *pstream, NPByteRange *rangeList);

static NPError NP_CALLBACK
_geturlnotify(NPP aNPP, const char* relativeURL, const char* target,
              void* notifyData);

static NPError NP_CALLBACK
_getvalue(NPP aNPP, NPNVariable variable, void *r_value);

static NPError NP_CALLBACK
_setvalue(NPP aNPP, NPPVariable variable, void *r_value);

static NPError NP_CALLBACK
_geturl(NPP aNPP, const char* relativeURL, const char* target);

static NPError NP_CALLBACK
_posturlnotify(NPP aNPP, const char* relativeURL, const char *target,
               uint32_t len, const char *buf, NPBool file, void* notifyData);

static NPError NP_CALLBACK
_posturl(NPP aNPP, const char* relativeURL, const char *target, uint32_t len,
         const char *buf, NPBool file);

static NPError NP_CALLBACK
_newstream(NPP aNPP, NPMIMEType type, const char* window, NPStream** pstream);

static int32_t NP_CALLBACK
_write(NPP aNPP, NPStream *pstream, int32_t len, void *buffer);

static NPError NP_CALLBACK
_destroystream(NPP aNPP, NPStream *pstream, NPError reason);

static void NP_CALLBACK
_status(NPP aNPP, const char *message);

static void NP_CALLBACK
_memfree (void *ptr);

static uint32_t NP_CALLBACK
_memflush(uint32_t size);

static void NP_CALLBACK
_reloadplugins(NPBool reloadPages);

static void NP_CALLBACK
_invalidaterect(NPP aNPP, NPRect *invalidRect);

static void NP_CALLBACK
_invalidateregion(NPP aNPP, NPRegion invalidRegion);

static void NP_CALLBACK
_forceredraw(NPP aNPP);

static const char* NP_CALLBACK
_useragent(NPP aNPP);

static void* NP_CALLBACK
_memalloc (uint32_t size);

// Deprecated entry points for the old Java plugin.
static void* NP_CALLBACK /* OJI type: JRIEnv* */
_getjavaenv(void);

// Deprecated entry points for the old Java plugin.
static void* NP_CALLBACK /* OJI type: jref */
_getjavapeer(NPP aNPP);

static NPIdentifier NP_CALLBACK
_getstringidentifier(const NPUTF8* name);

static void NP_CALLBACK
_getstringidentifiers(const NPUTF8** names, int32_t nameCount,
                      NPIdentifier *identifiers);

static bool NP_CALLBACK
_identifierisstring(NPIdentifier identifiers);

static NPIdentifier NP_CALLBACK
_getintidentifier(int32_t intid);

static NPUTF8* NP_CALLBACK
_utf8fromidentifier(NPIdentifier identifier);

static int32_t NP_CALLBACK
_intfromidentifier(NPIdentifier identifier);

static NPObject* NP_CALLBACK
_createobject(NPP aNPP, NPClass* aClass);

static NPObject* NP_CALLBACK
_retainobject(NPObject* npobj);

static void NP_CALLBACK
_releaseobject(NPObject* npobj);

static bool NP_CALLBACK
_invoke(NPP aNPP, NPObject* npobj, NPIdentifier method, const NPVariant *args,
        uint32_t argCount, NPVariant *result);

static bool NP_CALLBACK
_invokedefault(NPP aNPP, NPObject* npobj, const NPVariant *args,
               uint32_t argCount, NPVariant *result);

static bool NP_CALLBACK
_evaluate(NPP aNPP, NPObject* npobj, NPString *script, NPVariant *result);

static bool NP_CALLBACK
_getproperty(NPP aNPP, NPObject* npobj, NPIdentifier property,
             NPVariant *result);

static bool NP_CALLBACK
_setproperty(NPP aNPP, NPObject* npobj, NPIdentifier property,
             const NPVariant *value);

static bool NP_CALLBACK
_removeproperty(NPP aNPP, NPObject* npobj, NPIdentifier property);

static bool NP_CALLBACK
_hasproperty(NPP aNPP, NPObject* npobj, NPIdentifier propertyName);

static bool NP_CALLBACK
_hasmethod(NPP aNPP, NPObject* npobj, NPIdentifier methodName);

static bool NP_CALLBACK
_enumerate(NPP aNPP, NPObject *npobj, NPIdentifier **identifier,
           uint32_t *count);

static bool NP_CALLBACK
_construct(NPP aNPP, NPObject* npobj, const NPVariant *args,
           uint32_t argCount, NPVariant *result);

#if NP_VERSION_MINOR > 19
static void NP_CALLBACK
_releasevariantvalue(NPVariant *variant);

static void NP_CALLBACK
_setexception(NPObject* npobj, const NPUTF8 *message);

static bool NP_CALLBACK
_pushpopupsenabledstate(NPP aNPP, NPBool enabled);

static bool NP_CALLBACK
_poppopupsenabledstate(NPP aNPP);

static void NP_CALLBACK
_pluginthreadasynccall(NPP instance, PluginThreadCallback func,
                       void *userData);

#endif /* NP_VERSION_MINOR > 19 */

PR_END_EXTERN_C

const NPNetscapeFuncs PluginModuleChild::sBrowserFuncs = {
    sizeof(sBrowserFuncs),
    (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR,
    _geturl,
    _posturl,
    _requestread,
    _newstream,
    _write,
    _destroystream,
    _status,
    _useragent,
    _memalloc,
    _memfree,
    _memflush,
    _reloadplugins,
    _getjavaenv,
    _getjavapeer,
    _geturlnotify,
    _posturlnotify,
    _getvalue,
    _setvalue,
    _invalidaterect,
    _invalidateregion,
    _forceredraw,
    _getstringidentifier,
    _getstringidentifiers,
    _getintidentifier,
    _identifierisstring,
    _utf8fromidentifier,
    _intfromidentifier,
    _createobject,
    _retainobject,
    _releaseobject,
    _invoke,
    _invokedefault,
    _evaluate,
    _getproperty,
    _setproperty,
    _removeproperty,
    _hasproperty,
    _hasmethod
#if NP_VERSION_MINOR > 19
    , _releasevariantvalue
    , _setexception
    , _pushpopupsenabledstate
    , _poppopupsenabledstate
    , _enumerate
    , _pluginthreadasynccall
    , _construct
#endif
};

PluginInstanceChild*
InstCast(NPP aNPP)
{
    return static_cast<PluginInstanceChild*>(aNPP->ndata);
}

NPError NP_CALLBACK
_requestread(NPStream* aSstream,
             NPByteRange* aRangeList)
{
    _MOZ_LOG(__FUNCTION__);
    return NPERR_NO_ERROR;
}

NPError NP_CALLBACK
_geturlnotify(NPP aNPP,
              const char* aRelativeURL,
              const char* aTarget,
              void* aNotifyData)
{
    _MOZ_LOG(__FUNCTION__);

    const nsDependentCString url(aRelativeURL);
    NPError err;
    InstCast(aNPP)->CallPStreamNotifyConstructor(
        new StreamNotifyChild(url, aNotifyData),
        url, nsDependentCString(aTarget), false, nsCString(), false, &err);
    // TODO: what if this fails?
    return err;
}

NPError NP_CALLBACK
_getvalue(NPP aNPP,
          NPNVariable aVariable,
          void* aValue)
{
    _MOZ_LOG(__FUNCTION__);
    return InstCast(aNPP)->NPN_GetValue(aVariable, aValue);
}


NPError NP_CALLBACK
_setvalue(NPP aNPP,
          NPPVariable aVariable,
          void* aValue)
{
    _MOZ_LOG(__FUNCTION__);
    return NPERR_NO_ERROR;
}

NPError NP_CALLBACK
_geturl(NPP aNPP,
        const char* aRelativeURL,
        const char* aTarget)
{
    _MOZ_LOG(__FUNCTION__);
    NPError err;
    InstCast(aNPP)->CallNPN_GetURL(nsDependentCString(aRelativeURL),
                                   nsDependentCString(aTarget), &err);
    return err;
}

NPError NP_CALLBACK
_posturlnotify(NPP aNPP,
               const char* aRelativeURL,
               const char* aTarget,
               uint32_t aLength,
               const char* aBuffer,
               NPBool aIsFile,
               void* aNotifyData)
{
    _MOZ_LOG(__FUNCTION__);

    const nsDependentCString url(aRelativeURL);
    NPError err;
    InstCast(aNPP)->CallPStreamNotifyConstructor(
        new StreamNotifyChild(url, aNotifyData),
        url, nsDependentCString(aTarget), true,
        nsDependentCString(aBuffer, aLength), aIsFile, &err);
    // TODO: what if this fails?
    return err;
}

NPError NP_CALLBACK
_posturl(NPP aNPP,
         const char* aRelativeURL,
         const char* aTarget,
         uint32_t aLength,
         const char* aBuffer,
         NPBool aIsFile)
{
    _MOZ_LOG(__FUNCTION__);
    NPError err;
    InstCast(aNPP)->CallNPN_PostURL(nsDependentCString(aRelativeURL),
                                    nsDependentCString(aTarget),
                                    nsDependentCString(aBuffer, aLength),
                                    aIsFile, &err);
    return err;
}

NPError NP_CALLBACK
_newstream(NPP aNPP,
           NPMIMEType aMIMEType,
           const char* aWindow,
           NPStream** aStream)
{
    _MOZ_LOG(__FUNCTION__);
    return NPERR_NO_ERROR;
}

int32_t NP_CALLBACK
_write(NPP aNPP,
       NPStream* aStream,
       int32_t aLength,
       void* aBuffer)
{
    _MOZ_LOG(__FUNCTION__);
    return 0;
}

NPError NP_CALLBACK
_destroystream(NPP aNPP,
               NPStream* aStream,
               NPError aReason)
{
    _MOZ_LOG(__FUNCTION__);
    BrowserStreamChild* bs = static_cast<BrowserStreamChild*>(aStream->ndata);
    PluginInstanceChild* p = InstCast(aNPP);
    bs->EnsureCorrectInstance(p);

    p->CallPBrowserStreamDestructor(bs, aReason, false);
    return NPERR_NO_ERROR;
}

void NP_CALLBACK
_status(NPP aNPP,
        const char* aMessage)
{
    _MOZ_LOG(__FUNCTION__);
}

void NP_CALLBACK
_memfree(void* aPtr)
{
    _MOZ_LOG(__FUNCTION__);
    NS_Free(aPtr);
}

uint32_t NP_CALLBACK
_memflush(uint32_t aSize)
{
    _MOZ_LOG(__FUNCTION__);
    return 0;
}

void NP_CALLBACK
_reloadplugins(NPBool aReloadPages)
{
    _MOZ_LOG(__FUNCTION__);
}

void NP_CALLBACK
_invalidaterect(NPP aNPP,
                NPRect* aInvalidRect)
{
    _MOZ_LOG(__FUNCTION__);
}

void NP_CALLBACK
_invalidateregion(NPP aNPP,
                  NPRegion aInvalidRegion)
{
    _MOZ_LOG(__FUNCTION__);
}

void NP_CALLBACK
_forceredraw(NPP aNPP)
{
    _MOZ_LOG(__FUNCTION__);
}

const char* NP_CALLBACK
_useragent(NPP aNPP)
{
    _MOZ_LOG(__FUNCTION__);

    // FIXME/cjones: go back to the parent for this

    return "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9.2a1pre) Gecko/20090611 Minefield/3.6a1pre";
}

void* NP_CALLBACK
_memalloc(uint32_t aSize)
{
    _MOZ_LOG(__FUNCTION__);
    return NS_Alloc(aSize);
}

// Deprecated entry points for the old Java plugin.
void* NP_CALLBACK /* OJI type: JRIEnv* */
_getjavaenv(void)
{
    _MOZ_LOG(__FUNCTION__);
    return 0;
}

void* NP_CALLBACK /* OJI type: jref */
_getjavapeer(NPP aNPP)
{
    _MOZ_LOG(__FUNCTION__);
    return 0;
}

NPIdentifier NP_CALLBACK
_getstringidentifier(const NPUTF8* aName)
{
    _MOZ_LOG(__FUNCTION__);
    NS_ASSERTION(gInstance, "No instance!");

    NPRemoteIdentifier ident;
    nsresult rv =
        gInstance->SendNPN_GetStringIdentifier(nsDependentCString(aName),
                                               &ident);
    NS_ENSURE_SUCCESS(rv, 0);

    return (NPIdentifier)ident;
}

void NP_CALLBACK
_getstringidentifiers(const NPUTF8** aNames,
                      int32_t aNameCount,
                      NPIdentifier* aIdentifiers)
{
    _MOZ_LOG(__FUNCTION__);
    NS_ASSERTION(gInstance, "No instance!");

    if (!(aNames && aNameCount > 0 && aIdentifiers)) {
        NS_RUNTIMEABORT("Bad input! Headed for a crash!");
    }

    nsAutoTArray<nsCString, 10> names;
    nsAutoTArray<NPRemoteIdentifier, 10> ids;

    PRBool ok = names.SetCapacity(aNameCount);
    NS_WARN_IF_FALSE(ok, "Out of memory!");

    if (ok) {
        for (int32_t index = 0; index < aNameCount; index++) {
            names.AppendElement(nsDependentCString(aNames[index]));
        }
        NS_ASSERTION(names.Length() == aNameCount, "Should equal here!");

        nsresult rv = gInstance->SendNPN_GetStringIdentifiers(names, &ids);
        NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to send message!");

        if (NS_SUCCEEDED(rv)) {
            NS_ASSERTION(ids.Length() == aNameCount, "Bad length!");

            for (int32_t index = 0; index < aNameCount; index++) {
                aIdentifiers[index] = (NPIdentifier)ids[index];
            }
            return;
        }
    }

    // Something must have failed above.
    for (int32_t index = 0; index < aNameCount; index++) {
        aIdentifiers[index] = 0;
    }
}

bool NP_CALLBACK
_identifierisstring(NPIdentifier aIdentifier)
{
    _MOZ_LOG(__FUNCTION__);
    NS_ASSERTION(gInstance, "No instance!");

    bool isString;
    nsresult rv =
        gInstance->SendNPN_IdentifierIsString((NPRemoteIdentifier)aIdentifier,
                                              &isString);
    NS_ENSURE_SUCCESS(rv, false);

    return isString;
}

NPIdentifier NP_CALLBACK
_getintidentifier(int32_t aIntId)
{
    _MOZ_LOG(__FUNCTION__);
    NS_ASSERTION(gInstance, "No instance!");

    NPRemoteIdentifier ident;
    nsresult rv = gInstance->SendNPN_GetIntIdentifier(aIntId, &ident);
    NS_ENSURE_SUCCESS(rv, 0);

    return (NPIdentifier)ident;
}

NPUTF8* NP_CALLBACK
_utf8fromidentifier(NPIdentifier aIdentifier)
{
    _MOZ_LOG(__FUNCTION__);
    NS_ASSERTION(gInstance, "No instance!");

    nsCAutoString val;
    nsresult rv =
        gInstance->SendNPN_UTF8FromIdentifier((NPRemoteIdentifier)aIdentifier,
                                              &val);
    NS_ENSURE_SUCCESS(rv, 0);

    return val.IsVoid() ? 0 : strdup(val.get());
}

int32_t NP_CALLBACK
_intfromidentifier(NPIdentifier aIdentifier)
{
    _MOZ_LOG(__FUNCTION__);
    NS_ASSERTION(gInstance, "No instance!");

    int32_t val;
    nsresult rv =
        gInstance->SendNPN_IntFromIdentifier((NPRemoteIdentifier)aIdentifier,
                                             &val);
    NS_ENSURE_SUCCESS(rv, 0);

    return val;
}

NPObject* NP_CALLBACK
_createobject(NPP aNPP,
              NPClass* aClass)
{
    _MOZ_LOG(__FUNCTION__);
#if 0
    NS_ENSURE_TRUE(aNPP, 0);
    NS_ENSURE_TRUE(aClass, 0);

    NPObject* obj = sDelegate->GetNewScriptableObject(aNPP, aClass);
    if (!obj) {
        return 0;
    }

    int classId = sDelegate->GetClassId(aClass);
    PluginModuleChild::Instance* instance =
        static_cast<PluginModuleChild::Instance*>(aNPP);

    int objectId = -1;
    sDelegate->Send(new PluginHostMsg_MozCreateObject(
                        instance->GetId(), classId, &objectId));
    if (objectId == -1) {
        return 0;
    }

    PluginModuleChild::ScriptableObjectInfo& info =
        sDelegate->GetScriptableObjectInfo(obj);
    DCHECK(info.object = obj);
    info.id = objectId;
    info.instanceId = instance->GetId();

    return obj;
#endif
    return 0;
}

NPObject* NP_CALLBACK
_retainobject(NPObject* aNPObj)
{
    _MOZ_LOG(__FUNCTION__);
#if 0
    PluginModuleChild::ScriptableObjectInfo& info =
        sDelegate->GetScriptableObjectInfo(aNPObj);
    DCHECK(info.object == aNPObj);
    sDelegate->Send(new PluginHostMsg_MozRetainObject(info.instanceId, info.id));
    aNPObj->referenceCount++;
    return aNPObj;
#endif

    return 0;
}

void NP_CALLBACK
_releaseobject(NPObject* aNPObj)
{
    _MOZ_LOG(__FUNCTION__);
#if 0
    PluginModuleChild::ScriptableObjectInfo& info =
        sDelegate->GetScriptableObjectInfo(aNPObj);
    DCHECK(info.object == aNPObj);
    sDelegate->Send(new PluginHostMsg_MozReleaseObject(info.instanceId, info.id));

    if (--aNPObj->referenceCount == 0) {
        sDelegate->EraseScriptableObjectInfo(aNPObj);
        if (aNPObj->_class && aNPObj->_class->deallocate) {
            aNPObj->_class->deallocate(aNPObj);
        } else {
            sBrowserFunctions.memfree(aNPObj);
        }
    }
#endif
    return;
}

bool NP_CALLBACK
_invoke(NPP aNPP,
        NPObject* aNPObj,
        NPIdentifier aMethod,
        const NPVariant* aArgs,
        uint32_t aArgCount,
        NPVariant* aResult)
{
    _MOZ_LOG(__FUNCTION__);
    return false;
}

bool NP_CALLBACK
_invokedefault(NPP aNPP,
               NPObject* aNPObj,
               const NPVariant* aArgs,
               uint32_t aArgCount,
               NPVariant* aResult)
{
    _MOZ_LOG(__FUNCTION__);
    return false;
}

bool NP_CALLBACK
_evaluate(NPP aNPP,
          NPObject* aNPObj,
          NPString* aScript,
          NPVariant* aResult)
{
    _MOZ_LOG(__FUNCTION__);
    return false;
}

bool NP_CALLBACK
_getproperty(NPP aNPP,
             NPObject* aNPObj,
             NPIdentifier aPropertyName,
             NPVariant* aResult)
{
    _MOZ_LOG(__FUNCTION__);
    return false;
}

bool NP_CALLBACK
_setproperty(NPP aNPP,
             NPObject* aNPObj,
             NPIdentifier aPropertyName,
             const NPVariant* aValue)
{
    _MOZ_LOG(__FUNCTION__);
    return false;
}

bool NP_CALLBACK
_removeproperty(NPP aNPP,
                NPObject* aNPObj,
                NPIdentifier aPropertyName)
{
    _MOZ_LOG(__FUNCTION__);
    return false;
}

bool NP_CALLBACK
_hasproperty(NPP aNPP,
             NPObject* aNPObj,
             NPIdentifier aPropertyName)
{
    _MOZ_LOG(__FUNCTION__);
    return false;
}

bool NP_CALLBACK
_hasmethod(NPP aNPP,
           NPObject* aNPObj,
           NPIdentifier aMethodName)
{
    _MOZ_LOG(__FUNCTION__);
    return false;
}

bool NP_CALLBACK
_enumerate(NPP aNPP,
           NPObject* aNPObj,
           NPIdentifier** aIdentifiers,
           uint32_t* aCount)
{
    _MOZ_LOG(__FUNCTION__);
    return false;
}

bool NP_CALLBACK
_construct(NPP aNPP,
           NPObject* aNPObj,
           const NPVariant* aArgs,
           uint32_t aArgCount,
           NPVariant* aResult)
{
    _MOZ_LOG(__FUNCTION__);
    return false;
}

#if NP_VERSION_MINOR > 19
void NP_CALLBACK
_releasevariantvalue(NPVariant* aVariant)
{
    _MOZ_LOG(__FUNCTION__);
}

void NP_CALLBACK
_setexception(NPObject* aNPObj,
              const NPUTF8* aMessage)
{
    _MOZ_LOG(__FUNCTION__);
}

bool NP_CALLBACK
_pushpopupsenabledstate(NPP aNPP,
                        NPBool aEnabled)
{
    _MOZ_LOG(__FUNCTION__);
    return false;
}

bool NP_CALLBACK
_poppopupsenabledstate(NPP aNPP)
{
    _MOZ_LOG(__FUNCTION__);
    return false;
}

void NP_CALLBACK
_pluginthreadasynccall(NPP aNPP,
                       PluginThreadCallback aFunc,
                       void* aUserData)
{
    _MOZ_LOG(__FUNCTION__);
}

#endif /* NP_VERSION_MINOR > 19 */

nsresult
PluginModuleChild::AnswerNP_Initialize(NPError* _retval)
{
    _MOZ_LOG(__FUNCTION__);

#if defined(OS_LINUX)
    *_retval = mInitializeFunc(&sBrowserFuncs, &mFunctions);
    return NS_OK;

#elif defined(OS_WIN)
    nsresult rv = mGetEntryPointsFunc(&mFunctions);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ASSERTION(HIBYTE(mFunctions.version) >= NP_VERSION_MAJOR,
                 "callback version is less than NP version");

    *_retval = mInitializeFunc(&sBrowserFuncs);
    return NS_OK;
#else
#  error Please implement me for your platform
#endif
}

PPluginInstanceChild*
PluginModuleChild::PPluginInstanceConstructor(const nsCString& aMimeType,
                                              const uint16_t& aMode,
                                              const nsTArray<nsCString>& aNames,
                                              const nsTArray<nsCString>& aValues,
                                              NPError* rv)
{
    _MOZ_LOG(__FUNCTION__);

    // create our wrapper instance
    nsAutoPtr<PluginInstanceChild> childInstance(
        new PluginInstanceChild(&mFunctions));
    if (!childInstance->Initialize()) {
        *rv = NPERR_GENERIC_ERROR;
        return 0;
    }

    // unpack the arguments into a C format
    int argc = aNames.Length();
    NS_ASSERTION(argc == (int) aValues.Length(),
                 "argn.length != argv.length");

    nsAutoArrayPtr<char*> argn(new char*[1 + argc]);
    nsAutoArrayPtr<char*> argv(new char*[1 + argc]);
    argn[argc] = 0;
    argv[argc] = 0;

    printf ("(plugin args: ");
    for (int i = 0; i < argc; ++i) {
        argn[i] = const_cast<char*>(aNames[i].get());
        argv[i] = const_cast<char*>(aValues[i].get());
        printf("%s=%s, ", argn[i], argv[i]);
    }
    printf(")\n");

    NPP npp = childInstance->GetNPP();

    // FIXME/cjones: use SAFE_CALL stuff
    *rv = mFunctions.newp((char*)aMimeType.get(),
                          npp,
                          aMode,
                          argc,
                          argn,
                          argv,
                          0);
    if (NPERR_NO_ERROR != *rv) {
        return nsnull;
    }

    printf ("[PluginModuleChild] %s: returning %hd\n", __FUNCTION__, *rv);
    return childInstance.forget();
}

nsresult
PluginModuleChild::PPluginInstanceDestructor(PPluginInstanceChild* actor,
                                             NPError* rv)
{
    _MOZ_LOG(__FUNCTION__);

    PluginInstanceChild* inst = static_cast<PluginInstanceChild*>(actor);
    *rv = mFunctions.destroy(inst->GetNPP(), 0);
    delete actor;
    inst->GetNPP()->ndata = 0;

    return NS_OK;
}
