/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=4 et : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_QT
#include <unistd.h> // for _exit()
#include <QtCore/QTimer>
#include "nsQAppInstance.h"
#include "NestedLoopTimer.h"
#endif

#include "mozilla/plugins/PluginModuleChild.h"

/* This must occur *after* plugins/PluginModuleChild.h to avoid typedefs conflicts. */
#include "mozilla/ArrayUtils.h"

#include "mozilla/ipc/MessageChannel.h"

#ifdef MOZ_WIDGET_GTK
#include <gtk/gtk.h>
#endif

#include "nsIFile.h"

#include "pratom.h"
#include "nsDebug.h"
#include "nsCOMPtr.h"
#include "nsPluginsDir.h"
#include "nsXULAppAPI.h"

#ifdef MOZ_X11
# include "mozilla/X11Util.h"
#endif
#include "mozilla/plugins/PluginInstanceChild.h"
#include "mozilla/plugins/StreamNotifyChild.h"
#include "mozilla/plugins/BrowserStreamChild.h"
#include "mozilla/plugins/PluginStreamChild.h"
#include "mozilla/dom/CrashReporterChild.h"

#include "nsNPAPIPlugin.h"

#ifdef XP_WIN
#include "nsWindowsDllInterceptor.h"
#include "mozilla/widget/AudioSession.h"
#endif

#ifdef MOZ_WIDGET_COCOA
#include "PluginInterposeOSX.h"
#include "PluginUtilsOSX.h"
#endif

#include "GeckoProfiler.h"

using namespace mozilla;
using namespace mozilla::plugins;
using mozilla::dom::CrashReporterChild;
using mozilla::dom::PCrashReporterChild;

#if defined(XP_WIN)
const wchar_t * kFlashFullscreenClass = L"ShockwaveFlashFullScreen";
const wchar_t * kMozillaWindowClass = L"MozillaWindowClass";
#endif

namespace {
// see PluginModuleChild::GetChrome()
PluginModuleChild* gChromeInstance = nullptr;
nsTArray<PluginModuleChild*>* gAllInstances;
}

#ifdef MOZ_WIDGET_QT
typedef void (*_gtk_init_fn)(int argc, char **argv);
static _gtk_init_fn s_gtk_init = nullptr;
static PRLibrary *sGtkLib = nullptr;
#endif

#ifdef XP_WIN
// Hooking CreateFileW for protected-mode magic
static WindowsDllInterceptor sKernel32Intercept;
typedef HANDLE (WINAPI *CreateFileWPtr)(LPCWSTR fname, DWORD access,
                                        DWORD share,
                                        LPSECURITY_ATTRIBUTES security,
                                        DWORD creation, DWORD flags,
                                        HANDLE ftemplate);
static CreateFileWPtr sCreateFileWStub = nullptr;
typedef HANDLE (WINAPI *CreateFileAPtr)(LPCSTR fname, DWORD access,
                                        DWORD share,
                                        LPSECURITY_ATTRIBUTES security,
                                        DWORD creation, DWORD flags,
                                        HANDLE ftemplate);
static CreateFileAPtr sCreateFileAStub = nullptr;

// Used with fix for flash fullscreen window loosing focus.
static bool gDelayFlashFocusReplyUntilEval = false;
// Used to fix GetWindowInfo problems with internal flash settings dialogs
static WindowsDllInterceptor sUser32Intercept;
typedef BOOL (WINAPI *GetWindowInfoPtr)(HWND hwnd, PWINDOWINFO pwi);
static GetWindowInfoPtr sGetWindowInfoPtrStub = nullptr;
static HWND sBrowserHwnd = nullptr;
#endif

template<>
struct RunnableMethodTraits<PluginModuleChild>
{
    static void RetainCallee(PluginModuleChild* obj) { }
    static void ReleaseCallee(PluginModuleChild* obj) { }
};

/* static */
PluginModuleChild*
PluginModuleChild::CreateForContentProcess(mozilla::ipc::Transport* aTransport,
                                           base::ProcessId aOtherPid)
{
    PluginModuleChild* child = new PluginModuleChild(false);

    if (!child->InitForContent(aOtherPid, XRE_GetIOMessageLoop(), aTransport)) {
        return nullptr;
    }

    return child;
}

PluginModuleChild::PluginModuleChild(bool aIsChrome)
  : mLibrary(0)
  , mPluginFilename("")
  , mQuirks(QUIRKS_NOT_INITIALIZED)
  , mIsChrome(aIsChrome)
  , mTransport(nullptr)
  , mShutdownFunc(0)
  , mInitializeFunc(0)
#if defined(OS_WIN) || defined(OS_MACOSX)
  , mGetEntryPointsFunc(0)
#elif defined(MOZ_WIDGET_GTK)
  , mNestedLoopTimerId(0)
#elif defined(MOZ_WIDGET_QT)
  , mNestedLoopTimerObject(0)
#endif
#ifdef OS_WIN
  , mNestedEventHook(nullptr)
  , mGlobalCallWndProcHook(nullptr)
#endif
{
    if (!gAllInstances) {
        gAllInstances = new nsTArray<PluginModuleChild*>(1);
    }
    gAllInstances->AppendElement(this);

    memset(&mFunctions, 0, sizeof(mFunctions));
    if (mIsChrome) {
        MOZ_ASSERT(!gChromeInstance);
        gChromeInstance = this;
    }
    mUserAgent.SetIsVoid(true);
#ifdef XP_MACOSX
    if (aIsChrome) {
      mac_plugin_interposing::child::SetUpCocoaInterposing();
    }
#endif
}

PluginModuleChild::~PluginModuleChild()
{
    if (mTransport) {
        // For some reason IPDL doesn't autmatically delete the channel for a
        // bridged protocol (bug 1090570). So we have to do it ourselves. This
        // code is only invoked for PluginModuleChild instances created via
        // bridging; otherwise mTransport is null.
        XRE_GetIOMessageLoop()->PostTask(FROM_HERE, new DeleteTask<Transport>(mTransport));
    }

    gAllInstances->RemoveElement(this);
    MOZ_ASSERT_IF(mIsChrome, gAllInstances->Length() == 0);
    if (gAllInstances->IsEmpty()) {
        delete gAllInstances;
        gAllInstances = nullptr;
    }

    if (mIsChrome) {
        MOZ_ASSERT(gChromeInstance == this);

        // We don't unload the plugin library in case it uses atexit handlers or
        // other similar hooks.

        DeinitGraphics();
        PluginScriptableObjectChild::ClearIdentifiers();

        gChromeInstance = nullptr;
    }
}

// static
PluginModuleChild*
PluginModuleChild::GetChrome()
{
    // A special PluginModuleChild instance that talks to the chrome process
    // during startup and shutdown. Synchronous messages to or from this actor
    // should be avoided because they may lead to hangs.
    MOZ_ASSERT(gChromeInstance);
    return gChromeInstance;
}

bool
PluginModuleChild::CommonInit(base::ProcessId aParentPid,
                              MessageLoop* aIOLoop,
                              IPC::Channel* aChannel)
{
    PLUGIN_LOG_DEBUG_METHOD;

    // Request Windows message deferral behavior on our channel. This
    // applies to the top level and all sub plugin protocols since they
    // all share the same channel.
    // Bug 1090573 - Don't do this for connections to content processes.
    GetIPCChannel()->SetChannelFlags(MessageChannel::REQUIRE_DEFERRED_MESSAGE_PROTECTION);

    if (!Open(aChannel, aParentPid, aIOLoop)) {
        return false;
    }

    memset((void*) &mFunctions, 0, sizeof(mFunctions));
    mFunctions.size = sizeof(mFunctions);
    mFunctions.version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;

    return true;
}

bool
PluginModuleChild::InitForContent(base::ProcessId aParentPid,
                                  MessageLoop* aIOLoop,
                                  IPC::Channel* aChannel)
{
    if (!CommonInit(aParentPid, aIOLoop, aChannel)) {
        return false;
    }

    mTransport = aChannel;

    mLibrary = GetChrome()->mLibrary;
    mFunctions = GetChrome()->mFunctions;

    return true;
}

bool
PluginModuleChild::RecvDisableFlashProtectedMode()
{
    MOZ_ASSERT(mIsChrome);
#ifdef XP_WIN
    HookProtectedMode();
#else
    MOZ_ASSERT(false, "Should not be called");
#endif
    return true;
}

bool
PluginModuleChild::InitForChrome(const std::string& aPluginFilename,
                                 base::ProcessId aParentPid,
                                 MessageLoop* aIOLoop,
                                 IPC::Channel* aChannel)
{
    NS_ASSERTION(aChannel, "need a channel");

    if (!InitGraphics())
        return false;

    mPluginFilename = aPluginFilename.c_str();
    nsCOMPtr<nsIFile> localFile;
    NS_NewLocalFile(NS_ConvertUTF8toUTF16(mPluginFilename),
                    true,
                    getter_AddRefs(localFile));

    if (!localFile)
        return false;

    bool exists;
    localFile->Exists(&exists);
    NS_ASSERTION(exists, "plugin file ain't there");

    nsPluginFile pluginFile(localFile);

#if defined(MOZ_X11) || defined(OS_MACOSX)
    nsPluginInfo info = nsPluginInfo();
    if (NS_FAILED(pluginFile.GetPluginInfo(info, &mLibrary))) {
        return false;
    }

#if defined(MOZ_X11)
    NS_NAMED_LITERAL_CSTRING(flash10Head, "Shockwave Flash 10.");
    if (StringBeginsWith(nsDependentCString(info.fDescription), flash10Head)) {
        AddQuirk(QUIRK_FLASH_EXPOSE_COORD_TRANSLATION);
    }
#else // defined(OS_MACOSX)
    const char* namePrefix = "Plugin Content";
    char nameBuffer[80];
    snprintf(nameBuffer, sizeof(nameBuffer), "%s (%s)", namePrefix, info.fName);
    mozilla::plugins::PluginUtilsOSX::SetProcessName(nameBuffer);
#endif

    pluginFile.FreePluginInfo(info);

    if (!mLibrary)
#endif
    {
        nsresult rv = pluginFile.LoadPlugin(&mLibrary);
        if (NS_FAILED(rv))
            return false;
    }
    NS_ASSERTION(mLibrary, "couldn't open shared object");

    if (!CommonInit(aParentPid, aIOLoop, aChannel)) {
        return false;
    }

    GetIPCChannel()->SetAbortOnError(true);

    // TODO: use PluginPRLibrary here

#if defined(OS_LINUX) || defined(OS_BSD)
    mShutdownFunc =
        (NP_PLUGINSHUTDOWN) PR_FindFunctionSymbol(mLibrary, "NP_Shutdown");

    // create the new plugin handler

    mInitializeFunc =
        (NP_PLUGINUNIXINIT) PR_FindFunctionSymbol(mLibrary, "NP_Initialize");
    NS_ASSERTION(mInitializeFunc, "couldn't find NP_Initialize()");

#elif defined(OS_WIN) || defined(OS_MACOSX)
    mShutdownFunc =
        (NP_PLUGINSHUTDOWN)PR_FindFunctionSymbol(mLibrary, "NP_Shutdown");

    mGetEntryPointsFunc =
        (NP_GETENTRYPOINTS)PR_FindSymbol(mLibrary, "NP_GetEntryPoints");
    NS_ENSURE_TRUE(mGetEntryPointsFunc, false);

    mInitializeFunc =
        (NP_PLUGININIT)PR_FindFunctionSymbol(mLibrary, "NP_Initialize");
    NS_ENSURE_TRUE(mInitializeFunc, false);
#else

#  error Please copy the initialization code from nsNPAPIPlugin.cpp

#endif

    return true;
}

#if defined(MOZ_WIDGET_GTK)
typedef void (*GObjectDisposeFn)(GObject*);
typedef gboolean (*GtkWidgetScrollEventFn)(GtkWidget*, GdkEventScroll*);
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
    g_object_add_toggle_ref(object, undo_bogus_unref, nullptr);
    (*real_gtk_plug_dispose)(object);
    g_object_remove_toggle_ref(object, undo_bogus_unref, nullptr);
}

static gboolean
gtk_plug_scroll_event(GtkWidget *widget, GdkEventScroll *gdk_event)
{
    if (!gtk_widget_is_toplevel(widget)) // in same process as its GtkSocket
        return FALSE; // event not handled; propagate to GtkSocket

    GdkWindow* socket_window = gtk_plug_get_socket_window(GTK_PLUG(widget));
    if (!socket_window)
        return FALSE;

    // Propagate the event to the embedder.
    GdkScreen* screen = gdk_window_get_screen(socket_window);
    GdkWindow* plug_window = gtk_widget_get_window(widget);
    GdkWindow* event_window = gdk_event->window;
    gint x = gdk_event->x;
    gint y = gdk_event->y;
    unsigned int button;
    unsigned int button_mask = 0;
    XEvent xevent;
    Display* dpy = GDK_WINDOW_XDISPLAY(socket_window);

    /* Translate the event coordinates to the plug window,
     * which should be aligned with the socket window.
     */
    while (event_window != plug_window)
    {
        gint dx, dy;

        gdk_window_get_position(event_window, &dx, &dy);
        x += dx;
        y += dy;

        event_window = gdk_window_get_parent(event_window);
        if (!event_window)
            return FALSE;
    }

    switch (gdk_event->direction) {
    case GDK_SCROLL_UP:
        button = 4;
        button_mask = Button4Mask;
        break;
    case GDK_SCROLL_DOWN:
        button = 5;
        button_mask = Button5Mask;
        break;
    case GDK_SCROLL_LEFT:
        button = 6;
        break;
    case GDK_SCROLL_RIGHT:
        button = 7;
        break;
    default:
        return FALSE; // unknown GdkScrollDirection
    }

    memset(&xevent, 0, sizeof(xevent));
    xevent.xbutton.type = ButtonPress;
    xevent.xbutton.window = gdk_x11_window_get_xid(socket_window);
    xevent.xbutton.root = gdk_x11_window_get_xid(gdk_screen_get_root_window(screen));
    xevent.xbutton.subwindow = gdk_x11_window_get_xid(plug_window);
    xevent.xbutton.time = gdk_event->time;
    xevent.xbutton.x = x;
    xevent.xbutton.y = y;
    xevent.xbutton.x_root = gdk_event->x_root;
    xevent.xbutton.y_root = gdk_event->y_root;
    xevent.xbutton.state = gdk_event->state;
    xevent.xbutton.button = button;
    xevent.xbutton.same_screen = True;

    gdk_error_trap_push();

    XSendEvent(dpy, xevent.xbutton.window,
               True, ButtonPressMask, &xevent);

    xevent.xbutton.type = ButtonRelease;
    xevent.xbutton.state |= button_mask;
    XSendEvent(dpy, xevent.xbutton.window,
               True, ButtonReleaseMask, &xevent);

    gdk_display_sync(gdk_screen_get_display(screen));
    gdk_error_trap_pop();

    return TRUE; // event handled
}

static void
wrap_gtk_plug_embedded(GtkPlug* plug) {
    GdkWindow* socket_window = gtk_plug_get_socket_window(plug);
    if (socket_window) {
        if (gtk_check_version(2,18,7) != nullptr // older
            && g_object_get_data(G_OBJECT(socket_window),
                                 "moz-existed-before-set-window")) {
            // Add missing reference for
            // https://bugzilla.gnome.org/show_bug.cgi?id=607061
            g_object_ref(socket_window);
        }

        // Ensure the window exists to make this GtkPlug behave like an
        // in-process GtkPlug for Flash Player.  (Bugs 561308 and 539138).
        gtk_widget_realize(GTK_WIDGET(plug));
    }

    if (*real_gtk_plug_embedded) {
        (*real_gtk_plug_embedded)(plug);
    }
}

//
// The next four constants are knobs that can be tuned.  They trade
// off potential UI lag from delayed event processing with CPU time.
//
static const gint kNestedLoopDetectorPriority = G_PRIORITY_HIGH_IDLE;
// 90ms so that we can hopefully break livelocks before the user
// notices UI lag (100ms)
static const guint kNestedLoopDetectorIntervalMs = 90;

static const gint kBrowserEventPriority = G_PRIORITY_HIGH_IDLE;
static const guint kBrowserEventIntervalMs = 10;

// static
gboolean
PluginModuleChild::DetectNestedEventLoop(gpointer data)
{
    PluginModuleChild* pmc = static_cast<PluginModuleChild*>(data);

    MOZ_ASSERT(0 != pmc->mNestedLoopTimerId,
               "callback after descheduling");
    MOZ_ASSERT(pmc->mTopLoopDepth < g_main_depth(),
               "not canceled before returning to main event loop!");

    PLUGIN_LOG_DEBUG(("Detected nested glib event loop"));

    // just detected a nested loop; start a timer that will
    // periodically rpc-call back into the browser and process some
    // events
    pmc->mNestedLoopTimerId =
        g_timeout_add_full(kBrowserEventPriority,
                           kBrowserEventIntervalMs,
                           PluginModuleChild::ProcessBrowserEvents,
                           data,
                           nullptr);
    // cancel the nested-loop detection timer
    return FALSE;
}

// static
gboolean
PluginModuleChild::ProcessBrowserEvents(gpointer data)
{
    PluginModuleChild* pmc = static_cast<PluginModuleChild*>(data);

    MOZ_ASSERT(pmc->mTopLoopDepth < g_main_depth(),
               "not canceled before returning to main event loop!");

    pmc->CallProcessSomeEvents();

    return TRUE;
}

void
PluginModuleChild::EnteredCxxStack()
{
    MOZ_ASSERT(0 == mNestedLoopTimerId,
               "previous timer not descheduled");

    mNestedLoopTimerId =
        g_timeout_add_full(kNestedLoopDetectorPriority,
                           kNestedLoopDetectorIntervalMs,
                           PluginModuleChild::DetectNestedEventLoop,
                           this,
                           nullptr);

#ifdef DEBUG
    mTopLoopDepth = g_main_depth();
#endif
}

void
PluginModuleChild::ExitedCxxStack()
{
    MOZ_ASSERT(0 < mNestedLoopTimerId,
               "nested loop timeout not scheduled");

    g_source_remove(mNestedLoopTimerId);
    mNestedLoopTimerId = 0;
}
#elif defined (MOZ_WIDGET_QT)

void
PluginModuleChild::EnteredCxxStack()
{
    MOZ_ASSERT(mNestedLoopTimerObject == nullptr,
               "previous timer not descheduled");
    mNestedLoopTimerObject = new NestedLoopTimer(this);
    QTimer::singleShot(kNestedLoopDetectorIntervalMs,
                       mNestedLoopTimerObject, SLOT(timeOut()));
}

void
PluginModuleChild::ExitedCxxStack()
{
    MOZ_ASSERT(mNestedLoopTimerObject != nullptr,
               "nested loop timeout not scheduled");
    delete mNestedLoopTimerObject;
    mNestedLoopTimerObject = nullptr;
}

#endif

bool
PluginModuleChild::RecvSetParentHangTimeout(const uint32_t& aSeconds)
{
#ifdef XP_WIN
    SetReplyTimeoutMs(((aSeconds > 0) ? (1000 * aSeconds) : 0));
#endif
    return true;
}

bool
PluginModuleChild::ShouldContinueFromReplyTimeout()
{
#ifdef XP_WIN
    NS_RUNTIMEABORT("terminating child process");
#endif
    return true;
}

bool
PluginModuleChild::InitGraphics()
{
#if defined(MOZ_WIDGET_GTK)
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
    MOZ_ASSERT(*dispose != wrap_gtk_plug_dispose,
               "InitGraphics called twice");
    real_gtk_plug_dispose = *dispose;
    *dispose = wrap_gtk_plug_dispose;

    // If we ever stop setting GDK_NATIVE_WINDOWS, we'll also need to
    // gtk_widget_add_events GDK_SCROLL_MASK or GDK client-side windows will
    // not tell us about the scroll events that it intercepts.  With native
    // windows, this is called when GDK intercepts the events; if GDK doesn't
    // intercept the events, then the X server will instead send them directly
    // to an ancestor (embedder) window.
    GtkWidgetScrollEventFn* scroll_event =
        &GTK_WIDGET_CLASS(gtk_plug_class)->scroll_event;
    if (!*scroll_event) {
        *scroll_event = gtk_plug_scroll_event;
    }

    GtkPlugEmbeddedFn* embedded = &GTK_PLUG_CLASS(gtk_plug_class)->embedded;
    real_gtk_plug_embedded = *embedded;
    *embedded = wrap_gtk_plug_embedded;

#elif defined(MOZ_WIDGET_QT)
    nsQAppInstance::AddRef();
    // Work around plugins that don't interact well without gtk initialized
    // see bug 566845
#if defined(MOZ_X11)
    if (!sGtkLib)
         sGtkLib = PR_LoadLibrary("libgtk-x11-2.0.so.0");
#endif
    if (sGtkLib) {
         s_gtk_init = (_gtk_init_fn)PR_FindFunctionSymbol(sGtkLib, "gtk_init");
         if (s_gtk_init)
             s_gtk_init(0, 0);
    }
#else
    // may not be necessary on all platforms
#endif
#ifdef MOZ_X11
    // Do this after initializing GDK, or GDK will install its own handler.
    XRE_InstallX11ErrorHandler();
#endif
    return true;
}

void
PluginModuleChild::DeinitGraphics()
{
#ifdef MOZ_WIDGET_QT
    nsQAppInstance::Release();
    if (sGtkLib) {
        PR_UnloadLibrary(sGtkLib);
        sGtkLib = nullptr;
        s_gtk_init = nullptr;
    }
#endif

#if defined(MOZ_X11) && defined(NS_FREE_PERMANENT_DATA)
    // We free some data off of XDisplay close hooks, ensure they're
    // run.  Closing the display is pretty scary, so we only do it to
    // silence leak checkers.
    XCloseDisplay(DefaultXDisplay());
#endif
}

bool
PluginModuleChild::AnswerNP_Shutdown(NPError *rv)
{
    AssertPluginThread();
    MOZ_ASSERT(mIsChrome);

#if defined XP_WIN
    mozilla::widget::StopAudioSession();
#endif

    // the PluginModuleParent shuts down this process after this interrupt
    // call pops off its stack

    *rv = mShutdownFunc ? mShutdownFunc() : NPERR_NO_ERROR;

    // weakly guard against re-entry after NP_Shutdown
    memset(&mFunctions, 0, sizeof(mFunctions));

#ifdef OS_WIN
    ResetEventHooks();
#endif

    GetIPCChannel()->SetAbortOnError(false);

    return true;
}

bool
PluginModuleChild::AnswerOptionalFunctionsSupported(bool *aURLRedirectNotify,
                                                    bool *aClearSiteData,
                                                    bool *aGetSitesWithData)
{
    *aURLRedirectNotify = !!mFunctions.urlredirectnotify;
    *aClearSiteData = !!mFunctions.clearsitedata;
    *aGetSitesWithData = !!mFunctions.getsiteswithdata;
    return true;
}

bool
PluginModuleChild::AnswerNPP_ClearSiteData(const nsCString& aSite,
                                           const uint64_t& aFlags,
                                           const uint64_t& aMaxAge,
                                           NPError* aResult)
{
    *aResult =
        mFunctions.clearsitedata(NullableStringGet(aSite), aFlags, aMaxAge);
    return true;
}

bool
PluginModuleChild::AnswerNPP_GetSitesWithData(InfallibleTArray<nsCString>* aResult)
{
    char** result = mFunctions.getsiteswithdata();
    if (!result)
        return true;

    char** iterator = result;
    while (*iterator) {
        aResult->AppendElement(*iterator);
        NS_Free(*iterator);
        ++iterator;
    }
    NS_Free(result);

    return true;
}

bool
PluginModuleChild::RecvSetAudioSessionData(const nsID& aId,
                                           const nsString& aDisplayName,
                                           const nsString& aIconPath)
{
#if !defined XP_WIN
    NS_RUNTIMEABORT("Not Reached!");
    return false;
#else
    nsresult rv = mozilla::widget::RecvAudioSessionData(aId, aDisplayName, aIconPath);
    NS_ENSURE_SUCCESS(rv, true); // Bail early if this fails

    // Ignore failures here; we can't really do anything about them
    mozilla::widget::StartAudioSession();
    return true;
#endif
}

void
PluginModuleChild::QuickExit()
{
    NS_WARNING("plugin process _exit()ing");
    _exit(0);
}

PPluginModuleChild*
PluginModuleChild::AllocPPluginModuleChild(mozilla::ipc::Transport* aTransport,
                                           base::ProcessId aOtherPid)
{
    return PluginModuleChild::CreateForContentProcess(aTransport, aOtherPid);
}

PCrashReporterChild*
PluginModuleChild::AllocPCrashReporterChild(mozilla::dom::NativeThreadId* id,
                                            uint32_t* processType)
{
    return new CrashReporterChild();
}

bool
PluginModuleChild::DeallocPCrashReporterChild(PCrashReporterChild* actor)
{
    delete actor;
    return true;
}

bool
PluginModuleChild::AnswerPCrashReporterConstructor(
        PCrashReporterChild* actor,
        mozilla::dom::NativeThreadId* id,
        uint32_t* processType)
{
#ifdef MOZ_CRASHREPORTER
    *id = CrashReporter::CurrentThreadId();
    *processType = XRE_GetProcessType();
#endif
    return true;
}

void
PluginModuleChild::ActorDestroy(ActorDestroyReason why)
{
    if (!mIsChrome) {
        PluginModuleChild* chromeInstance = PluginModuleChild::GetChrome();
        if (chromeInstance) {
            chromeInstance->SendNotifyContentModuleDestroyed();
        }

        // Destroy ourselves once we finish other teardown activities.
        MessageLoop::current()->PostTask(FROM_HERE, new DeleteTask<PluginModuleChild>(this));
        return;
    }

    if (AbnormalShutdown == why) {
        NS_WARNING("shutting down early because of crash!");
        QuickExit();
    }

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
    return NullableStringGet(Settings().userAgent());
}

//-----------------------------------------------------------------------------
// FIXME/cjones: just getting this out of the way for the moment ...

namespace mozilla {
namespace plugins {
namespace child {

static NPError
_requestread(NPStream *pstream, NPByteRange *rangeList);

static NPError
_geturlnotify(NPP aNPP, const char* relativeURL, const char* target,
              void* notifyData);

static NPError
_getvalue(NPP aNPP, NPNVariable variable, void *r_value);

static NPError
_setvalue(NPP aNPP, NPPVariable variable, void *r_value);

static NPError
_geturl(NPP aNPP, const char* relativeURL, const char* target);

static NPError
_posturlnotify(NPP aNPP, const char* relativeURL, const char *target,
               uint32_t len, const char *buf, NPBool file, void* notifyData);

static NPError
_posturl(NPP aNPP, const char* relativeURL, const char *target, uint32_t len,
         const char *buf, NPBool file);

static NPError
_newstream(NPP aNPP, NPMIMEType type, const char* window, NPStream** pstream);

static int32_t
_write(NPP aNPP, NPStream *pstream, int32_t len, void *buffer);

static NPError
_destroystream(NPP aNPP, NPStream *pstream, NPError reason);

static void
_status(NPP aNPP, const char *message);

static void
_memfree (void *ptr);

static uint32_t
_memflush(uint32_t size);

static void
_reloadplugins(NPBool reloadPages);

static void
_invalidaterect(NPP aNPP, NPRect *invalidRect);

static void
_invalidateregion(NPP aNPP, NPRegion invalidRegion);

static void
_forceredraw(NPP aNPP);

static const char*
_useragent(NPP aNPP);

static void*
_memalloc (uint32_t size);

// Deprecated entry points for the old Java plugin.
static void* /* OJI type: JRIEnv* */
_getjavaenv(void);

// Deprecated entry points for the old Java plugin.
static void* /* OJI type: jref */
_getjavapeer(NPP aNPP);

static bool
_invoke(NPP aNPP, NPObject* npobj, NPIdentifier method, const NPVariant *args,
        uint32_t argCount, NPVariant *result);

static bool
_invokedefault(NPP aNPP, NPObject* npobj, const NPVariant *args,
               uint32_t argCount, NPVariant *result);

static bool
_evaluate(NPP aNPP, NPObject* npobj, NPString *script, NPVariant *result);

static bool
_getproperty(NPP aNPP, NPObject* npobj, NPIdentifier property,
             NPVariant *result);

static bool
_setproperty(NPP aNPP, NPObject* npobj, NPIdentifier property,
             const NPVariant *value);

static bool
_removeproperty(NPP aNPP, NPObject* npobj, NPIdentifier property);

static bool
_hasproperty(NPP aNPP, NPObject* npobj, NPIdentifier propertyName);

static bool
_hasmethod(NPP aNPP, NPObject* npobj, NPIdentifier methodName);

static bool
_enumerate(NPP aNPP, NPObject *npobj, NPIdentifier **identifier,
           uint32_t *count);

static bool
_construct(NPP aNPP, NPObject* npobj, const NPVariant *args,
           uint32_t argCount, NPVariant *result);

static void
_releasevariantvalue(NPVariant *variant);

static void
_setexception(NPObject* npobj, const NPUTF8 *message);

static void
_pushpopupsenabledstate(NPP aNPP, NPBool enabled);

static void
_poppopupsenabledstate(NPP aNPP);

static void
_pluginthreadasynccall(NPP instance, PluginThreadCallback func,
                       void *userData);

static NPError
_getvalueforurl(NPP npp, NPNURLVariable variable, const char *url,
                char **value, uint32_t *len);

static NPError
_setvalueforurl(NPP npp, NPNURLVariable variable, const char *url,
                const char *value, uint32_t len);

static NPError
_getauthenticationinfo(NPP npp, const char *protocol,
                       const char *host, int32_t port,
                       const char *scheme, const char *realm,
                       char **username, uint32_t *ulen,
                       char **password, uint32_t *plen);

static uint32_t
_scheduletimer(NPP instance, uint32_t interval, NPBool repeat,
               void (*timerFunc)(NPP npp, uint32_t timerID));

static void
_unscheduletimer(NPP instance, uint32_t timerID);

static NPError
_popupcontextmenu(NPP instance, NPMenu* menu);

static NPBool
_convertpoint(NPP instance, 
              double sourceX, double sourceY, NPCoordinateSpace sourceSpace,
              double *destX, double *destY, NPCoordinateSpace destSpace);

static void
_urlredirectresponse(NPP instance, void* notifyData, NPBool allow);

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
    PluginModuleChild::NPN_GetStringIdentifier,
    PluginModuleChild::NPN_GetStringIdentifiers,
    PluginModuleChild::NPN_GetIntIdentifier,
    PluginModuleChild::NPN_IdentifierIsString,
    PluginModuleChild::NPN_UTF8FromIdentifier,
    PluginModuleChild::NPN_IntFromIdentifier,
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
    mozilla::plugins::child::_convertpoint,
    nullptr, // handleevent, unimplemented
    nullptr, // unfocusinstance, unimplemented
    mozilla::plugins::child::_urlredirectresponse
};

PluginInstanceChild*
InstCast(NPP aNPP)
{
    MOZ_ASSERT(!!(aNPP->ndata), "nil instance");
    return static_cast<PluginInstanceChild*>(aNPP->ndata);
}

namespace mozilla {
namespace plugins {
namespace child {

NPError
_requestread(NPStream* aStream,
             NPByteRange* aRangeList)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(NPERR_INVALID_PARAM);

    BrowserStreamChild* bs =
        static_cast<BrowserStreamChild*>(static_cast<AStream*>(aStream->ndata));
    bs->EnsureCorrectStream(aStream);
    return bs->NPN_RequestRead(aRangeList);
}

NPError
_geturlnotify(NPP aNPP,
              const char* aRelativeURL,
              const char* aTarget,
              void* aNotifyData)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(NPERR_INVALID_PARAM);

    if (!aNPP) // nullptr check for nspluginwrapper (bug 561690)
        return NPERR_INVALID_INSTANCE_ERROR;

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

NPError
_getvalue(NPP aNPP,
          NPNVariable aVariable,
          void* aValue)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(NPERR_INVALID_PARAM);

    switch (aVariable) {
        // Copied from nsNPAPIPlugin.cpp
        case NPNVToolkit:
#if defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_QT)
            *static_cast<NPNToolkitType*>(aValue) = NPNVGtk2;
            return NPERR_NO_ERROR;
#endif
            return NPERR_GENERIC_ERROR;

        case NPNVjavascriptEnabledBool:
            *(NPBool*)aValue = PluginModuleChild::GetChrome()->Settings().javascriptEnabled();
            return NPERR_NO_ERROR;
        case NPNVasdEnabledBool:
            *(NPBool*)aValue = PluginModuleChild::GetChrome()->Settings().asdEnabled();
            return NPERR_NO_ERROR;
        case NPNVisOfflineBool:
            *(NPBool*)aValue = PluginModuleChild::GetChrome()->Settings().isOffline();
            return NPERR_NO_ERROR;
        case NPNVSupportsXEmbedBool:
            *(NPBool*)aValue = PluginModuleChild::GetChrome()->Settings().supportsXembed();
            return NPERR_NO_ERROR;
        case NPNVSupportsWindowless:
            *(NPBool*)aValue = PluginModuleChild::GetChrome()->Settings().supportsWindowless();
            return NPERR_NO_ERROR;
#if defined(MOZ_WIDGET_GTK)
        case NPNVxDisplay: {
            if (aNPP) {
                return InstCast(aNPP)->NPN_GetValue(aVariable, aValue);
            } 
            else {
                *(void **)aValue = xt_client_get_display();
            }          
            return NPERR_NO_ERROR;
        }
        case NPNVxtAppContext:
            return NPERR_GENERIC_ERROR;
#endif
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

NPError
_setvalue(NPP aNPP,
          NPPVariable aVariable,
          void* aValue)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(NPERR_INVALID_PARAM);
    return InstCast(aNPP)->NPN_SetValue(aVariable, aValue);
}

NPError
_geturl(NPP aNPP,
        const char* aRelativeURL,
        const char* aTarget)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(NPERR_INVALID_PARAM);

    NPError err;
    InstCast(aNPP)->CallNPN_GetURL(NullableString(aRelativeURL),
                                   NullableString(aTarget), &err);
    return err;
}

NPError
_posturlnotify(NPP aNPP,
               const char* aRelativeURL,
               const char* aTarget,
               uint32_t aLength,
               const char* aBuffer,
               NPBool aIsFile,
               void* aNotifyData)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(NPERR_INVALID_PARAM);

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

NPError
_posturl(NPP aNPP,
         const char* aRelativeURL,
         const char* aTarget,
         uint32_t aLength,
         const char* aBuffer,
         NPBool aIsFile)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(NPERR_INVALID_PARAM);

    NPError err;
    // FIXME what should happen when |aBuffer| is null?
    InstCast(aNPP)->CallNPN_PostURL(NullableString(aRelativeURL),
                                    NullableString(aTarget),
                                    nsDependentCString(aBuffer, aLength),
                                    aIsFile, &err);
    return err;
}

NPError
_newstream(NPP aNPP,
           NPMIMEType aMIMEType,
           const char* aWindow,
           NPStream** aStream)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(NPERR_INVALID_PARAM);
    return InstCast(aNPP)->NPN_NewStream(aMIMEType, aWindow, aStream);
}

int32_t
_write(NPP aNPP,
       NPStream* aStream,
       int32_t aLength,
       void* aBuffer)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(0);

    PluginStreamChild* ps =
        static_cast<PluginStreamChild*>(static_cast<AStream*>(aStream->ndata));
    ps->EnsureCorrectInstance(InstCast(aNPP));
    ps->EnsureCorrectStream(aStream);
    return ps->NPN_Write(aLength, aBuffer);
}

NPError
_destroystream(NPP aNPP,
               NPStream* aStream,
               NPError aReason)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(NPERR_INVALID_PARAM);

    PluginInstanceChild* p = InstCast(aNPP);
    AStream* s = static_cast<AStream*>(aStream->ndata);
    if (s->IsBrowserStream()) {
        BrowserStreamChild* bs = static_cast<BrowserStreamChild*>(s);
        bs->EnsureCorrectInstance(p);
        bs->NPN_DestroyStream(aReason);
    }
    else {
        PluginStreamChild* ps = static_cast<PluginStreamChild*>(s);
        ps->EnsureCorrectInstance(p);
        PPluginStreamChild::Call__delete__(ps, aReason, false);
    }
    return NPERR_NO_ERROR;
}

void
_status(NPP aNPP,
        const char* aMessage)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD_VOID();
    NS_WARNING("Not yet implemented!");
}

void
_memfree(void* aPtr)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    // Only assert plugin thread here for consistency with in-process plugins.
    AssertPluginThread();
    NS_Free(aPtr);
}

uint32_t
_memflush(uint32_t aSize)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    // Only assert plugin thread here for consistency with in-process plugins.
    AssertPluginThread();
    return 0;
}

void
_reloadplugins(NPBool aReloadPages)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD_VOID();

    // Send the reload message to all modules. Chrome will need to reload from
    // disk and content will need to request a new list of plugin tags from
    // chrome.
    PluginModuleChild::GetChrome()->SendNPN_ReloadPlugins(!!aReloadPages);
}

void
_invalidaterect(NPP aNPP,
                NPRect* aInvalidRect)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD_VOID();
    // nullptr check for nspluginwrapper (bug 548434)
    if (aNPP) {
        InstCast(aNPP)->InvalidateRect(aInvalidRect);
    }
}

void
_invalidateregion(NPP aNPP,
                  NPRegion aInvalidRegion)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD_VOID();
    NS_WARNING("Not yet implemented!");
}

void
_forceredraw(NPP aNPP)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD_VOID();

    // We ignore calls to NPN_ForceRedraw. Such calls should
    // never be necessary.
}

const char*
_useragent(NPP aNPP)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(nullptr);
    return PluginModuleChild::GetChrome()->GetUserAgent();
}

void*
_memalloc(uint32_t aSize)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    // Only assert plugin thread here for consistency with in-process plugins.
    AssertPluginThread();
    return NS_Alloc(aSize);
}

// Deprecated entry points for the old Java plugin.
void* /* OJI type: JRIEnv* */
_getjavaenv(void)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    return 0;
}

void* /* OJI type: jref */
_getjavapeer(NPP aNPP)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    return 0;
}

bool
_invoke(NPP aNPP,
        NPObject* aNPObj,
        NPIdentifier aMethod,
        const NPVariant* aArgs,
        uint32_t aArgCount,
        NPVariant* aResult)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(false);

    if (!aNPP || !aNPObj || !aNPObj->_class || !aNPObj->_class->invoke)
        return false;

    return aNPObj->_class->invoke(aNPObj, aMethod, aArgs, aArgCount, aResult);
}

bool
_invokedefault(NPP aNPP,
               NPObject* aNPObj,
               const NPVariant* aArgs,
               uint32_t aArgCount,
               NPVariant* aResult)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(false);

    if (!aNPP || !aNPObj || !aNPObj->_class || !aNPObj->_class->invokeDefault)
        return false;

    return aNPObj->_class->invokeDefault(aNPObj, aArgs, aArgCount, aResult);
}

bool
_evaluate(NPP aNPP,
          NPObject* aObject,
          NPString* aScript,
          NPVariant* aResult)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(false);

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

#ifdef XP_WIN
    if (gDelayFlashFocusReplyUntilEval) {
        ReplyMessage(0);
        gDelayFlashFocusReplyUntilEval = false;
    }
#endif

    return actor->Evaluate(aScript, aResult);
}

bool
_getproperty(NPP aNPP,
             NPObject* aNPObj,
             NPIdentifier aPropertyName,
             NPVariant* aResult)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(false);

    if (!aNPP || !aNPObj || !aNPObj->_class || !aNPObj->_class->getProperty)
        return false;

    return aNPObj->_class->getProperty(aNPObj, aPropertyName, aResult);
}

bool
_setproperty(NPP aNPP,
             NPObject* aNPObj,
             NPIdentifier aPropertyName,
             const NPVariant* aValue)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(false);

    if (!aNPP || !aNPObj || !aNPObj->_class || !aNPObj->_class->setProperty)
        return false;

    return aNPObj->_class->setProperty(aNPObj, aPropertyName, aValue);
}

bool
_removeproperty(NPP aNPP,
                NPObject* aNPObj,
                NPIdentifier aPropertyName)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(false);

    if (!aNPP || !aNPObj || !aNPObj->_class || !aNPObj->_class->removeProperty)
        return false;

    return aNPObj->_class->removeProperty(aNPObj, aPropertyName);
}

bool
_hasproperty(NPP aNPP,
             NPObject* aNPObj,
             NPIdentifier aPropertyName)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(false);

    if (!aNPP || !aNPObj || !aNPObj->_class || !aNPObj->_class->hasProperty)
        return false;

    return aNPObj->_class->hasProperty(aNPObj, aPropertyName);
}

bool
_hasmethod(NPP aNPP,
           NPObject* aNPObj,
           NPIdentifier aMethodName)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(false);

    if (!aNPP || !aNPObj || !aNPObj->_class || !aNPObj->_class->hasMethod)
        return false;

    return aNPObj->_class->hasMethod(aNPObj, aMethodName);
}

bool
_enumerate(NPP aNPP,
           NPObject* aNPObj,
           NPIdentifier** aIdentifiers,
           uint32_t* aCount)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(false);

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

bool
_construct(NPP aNPP,
           NPObject* aNPObj,
           const NPVariant* aArgs,
           uint32_t aArgCount,
           NPVariant* aResult)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(false);

    if (!aNPP || !aNPObj || !aNPObj->_class ||
        !NP_CLASS_STRUCT_VERSION_HAS_CTOR(aNPObj->_class) ||
        !aNPObj->_class->construct) {
        return false;
    }

    return aNPObj->_class->construct(aNPObj, aArgs, aArgCount, aResult);
}

void
_releasevariantvalue(NPVariant* aVariant)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    // Only assert plugin thread here for consistency with in-process plugins.
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

void
_setexception(NPObject* aNPObj,
              const NPUTF8* aMessage)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD_VOID();

    // Do nothing. We no longer support this API.
}

void
_pushpopupsenabledstate(NPP aNPP,
                        NPBool aEnabled)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD_VOID();

    InstCast(aNPP)->CallNPN_PushPopupsEnabledState(aEnabled ? true : false);
}

void
_poppopupsenabledstate(NPP aNPP)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD_VOID();

    InstCast(aNPP)->CallNPN_PopPopupsEnabledState();
}

void
_pluginthreadasynccall(NPP aNPP,
                       PluginThreadCallback aFunc,
                       void* aUserData)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    if (!aFunc)
        return;

    InstCast(aNPP)->AsyncCall(aFunc, aUserData);
}

NPError
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

NPError
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

NPError
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

uint32_t
_scheduletimer(NPP npp, uint32_t interval, NPBool repeat,
               void (*timerFunc)(NPP npp, uint32_t timerID))
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();
    return InstCast(npp)->ScheduleTimer(interval, repeat, timerFunc);
}

void
_unscheduletimer(NPP npp, uint32_t timerID)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();
    InstCast(npp)->UnscheduleTimer(timerID);
}


#ifdef OS_MACOSX
static void ProcessBrowserEvents(void* pluginModule) {
    PluginModuleChild* pmc = static_cast<PluginModuleChild*>(pluginModule);

    if (!pmc)
        return;

    pmc->CallProcessSomeEvents();
}
#endif

NPError
_popupcontextmenu(NPP instance, NPMenu* menu)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();

#ifdef MOZ_WIDGET_COCOA
    double pluginX, pluginY; 
    double screenX, screenY;

    const NPCocoaEvent* currentEvent = InstCast(instance)->getCurrentEvent();
    if (!currentEvent) {
        return NPERR_GENERIC_ERROR;
    }

    // Ensure that the events has an x/y value.
    if (currentEvent->type != NPCocoaEventMouseDown    &&
        currentEvent->type != NPCocoaEventMouseUp      &&
        currentEvent->type != NPCocoaEventMouseMoved   &&
        currentEvent->type != NPCocoaEventMouseEntered &&
        currentEvent->type != NPCocoaEventMouseExited  &&
        currentEvent->type != NPCocoaEventMouseDragged) {
        return NPERR_GENERIC_ERROR;
    }

    pluginX = currentEvent->data.mouse.pluginX;
    pluginY = currentEvent->data.mouse.pluginY;

    if ((pluginX < 0.0) || (pluginY < 0.0))
        return NPERR_GENERIC_ERROR;

    NPBool success = _convertpoint(instance, 
                                  pluginX,  pluginY, NPCoordinateSpacePlugin, 
                                 &screenX, &screenY, NPCoordinateSpaceScreen);

    if (success) {
        return mozilla::plugins::PluginUtilsOSX::ShowCocoaContextMenu(menu,
                                    screenX, screenY,
                                    InstCast(instance)->Manager(),
                                    ProcessBrowserEvents);
    } else {
        NS_WARNING("Convertpoint failed, could not created contextmenu.");
        return NPERR_GENERIC_ERROR;
    }

#else
    NS_WARNING("Not supported on this platform!");
    return NPERR_GENERIC_ERROR;
#endif
}

NPBool
_convertpoint(NPP instance, 
              double sourceX, double sourceY, NPCoordinateSpace sourceSpace,
              double *destX, double *destY, NPCoordinateSpace destSpace)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    if (!IsPluginThread()) {
        NS_WARNING("Not running on the plugin's main thread!");
        return false;
    }

    double rDestX = 0;
    bool ignoreDestX = !destX;
    double rDestY = 0;
    bool ignoreDestY = !destY;
    bool result = false;
    InstCast(instance)->CallNPN_ConvertPoint(sourceX, ignoreDestX, sourceY, ignoreDestY, sourceSpace, destSpace,
                                             &rDestX,  &rDestY, &result);
    if (result) {
        if (destX)
            *destX = rDestX;
        if (destY)
            *destY = rDestY;
    }

    return result;
}

void
_urlredirectresponse(NPP instance, void* notifyData, NPBool allow)
{
    InstCast(instance)->NPN_URLRedirectResponse(notifyData, allow);
}

} /* namespace child */
} /* namespace plugins */
} /* namespace mozilla */

//-----------------------------------------------------------------------------

bool
PluginModuleChild::RecvSettingChanged(const PluginSettings& aSettings)
{
    mCachedSettings = aSettings;
    return true;
}

bool
PluginModuleChild::AnswerNP_GetEntryPoints(NPError* _retval)
{
    PLUGIN_LOG_DEBUG_METHOD;
    AssertPluginThread();
    MOZ_ASSERT(mIsChrome);

#if defined(OS_LINUX) || defined(OS_BSD)
    return true;
#elif defined(OS_WIN) || defined(OS_MACOSX)
    *_retval = mGetEntryPointsFunc(&mFunctions);
    return true;
#else
#  error Please implement me for your platform
#endif
}

bool
PluginModuleChild::AnswerNP_Initialize(const PluginSettings& aSettings, NPError* rv)
{
    *rv = DoNP_Initialize(aSettings);
    return true;
}

bool
PluginModuleChild::RecvAsyncNP_Initialize(const PluginSettings& aSettings)
{
    NPError error = DoNP_Initialize(aSettings);
    return SendNP_InitializeResult(error);
}

NPError
PluginModuleChild::DoNP_Initialize(const PluginSettings& aSettings)
{
    PLUGIN_LOG_DEBUG_METHOD;
    AssertPluginThread();
    MOZ_ASSERT(mIsChrome);

    mCachedSettings = aSettings;

#ifdef OS_WIN
    SetEventHooks();
#endif

#ifdef MOZ_X11
    // Send the parent our X socket to act as a proxy reference for our X
    // resources.
    int xSocketFd = ConnectionNumber(DefaultXDisplay());
    SendBackUpXResources(FileDescriptor(xSocketFd));
#endif

    NPError result;
#if defined(OS_LINUX) || defined(OS_BSD)
    result = mInitializeFunc(&sBrowserFuncs, &mFunctions);
#elif defined(OS_WIN) || defined(OS_MACOSX)
    result = mInitializeFunc(&sBrowserFuncs);
#else
#  error Please implement me for your platform
#endif

    return result;
}

#if defined(XP_WIN)

// Windows 8 RTM (kernelbase's version is 6.2.9200.16384) doesn't call
// CreateFileW from CreateFileA.
// So we hook CreateFileA too to use CreateFileW hook.

static HANDLE WINAPI
CreateFileAHookFn(LPCSTR fname, DWORD access, DWORD share,
                  LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags,
                  HANDLE ftemplate)
{
    while (true) { // goto out
        // Our hook is for mms.cfg into \Windows\System32\Macromed\Flash
        // We don't requrie supporting too long path.
        WCHAR unicodeName[MAX_PATH];
        size_t len = strlen(fname);

        if (len >= MAX_PATH) {
            break;
        }

        // We call to CreateFileW for workaround of Windows 8 RTM
        int newLen = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, fname,
                                         len, unicodeName, MAX_PATH);
        if (newLen == 0 || newLen >= MAX_PATH) {
            break;
        }
        unicodeName[newLen] = '\0';

        return CreateFileW(unicodeName, access, share, security, creation, flags, ftemplate);
    }

    return sCreateFileAStub(fname, access, share, security, creation, flags,
                            ftemplate);
}

HANDLE WINAPI
CreateFileWHookFn(LPCWSTR fname, DWORD access, DWORD share,
                  LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags,
                  HANDLE ftemplate)
{
    static const WCHAR kConfigFile[] = L"mms.cfg";
    static const size_t kConfigLength = ArrayLength(kConfigFile) - 1;

    while (true) { // goto out, in sheep's clothing
        size_t len = wcslen(fname);
        if (len < kConfigLength) {
            break;
        }
        if (wcscmp(fname + len - kConfigLength, kConfigFile) != 0) {
            break;
        }

        // This is the config file we want to rewrite
        WCHAR tempPath[MAX_PATH+1];
        if (GetTempPathW(MAX_PATH, tempPath) == 0) {
            break;
        }
        WCHAR tempFile[MAX_PATH+1];
        if (GetTempFileNameW(tempPath, L"fx", 0, tempFile) == 0) {
            break;
        }
        HANDLE replacement =
            sCreateFileWStub(tempFile, GENERIC_READ | GENERIC_WRITE, share,
                             security, TRUNCATE_EXISTING,
                             FILE_ATTRIBUTE_TEMPORARY |
                               FILE_FLAG_DELETE_ON_CLOSE,
                             NULL);
        if (replacement == INVALID_HANDLE_VALUE) {
            break;
        }

        HANDLE original = sCreateFileWStub(fname, access, share, security,
                                           creation, flags, ftemplate);
        if (original != INVALID_HANDLE_VALUE) {
            // copy original to replacement
            static const size_t kBufferSize = 1024;
            char buffer[kBufferSize];
            DWORD bytes;
            while (ReadFile(original, buffer, kBufferSize, &bytes, NULL)) {
                if (bytes == 0) {
                    break;
                }
                DWORD wbytes;
                WriteFile(replacement, buffer, bytes, &wbytes, NULL);
                if (bytes < kBufferSize) {
                    break;
                }
            }
            CloseHandle(original);
        }
        static const char kSettingString[] = "\nProtectedMode=0\n";
        DWORD wbytes;
        WriteFile(replacement, static_cast<const void*>(kSettingString),
                  sizeof(kSettingString) - 1, &wbytes, NULL);
        SetFilePointer(replacement, 0, NULL, FILE_BEGIN);
        return replacement;
    }
    return sCreateFileWStub(fname, access, share, security, creation, flags,
                            ftemplate);
}

void
PluginModuleChild::HookProtectedMode()
{
    sKernel32Intercept.Init("kernel32.dll");
    sKernel32Intercept.AddHook("CreateFileW",
                               reinterpret_cast<intptr_t>(CreateFileWHookFn),
                               (void**) &sCreateFileWStub);
    sKernel32Intercept.AddHook("CreateFileA",
                               reinterpret_cast<intptr_t>(CreateFileAHookFn),
                               (void**) &sCreateFileAStub);
}

BOOL WINAPI
PMCGetWindowInfoHook(HWND hWnd, PWINDOWINFO pwi)
{
  if (!pwi)
      return FALSE;

  if (!sGetWindowInfoPtrStub) {
     NS_ASSERTION(FALSE, "Something is horribly wrong in PMCGetWindowInfoHook!");
     return FALSE;
  }

  if (!sBrowserHwnd) {
      wchar_t szClass[20];
      if (GetClassNameW(hWnd, szClass, ArrayLength(szClass)) &&
          !wcscmp(szClass, kMozillaWindowClass)) {
          sBrowserHwnd = hWnd;
      }
  }
  // Oddity: flash does strange rect comparisons for mouse input destined for
  // it's internal settings window. Post removing sub widgets for tabs, touch
  // this up so they get the rect they expect.
  // XXX potentially tie this to a specific major version?
  BOOL result = sGetWindowInfoPtrStub(hWnd, pwi);
  if (sBrowserHwnd && sBrowserHwnd == hWnd)
      pwi->rcWindow = pwi->rcClient;
  return result;
}
#endif

PPluginInstanceChild*
PluginModuleChild::AllocPPluginInstanceChild(const nsCString& aMimeType,
                                             const uint16_t& aMode,
                                             const InfallibleTArray<nsCString>& aNames,
                                             const InfallibleTArray<nsCString>& aValues)
{
    PLUGIN_LOG_DEBUG_METHOD;
    AssertPluginThread();

    // In e10s, gChromeInstance hands out quirks to instances, but never
    // allocates an instance on its own. Make sure it gets the latest copy
    // of quirks once we have them. Also note, with process-per-tab, we may
    // have multiple PluginModuleChilds in the same plugin process, so only
    // initialize this once in gChromeInstance, which is a singleton.
    GetChrome()->InitQuirksModes(aMimeType);
    mQuirks = GetChrome()->mQuirks;

#ifdef XP_WIN
    if ((mQuirks & QUIRK_FLASH_HOOK_GETWINDOWINFO) &&
        !sGetWindowInfoPtrStub) {
        sUser32Intercept.Init("user32.dll");
        sUser32Intercept.AddHook("GetWindowInfo", reinterpret_cast<intptr_t>(PMCGetWindowInfoHook),
                                 (void**) &sGetWindowInfoPtrStub);
    }
#endif

    return new PluginInstanceChild(&mFunctions, aMimeType, aMode, aNames,
                                   aValues);
}

void
PluginModuleChild::InitQuirksModes(const nsCString& aMimeType)
{
    if (mQuirks != QUIRKS_NOT_INITIALIZED)
      return;
    mQuirks = 0;

    nsPluginHost::SpecialType specialType = nsPluginHost::GetSpecialType(aMimeType);

    if (specialType == nsPluginHost::eSpecialType_Silverlight) {
        mQuirks |= QUIRK_SILVERLIGHT_DEFAULT_TRANSPARENT;
#ifdef OS_WIN
        mQuirks |= QUIRK_WINLESS_TRACKPOPUP_HOOK;
        mQuirks |= QUIRK_SILVERLIGHT_FOCUS_CHECK_PARENT;
#endif
    }

#ifdef OS_WIN
    if (specialType == nsPluginHost::eSpecialType_Flash) {
        mQuirks |= QUIRK_WINLESS_TRACKPOPUP_HOOK;
        mQuirks |= QUIRK_FLASH_THROTTLE_WMUSER_EVENTS;
        mQuirks |= QUIRK_FLASH_HOOK_SETLONGPTR;
        mQuirks |= QUIRK_FLASH_HOOK_GETWINDOWINFO;
        mQuirks |= QUIRK_FLASH_FIXUP_MOUSE_CAPTURE;
    }

    // QuickTime plugin usually loaded with audio/mpeg mimetype
    NS_NAMED_LITERAL_CSTRING(quicktime, "npqtplugin");
    if (FindInReadable(quicktime, mPluginFilename)) {
        mQuirks |= QUIRK_QUICKTIME_AVOID_SETWINDOW;
    }
#endif

#ifdef XP_MACOSX
    // Whitelist Flash and Quicktime to support offline renderer
    NS_NAMED_LITERAL_CSTRING(quicktime, "QuickTime Plugin.plugin");
    if (specialType == nsPluginHost::eSpecialType_Flash) {
        mQuirks |= QUIRK_FLASH_AVOID_CGMODE_CRASHES;
        mQuirks |= QUIRK_ALLOW_OFFLINE_RENDERER;
    } else if (FindInReadable(quicktime, mPluginFilename)) {
        mQuirks |= QUIRK_ALLOW_OFFLINE_RENDERER;
    }
#endif
}

bool
PluginModuleChild::RecvPPluginInstanceConstructor(PPluginInstanceChild* aActor,
                                                  const nsCString& aMimeType,
                                                  const uint16_t& aMode,
                                                  InfallibleTArray<nsCString>&& aNames,
                                                  InfallibleTArray<nsCString>&& aValues)
{
    PLUGIN_LOG_DEBUG_METHOD;
    AssertPluginThread();

    NS_ASSERTION(aActor, "Null actor!");
    return true;
}

bool
PluginModuleChild::AnswerSyncNPP_New(PPluginInstanceChild* aActor, NPError* rv)
{
    PLUGIN_LOG_DEBUG_METHOD;
    PluginInstanceChild* childInstance =
        reinterpret_cast<PluginInstanceChild*>(aActor);
    AssertPluginThread();
    *rv = childInstance->DoNPP_New();
    return true;
}

class AsyncNewResultSender : public ChildAsyncCall
{
public:
    AsyncNewResultSender(PluginInstanceChild* aInstance, NPError aResult)
        : ChildAsyncCall(aInstance, nullptr, nullptr)
        , mResult(aResult)
    {
    }

    void Run() override
    {
        RemoveFromAsyncList();
        DebugOnly<bool> sendOk = mInstance->SendAsyncNPP_NewResult(mResult);
        MOZ_ASSERT(sendOk);
    }

private:
    NPError  mResult;
};

bool
PluginModuleChild::RecvAsyncNPP_New(PPluginInstanceChild* aActor)
{
    PLUGIN_LOG_DEBUG_METHOD;
    PluginInstanceChild* childInstance =
        reinterpret_cast<PluginInstanceChild*>(aActor);
    AssertPluginThread();
    NPError rv = childInstance->DoNPP_New();
    AsyncNewResultSender* task = new AsyncNewResultSender(childInstance, rv);
    childInstance->PostChildAsyncCall(task);
    return true;
}

bool
PluginModuleChild::DeallocPPluginInstanceChild(PPluginInstanceChild* aActor)
{
    PLUGIN_LOG_DEBUG_METHOD;
    AssertPluginThread();

    delete aActor;

    return true;
}

NPObject*
PluginModuleChild::NPN_CreateObject(NPP aNPP, NPClass* aClass)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    ENSURE_PLUGIN_THREAD(nullptr);

    PluginInstanceChild* i = InstCast(aNPP);
    if (i->mDeletingHash) {
        NS_ERROR("Plugin used NPP after NPP_Destroy");
        return nullptr;
    }

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

    PluginScriptableObjectChild::RegisterObject(newObject, i);

    return newObject;
}

NPObject*
PluginModuleChild::NPN_RetainObject(NPObject* aNPObj)
{
    AssertPluginThread();

#ifdef NS_BUILD_REFCNT_LOGGING
    int32_t refCnt =
#endif
    PR_ATOMIC_INCREMENT((int32_t*)&aNPObj->referenceCount);
    NS_LOG_ADDREF(aNPObj, refCnt, "NPObject", sizeof(NPObject));

    return aNPObj;
}

void
PluginModuleChild::NPN_ReleaseObject(NPObject* aNPObj)
{
    AssertPluginThread();

    PluginInstanceChild* instance = PluginScriptableObjectChild::GetInstanceForNPObject(aNPObj);
    if (!instance) {
        NS_ERROR("Releasing object not in mObjectMap?");
        return;
    }

    DeletingObjectEntry* doe = nullptr;
    if (instance->mDeletingHash) {
        doe = instance->mDeletingHash->GetEntry(aNPObj);
        if (!doe) {
            NS_ERROR("An object for a destroyed instance isn't in the instance deletion hash");
            return;
        }
        if (doe->mDeleted)
            return;
    }

    int32_t refCnt = PR_ATOMIC_DECREMENT((int32_t*)&aNPObj->referenceCount);
    NS_LOG_RELEASE(aNPObj, refCnt, "NPObject");

    if (refCnt == 0) {
        DeallocNPObject(aNPObj);
        if (doe)
            doe->mDeleted = true;
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

    PluginScriptableObjectChild* actor = PluginScriptableObjectChild::GetActorForNPObject(aNPObj);
    if (actor)
        actor->NPObjectDestroyed();

    PluginScriptableObjectChild::UnregisterObject(aNPObj);
}

NPIdentifier
PluginModuleChild::NPN_GetStringIdentifier(const NPUTF8* aName)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();

    if (!aName)
        return 0;

    nsDependentCString name(aName);
    PluginIdentifier ident(name);
    PluginScriptableObjectChild::StackIdentifier stackID(ident);
    stackID.MakePermanent();
    return stackID.ToNPIdentifier();
}

void
PluginModuleChild::NPN_GetStringIdentifiers(const NPUTF8** aNames,
                                            int32_t aNameCount,
                                            NPIdentifier* aIdentifiers)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();

    if (!(aNames && aNameCount > 0 && aIdentifiers)) {
        NS_RUNTIMEABORT("Bad input! Headed for a crash!");
    }

    for (int32_t index = 0; index < aNameCount; ++index) {
        if (!aNames[index]) {
            aIdentifiers[index] = 0;
            continue;
        }
        nsDependentCString name(aNames[index]);
        PluginIdentifier ident(name);
        PluginScriptableObjectChild::StackIdentifier stackID(ident);
        stackID.MakePermanent();
        aIdentifiers[index] = stackID.ToNPIdentifier();
    }
}

bool
PluginModuleChild::NPN_IdentifierIsString(NPIdentifier aIdentifier)
{
    PLUGIN_LOG_DEBUG_FUNCTION;

    PluginScriptableObjectChild::StackIdentifier stack(aIdentifier);
    return stack.IsString();
}

NPIdentifier
PluginModuleChild::NPN_GetIntIdentifier(int32_t aIntId)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();

    PluginIdentifier ident(aIntId);
    PluginScriptableObjectChild::StackIdentifier stackID(ident);
    stackID.MakePermanent();
    return stackID.ToNPIdentifier();
}

NPUTF8*
PluginModuleChild::NPN_UTF8FromIdentifier(NPIdentifier aIdentifier)
{
    PLUGIN_LOG_DEBUG_FUNCTION;

    PluginScriptableObjectChild::StackIdentifier stackID(aIdentifier);
    if (stackID.IsString()) {
        return ToNewCString(stackID.GetString());
    }
    return nullptr;
}

int32_t
PluginModuleChild::NPN_IntFromIdentifier(NPIdentifier aIdentifier)
{
    PLUGIN_LOG_DEBUG_FUNCTION;

    PluginScriptableObjectChild::StackIdentifier stackID(aIdentifier);
    if (!stackID.IsString()) {
        return stackID.GetInt();
    }
    return INT32_MIN;
}

#ifdef OS_WIN
void
PluginModuleChild::EnteredCall()
{
    mIncallPumpingStack.AppendElement();
}

void
PluginModuleChild::ExitedCall()
{
    NS_ASSERTION(mIncallPumpingStack.Length(), "mismatched entered/exited");
    uint32_t len = mIncallPumpingStack.Length();
    const IncallFrame& f = mIncallPumpingStack[len - 1];
    if (f._spinning)
        MessageLoop::current()->SetNestableTasksAllowed(f._savedNestableTasksAllowed);

    mIncallPumpingStack.TruncateLength(len - 1);
}

LRESULT CALLBACK
PluginModuleChild::CallWindowProcHook(int nCode, WPARAM wParam, LPARAM lParam)
{
    // Trap and reply to anything we recognize as the source of a
    // potential send message deadlock.
    if (nCode >= 0 &&
        (InSendMessageEx(nullptr)&(ISMEX_REPLIED|ISMEX_SEND)) == ISMEX_SEND) {
        CWPSTRUCT* pCwp = reinterpret_cast<CWPSTRUCT*>(lParam);
        if (pCwp->message == WM_KILLFOCUS) {
            // Fix for flash fullscreen window loosing focus. On single
            // core systems, sync killfocus events need to be handled
            // after the flash fullscreen window procedure processes this
            // message, otherwise fullscreen focus will not work correctly.
            wchar_t szClass[26];
            if (GetClassNameW(pCwp->hwnd, szClass,
                              sizeof(szClass)/sizeof(char16_t)) &&
                !wcscmp(szClass, kFlashFullscreenClass)) {
                gDelayFlashFocusReplyUntilEval = true;
            }
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK
PluginModuleChild::NestedInputEventHook(int nCode, WPARAM wParam, LPARAM lParam)
{
    PluginModuleChild* self = GetChrome();
    uint32_t len = self->mIncallPumpingStack.Length();
    if (nCode >= 0 && len && !self->mIncallPumpingStack[len - 1]._spinning) {
        MessageLoop* loop = MessageLoop::current();
        self->SendProcessNativeEventsInInterruptCall();
        IncallFrame& f = self->mIncallPumpingStack[len - 1];
        f._spinning = true;
        f._savedNestableTasksAllowed = loop->NestableTasksAllowed();
        loop->SetNestableTasksAllowed(true);
        loop->set_os_modal_loop(true);
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void
PluginModuleChild::SetEventHooks()
{
    NS_ASSERTION(!mNestedEventHook,
        "mNestedEventHook already setup in call to SetNestedInputEventHook?");
    NS_ASSERTION(!mGlobalCallWndProcHook,
        "mGlobalCallWndProcHook already setup in call to CallWindowProcHook?");

    PLUGIN_LOG_DEBUG(("%s", FULLFUNCTION));

    // WH_MSGFILTER event hook for detecting modal loops in the child.
    mNestedEventHook = SetWindowsHookEx(WH_MSGFILTER,
                                        NestedInputEventHook,
                                        nullptr,
                                        GetCurrentThreadId());

    // WH_CALLWNDPROC event hook for trapping sync messages sent from
    // parent that can cause deadlocks.
    mGlobalCallWndProcHook = SetWindowsHookEx(WH_CALLWNDPROC,
                                              CallWindowProcHook,
                                              nullptr,
                                              GetCurrentThreadId());
}

void
PluginModuleChild::ResetEventHooks()
{
    PLUGIN_LOG_DEBUG(("%s", FULLFUNCTION));
    if (mNestedEventHook)
        UnhookWindowsHookEx(mNestedEventHook);
    mNestedEventHook = nullptr;
    if (mGlobalCallWndProcHook)
        UnhookWindowsHookEx(mGlobalCallWndProcHook);
    mGlobalCallWndProcHook = nullptr;
}
#endif

bool
PluginModuleChild::RecvProcessNativeEventsInInterruptCall()
{
    PLUGIN_LOG_DEBUG(("%s", FULLFUNCTION));
#if defined(OS_WIN)
    ProcessNativeEventsInInterruptCall();
    return true;
#else
    NS_RUNTIMEABORT(
        "PluginModuleChild::RecvProcessNativeEventsInInterruptCall not implemented!");
    return false;
#endif
}

#ifdef MOZ_WIDGET_COCOA
void
PluginModuleChild::ProcessNativeEvents() {
    CallProcessSomeEvents();    
}
#endif

bool
PluginModuleChild::RecvStartProfiler(const uint32_t& aEntries,
                                     const double& aInterval,
                                     nsTArray<nsCString>&& aFeatures,
                                     nsTArray<nsCString>&& aThreadNameFilters)
{
    nsTArray<const char*> featureArray;
    for (size_t i = 0; i < aFeatures.Length(); ++i) {
        featureArray.AppendElement(aFeatures[i].get());
    }

    nsTArray<const char*> threadNameFilterArray;
    for (size_t i = 0; i < aThreadNameFilters.Length(); ++i) {
        threadNameFilterArray.AppendElement(aThreadNameFilters[i].get());
    }

    profiler_start(aEntries, aInterval, featureArray.Elements(), featureArray.Length(),
                   threadNameFilterArray.Elements(), threadNameFilterArray.Length());

    return true;
}

bool
PluginModuleChild::RecvStopProfiler()
{
    profiler_stop();
    return true;
}

bool
PluginModuleChild::AnswerGetProfile(nsCString* aProfile)
{
    char* profile = profiler_get_profile();
    if (profile != nullptr) {
        *aProfile = nsCString(profile, strlen(profile));
        free(profile);
    } else {
        *aProfile = nsCString("", 0);
    }
    return true;
}
