/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_plugins_PluginModuleParent_h
#define mozilla_plugins_PluginModuleParent_h

#include "base/process.h"
#include "mozilla/FileUtils.h"
#include "mozilla/HangMonitor.h"
#include "mozilla/PluginLibrary.h"
#include "mozilla/plugins/ScopedMethodFactory.h"
#include "mozilla/plugins/PluginProcessParent.h"
#include "mozilla/plugins/PPluginModuleParent.h"
#include "mozilla/plugins/PluginMessageUtils.h"
#include "mozilla/plugins/PluginTypes.h"
#include "npapi.h"
#include "npfunctions.h"
#include "nsAutoPtr.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsIObserver.h"

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

namespace mozilla {
namespace dom {
class PCrashReporterParent;
class CrashReporterParent;
}

namespace plugins {
//-----------------------------------------------------------------------------

class BrowserStreamParent;
class PluginInstanceParent;

#ifdef XP_WIN
class PluginHangUIParent;
#endif

/**
 * PluginModuleParent
 *
 * This class implements the NPP API from the perspective of the rest
 * of Gecko, forwarding NPP calls along to the child process that is
 * actually running the plugin.
 *
 * This class /also/ implements a version of the NPN API, because the
 * child process needs to make these calls back into Gecko proper.
 * This class is responsible for "actually" making those function calls.
 *
 * If a plugin is running, there will always be one PluginModuleParent for it in
 * the chrome process. In addition, any content process using the plugin will
 * have its own PluginModuleParent. The subclasses PluginModuleChromeParent and
 * PluginModuleContentParent implement functionality that is specific to one
 * case or the other.
 */
class PluginModuleParent
    : public PPluginModuleParent
    , public PluginLibrary
#ifdef MOZ_CRASHREPORTER_INJECTOR
    , public CrashReporter::InjectorCrashCallback
#endif
{
protected:
    typedef mozilla::PluginLibrary PluginLibrary;
    typedef mozilla::dom::PCrashReporterParent PCrashReporterParent;
    typedef mozilla::dom::CrashReporterParent CrashReporterParent;

    PPluginInstanceParent*
    AllocPPluginInstanceParent(const nsCString& aMimeType,
                               const uint16_t& aMode,
                               const InfallibleTArray<nsCString>& aNames,
                               const InfallibleTArray<nsCString>& aValues,
                               NPError* rv) MOZ_OVERRIDE;

    virtual bool
    DeallocPPluginInstanceParent(PPluginInstanceParent* aActor) MOZ_OVERRIDE;

public:
    explicit PluginModuleParent(bool aIsChrome);
    virtual ~PluginModuleParent();

    bool IsChrome() const { return mIsChrome; }

    virtual void SetPlugin(nsNPAPIPlugin* plugin) MOZ_OVERRIDE
    {
        mPlugin = plugin;
    }

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

    const NPNetscapeFuncs* GetNetscapeFuncs() {
        return mNPNIface;
    }

    bool OkToCleanup() const {
        return !IsOnCxxStack();
    }

    void ProcessRemoteNativeEventsInInterruptCall();

protected:
    virtual mozilla::ipc::RacyInterruptPolicy
    MediateInterruptRace(const Message& parent, const Message& child) MOZ_OVERRIDE
    {
        return MediateRace(parent, child);
    }

    virtual bool
    RecvBackUpXResources(const FileDescriptor& aXSocketFd) MOZ_OVERRIDE;

    virtual bool
    AnswerNPN_UserAgent(nsCString* userAgent) MOZ_OVERRIDE;

    virtual bool
    AnswerNPN_GetValue_WithBoolReturn(const NPNVariable& aVariable,
                                      NPError* aError,
                                      bool* aBoolVal) MOZ_OVERRIDE;

    virtual bool AnswerProcessSomeEvents() MOZ_OVERRIDE;

    virtual bool
    RecvProcessNativeEventsInInterruptCall() MOZ_OVERRIDE;

    virtual bool
    RecvPluginShowWindow(const uint32_t& aWindowId, const bool& aModal,
                         const int32_t& aX, const int32_t& aY,
                         const size_t& aWidth, const size_t& aHeight) MOZ_OVERRIDE;

    virtual bool
    RecvPluginHideWindow(const uint32_t& aWindowId) MOZ_OVERRIDE;

    virtual PCrashReporterParent*
    AllocPCrashReporterParent(mozilla::dom::NativeThreadId* id,
                              uint32_t* processType) MOZ_OVERRIDE;
    virtual bool
    DeallocPCrashReporterParent(PCrashReporterParent* actor) MOZ_OVERRIDE;

    virtual bool
    RecvSetCursor(const NSCursorInfo& aCursorInfo) MOZ_OVERRIDE;

    virtual bool
    RecvShowCursor(const bool& aShow) MOZ_OVERRIDE;

    virtual bool
    RecvPushCursor(const NSCursorInfo& aCursorInfo) MOZ_OVERRIDE;

    virtual bool
    RecvPopCursor() MOZ_OVERRIDE;

    virtual bool
    RecvGetNativeCursorsSupported(bool* supported) MOZ_OVERRIDE;

    virtual bool
    RecvNPN_SetException(const nsCString& aMessage) MOZ_OVERRIDE;

    virtual bool
    RecvNPN_ReloadPlugins(const bool& aReloadPages) MOZ_OVERRIDE;

    static PluginInstanceParent* InstCast(NPP instance);
    static BrowserStreamParent* StreamCast(NPP instance, NPStream* s);

protected:
    virtual void UpdatePluginTimeout() {}

    virtual bool RecvNotifyContentModuleDestroyed() MOZ_OVERRIDE { return true; }

    void SetPluginFuncs(NPPluginFuncs* aFuncs);

    // NPP-like API that Gecko calls are trampolined into.  These 
    // messages then get forwarded along to the plugin instance,
    // and then eventually the child process.

    static NPError NPP_Destroy(NPP instance, NPSavedData** save);

    static NPError NPP_SetWindow(NPP instance, NPWindow* window);
    static NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream,
                                 NPBool seekable, uint16_t* stype);
    static NPError NPP_DestroyStream(NPP instance,
                                     NPStream* stream, NPReason reason);
    static int32_t NPP_WriteReady(NPP instance, NPStream* stream);
    static int32_t NPP_Write(NPP instance, NPStream* stream,
                             int32_t offset, int32_t len, void* buffer);
    static void NPP_StreamAsFile(NPP instance,
                                 NPStream* stream, const char* fname);
    static void NPP_Print(NPP instance, NPPrint* platformPrint);
    static int16_t NPP_HandleEvent(NPP instance, void* event);
    static void NPP_URLNotify(NPP instance, const char* url,
                              NPReason reason, void* notifyData);
    static NPError NPP_GetValue(NPP instance,
                                NPPVariable variable, void *ret_value);
    static NPError NPP_SetValue(NPP instance, NPNVariable variable,
                                void *value);
    static void NPP_URLRedirectNotify(NPP instance, const char* url,
                                      int32_t status, void* notifyData);

    virtual bool HasRequiredFunctions();
    virtual nsresult AsyncSetWindow(NPP instance, NPWindow* window);
    virtual nsresult GetImageContainer(NPP instance, mozilla::layers::ImageContainer** aContainer);
    virtual nsresult GetImageSize(NPP instance, nsIntSize* aSize);
    virtual bool IsOOP() MOZ_OVERRIDE { return true; }
    virtual nsresult SetBackgroundUnknown(NPP instance) MOZ_OVERRIDE;
    virtual nsresult BeginUpdateBackground(NPP instance,
                                           const nsIntRect& aRect,
                                           gfxContext** aCtx) MOZ_OVERRIDE;
    virtual nsresult EndUpdateBackground(NPP instance,
                                         gfxContext* aCtx,
                                         const nsIntRect& aRect) MOZ_OVERRIDE;

#if defined(XP_UNIX) && !defined(XP_MACOSX) && !defined(MOZ_WIDGET_GONK)
    virtual nsresult NP_Initialize(NPNetscapeFuncs* bFuncs, NPPluginFuncs* pFuncs, NPError* error);
#else
    virtual nsresult NP_Initialize(NPNetscapeFuncs* bFuncs, NPError* error);
#endif
    virtual nsresult NP_Shutdown(NPError* error);

    virtual nsresult NP_GetMIMEDescription(const char** mimeDesc);
    virtual nsresult NP_GetValue(void *future, NPPVariable aVariable,
                                 void *aValue, NPError* error);
#if defined(XP_WIN) || defined(XP_MACOSX)
    virtual nsresult NP_GetEntryPoints(NPPluginFuncs* pFuncs, NPError* error);
#endif
    virtual nsresult NPP_New(NPMIMEType pluginType, NPP instance,
                             uint16_t mode, int16_t argc, char* argn[],
                             char* argv[], NPSavedData* saved,
                             NPError* error);
    virtual nsresult NPP_ClearSiteData(const char* site, uint64_t flags,
                                       uint64_t maxAge);
    virtual nsresult NPP_GetSitesWithData(InfallibleTArray<nsCString>& result);

#if defined(XP_MACOSX)
    virtual nsresult IsRemoteDrawingCoreAnimation(NPP instance, bool *aDrawing);
    virtual nsresult ContentsScaleFactorChanged(NPP instance, double aContentsScaleFactor);
#endif

protected:
    void NotifyPluginCrashed();

    bool mIsChrome;
    bool mShutdown;
    bool mClearSiteDataSupported;
    bool mGetSitesWithDataSupported;
    const NPNetscapeFuncs* mNPNIface;
    nsNPAPIPlugin* mPlugin;
    ScopedMethodFactory<PluginModuleParent> mTaskFactory;
    nsString mPluginDumpID;
    nsString mBrowserDumpID;
    nsString mHangID;
    nsRefPtr<nsIObserver> mProfilerObserver;
    nsCString mPluginName;
    nsCString mPluginVersion;

#ifdef MOZ_X11
    // Dup of plugin's X socket, used to scope its resources to this
    // object instead of the plugin process's lifetime
    ScopedClose mPluginXSocketFdDup;
#endif

    bool
    GetPluginDetails(nsACString& aPluginName, nsACString& aPluginVersion);

    friend class mozilla::dom::CrashReporterParent;
};

class PluginModuleContentParent : public PluginModuleParent
{
  public:
    static PluginLibrary* LoadModule(uint32_t aPluginId);

    static PluginModuleContentParent* Create(mozilla::ipc::Transport* aTransport,
                                             base::ProcessId aOtherProcess);

  private:
    explicit PluginModuleContentParent();

#ifdef MOZ_CRASHREPORTER_INJECTOR
    void OnCrash(DWORD processID) MOZ_OVERRIDE {}
#endif

    static PluginModuleContentParent* sSavedModuleParent;
};

class PluginModuleChromeParent
    : public PluginModuleParent
    , public mozilla::HangMonitor::Annotator
{
  public:
    /**
     * LoadModule
     *
     * This may or may not launch a plugin child process,
     * and may or may not be very expensive.
     */
    static PluginLibrary* LoadModule(const char* aFilePath, uint32_t aPluginId);

    virtual ~PluginModuleChromeParent();

    void TerminateChildProcess(MessageLoop* aMsgLoop);

#ifdef XP_WIN
    /**
     * Called by Plugin Hang UI to notify that the user has clicked continue.
     * Used for chrome hang annotations.
     */
    void
    OnHangUIContinue();
#endif // XP_WIN

private:
    virtual void
    EnteredCxxStack() MOZ_OVERRIDE;

    void
    ExitedCxxStack() MOZ_OVERRIDE;

    virtual void
    AnnotateHang(mozilla::HangMonitor::HangAnnotations& aAnnotations) MOZ_OVERRIDE;

    virtual bool ShouldContinueFromReplyTimeout() MOZ_OVERRIDE;

#ifdef MOZ_CRASHREPORTER
    void ProcessFirstMinidump();
    void WriteExtraDataForMinidump(CrashReporter::AnnotationTable& notes);
#endif

    virtual PCrashReporterParent*
    AllocPCrashReporterParent(mozilla::dom::NativeThreadId* id,
                              uint32_t* processType) MOZ_OVERRIDE;
    virtual bool
    DeallocPCrashReporterParent(PCrashReporterParent* actor) MOZ_OVERRIDE;

    PluginProcessParent* Process() const { return mSubprocess; }
    base::ProcessHandle ChildProcessHandle() { return mSubprocess->GetChildProcessHandle(); }

#if !defined(XP_UNIX) || defined(XP_MACOSX) || defined(MOZ_WIDGET_GONK)
    virtual nsresult NP_Initialize(NPNetscapeFuncs* bFuncs, NPError* error);
#endif

    virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

    // aFilePath is UTF8, not native!
    explicit PluginModuleChromeParent(const char* aFilePath, uint32_t aPluginId);

    CrashReporterParent* CrashReporter();

    void CleanupFromTimeout(const bool aByHangUI);
    void SetChildTimeout(const int32_t aChildTimeout);
    static void TimeoutChanged(const char* aPref, void* aModule);

    virtual void UpdatePluginTimeout() MOZ_OVERRIDE;

#ifdef MOZ_ENABLE_PROFILER_SPS
    void InitPluginProfiling();
    void ShutdownPluginProfiling();
#endif

    virtual bool RecvNotifyContentModuleDestroyed() MOZ_OVERRIDE;

    PluginProcessParent* mSubprocess;
    uint32_t mPluginId;

    ScopedMethodFactory<PluginModuleChromeParent> mChromeTaskFactory;

    enum HangAnnotationFlags
    {
        kInPluginCall = (1u << 0),
        kHangUIShown = (1u << 1),
        kHangUIContinued = (1u << 2),
        kHangUIDontShow = (1u << 3)
    };
    Atomic<uint32_t> mHangAnnotationFlags;
#ifdef XP_WIN
    InfallibleTArray<float> mPluginCpuUsageOnHang;
    PluginHangUIParent *mHangUIParent;
    bool mHangUIEnabled;
    bool mIsTimerReset;
#ifdef MOZ_CRASHREPORTER
    /**
     * This mutex protects the crash reporter when the Plugin Hang UI event
     * handler is executing off main thread. It is intended to protect both
     * the mCrashReporter variable in addition to the CrashReporterParent object
     * that mCrashReporter refers to.
     */
    mozilla::Mutex mCrashReporterMutex;
    CrashReporterParent* mCrashReporter;
#endif // MOZ_CRASHREPORTER


    void
    EvaluateHangUIState(const bool aReset);

    /**
     * Launches the Plugin Hang UI.
     *
     * @return true if plugin-hang-ui.exe has been successfully launched.
     *         false if the Plugin Hang UI is disabled, already showing,
     *               or the launch failed.
     */
    bool
    LaunchHangUI();

    /**
     * Finishes the Plugin Hang UI and cancels if it is being shown to the user.
     */
    void
    FinishHangUI();
#endif

    friend class mozilla::dom::CrashReporterParent;

#ifdef MOZ_CRASHREPORTER_INJECTOR
    void InitializeInjector();
    
    void OnCrash(DWORD processID) MOZ_OVERRIDE;

    DWORD mFlashProcess1;
    DWORD mFlashProcess2;
#endif
};

} // namespace plugins
} // namespace mozilla

#endif // mozilla_plugins_PluginModuleParent_h
