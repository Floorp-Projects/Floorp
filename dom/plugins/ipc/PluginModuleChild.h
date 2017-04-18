/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_plugins_PluginModuleChild_h
#define dom_plugins_PluginModuleChild_h 1

#include "mozilla/Attributes.h"

#include <string>
#include <vector>

#include "base/basictypes.h"

#include "prlink.h"

#include "npapi.h"
#include "npfunctions.h"

#include "nsDataHashtable.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"

#ifdef MOZ_WIDGET_COCOA
#include "PluginInterposeOSX.h"
#endif

#include "mozilla/plugins/PPluginModuleChild.h"
#include "mozilla/plugins/PluginInstanceChild.h"
#include "mozilla/plugins/PluginMessageUtils.h"
#include "mozilla/plugins/PluginQuirks.h"

// NOTE: stolen from nsNPAPIPlugin.h

#if defined(XP_WIN)
#define NS_NPAPIPLUGIN_CALLBACK(_type, _name) _type (__stdcall * _name)
#else
#define NS_NPAPIPLUGIN_CALLBACK(_type, _name) _type (* _name)
#endif

typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_GETENTRYPOINTS) (NPPluginFuncs* pCallbacks);
typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_PLUGININIT) (const NPNetscapeFuncs* pCallbacks);
typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_PLUGINUNIXINIT) (const NPNetscapeFuncs* pCallbacks, NPPluginFuncs* fCallbacks);
typedef NS_NPAPIPLUGIN_CALLBACK(NPError, NP_PLUGINSHUTDOWN) (void);

namespace mozilla {

class ChildProfilerController;

namespace plugins {

class PluginInstanceChild;

class PluginModuleChild : public PPluginModuleChild
{
protected:
    virtual mozilla::ipc::RacyInterruptPolicy
    MediateInterruptRace(const MessageInfo& parent,
                         const MessageInfo& child) override
    {
        return MediateRace(parent, child);
    }

    virtual bool ShouldContinueFromReplyTimeout() override;

    virtual mozilla::ipc::IPCResult RecvSettingChanged(const PluginSettings& aSettings) override;

    // Implement the PPluginModuleChild interface
    virtual mozilla::ipc::IPCResult RecvInitProfiler(Endpoint<mozilla::PProfilerChild>&& aEndpoint) override;
    virtual mozilla::ipc::IPCResult RecvDisableFlashProtectedMode() override;
    virtual mozilla::ipc::IPCResult AnswerNP_GetEntryPoints(NPError* rv) override;
    virtual mozilla::ipc::IPCResult AnswerNP_Initialize(const PluginSettings& aSettings, NPError* rv) override;
    virtual mozilla::ipc::IPCResult AnswerSyncNPP_New(PPluginInstanceChild* aActor, NPError* rv)
                                   override;

    virtual mozilla::ipc::IPCResult
    RecvInitPluginModuleChild(Endpoint<PPluginModuleChild>&& endpoint) override;

    virtual PPluginInstanceChild*
    AllocPPluginInstanceChild(const nsCString& aMimeType,
                              const InfallibleTArray<nsCString>& aNames,
                              const InfallibleTArray<nsCString>& aValues)
                              override;

    virtual bool
    DeallocPPluginInstanceChild(PPluginInstanceChild* aActor) override;

    virtual mozilla::ipc::IPCResult
    RecvPPluginInstanceConstructor(PPluginInstanceChild* aActor,
                                   const nsCString& aMimeType,
                                   InfallibleTArray<nsCString>&& aNames,
                                   InfallibleTArray<nsCString>&& aValues)
                                   override;
    virtual mozilla::ipc::IPCResult
    AnswerNP_Shutdown(NPError *rv) override;

    virtual mozilla::ipc::IPCResult
    AnswerOptionalFunctionsSupported(bool *aURLRedirectNotify,
                                     bool *aClearSiteData,
                                     bool *aGetSitesWithData) override;

    virtual mozilla::ipc::IPCResult
    RecvNPP_ClearSiteData(const nsCString& aSite,
                            const uint64_t& aFlags,
                            const uint64_t& aMaxAge,
                            const uint64_t& aCallbackId) override;

    virtual mozilla::ipc::IPCResult
    RecvNPP_GetSitesWithData(const uint64_t& aCallbackId) override;

    virtual mozilla::ipc::IPCResult
    RecvSetAudioSessionData(const nsID& aId,
                            const nsString& aDisplayName,
                            const nsString& aIconPath) override;

    virtual mozilla::ipc::IPCResult
    RecvSetParentHangTimeout(const uint32_t& aSeconds) override;

    virtual mozilla::ipc::IPCResult
    AnswerInitCrashReporter(Shmem&& aShmem, mozilla::dom::NativeThreadId* aId) override;

    virtual void
    ActorDestroy(ActorDestroyReason why) override;

    virtual mozilla::ipc::IPCResult
    RecvProcessNativeEventsInInterruptCall() override;

    virtual mozilla::ipc::IPCResult
    AnswerModuleSupportsAsyncRender(bool* aResult) override;
public:
    explicit PluginModuleChild(bool aIsChrome);
    virtual ~PluginModuleChild();

    void CommonInit();

    // aPluginFilename is UTF8, not native-charset!
    bool InitForChrome(const std::string& aPluginFilename,
                       base::ProcessId aParentPid,
                       MessageLoop* aIOLoop,
                       IPC::Channel* aChannel);

    bool InitForContent(Endpoint<PPluginModuleChild>&& aEndpoint);

    static bool
    CreateForContentProcess(Endpoint<PPluginModuleChild>&& aEndpoint);

    void CleanUp();

    NPError NP_Shutdown();

    const char* GetUserAgent();

    static const NPNetscapeFuncs sBrowserFuncs;

    static PluginModuleChild* GetChrome();

    /**
     * The child implementation of NPN_CreateObject.
     */
    static NPObject* NPN_CreateObject(NPP aNPP, NPClass* aClass);
    /**
     * The child implementation of NPN_RetainObject.
     */
    static NPObject* NPN_RetainObject(NPObject* aNPObj);
    /**
     * The child implementation of NPN_ReleaseObject.
     */
    static void NPN_ReleaseObject(NPObject* aNPObj);

    /**
     * The child implementations of NPIdentifier-related functions.
     */
    static NPIdentifier NPN_GetStringIdentifier(const NPUTF8* aName);
    static void NPN_GetStringIdentifiers(const NPUTF8** aNames,
                                                     int32_t aNameCount,
                                                     NPIdentifier* aIdentifiers);
    static NPIdentifier NPN_GetIntIdentifier(int32_t aIntId);
    static bool NPN_IdentifierIsString(NPIdentifier aIdentifier);
    static NPUTF8* NPN_UTF8FromIdentifier(NPIdentifier aIdentifier);
    static int32_t NPN_IntFromIdentifier(NPIdentifier aIdentifier);

#ifdef MOZ_WIDGET_COCOA
    void ProcessNativeEvents();
    
    void PluginShowWindow(uint32_t window_id, bool modal, CGRect r) {
        SendPluginShowWindow(window_id, modal, r.origin.x, r.origin.y, r.size.width, r.size.height);
    }

    void PluginHideWindow(uint32_t window_id) {
        SendPluginHideWindow(window_id);
    }

    void SetCursor(NSCursorInfo& cursorInfo) {
        SendSetCursor(cursorInfo);
    }

    void ShowCursor(bool show) {
        SendShowCursor(show);
    }

    void PushCursor(NSCursorInfo& cursorInfo) {
        SendPushCursor(cursorInfo);
    }

    void PopCursor() {
        SendPopCursor();
    }

    bool GetNativeCursorsSupported() {
        return Settings().nativeCursorsSupported();
    }
#endif

    int GetQuirks() { return mQuirks; }

    const PluginSettings& Settings() const { return mCachedSettings; }

    NPError PluginRequiresAudioDeviceChanges(PluginInstanceChild* aInstance,
                                             NPBool aShouldRegister);
    mozilla::ipc::IPCResult RecvNPP_SetValue_NPNVaudioDeviceChangeDetails(
        const NPAudioDeviceChangeDetailsIPC& detailsIPC) override;

private:
    NPError DoNP_Initialize(const PluginSettings& aSettings);
    void AddQuirk(PluginQuirks quirk) {
      if (mQuirks == QUIRKS_NOT_INITIALIZED)
        mQuirks = 0;
      mQuirks |= quirk;
    }
    void InitQuirksModes(const nsCString& aMimeType);
    bool InitGraphics();
    void DeinitGraphics();

#if defined(OS_WIN)
    void HookProtectedMode();
#endif

#if defined(MOZ_WIDGET_GTK)
    static gboolean DetectNestedEventLoop(gpointer data);
    static gboolean ProcessBrowserEvents(gpointer data);

    virtual void EnteredCxxStack() override;
    virtual void ExitedCxxStack() override;
#endif

    PRLibrary* mLibrary;
    nsCString mPluginFilename; // UTF8
    int mQuirks;

    bool mIsChrome;
    bool mHasShutdown; // true if NP_Shutdown has run

#ifdef MOZ_GECKO_PROFILER
    RefPtr<ChildProfilerController> mProfilerController;
#endif

    // we get this from the plugin
    NP_PLUGINSHUTDOWN mShutdownFunc;
#if defined(OS_LINUX) || defined(OS_BSD) || defined(OS_SOLARIS)
    NP_PLUGINUNIXINIT mInitializeFunc;
#elif defined(OS_WIN) || defined(OS_MACOSX)
    NP_PLUGININIT mInitializeFunc;
    NP_GETENTRYPOINTS mGetEntryPointsFunc;
#endif

    NPPluginFuncs mFunctions;

    PluginSettings mCachedSettings;

#if defined(MOZ_WIDGET_GTK)
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
#  ifdef DEBUG
    // Depth of the stack of calls to g_main_context_dispatch before any
    // nested loops are run.  This is 1 when IPC calls are dispatched from
    // g_main_context_iteration, or 0 when dispatched directly from
    // MessagePumpForUI.
    int mTopLoopDepth;
#  endif
#endif

#if defined(XP_WIN)
  typedef nsTHashtable<nsPtrHashKey<PluginInstanceChild>> PluginInstanceSet;
  // Set of plugins that have registered to be notified when the audio device
  // changes.
  PluginInstanceSet mAudioNotificationSet;
#endif

public: // called by PluginInstanceChild
    /**
     * Dealloc an NPObject after last-release or when the associated instance
     * is destroyed. This function will remove the object from mObjectMap.
     */
    static void DeallocNPObject(NPObject* o);

    NPError NPP_Destroy(PluginInstanceChild* instance) {
        return mFunctions.destroy(instance->GetNPP(), 0);
    }

private:
#if defined(OS_WIN)
    virtual void EnteredCall() override;
    virtual void ExitedCall() override;

    // Entered/ExitedCall notifications keep track of whether the plugin has
    // entered a nested event loop within this interrupt call.
    struct IncallFrame
    {
        IncallFrame()
            : _spinning(false)
            , _savedNestableTasksAllowed(false)
        { }

        bool _spinning;
        bool _savedNestableTasksAllowed;
    };

    AutoTArray<IncallFrame, 8> mIncallPumpingStack;

    static LRESULT CALLBACK NestedInputEventHook(int code,
                                                 WPARAM wParam,
                                                 LPARAM lParam);
    static LRESULT CALLBACK CallWindowProcHook(int code,
                                               WPARAM wParam,
                                               LPARAM lParam);
    void SetEventHooks();
    void ResetEventHooks();
    HHOOK mNestedEventHook;
    HHOOK mGlobalCallWndProcHook;
public:
    bool mAsyncRenderSupport;
#endif
};

} /* namespace plugins */
} /* namespace mozilla */

#endif  // ifndef dom_plugins_PluginModuleChild_h
