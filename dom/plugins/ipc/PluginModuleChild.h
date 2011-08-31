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
#include "nsDataHashtable.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"

#ifdef MOZ_WIDGET_COCOA
#include "PluginInterposeOSX.h"
#endif

#include "mozilla/plugins/PPluginModuleChild.h"
#include "mozilla/plugins/PluginInstanceChild.h"
#include "mozilla/plugins/PluginIdentifierChild.h"

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

#ifdef MOZ_WIDGET_QT
class NestedLoopTimer;
static const int kNestedLoopDetectorIntervalMs = 90;
#endif

class PluginScriptableObjectChild;
class PluginInstanceChild;

class PluginModuleChild : public PPluginModuleChild
{
protected:
    NS_OVERRIDE
    virtual mozilla::ipc::RPCChannel::RacyRPCPolicy
    MediateRPCRace(const Message& parent, const Message& child)
    {
        return MediateRace(parent, child);
    }

    NS_OVERRIDE
    virtual bool ShouldContinueFromReplyTimeout();

    // Implement the PPluginModuleChild interface
    virtual bool AnswerNP_GetEntryPoints(NPError* rv);
    virtual bool AnswerNP_Initialize(NativeThreadId* tid, NPError* rv);

    virtual PPluginIdentifierChild*
    AllocPPluginIdentifier(const nsCString& aString,
                           const int32_t& aInt,
                           const bool& aTemporary);

    virtual bool
    RecvPPluginIdentifierConstructor(PPluginIdentifierChild* actor,
                                     const nsCString& aString,
                                     const int32_t& aInt,
                                     const bool& aTemporary);

    virtual bool
    DeallocPPluginIdentifier(PPluginIdentifierChild* aActor);

    virtual PPluginInstanceChild*
    AllocPPluginInstance(const nsCString& aMimeType,
                         const uint16_t& aMode,
                         const InfallibleTArray<nsCString>& aNames,
                         const InfallibleTArray<nsCString>& aValues,
                         NPError* rv);

    virtual bool
    DeallocPPluginInstance(PPluginInstanceChild* aActor);

    virtual bool
    AnswerPPluginInstanceConstructor(PPluginInstanceChild* aActor,
                                     const nsCString& aMimeType,
                                     const uint16_t& aMode,
                                     const InfallibleTArray<nsCString>& aNames,
                                     const InfallibleTArray<nsCString>& aValues,
                                     NPError* rv);
    virtual bool
    AnswerNP_Shutdown(NPError *rv);

    virtual bool
    AnswerOptionalFunctionsSupported(bool *aURLRedirectNotify,
                                     bool *aClearSiteData,
                                     bool *aGetSitesWithData);

    virtual bool
    AnswerNPP_ClearSiteData(const nsCString& aSite,
                            const uint64_t& aFlags,
                            const uint64_t& aMaxAge,
                            NPError* aResult);

    virtual bool
    AnswerNPP_GetSitesWithData(InfallibleTArray<nsCString>* aResult);

    virtual bool
    RecvSetAudioSessionData(const nsID& aId,
                            const nsString& aDisplayName,
                            const nsString& aIconPath);

    virtual bool
    RecvSetParentHangTimeout(const uint32_t& aSeconds);

    virtual void
    ActorDestroy(ActorDestroyReason why);

    NS_NORETURN void QuickExit();

    NS_OVERRIDE virtual bool
    RecvProcessNativeEventsInRPCCall();

public:
    PluginModuleChild();
    virtual ~PluginModuleChild();

    // aPluginFilename is UTF8, not native-charset!
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

    /**
     * The child implementations of NPIdentifier-related functions.
     */
    static NPIdentifier NP_CALLBACK NPN_GetStringIdentifier(const NPUTF8* aName);
    static void NP_CALLBACK NPN_GetStringIdentifiers(const NPUTF8** aNames,
                                                     int32_t aNameCount,
                                                     NPIdentifier* aIdentifiers);
    static NPIdentifier NP_CALLBACK NPN_GetIntIdentifier(int32_t aIntId);
    static bool NP_CALLBACK NPN_IdentifierIsString(NPIdentifier aIdentifier);
    static NPUTF8* NP_CALLBACK NPN_UTF8FromIdentifier(NPIdentifier aIdentifier);
    static int32_t NP_CALLBACK NPN_IntFromIdentifier(NPIdentifier aIdentifier);

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
        bool supported = false;
        SendGetNativeCursorsSupported(&supported);
        return supported;
    }
#endif

    // Quirks mode support for various plugin mime types
    enum PluginQuirks {
        QUIRKS_NOT_INITIALIZED                          = 0,
        // Silverlight assumes it is transparent in windowless mode. This quirk
        // matches the logic in nsNPAPIPluginInstance::SetWindowless.
        QUIRK_SILVERLIGHT_DEFAULT_TRANSPARENT           = 1 << 0,
        // Win32: Hook TrackPopupMenu api so that we can swap out parent
        // hwnds. The api will fail with parents not associated with our
        // child ui thread. See WinlessHandleEvent for details.
        QUIRK_WINLESS_TRACKPOPUP_HOOK                   = 1 << 1,
        // Win32: Throttle flash WM_USER+1 heart beat messages to prevent
        // flooding chromium's dispatch loop, which can cause ipc traffic
        // processing lag.
        QUIRK_FLASH_THROTTLE_WMUSER_EVENTS              = 1 << 2,
        // Win32: Catch resets on our subclass by hooking SetWindowLong.
        QUIRK_FLASH_HOOK_SETLONGPTR                     = 1 << 3,
        // X11: Work around a bug in Flash up to 10.1 d51 at least, where
        // expose event top left coordinates within the plugin-rect and
        // not at the drawable origin are misinterpreted.
        QUIRK_FLASH_EXPOSE_COORD_TRANSLATION            = 1 << 4,
        // Win32: Catch get window info calls on the browser and tweak the
        // results so mouse input works when flash is displaying it's settings
        // window.
        QUIRK_FLASH_HOOK_GETWINDOWINFO                  = 1 << 5,
        // Win: Addresses a flash bug with mouse capture and full screen
        // windows.
        QUIRK_FLASH_FIXUP_MOUSE_CAPTURE                 = 1 << 6,
        // Win: QuickTime steals focus on SetWindow calls even if it's hidden.
        // Avoid calling SetWindow in that case.
        QUIRK_QUICKTIME_AVOID_SETWINDOW                 = 1 << 7,
        // Win: Check to make sure the parent window has focus before calling
        // set focus on the child. Addresses a full screen dialog prompt
        // problem in Silverlight.
        QUIRK_SILVERLIGHT_FOCUS_CHECK_PARENT            = 1 << 8,
    };

    int GetQuirks() { return mQuirks; }

private:
    void AddQuirk(PluginQuirks quirk) {
      if (mQuirks == QUIRKS_NOT_INITIALIZED)
        mQuirks = 0;
      mQuirks |= quirk;
    }
    void InitQuirksModes(const nsCString& aMimeType);
    bool InitGraphics();
    void DeinitGraphics();
#if defined(MOZ_WIDGET_GTK2)
    static gboolean DetectNestedEventLoop(gpointer data);
    static gboolean ProcessBrowserEvents(gpointer data);

    NS_OVERRIDE
    virtual void EnteredCxxStack();
    NS_OVERRIDE
    virtual void ExitedCxxStack();
#elif defined(MOZ_WIDGET_QT)

    NS_OVERRIDE
    virtual void EnteredCxxStack();
    NS_OVERRIDE
    virtual void ExitedCxxStack();
#endif

    PRLibrary* mLibrary;
    nsCString mPluginFilename; // UTF8
    nsCString mUserAgent;
    int mQuirks;

    // we get this from the plugin
    NP_PLUGINSHUTDOWN mShutdownFunc;
#ifdef OS_LINUX
    NP_PLUGINUNIXINIT mInitializeFunc;
#elif defined(OS_WIN) || defined(OS_MACOSX)
    NP_PLUGININIT mInitializeFunc;
    NP_GETENTRYPOINTS mGetEntryPointsFunc;
#endif

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
#  ifdef DEBUG
    // Depth of the stack of calls to g_main_context_dispatch before any
    // nested loops are run.  This is 1 when IPC calls are dispatched from
    // g_main_context_iteration, or 0 when dispatched directly from
    // MessagePumpForUI.
    int mTopLoopDepth;
#  endif
#elif defined (MOZ_WIDGET_QT)
    NestedLoopTimer *mNestedLoopTimerObject;
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

    friend class PluginIdentifierChild;
    friend class PluginIdentifierChildString;
    friend class PluginIdentifierChildInt;
    nsDataHashtable<nsCStringHashKey, PluginIdentifierChildString*> mStringIdentifiers;
    nsDataHashtable<nsUint32HashKey, PluginIdentifierChildInt*> mIntIdentifiers;

public: // called by PluginInstanceChild
    /**
     * Dealloc an NPObject after last-release or when the associated instance
     * is destroyed. This function will remove the object from mObjectMap.
     */
    static void DeallocNPObject(NPObject* o);

    NPError NPP_Destroy(PluginInstanceChild* instance) {
        return mFunctions.destroy(instance->GetNPP(), 0);
    }

    /**
     * Fill PluginInstanceChild.mDeletingHash with all the remaining NPObjects
     * associated with that instance.
     */
    void FindNPObjectsForInstance(PluginInstanceChild* instance);

private:
    static PLDHashOperator CollectForInstance(NPObjectData* d, void* userArg);

#if defined(OS_WIN)
    NS_OVERRIDE
    virtual void EnteredCall();
    NS_OVERRIDE
    virtual void ExitedCall();

    // Entered/ExitedCall notifications keep track of whether the plugin has
    // entered a nested event loop within this RPC call.
    struct IncallFrame
    {
        IncallFrame()
            : _spinning(false)
            , _savedNestableTasksAllowed(false)
        { }

        bool _spinning;
        bool _savedNestableTasksAllowed;
    };

    nsAutoTArray<IncallFrame, 8> mIncallPumpingStack;

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
#endif
};

} /* namespace plugins */
} /* namespace mozilla */

#endif  // ifndef dom_plugins_PluginModuleChild_h
