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
#include "mozilla/plugins/PluginStreamChild.h"

using mozilla::ipc::NPRemoteIdentifier;

using namespace mozilla::plugins;

namespace {
PluginModuleChild* gInstance = nsnull;
// NPN_UserAgent returns a |const char*| that must outlive its stack frame
nsCString gUserAgent;
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
    NS_ASSERTION(!gInstance, "Something terribly wrong here!");
    memset(&mFunctions, 0, sizeof(mFunctions));
    memset(&mSavedData, 0, sizeof(mSavedData));
    gInstance = this;
}

PluginModuleChild::~PluginModuleChild()
{
    NS_ASSERTION(gInstance == this, "Something terribly wrong here!");
    if (mLibrary) {
        PR_UnloadLibrary(mLibrary);
    }
    gInstance = nsnull;
}

// static
PluginModuleChild*
PluginModuleChild::current()
{
    NS_ASSERTION(gInstance, "Null instance!");
    return gInstance;
}

bool
PluginModuleChild::Init(const std::string& aPluginFilename,
                        base::ProcessHandle aParentProcessHandle,
                        MessageLoop* aIOLoop,
                        IPC::Channel* aChannel)
{
    _MOZ_LOG(__FUNCTION__);

    NS_ASSERTION(aChannel, "need a channel");

    if (!mObjectMap.Init()) {
       NS_WARNING("Failed to initialize object hashtable!");
       return false;
    }

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

    if (!Open(aChannel, aParentProcessHandle, aIOLoop))
        return false;

    memset((void*) &mFunctions, 0, sizeof(mFunctions));
    mFunctions.size = sizeof(mFunctions);

    // TODO: use PluginPRLibrary here

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
#elif defined(OS_MACOSX)
#  warning IMPLEMENT ME

#else

#  error Please copy the initialization code from nsNPAPIPlugin.cpp

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

bool
PluginModuleChild::AnswerNP_Shutdown(NPError *rv)
{
    AssertPluginThread();

    // FIXME/cjones: should all instances be dead by now?

    *rv = mShutdownFunc ? mShutdownFunc() : NPERR_NO_ERROR;
    return true;
}

void
PluginModuleChild::CleanUp()
{
}

bool
PluginModuleChild::RegisterNPObject(NPObject* aObject,
                                    PluginScriptableObjectChild* aActor)
{
    AssertPluginThread();
    NS_ASSERTION(mObjectMap.IsInitialized(), "Not initialized!");
    NS_ASSERTION(aObject && aActor, "Null pointer!");
    NS_ASSERTION(!mObjectMap.Get(aObject, nsnull),
                 "Reregistering the same object!");
    return !!mObjectMap.Put(aObject, aActor);
}

void
PluginModuleChild::UnregisterNPObject(NPObject* aObject)
{
    AssertPluginThread();
    NS_ASSERTION(mObjectMap.IsInitialized(), "Not initialized!");
    NS_ASSERTION(aObject, "Null pointer!");
    NS_ASSERTION(mObjectMap.Get(aObject, nsnull),
                 "Unregistering an object that was never added!");
    mObjectMap.Remove(aObject);
}

PluginScriptableObjectChild*
PluginModuleChild::GetActorForNPObject(NPObject* aObject)
{
    AssertPluginThread();
    NS_ASSERTION(mObjectMap.IsInitialized(), "Not initialized!");
    NS_ASSERTION(aObject, "Null pointer!");
    PluginScriptableObjectChild* actor;
    return mObjectMap.Get(aObject, &actor) ? actor : nsnull;
}

#ifdef DEBUG
bool
PluginModuleChild::NPObjectIsRegistered(NPObject* aObject)
{
    AssertPluginThread();
    NS_ASSERTION(mObjectMap.IsInitialized(), "Not initialized!");
    NS_ASSERTION(aObject, "Null pointer!");
    return mObjectMap.Get(aObject, nsnull);
}
#endif

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

static NPError NP_CALLBACK
_getvalueforurl(NPP npp, NPNURLVariable variable, const char *url,
                char **value, uint32_t *len);

static NPError NP_CALLBACK
_setvalueforurl(NPP npp, NPNURLVariable variable, const char *url,
                const char *value, uint32_t len);

static NPError NP_CALLBACK
_getauthenticationinfo(NPP npp, const char *protocol,
                       const char *host, int32_t port,
                       const char *scheme, const char *realm,
                       char **username, uint32_t *ulen,
                       char **password, uint32_t *plen);

static uint32_t NP_CALLBACK
_scheduletimer(NPP instance, uint32_t interval, NPBool repeat,
               void (*timerFunc)(NPP npp, uint32_t timerID));

static void NP_CALLBACK
_unscheduletimer(NPP instance, uint32_t timerID);

static NPError NP_CALLBACK
_popupcontextmenu(NPP instance, NPMenu* menu);

static NPBool NP_CALLBACK
_convertpoint(NPP instance, 
              double sourceX, double sourceY, NPCoordinateSpace sourceSpace,
              double *destX, double *destY, NPCoordinateSpace destSpace);

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
    _hasmethod,
    _releasevariantvalue,
    _setexception,
    _pushpopupsenabledstate,
    _poppopupsenabledstate,
    _enumerate,
    _pluginthreadasynccall,
    _construct,
    _getvalueforurl,
    _setvalueforurl,
    _getauthenticationinfo,
    _scheduletimer,
    _unscheduletimer,
    _popupcontextmenu,
    _convertpoint
};

PluginInstanceChild*
InstCast(NPP aNPP)
{
    NS_ABORT_IF_FALSE(!!(aNPP->ndata), "nil instance");
    return static_cast<PluginInstanceChild*>(aNPP->ndata);
}

NPError NP_CALLBACK
_requestread(NPStream* aStream,
             NPByteRange* aRangeList)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    BrowserStreamChild* bs =
        static_cast<BrowserStreamChild*>(static_cast<AStream*>(aStream->ndata));
    bs->EnsureCorrectStream(aStream);
    return bs->NPN_RequestRead(aRangeList);
}

NPError NP_CALLBACK
_geturlnotify(NPP aNPP,
              const char* aRelativeURL,
              const char* aTarget,
              void* aNotifyData)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    nsCString url = NullableString(aRelativeURL);
    NPError err;
    InstCast(aNPP)->CallPStreamNotifyConstructor(
        new StreamNotifyChild(url, aNotifyData),
        url, NullableString(aTarget), false, nsCString(), false, &err);
    // TODO: what if this fails?
    return err;
}

NPError NP_CALLBACK
_getvalue(NPP aNPP,
          NPNVariable aVariable,
          void* aValue)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
    return InstCast(aNPP)->NPN_GetValue(aVariable, aValue);
}


NPError NP_CALLBACK
_setvalue(NPP aNPP,
          NPPVariable aVariable,
          void* aValue)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
    return InstCast(aNPP)->NPN_SetValue(aVariable, aValue);
}

NPError NP_CALLBACK
_geturl(NPP aNPP,
        const char* aRelativeURL,
        const char* aTarget)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    NPError err;
    InstCast(aNPP)->CallNPN_GetURL(NullableString(aRelativeURL),
                                   NullableString(aTarget), &err);
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
    AssertPluginThread();

    nsCString url = NullableString(aRelativeURL);
    NPError err;

    // FIXME what should happen when |aBuffer| is null?

    // FIXME bad hack around using nsDependentCString when we really
    // want a real byte buffer
    nsAutoPtr<char> bufferCopy(new char[aLength+1]);
    memcpy(bufferCopy, aBuffer, aLength);
    bufferCopy[aLength] = 0;

    InstCast(aNPP)->CallPStreamNotifyConstructor(
        new StreamNotifyChild(url, aNotifyData),
        url, NullableString(aTarget), true,
        nsDependentCString(bufferCopy, aLength), aIsFile, &err);
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
    AssertPluginThread();

    NPError err;
    // FIXME what should happen when |aBuffer| is null?
    InstCast(aNPP)->CallNPN_PostURL(NullableString(aRelativeURL),
                                    NullableString(aTarget),
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
    AssertPluginThread();
    return InstCast(aNPP)->NPN_NewStream(aMIMEType, aWindow, aStream);
}

int32_t NP_CALLBACK
_write(NPP aNPP,
       NPStream* aStream,
       int32_t aLength,
       void* aBuffer)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    PluginStreamChild* ps =
        static_cast<PluginStreamChild*>(static_cast<AStream*>(aStream->ndata));
    ps->EnsureCorrectInstance(InstCast(aNPP));
    ps->EnsureCorrectStream(aStream);
    return ps->NPN_Write(aLength, aBuffer);
}

NPError NP_CALLBACK
_destroystream(NPP aNPP,
               NPStream* aStream,
               NPError aReason)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    PluginInstanceChild* p = InstCast(aNPP);
    AStream* s = static_cast<AStream*>(aStream->ndata);
    if (s->IsBrowserStream()) {
        BrowserStreamChild* bs = static_cast<BrowserStreamChild*>(s);
        bs->EnsureCorrectInstance(p);
        p->CallPBrowserStreamDestructor(bs, aReason, false);
    }
    else {
        PluginStreamChild* ps = static_cast<PluginStreamChild*>(s);
        ps->EnsureCorrectInstance(p);
        p->CallPPluginStreamDestructor(ps, aReason, false);
    }
    return NPERR_NO_ERROR;
}

void NP_CALLBACK
_status(NPP aNPP,
        const char* aMessage)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
}

void NP_CALLBACK
_memfree(void* aPtr)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
    NS_Free(aPtr);
}

uint32_t NP_CALLBACK
_memflush(uint32_t aSize)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
    return 0;
}

void NP_CALLBACK
_reloadplugins(NPBool aReloadPages)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
}

void NP_CALLBACK
_invalidaterect(NPP aNPP,
                NPRect* aInvalidRect)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
    InstCast(aNPP)->SendNPN_InvalidateRect(*aInvalidRect);
}

void NP_CALLBACK
_invalidateregion(NPP aNPP,
                  NPRegion aInvalidRegion)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
    // Not implemented in Mozilla.
}

void NP_CALLBACK
_forceredraw(NPP aNPP)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
}

const char* NP_CALLBACK
_useragent(NPP aNPP)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    PluginModuleChild::current()->CallNPN_UserAgent(&gUserAgent);
    return NullableStringGet(gUserAgent);
}

void* NP_CALLBACK
_memalloc(uint32_t aSize)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
    return NS_Alloc(aSize);
}

// Deprecated entry points for the old Java plugin.
void* NP_CALLBACK /* OJI type: JRIEnv* */
_getjavaenv(void)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
    return 0;
}

void* NP_CALLBACK /* OJI type: jref */
_getjavapeer(NPP aNPP)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
    return 0;
}

NPIdentifier NP_CALLBACK
_getstringidentifier(const NPUTF8* aName)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    NPRemoteIdentifier ident;
    if (!PluginModuleChild::current()->
             SendNPN_GetStringIdentifier(nsDependentCString(aName), &ident)) {
        NS_WARNING("Failed to send message!");
        ident = 0;
    }

    return (NPIdentifier)ident;
}

void NP_CALLBACK
_getstringidentifiers(const NPUTF8** aNames,
                      int32_t aNameCount,
                      NPIdentifier* aIdentifiers)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    if (!(aNames && aNameCount > 0 && aIdentifiers)) {
        NS_RUNTIMEABORT("Bad input! Headed for a crash!");
    }

    nsAutoTArray<nsCString, 10> names;
    nsAutoTArray<NPRemoteIdentifier, 10> ids;

    if (names.SetCapacity(aNameCount)) {
        for (int32_t index = 0; index < aNameCount; index++) {
            names.AppendElement(nsDependentCString(aNames[index]));
        }
        NS_ASSERTION(int32_t(names.Length()) == aNameCount,
                     "Should equal here!");

        if (PluginModuleChild::current()->
                SendNPN_GetStringIdentifiers(names, &ids)) {
            NS_ASSERTION(int32_t(ids.Length()) == aNameCount, "Bad length!");

            for (int32_t index = 0; index < aNameCount; index++) {
                aIdentifiers[index] = (NPIdentifier)ids[index];
            }
            return;
        }
        NS_WARNING("Failed to send message!");
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
    AssertPluginThread();

    bool isString;
    if (!PluginModuleChild::current()->
             SendNPN_IdentifierIsString((NPRemoteIdentifier)aIdentifier,
                                        &isString)) {
        NS_WARNING("Failed to send message!");
        isString = false;
    }

    return isString;
}

NPIdentifier NP_CALLBACK
_getintidentifier(int32_t aIntId)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    NPRemoteIdentifier ident;
    if (!PluginModuleChild::current()->
             SendNPN_GetIntIdentifier(aIntId, &ident)) {
        NS_WARNING("Failed to send message!");
        ident = 0;
    }

    return (NPIdentifier)ident;
}

NPUTF8* NP_CALLBACK
_utf8fromidentifier(NPIdentifier aIdentifier)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    NPError err;
    nsCAutoString val;
    if (!PluginModuleChild::current()->
             SendNPN_UTF8FromIdentifier((NPRemoteIdentifier)aIdentifier,
                                         &err, &val)) {
        NS_WARNING("Failed to send message!");
        return 0;
    }

    return (NPERR_NO_ERROR == err) ? strdup(val.get()) : 0;
}

int32_t NP_CALLBACK
_intfromidentifier(NPIdentifier aIdentifier)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    NPError err;
    int32_t val;
    if (!PluginModuleChild::current()->
             SendNPN_IntFromIdentifier((NPRemoteIdentifier)aIdentifier,
                                       &err, &val)) {
        NS_WARNING("Failed to send message!");
        return -1;
    }

    // -1 for consistency
    return (NPERR_NO_ERROR == err) ? val : -1;
}

NPObject* NP_CALLBACK
_createobject(NPP aNPP,
              NPClass* aClass)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    NPObject* newObject;
    if (aClass && aClass->allocate) {
        newObject = aClass->allocate(aNPP, aClass);
    }
    else {
        newObject = reinterpret_cast<NPObject*>(_memalloc(sizeof(NPObject)));
    }

    if (newObject) {
        newObject->_class = aClass;
        newObject->referenceCount = 1;
    }
    return newObject;
}

NPObject* NP_CALLBACK
_retainobject(NPObject* aNPObj)
{
    AssertPluginThread();

#ifdef DEBUG
    printf("[PluginModuleChild] %s: object %p, refcnt %d\n", __FUNCTION__,
           aNPObj, aNPObj->referenceCount + 1);
#endif
    ++aNPObj->referenceCount;
    return aNPObj;
}

void NP_CALLBACK
_releaseobject(NPObject* aNPObj)
{
    AssertPluginThread();

#ifdef DEBUG
    printf("[PluginModuleChild] %s: object %p, refcnt %d\n", __FUNCTION__,
           aNPObj, aNPObj->referenceCount - 1);
#endif
    if (--aNPObj->referenceCount == 0) {
        if (aNPObj->_class && aNPObj->_class->deallocate) {
            aNPObj->_class->deallocate(aNPObj);
        } else {
            _memfree(aNPObj);
        }
    }
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
    AssertPluginThread();

    if (!aNPP || !aNPObj || !aNPObj->_class || !aNPObj->_class->invoke)
        return false;

    return aNPObj->_class->invoke(aNPObj, aMethod, aArgs, aArgCount, aResult);
}

bool NP_CALLBACK
_invokedefault(NPP aNPP,
               NPObject* aNPObj,
               const NPVariant* aArgs,
               uint32_t aArgCount,
               NPVariant* aResult)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    if (!aNPP || !aNPObj || !aNPObj->_class || !aNPObj->_class->invokeDefault)
        return false;

    return aNPObj->_class->invokeDefault(aNPObj, aArgs, aArgCount, aResult);
}

bool NP_CALLBACK
_evaluate(NPP aNPP,
          NPObject* aObject,
          NPString* aScript,
          NPVariant* aResult)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    if (!(aNPP && aObject && aScript && aResult)) {
        NS_ERROR("Bad arguments!");
        return false;
    }

    PluginScriptableObjectChild* actor =
      InstCast(aNPP)->GetActorForNPObject(aObject);
    if (!actor) {
        NS_ERROR("Failed to create actor?!");
        return false;
    }

    return actor->Evaluate(aScript, aResult);
}

bool NP_CALLBACK
_getproperty(NPP aNPP,
             NPObject* aNPObj,
             NPIdentifier aPropertyName,
             NPVariant* aResult)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    if (!aNPP || !aNPObj || !aNPObj->_class || !aNPObj->_class->getProperty)
        return false;

    return aNPObj->_class->getProperty(aNPObj, aPropertyName, aResult);
}

bool NP_CALLBACK
_setproperty(NPP aNPP,
             NPObject* aNPObj,
             NPIdentifier aPropertyName,
             const NPVariant* aValue)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    if (!aNPP || !aNPObj || !aNPObj->_class || !aNPObj->_class->setProperty)
        return false;

    return aNPObj->_class->setProperty(aNPObj, aPropertyName, aValue);
}

bool NP_CALLBACK
_removeproperty(NPP aNPP,
                NPObject* aNPObj,
                NPIdentifier aPropertyName)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    if (!aNPP || !aNPObj || !aNPObj->_class || !aNPObj->_class->removeProperty)
        return false;

    return aNPObj->_class->removeProperty(aNPObj, aPropertyName);
}

bool NP_CALLBACK
_hasproperty(NPP aNPP,
             NPObject* aNPObj,
             NPIdentifier aPropertyName)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    if (!aNPP || !aNPObj || !aNPObj->_class || !aNPObj->_class->hasProperty)
        return false;

    return aNPObj->_class->hasProperty(aNPObj, aPropertyName);
}

bool NP_CALLBACK
_hasmethod(NPP aNPP,
           NPObject* aNPObj,
           NPIdentifier aMethodName)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    if (!aNPP || !aNPObj || !aNPObj->_class || !aNPObj->_class->hasMethod)
        return false;

    return aNPObj->_class->hasMethod(aNPObj, aMethodName);
}

bool NP_CALLBACK
_enumerate(NPP aNPP,
           NPObject* aNPObj,
           NPIdentifier** aIdentifiers,
           uint32_t* aCount)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    if (!aNPP || !aNPObj || !aNPObj->_class)
        return false;

    if (!NP_CLASS_STRUCT_VERSION_HAS_ENUM(aNPObj->_class) ||
        !aNPObj->_class->enumerate) {
        *aIdentifiers = 0;
        *aCount = 0;
        return true;
    }

    return aNPObj->_class->enumerate(aNPObj, aIdentifiers, aCount);
}

bool NP_CALLBACK
_construct(NPP aNPP,
           NPObject* aNPObj,
           const NPVariant* aArgs,
           uint32_t aArgCount,
           NPVariant* aResult)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    if (!aNPP || !aNPObj || !aNPObj->_class ||
        !NP_CLASS_STRUCT_VERSION_HAS_CTOR(aNPObj->_class) ||
        !aNPObj->_class->construct) {
        return false;
    }

    return aNPObj->_class->construct(aNPObj, aArgs, aArgCount, aResult);
}

void NP_CALLBACK
_releasevariantvalue(NPVariant* aVariant)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    if (NPVARIANT_IS_STRING(*aVariant)) {
        NPString str = NPVARIANT_TO_STRING(*aVariant);
        free(const_cast<NPUTF8*>(str.UTF8Characters));
    }
    else if (NPVARIANT_IS_OBJECT(*aVariant)) {
        NPObject* object = NPVARIANT_TO_OBJECT(*aVariant);
        if (object) {
            _releaseobject(object);
        }
    }
    VOID_TO_NPVARIANT(*aVariant);
}

void NP_CALLBACK
_setexception(NPObject* aNPObj,
              const NPUTF8* aMessage)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
    NS_NOTYETIMPLEMENTED("Implement me!");
}

bool NP_CALLBACK
_pushpopupsenabledstate(NPP aNPP,
                        NPBool aEnabled)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
    bool retval;
    if (InstCast(aNPP)->CallNPN_PushPopupsEnabledState(aEnabled ? true : false,
                                                       &retval)) {
        return retval;
    }
    return false;
}

bool NP_CALLBACK
_poppopupsenabledstate(NPP aNPP)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
    bool retval;
    if (InstCast(aNPP)->CallNPN_PopPopupsEnabledState(&retval)) {
        return retval;
    }
    return false;
}

void NP_CALLBACK
_pluginthreadasynccall(NPP aNPP,
                       PluginThreadCallback aFunc,
                       void* aUserData)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
    NS_NOTYETIMPLEMENTED("Implement me!");
}

NPError NP_CALLBACK
_getvalueforurl(NPP npp, NPNURLVariable variable, const char *url,
                char **value, uint32_t *len)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
    NS_NOTYETIMPLEMENTED("Implement me!");
    return NPERR_GENERIC_ERROR;
}

NPError NP_CALLBACK
_setvalueforurl(NPP npp, NPNURLVariable variable, const char *url,
                const char *value, uint32_t len)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
    NS_NOTYETIMPLEMENTED("Implement me!");
    return NPERR_GENERIC_ERROR;
}

NPError NP_CALLBACK
_getauthenticationinfo(NPP npp, const char *protocol,
                       const char *host, int32_t port,
                       const char *scheme, const char *realm,
                       char **username, uint32_t *ulen,
                       char **password, uint32_t *plen)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
    NS_NOTYETIMPLEMENTED("Implement me!");
    return NPERR_GENERIC_ERROR;
}

uint32_t NP_CALLBACK
_scheduletimer(NPP instance, uint32_t interval, NPBool repeat,
               void (*timerFunc)(NPP npp, uint32_t timerID))
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
    NS_NOTYETIMPLEMENTED("Implement me!");
    return 0;
}

void NP_CALLBACK
_unscheduletimer(NPP instance, uint32_t timerID)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
    NS_NOTYETIMPLEMENTED("Implement me!");
}

NPError NP_CALLBACK
_popupcontextmenu(NPP instance, NPMenu* menu)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
    NS_NOTYETIMPLEMENTED("Implement me!");
    return NPERR_GENERIC_ERROR;
}

NPBool NP_CALLBACK
_convertpoint(NPP instance, 
              double sourceX, double sourceY, NPCoordinateSpace sourceSpace,
              double *destX, double *destY, NPCoordinateSpace destSpace)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();
    NS_NOTYETIMPLEMENTED("Implement me!");
    return 0;
}

//-----------------------------------------------------------------------------

bool
PluginModuleChild::AnswerNP_Initialize(NPError* _retval)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

#if defined(OS_LINUX)
    *_retval = mInitializeFunc(&sBrowserFuncs, &mFunctions);
    return true;

#elif defined(OS_WIN)
    nsresult rv = mGetEntryPointsFunc(&mFunctions);
    if (NS_FAILED(rv)) {
        return false;
    }

    NS_ASSERTION(HIBYTE(mFunctions.version) >= NP_VERSION_MAJOR,
                 "callback version is less than NP version");

    *_retval = mInitializeFunc(&sBrowserFuncs);
    return true;
#elif defined(OS_MACOSX)
#  warning IMPLEMENT ME
    return false;

#else
#  error Please implement me for your platform
#endif
}

PPluginInstanceChild*
PluginModuleChild::AllocPPluginInstance(const nsCString& aMimeType,
                                        const uint16_t& aMode,
                                        const nsTArray<nsCString>& aNames,
                                        const nsTArray<nsCString>& aValues,
                                        NPError* rv)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    nsAutoPtr<PluginInstanceChild> childInstance(
        new PluginInstanceChild(&mFunctions));
    if (!childInstance->Initialize()) {
        *rv = NPERR_GENERIC_ERROR;
        return 0;
    }
    return childInstance.forget();
}

bool
PluginModuleChild::AnswerPPluginInstanceConstructor(PPluginInstanceChild* aActor,
                                                    const nsCString& aMimeType,
                                                    const uint16_t& aMode,
                                                    const nsTArray<nsCString>& aNames,
                                                    const nsTArray<nsCString>& aValues,
                                                    NPError* rv)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    PluginInstanceChild* childInstance =
        reinterpret_cast<PluginInstanceChild*>(aActor);
    NS_ASSERTION(childInstance, "Null actor!");

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
        argn[i] = const_cast<char*>(NullableStringGet(aNames[i]));
        argv[i] = const_cast<char*>(NullableStringGet(aValues[i]));
        printf("%s=%s, ", argn[i], argv[i]);
    }
    printf(")\n");

    NPP npp = childInstance->GetNPP();

    // FIXME/cjones: use SAFE_CALL stuff
    *rv = mFunctions.newp((char*)NullableStringGet(aMimeType),
                          npp,
                          aMode,
                          argc,
                          argn,
                          argv,
                          0);
    if (NPERR_NO_ERROR != *rv) {
        return false;
    }

    printf ("[PluginModuleChild] %s: returning %hd\n", __FUNCTION__, *rv);
    return true;
}

bool
PluginModuleChild::DeallocPPluginInstance(PPluginInstanceChild* aActor,
                                          NPError* rv)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    delete aActor;

    return true;
}

bool
PluginModuleChild::AnswerPPluginInstanceDestructor(PPluginInstanceChild* aActor,
                                                   NPError* rv)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

    PluginInstanceChild* inst = static_cast<PluginInstanceChild*>(aActor);
    *rv = mFunctions.destroy(inst->GetNPP(), 0);
    inst->Destroy();
    inst->GetNPP()->ndata = 0;

    return true;
}
