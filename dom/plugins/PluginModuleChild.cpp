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

#ifdef MOZ_WIDGET_QT
#include <QApplication>
#endif

#include "mozilla/plugins/PluginModuleChild.h"

#ifdef MOZ_WIDGET_GTK2
#include <gtk/gtk.h>
#endif

#include "nsILocalFile.h"

#include "pratom.h"
#include "nsDebug.h"
#include "nsCOMPtr.h"
#include "nsPluginsDir.h"
#include "nsXULAppAPI.h"

#include "mozilla/plugins/PluginInstanceChild.h"
#include "mozilla/plugins/StreamNotifyChild.h"
#include "mozilla/plugins/BrowserStreamChild.h"
#include "mozilla/plugins/PluginStreamChild.h"
#include "mozilla/plugins/PluginThreadChild.h"

#include "nsNPAPIPlugin.h"

using mozilla::ipc::NPRemoteIdentifier;

using namespace mozilla::plugins;

namespace {
PluginModuleChild* gInstance = nsnull;
#ifdef MOZ_WIDGET_QT
static QApplication *gQApp = nsnull;
#endif
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
#ifdef MOZ_WIDGET_QT
    if (gQApp)
        delete gQApp;
    gQApp = nsnull;
#endif
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
    PLUGIN_LOG_DEBUG_METHOD;

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

#if defined(MOZ_WIDGET_GTK2)
typedef void (*GObjectDisposeFn)(GObject*);
typedef void (*GtkPlugEmbeddedFn)(GtkPlug*);

static GObjectDisposeFn real_gtk_plug_dispose;
static GtkPlugEmbeddedFn real_gtk_plug_embedded;

static void
undo_bogus_unref(gpointer data, GObject* object, gboolean is_last_ref) {
    if (!is_last_ref) // recursion in g_object_ref
        return;

    g_object_ref(object);
}

static void
wrap_gtk_plug_dispose(GObject* object) {
    // Work around Flash Player bug described in bug 538914.
    //
    // This function is called during gtk_widget_destroy and/or before
    // the object's last reference is removed.  A reference to the
    // object is held during the call so the ref count should not drop
    // to zero.  However, Flash Player tries to destroy the GtkPlug
    // using g_object_unref instead of gtk_widget_destroy.  The
    // reference that Flash is removing actually belongs to the
    // GtkPlug.  During real_gtk_plug_dispose, the GtkPlug removes its
    // reference.
    //
    // A toggle ref is added to prevent premature deletion of the object
    // caused by Flash Player's extra unref, and to detect when there are
    // unexpectedly no other references.
    g_object_add_toggle_ref(object, undo_bogus_unref, NULL);
    (*real_gtk_plug_dispose)(object);
    g_object_remove_toggle_ref(object, undo_bogus_unref, NULL);
}

static void
wrap_gtk_plug_embedded(GtkPlug* plug) {
    GdkWindow* socket_window = plug->socket_window;
    if (socket_window &&
        g_object_get_data(G_OBJECT(socket_window),
                          "moz-existed-before-set-window")) {
        // Add missing reference for
        // https://bugzilla.gnome.org/show_bug.cgi?id=607061
        g_object_ref(socket_window);
    }

    if (*real_gtk_plug_embedded) {
        (*real_gtk_plug_embedded)(plug);
    }
}
#endif

bool
PluginModuleChild::InitGraphics()
{
    // FIXME/cjones: is this the place for this?
#if defined(MOZ_WIDGET_GTK2)
    // Work around plugins that don't interact well with GDK
    // client-side windows.
    PR_SetEnv("GDK_NATIVE_WINDOWS=1");

    gtk_init(0, 0);

    // GtkPlug is a static class so will leak anyway but this ref makes sure.
    gpointer gtk_plug_class = g_type_class_ref(GTK_TYPE_PLUG);

    // The dispose method is a good place to hook into the destruction process
    // because the reference count should be 1 the last time dispose is
    // called.  (Toggle references wouldn't detect if the reference count
    // might be higher.)
    GObjectDisposeFn* dispose = &G_OBJECT_CLASS(gtk_plug_class)->dispose;
    NS_ABORT_IF_FALSE(*dispose != wrap_gtk_plug_dispose,
                      "InitGraphics called twice");
    real_gtk_plug_dispose = *dispose;
    *dispose = wrap_gtk_plug_dispose;

    GtkPlugEmbeddedFn* embedded = &GTK_PLUG_CLASS(gtk_plug_class)->embedded;
    real_gtk_plug_embedded = *embedded;
    *embedded = wrap_gtk_plug_embedded;
#elif defined(MOZ_WIDGET_QT)
    if (!qApp)
        gQApp = new QApplication(0, NULL);
#else
    // may not be necessary on all platforms
#endif

    return true;
}

bool
PluginModuleChild::AnswerNP_Shutdown(NPError *rv)
{
    AssertPluginThread();

    // the PluginModuleParent shuts down this process after this RPC
    // call pops off its stack

    *rv = mShutdownFunc ? mShutdownFunc() : NPERR_NO_ERROR;

    // weakly guard against re-entry after NP_Shutdown
    memset(&mFunctions, 0, sizeof(mFunctions));

    return true;
}

void
PluginModuleChild::ActorDestroy(ActorDestroyReason why)
{
    // doesn't matter why we're being destroyed; it's up to us to
    // initiate (clean) shutdown
    XRE_ShutdownChildProcess();
}

void
PluginModuleChild::CleanUp()
{
}

const char*
PluginModuleChild::GetUserAgent()
{
    if (!CallNPN_UserAgent(&mUserAgent))
        return NULL;

    return NullableStringGet(mUserAgent);
}

bool
PluginModuleChild::RegisterActorForNPObject(NPObject* aObject,
                                            PluginScriptableObjectChild* aActor)
{
    AssertPluginThread();
    NS_ASSERTION(mObjectMap.IsInitialized(), "Not initialized!");
    NS_ASSERTION(aObject && aActor, "Null pointer!");

    NPObjectData* d = mObjectMap.GetEntry(aObject);
    if (!d) {
        NS_ERROR("NPObject not in object table");
        return false;
    }

    d->actor = aActor;
    return true;
}

void
PluginModuleChild::UnregisterActorForNPObject(NPObject* aObject)
{
    AssertPluginThread();
    NS_ASSERTION(mObjectMap.IsInitialized(), "Not initialized!");
    NS_ASSERTION(aObject, "Null pointer!");

    mObjectMap.GetEntry(aObject)->actor = NULL;
}

PluginScriptableObjectChild*
PluginModuleChild::GetActorForNPObject(NPObject* aObject)
{
    AssertPluginThread();
    NS_ASSERTION(mObjectMap.IsInitialized(), "Not initialized!");
    NS_ASSERTION(aObject, "Null pointer!");

    NPObjectData* d = mObjectMap.GetEntry(aObject);
    if (!d) {
        NS_ERROR("Plugin using object not created with NPN_CreateObject?");
        return NULL;
    }

    return d->actor;
}

#ifdef DEBUG
bool
PluginModuleChild::NPObjectIsRegistered(NPObject* aObject)
{
    return !!mObjectMap.GetEntry(aObject);
}
#endif

//-----------------------------------------------------------------------------
// FIXME/cjones: just getting this out of the way for the moment ...

namespace mozilla {
namespace plugins {
namespace child {

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

} /* namespace child */
} /* namespace plugins */
} /* namespace mozilla */

const NPNetscapeFuncs PluginModuleChild::sBrowserFuncs = {
    sizeof(sBrowserFuncs),
    (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR,
    mozilla::plugins::child::_geturl,
    mozilla::plugins::child::_posturl,
    mozilla::plugins::child::_requestread,
    mozilla::plugins::child::_newstream,
    mozilla::plugins::child::_write,
    mozilla::plugins::child::_destroystream,
    mozilla::plugins::child::_status,
    mozilla::plugins::child::_useragent,
    mozilla::plugins::child::_memalloc,
    mozilla::plugins::child::_memfree,
    mozilla::plugins::child::_memflush,
    mozilla::plugins::child::_reloadplugins,
    mozilla::plugins::child::_getjavaenv,
    mozilla::plugins::child::_getjavapeer,
    mozilla::plugins::child::_geturlnotify,
    mozilla::plugins::child::_posturlnotify,
    mozilla::plugins::child::_getvalue,
    mozilla::plugins::child::_setvalue,
    mozilla::plugins::child::_invalidaterect,
    mozilla::plugins::child::_invalidateregion,
    mozilla::plugins::child::_forceredraw,
    mozilla::plugins::child::_getstringidentifier,
    mozilla::plugins::child::_getstringidentifiers,
    mozilla::plugins::child::_getintidentifier,
    mozilla::plugins::child::_identifierisstring,
    mozilla::plugins::child::_utf8fromidentifier,
    mozilla::plugins::child::_intfromidentifier,
    PluginModuleChild::NPN_CreateObject,
    PluginModuleChild::NPN_RetainObject,
    PluginModuleChild::NPN_ReleaseObject,
    mozilla::plugins::child::_invoke,
    mozilla::plugins::child::_invokedefault,
    mozilla::plugins::child::_evaluate,
    mozilla::plugins::child::_getproperty,
    mozilla::plugins::child::_setproperty,
    mozilla::plugins::child::_removeproperty,
    mozilla::plugins::child::_hasproperty,
    mozilla::plugins::child::_hasmethod,
    mozilla::plugins::child::_releasevariantvalue,
    mozilla::plugins::child::_setexception,
    mozilla::plugins::child::_pushpopupsenabledstate,
    mozilla::plugins::child::_poppopupsenabledstate,
    mozilla::plugins::child::_enumerate,
    mozilla::plugins::child::_pluginthreadasynccall,
    mozilla::plugins::child::_construct,
    mozilla::plugins::child::_getvalueforurl,
    mozilla::plugins::child::_setvalueforurl,
    mozilla::plugins::child::_getauthenticationinfo,
    mozilla::plugins::child::_scheduletimer,
    mozilla::plugins::child::_unscheduletimer,
    mozilla::plugins::child::_popupcontextmenu,
    mozilla::plugins::child::_convertpoint
};

PluginInstanceChild*
InstCast(NPP aNPP)
{
    NS_ABORT_IF_FALSE(!!(aNPP->ndata), "nil instance");
    return static_cast<PluginInstanceChild*>(aNPP->ndata);
}

namespace mozilla {
namespace plugins {
namespace child {

NPError NP_CALLBACK
_requestread(NPStream* aStream,
             NPByteRange* aRangeList)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();

    nsCString url = NullableString(aRelativeURL);
    StreamNotifyChild* sn = new StreamNotifyChild(url);

    NPError err;
    InstCast(aNPP)->CallPStreamNotifyConstructor(
        sn, url, NullableString(aTarget), false, nsCString(), false, &err);

    if (NPERR_NO_ERROR == err) {
        // If NPN_PostURLNotify fails, the parent will immediately send us
        // a PStreamNotifyDestructor, which should not call NPP_URLNotify.
        sn->SetValid(aNotifyData);
    }

    return err;
}

NPError NP_CALLBACK
_getvalue(NPP aNPP,
          NPNVariable aVariable,
          void* aValue)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();

    switch (aVariable) {
        // Copied from nsNPAPIPlugin.cpp
        case NPNVToolkit:
#ifdef MOZ_WIDGET_GTK2
            *static_cast<NPNToolkitType*>(aValue) = NPNVGtk2;
            return NPERR_NO_ERROR;
#endif
            return NPERR_GENERIC_ERROR;

        case NPNVjavascriptEnabledBool: // Intentional fall-through
        case NPNVasdEnabledBool: // Intentional fall-through
        case NPNVisOfflineBool: // Intentional fall-through
        case NPNVSupportsXEmbedBool: // Intentional fall-through
        case NPNVSupportsWindowless: // Intentional fall-through
        case NPNVprivateModeBool: {
            NPError result;
            bool value;
            PluginModuleChild::current()->
                CallNPN_GetValue_WithBoolReturn(aVariable, &result, &value);
            *(NPBool*)aValue = value ? true : false;
            return result;
        }

        default: {
            if (aNPP) {
                return InstCast(aNPP)->NPN_GetValue(aVariable, aValue);
            }

            NS_WARNING("Null NPP!");
            return NPERR_INVALID_INSTANCE_ERROR;
        }
    }

    NS_NOTREACHED("Shouldn't get here!");
    return NPERR_GENERIC_ERROR;
}

NPError NP_CALLBACK
_setvalue(NPP aNPP,
          NPPVariable aVariable,
          void* aValue)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();
    return InstCast(aNPP)->NPN_SetValue(aVariable, aValue);
}

NPError NP_CALLBACK
_geturl(NPP aNPP,
        const char* aRelativeURL,
        const char* aTarget)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();

    if (!aBuffer)
        return NPERR_INVALID_PARAM;

    nsCString url = NullableString(aRelativeURL);
    StreamNotifyChild* sn = new StreamNotifyChild(url);

    NPError err;
    InstCast(aNPP)->CallPStreamNotifyConstructor(
        sn, url, NullableString(aTarget), true,
        nsCString(aBuffer, aLength), aIsFile, &err);

    if (NPERR_NO_ERROR == err) {
        // If NPN_PostURLNotify fails, the parent will immediately send us
        // a PStreamNotifyDestructor, which should not call NPP_URLNotify.
        sn->SetValid(aNotifyData);
    }

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
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();
    return InstCast(aNPP)->NPN_NewStream(aMIMEType, aWindow, aStream);
}

int32_t NP_CALLBACK
_write(NPP aNPP,
       NPStream* aStream,
       int32_t aLength,
       void* aBuffer)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();

    PluginInstanceChild* p = InstCast(aNPP);
    AStream* s = static_cast<AStream*>(aStream->ndata);
    if (s->IsBrowserStream()) {
        BrowserStreamChild* bs = static_cast<BrowserStreamChild*>(s);
        bs->EnsureCorrectInstance(p);
        PBrowserStreamChild::Call__delete__(bs, aReason, false);
    }
    else {
        PluginStreamChild* ps = static_cast<PluginStreamChild*>(s);
        ps->EnsureCorrectInstance(p);
        PPluginStreamChild::Call__delete__(ps, aReason, false);
    }
    return NPERR_NO_ERROR;
}

void NP_CALLBACK
_status(NPP aNPP,
        const char* aMessage)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();
}

void NP_CALLBACK
_memfree(void* aPtr)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();
    NS_Free(aPtr);
}

uint32_t NP_CALLBACK
_memflush(uint32_t aSize)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();
    return 0;
}

void NP_CALLBACK
_reloadplugins(NPBool aReloadPages)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();
}

void NP_CALLBACK
_invalidaterect(NPP aNPP,
                NPRect* aInvalidRect)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();
    InstCast(aNPP)->InvalidateRect(aInvalidRect);
}

void NP_CALLBACK
_invalidateregion(NPP aNPP,
                  NPRegion aInvalidRegion)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();
    // Not implemented in Mozilla.
}

void NP_CALLBACK
_forceredraw(NPP aNPP)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();
}

const char* NP_CALLBACK
_useragent(NPP aNPP)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();

    return PluginModuleChild::current()->GetUserAgent();
}

void* NP_CALLBACK
_memalloc(uint32_t aSize)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();
    return NS_Alloc(aSize);
}

// Deprecated entry points for the old Java plugin.
void* NP_CALLBACK /* OJI type: JRIEnv* */
_getjavaenv(void)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();
    return 0;
}

void* NP_CALLBACK /* OJI type: jref */
_getjavapeer(NPP aNPP)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();
    return 0;
}

NPIdentifier NP_CALLBACK
_getstringidentifier(const NPUTF8* aName)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
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

bool NP_CALLBACK
_invoke(NPP aNPP,
        NPObject* aNPObj,
        NPIdentifier aMethod,
        const NPVariant* aArgs,
        uint32_t aArgCount,
        NPVariant* aResult)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();

    if (NPVARIANT_IS_STRING(*aVariant)) {
        NPString str = NPVARIANT_TO_STRING(*aVariant);
        free(const_cast<NPUTF8*>(str.UTF8Characters));
    }
    else if (NPVARIANT_IS_OBJECT(*aVariant)) {
        NPObject* object = NPVARIANT_TO_OBJECT(*aVariant);
        if (object) {
            PluginModuleChild::NPN_ReleaseObject(object);
        }
    }
    VOID_TO_NPVARIANT(*aVariant);
}

void NP_CALLBACK
_setexception(NPObject* aNPObj,
              const NPUTF8* aMessage)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();
    NS_NOTYETIMPLEMENTED("Implement me!");
}

bool NP_CALLBACK
_pushpopupsenabledstate(NPP aNPP,
                        NPBool aEnabled)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
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
    PLUGIN_LOG_DEBUG_FUNCTION;
    if (!aFunc)
        return;

    PluginThreadChild::current()->message_loop()
        ->PostTask(FROM_HERE, new ChildAsyncCall(InstCast(aNPP), aFunc,
                                                 aUserData));
}

NPError NP_CALLBACK
_getvalueforurl(NPP npp, NPNURLVariable variable, const char *url,
                char **value, uint32_t *len)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();

    if (!url)
        return NPERR_INVALID_URL;

    if (!npp || !value || !len)
        return NPERR_INVALID_PARAM;

    switch (variable) {
    case NPNURLVCookie:
    case NPNURLVProxy:
        nsCString v;
        NPError result;
        InstCast(npp)->
            CallNPN_GetValueForURL(variable, nsCString(url), &v, &result);
        if (NPERR_NO_ERROR == result) {
            *value = ToNewCString(v);
            *len = v.Length();
        }
        return result;
    }

    return NPERR_INVALID_PARAM;
}

NPError NP_CALLBACK
_setvalueforurl(NPP npp, NPNURLVariable variable, const char *url,
                const char *value, uint32_t len)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();

    if (!value)
        return NPERR_INVALID_PARAM;

    if (!url)
        return NPERR_INVALID_URL;

    switch (variable) {
    case NPNURLVCookie:
    case NPNURLVProxy:
        NPError result;
        InstCast(npp)->CallNPN_SetValueForURL(variable, nsCString(url),
                                              nsDependentCString(value, len),
                                              &result);
        return result;
    }

    return NPERR_INVALID_PARAM;
}

NPError NP_CALLBACK
_getauthenticationinfo(NPP npp, const char *protocol,
                       const char *host, int32_t port,
                       const char *scheme, const char *realm,
                       char **username, uint32_t *ulen,
                       char **password, uint32_t *plen)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();

    if (!protocol || !host || !scheme || !realm || !username || !ulen ||
        !password || !plen)
        return NPERR_INVALID_PARAM;

    nsCString u;
    nsCString p;
    NPError result;
    InstCast(npp)->
        CallNPN_GetAuthenticationInfo(nsDependentCString(protocol),
                                      nsDependentCString(host),
                                      port,
                                      nsDependentCString(scheme),
                                      nsDependentCString(realm),
                                      &u, &p, &result);
    if (NPERR_NO_ERROR == result) {
        *username = ToNewCString(u);
        *ulen = u.Length();
        *password = ToNewCString(p);
        *plen = p.Length();
    }
    return result;
}

uint32_t NP_CALLBACK
_scheduletimer(NPP npp, uint32_t interval, NPBool repeat,
               void (*timerFunc)(NPP npp, uint32_t timerID))
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();

    return InstCast(npp)->ScheduleTimer(interval, repeat, timerFunc);
}

void NP_CALLBACK
_unscheduletimer(NPP npp, uint32_t timerID)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();
    InstCast(npp)->UnscheduleTimer(timerID);
}

NPError NP_CALLBACK
_popupcontextmenu(NPP instance, NPMenu* menu)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();
    NS_NOTYETIMPLEMENTED("Implement me!");
    return NPERR_GENERIC_ERROR;
}

NPBool NP_CALLBACK
_convertpoint(NPP instance, 
              double sourceX, double sourceY, NPCoordinateSpace sourceSpace,
              double *destX, double *destY, NPCoordinateSpace destSpace)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();
    NS_NOTYETIMPLEMENTED("Implement me!");
    return 0;
}

} /* namespace child */
} /* namespace plugins */
} /* namespace mozilla */

//-----------------------------------------------------------------------------

bool
PluginModuleChild::AnswerNP_Initialize(NPError* _retval)
{
    PLUGIN_LOG_DEBUG_METHOD;
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
    PLUGIN_LOG_DEBUG_METHOD;
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
    PLUGIN_LOG_DEBUG_METHOD;
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

    for (int i = 0; i < argc; ++i) {
        argn[i] = const_cast<char*>(NullableStringGet(aNames[i]));
        argv[i] = const_cast<char*>(NullableStringGet(aValues[i]));
    }

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

    return true;
}

bool
PluginModuleChild::DeallocPPluginInstance(PPluginInstanceChild* aActor)
{
    PLUGIN_LOG_DEBUG_METHOD;
    AssertPluginThread();

    delete aActor;

    return true;
}

bool
PluginModuleChild::PluginInstanceDestroyed(PluginInstanceChild* aActor,
                                           NPError* rv)
{
    PLUGIN_LOG_DEBUG_METHOD;
    AssertPluginThread();

    NPP npp = aActor->GetNPP();

    *rv = mFunctions.destroy(npp, 0);
    npp->ndata = 0;

    DeallocNPObjectsForInstance(aActor);

    return true;
}

NPObject* NP_CALLBACK
PluginModuleChild::NPN_CreateObject(NPP aNPP, NPClass* aClass)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();

    PluginInstanceChild* i = InstCast(aNPP);

    NPObject* newObject;
    if (aClass && aClass->allocate) {
        newObject = aClass->allocate(aNPP, aClass);
    }
    else {
        newObject = reinterpret_cast<NPObject*>(child::_memalloc(sizeof(NPObject)));
    }

    if (newObject) {
        newObject->_class = aClass;
        newObject->referenceCount = 1;
        NS_LOG_ADDREF(newObject, 1, "NPObject", sizeof(NPObject));
    }

    NPObjectData* d = static_cast<PluginModuleChild*>(i->Manager())
        ->mObjectMap.PutEntry(newObject);
    NS_ASSERTION(!d->instance, "New NPObject already mapped?");
    d->instance = i;

    return newObject;
}

NPObject* NP_CALLBACK
PluginModuleChild::NPN_RetainObject(NPObject* aNPObj)
{
    AssertPluginThread();

    int32_t refCnt = PR_AtomicIncrement((PRInt32*)&aNPObj->referenceCount);
    NS_LOG_ADDREF(aNPObj, refCnt, "NPObject", sizeof(NPObject));

    return aNPObj;
}

void NP_CALLBACK
PluginModuleChild::NPN_ReleaseObject(NPObject* aNPObj)
{
    AssertPluginThread();

    int32_t refCnt = PR_AtomicDecrement((PRInt32*)&aNPObj->referenceCount);
    NS_LOG_RELEASE(aNPObj, refCnt, "NPObject");

    if (refCnt == 0) {
        DeallocNPObject(aNPObj);
#ifdef DEBUG
        NPObjectData* d = current()->mObjectMap.GetEntry(aNPObj);
        NS_ASSERTION(d, "NPObject not mapped?");
        NS_ASSERTION(!d->actor, "NPObject has actor at destruction?");
#endif
        current()->mObjectMap.RemoveEntry(aNPObj);
    }
    return;
}

void
PluginModuleChild::DeallocNPObject(NPObject* aNPObj)
{
    if (aNPObj->_class && aNPObj->_class->deallocate) {
        aNPObj->_class->deallocate(aNPObj);
    } else {
        child::_memfree(aNPObj);
    }
}

PLDHashOperator
PluginModuleChild::DeallocForInstance(NPObjectData* d, void* userArg)
{
    if (d->instance == static_cast<PluginInstanceChild*>(userArg)) {
        NPObject* o = d->GetKey();
        if (o->_class && o->_class->invalidate)
            o->_class->invalidate(o);

#ifdef NS_BUILD_REFCNT_LOGGING
        {
            int32_t refCnt = o->referenceCount;
            while (refCnt) {
                --refCnt;
                NS_LOG_RELEASE(o, refCnt, "NPObject");
            }
        }
#endif

        DeallocNPObject(o);

        if (d->actor)
            d->actor->NPObjectDestroyed();

        return PL_DHASH_REMOVE;
    }

    return PL_DHASH_NEXT;
}

void
PluginModuleChild::DeallocNPObjectsForInstance(PluginInstanceChild* instance)
{
    mObjectMap.EnumerateEntries(DeallocForInstance, instance);
}
