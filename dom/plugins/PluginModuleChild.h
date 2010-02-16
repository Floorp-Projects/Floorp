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

#ifndef dom_plugins_PluginModuleChild_h
#define dom_plugins_PluginModuleChild_h 1

#include <string>
#include <vector>

#include "base/basictypes.h"

#include "prlink.h"

#include "npapi.h"
#include "npfunctions.h"

#include "nsAutoPtr.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"

#include "mozilla/plugins/PPluginModuleChild.h"
#include "mozilla/plugins/PluginInstanceChild.h"

// NOTE: stolen from nsNPAPIPlugin.h

/*
 * Use this macro before each exported function
 * (between the return address and the function
 * itself), to ensure that the function has the
 * right calling conventions on OS/2.
 */
#ifdef XP_OS2
#define NP_CALLBACK _System
#else
#define NP_CALLBACK
#endif

#if defined(XP_WIN)
#define NS_NPAPIPLUGIN_CALLBACK(_type, _name) _type (__stdcall * _name)
#elif defined(XP_OS2)
#define NS_NPAPIPLUGIN_CALLBACK(_type, _name) _type (_System * _name)
#else
#define NS_NPAPIPLUGIN_CALLBACK(_type, _name) _type (* _name)
#endif

typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_GETENTRYPOINTS) (NPPluginFuncs* pCallbacks);
typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_PLUGININIT) (const NPNetscapeFuncs* pCallbacks);
typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_PLUGINUNIXINIT) (const NPNetscapeFuncs* pCallbacks, NPPluginFuncs* fCallbacks);
typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_PLUGINSHUTDOWN) (void);

namespace mozilla {
namespace plugins {

class PluginScriptableObjectChild;
class PluginInstanceChild;

class PluginModuleChild : public PPluginModuleChild
{
protected:
    // Implement the PPluginModuleChild interface
    virtual bool AnswerNP_Initialize(NPError* rv);

    virtual PPluginInstanceChild*
    AllocPPluginInstance(const nsCString& aMimeType,
                         const uint16_t& aMode,
                         const nsTArray<nsCString>& aNames,
                         const nsTArray<nsCString>& aValues,
                         NPError* rv);

    virtual bool
    DeallocPPluginInstance(PPluginInstanceChild* aActor);

    virtual bool
    AnswerPPluginInstanceConstructor(PPluginInstanceChild* aActor,
                                     const nsCString& aMimeType,
                                     const uint16_t& aMode,
                                     const nsTArray<nsCString>& aNames,
                                     const nsTArray<nsCString>& aValues,
                                     NPError* rv);
    virtual bool
    AnswerNP_Shutdown(NPError *rv);

    virtual void
    ActorDestroy(ActorDestroyReason why);

public:
    PluginModuleChild();
    virtual ~PluginModuleChild();

    bool Init(const std::string& aPluginFilename,
              base::ProcessHandle aParentProcessHandle,
              MessageLoop* aIOLoop,
              IPC::Channel* aChannel);

    void CleanUp();

    const char* GetUserAgent();

    static const NPNetscapeFuncs sBrowserFuncs;

    static PluginModuleChild* current();

    bool RegisterActorForNPObject(NPObject* aObject,
                                  PluginScriptableObjectChild* aActor);

    void UnregisterActorForNPObject(NPObject* aObject);

    PluginScriptableObjectChild* GetActorForNPObject(NPObject* aObject);

#ifdef DEBUG
    bool NPObjectIsRegistered(NPObject* aObject);
#endif

    bool
    PluginInstanceDestroyed(PluginInstanceChild* aActor,
                            NPError* rv);

    /**
     * The child implementation of NPN_CreateObject.
     */
    static NPObject* NP_CALLBACK NPN_CreateObject(NPP aNPP, NPClass* aClass);
    /**
     * The child implementation of NPN_RetainObject.
     */
    static NPObject* NP_CALLBACK NPN_RetainObject(NPObject* aNPObj);
    /**
     * The child implementation of NPN_ReleaseObject.
     */
    static void NP_CALLBACK NPN_ReleaseObject(NPObject* aNPObj);

private:
    bool InitGraphics();
#if defined(MOZ_WIDGET_GTK2)
    static gboolean DetectNestedEventLoop(gpointer data);
    static gboolean ProcessBrowserEvents(gpointer data);

    NS_OVERRIDE
    virtual void EnteredCxxStack();
    NS_OVERRIDE
    virtual void ExitedCxxStack();
#endif

    std::string mPluginFilename;
    PRLibrary* mLibrary;
    nsCString mUserAgent;

    // we get this from the plugin
#ifdef OS_POSIX
    NP_PLUGINUNIXINIT mInitializeFunc;
#elif OS_WIN
    NP_PLUGININIT mInitializeFunc;
    NP_GETENTRYPOINTS mGetEntryPointsFunc;
#endif

    NP_PLUGINSHUTDOWN mShutdownFunc;
    NPPluginFuncs mFunctions;
    NPSavedData mSavedData;

#if defined(MOZ_WIDGET_GTK2)
    // If a plugin spins a nested glib event loop in response to a
    // synchronous IPC message from the browser, the loop might break
    // only after the browser responds to a request sent by the
    // plugin.  This can happen if a plugin uses gtk's synchronous
    // copy/paste, for example.  But because the browser is blocked on
    // a condvar, it can't respond to the request.  This situation
    // isn't technically a deadlock, but the symptoms are basically
    // the same from the user's perspective.
    //
    // We take two steps to prevent this
    //
    //  (1) Detect nested event loops spun by the plugin.  This is
    //      done by scheduling a glib timer event in the plugin
    //      process whenever the browser might block on the plugin.
    //      If the plugin indeed spins a nested loop, this timer event
    //      will fire "soon" thereafter.
    //
    //  (2) When a nested loop is detected, deschedule the
    //      nested-loop-detection timer and in its place, schedule
    //      another timer that periodically calls back into the
    //      browser and spins a mini event loop.  This mini event loop
    //      processes a handful of pending native events.
    //
    // Because only timer (1) or (2) (or neither) may be active at any
    // point in time, we use the same member variable
    // |mNestedLoopTimerId| to refer to both.
    //
    // When the browser no longer might be blocked on a plugin's IPC
    // response, we deschedule whichever of (1) or (2) is active.
    guint mNestedLoopTimerId;
#endif

    struct NPObjectData : public nsPtrHashKey<NPObject>
    {
        NPObjectData(const NPObject* key)
            : nsPtrHashKey<NPObject>(key)
            , instance(NULL)
            , actor(NULL)
        { }

        // never NULL
        PluginInstanceChild* instance;

        // sometimes NULL (no actor associated with an NPObject)
        PluginScriptableObjectChild* actor;
    };
    /**
     * mObjectMap contains all the currently active NPObjects (from NPN_CreateObject until the
     * final release/dealloc, whether or not an actor is currently associated with the object.
     */
    nsTHashtable<NPObjectData> mObjectMap;

    /**
     * Dealloc an NPObject after last-release or when the associated instance
     * is destroyed. It is the callers responsibility to remove the object
     * from mObjectMap.
     */
    static void DeallocNPObject(NPObject* o);

    /**
     * After an instance has been destroyed, dealloc the objects associated
     * with that instance.
     */
    void DeallocNPObjectsForInstance(PluginInstanceChild* instance);
    /**
     * Enumeration helper function for DeallocNPObjectsForInstance.
     */
    static PLDHashOperator DeallocForInstance(NPObjectData* d, void* userArg);
};

} /* namespace plugins */
} /* namespace mozilla */

#endif  // ifndef dom_plugins_PluginModuleChild_h
