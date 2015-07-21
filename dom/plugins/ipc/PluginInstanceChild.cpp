/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PluginBackgroundDestroyer.h"
#include "PluginInstanceChild.h"
#include "PluginModuleChild.h"
#include "BrowserStreamChild.h"
#include "PluginStreamChild.h"
#include "StreamNotifyChild.h"
#include "PluginProcessChild.h"
#include "gfxASurface.h"
#include "gfxPlatform.h"
#include "gfx2DGlue.h"
#include "nsNPAPIPluginInstance.h"
#include "mozilla/gfx/2D.h"
#ifdef MOZ_X11
#include "gfxXlibSurface.h"
#endif
#ifdef XP_WIN
#include "mozilla/gfx/SharedDIBSurface.h"
#include "nsCrashOnException.h"
extern const wchar_t* kFlashFullscreenClass;
using mozilla::gfx::SharedDIBSurface;
#endif
#include "gfxSharedImageSurface.h"
#include "gfxUtils.h"
#include "gfxAlphaRecovery.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/ipc/MessageChannel.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/StaticPtr.h"
#include "ImageContainer.h"

using namespace mozilla;
using mozilla::ipc::ProcessChild;
using namespace mozilla::plugins;
using namespace mozilla::layers;
using namespace mozilla::gfx;
using namespace std;

#ifdef MOZ_WIDGET_GTK

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include "gtk2xtbin.h"

#elif defined(MOZ_WIDGET_QT)
#undef KeyPress
#undef KeyRelease
#elif defined(OS_WIN)
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL     0x020E
#endif

#include "nsWindowsDllInterceptor.h"

typedef BOOL (WINAPI *User32TrackPopupMenu)(HMENU hMenu,
                                            UINT uFlags,
                                            int x,
                                            int y,
                                            int nReserved,
                                            HWND hWnd,
                                            CONST RECT *prcRect);
static WindowsDllInterceptor sUser32Intercept;
static HWND sWinlessPopupSurrogateHWND = nullptr;
static User32TrackPopupMenu sUser32TrackPopupMenuStub = nullptr;

using mozilla::gfx::SharedDIB;

#include <windows.h>
#include <windowsx.h>

// Flash WM_USER message delay time for PostDelayedTask. Borrowed
// from Chromium's web plugin delegate src. See 'flash msg throttling
// helpers' section for details.
const int kFlashWMUSERMessageThrottleDelayMs = 5;

static const TCHAR kPluginIgnoreSubclassProperty[] = TEXT("PluginIgnoreSubclassProperty");

#elif defined(XP_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#include "nsCocoaFeatures.h"
#include "PluginUtilsOSX.h"
#endif // defined(XP_MACOSX)

template<>
struct RunnableMethodTraits<PluginInstanceChild>
{
    static void RetainCallee(PluginInstanceChild* obj) { }
    static void ReleaseCallee(PluginInstanceChild* obj) { }
};

/**
 * We can't use gfxPlatform::CreateDrawTargetForSurface() because calling
 * gfxPlatform::GetPlatform() instantiates the prefs service, and that's not
 * allowed from processes other than the main process. So we have our own
 * version here.
 */
static RefPtr<DrawTarget>
CreateDrawTargetForSurface(gfxASurface *aSurface)
{
  SurfaceFormat format;
  if (aSurface->GetContentType() == gfxContentType::ALPHA) {
    format = SurfaceFormat::A8;
  } else if (aSurface->GetContentType() == gfxContentType::COLOR) {
    format = SurfaceFormat::B8G8R8X8;
  } else {
    format = SurfaceFormat::B8G8R8A8;
  }
  RefPtr<DrawTarget> drawTarget =
    Factory::CreateDrawTargetForCairoSurface(aSurface->CairoSurface(),
                                             aSurface->GetSize(),
                                             &format);
  aSurface->SetData(&kDrawTarget, drawTarget, nullptr);
  return drawTarget;
}

PluginInstanceChild::PluginInstanceChild(const NPPluginFuncs* aPluginIface,
                                         const nsCString& aMimeType,
                                         const uint16_t& aMode,
                                         const InfallibleTArray<nsCString>& aNames,
                                         const InfallibleTArray<nsCString>& aValues)
    : mPluginIface(aPluginIface)
    , mMimeType(aMimeType)
    , mMode(aMode)
    , mNames(aNames)
    , mValues(aValues)
#if defined(XP_DARWIN)
    , mContentsScaleFactor(1.0)
#endif
    , mDrawingModel(kDefaultDrawingModel)
    , mAsyncInvalidateMutex("PluginInstanceChild::mAsyncInvalidateMutex")
    , mAsyncInvalidateTask(0)
    , mCachedWindowActor(nullptr)
    , mCachedElementActor(nullptr)
#ifdef MOZ_WIDGET_GTK
    , mXEmbed(false)
#endif // MOZ_WIDGET_GTK
#if defined(OS_WIN)
    , mPluginWindowHWND(0)
    , mPluginWndProc(0)
    , mPluginParentHWND(0)
    , mCachedWinlessPluginHWND(0)
    , mWinlessPopupSurrogateHWND(0)
    , mWinlessThrottleOldWndProc(0)
    , mWinlessHiddenMsgHWND(0)
    , mUnityGetMessageHook(NULL)
    , mUnitySendMessageHook(NULL)
#endif // OS_WIN
    , mAsyncCallMutex("PluginInstanceChild::mAsyncCallMutex")
#if defined(MOZ_WIDGET_COCOA)
#if defined(__i386__)
    , mEventModel(NPEventModelCarbon)
#endif
    , mShColorSpace(nullptr)
    , mShContext(nullptr)
    , mCGLayer(nullptr)
    , mCurrentEvent(nullptr)
#endif
    , mLayersRendering(false)
#ifdef XP_WIN
    , mCurrentSurfaceActor(nullptr)
    , mBackSurfaceActor(nullptr)
#endif
    , mAccumulatedInvalidRect(0,0,0,0)
    , mIsTransparent(false)
    , mSurfaceType(gfxSurfaceType::Max)
    , mCurrentInvalidateTask(nullptr)
    , mCurrentAsyncSetWindowTask(nullptr)
    , mPendingPluginCall(false)
    , mDoAlphaExtraction(false)
    , mHasPainted(false)
    , mSurfaceDifferenceRect(0,0,0,0)
    , mDestroyed(false)
{
    memset(&mWindow, 0, sizeof(mWindow));
    mWindow.type = NPWindowTypeWindow;
    mData.ndata = (void*) this;
    mData.pdata = nullptr;
#if defined(MOZ_X11) && defined(XP_UNIX) && !defined(XP_MACOSX)
    mWindow.ws_info = &mWsInfo;
    memset(&mWsInfo, 0, sizeof(mWsInfo));
#ifdef MOZ_WIDGET_GTK
    mWsInfo.display = nullptr;
    mXtClient.top_widget = nullptr;
#else
    mWsInfo.display = DefaultXDisplay();
#endif
#endif // MOZ_X11 && XP_UNIX && !XP_MACOSX
#if defined(OS_WIN)
    memset(&mAlphaExtract, 0, sizeof(mAlphaExtract));
#endif // OS_WIN
#if defined(OS_WIN)
    InitPopupMenuHook();
    if (GetQuirks() & PluginModuleChild::QUIRK_UNITY_FIXUP_MOUSE_CAPTURE) {
        SetUnityHooks();
    }
#endif // OS_WIN
}

PluginInstanceChild::~PluginInstanceChild()
{
#if defined(OS_WIN)
    NS_ASSERTION(!mPluginWindowHWND, "Destroying PluginInstanceChild without NPP_Destroy?");
    if (GetQuirks() & PluginModuleChild::QUIRK_UNITY_FIXUP_MOUSE_CAPTURE) {
        ClearUnityHooks();
    }
#endif
#if defined(MOZ_WIDGET_COCOA)
    if (mShColorSpace) {
        ::CGColorSpaceRelease(mShColorSpace);
    }
    if (mShContext) {
        ::CGContextRelease(mShContext);
    }
    if (mCGLayer) {
        PluginUtilsOSX::ReleaseCGLayer(mCGLayer);
    }
    if (mDrawingModel == NPDrawingModelCoreAnimation) {
        UnscheduleTimer(mCARefreshTimer);
    }
#endif
}

NPError
PluginInstanceChild::DoNPP_New()
{
    // unpack the arguments into a C format
    int argc = mNames.Length();
    NS_ASSERTION(argc == (int) mValues.Length(),
                 "argn.length != argv.length");

    nsAutoArrayPtr<char*> argn(new char*[1 + argc]);
    nsAutoArrayPtr<char*> argv(new char*[1 + argc]);
    argn[argc] = 0;
    argv[argc] = 0;

    for (int i = 0; i < argc; ++i) {
        argn[i] = const_cast<char*>(NullableStringGet(mNames[i]));
        argv[i] = const_cast<char*>(NullableStringGet(mValues[i]));
    }

    NPP npp = GetNPP();

    NPError rv = mPluginIface->newp((char*)NullableStringGet(mMimeType), npp,
                                    mMode, argc, argn, argv, 0);
    if (NPERR_NO_ERROR != rv) {
        return rv;
    }

    Initialize();

#if defined(XP_MACOSX) && defined(__i386__)
    // If an i386 Mac OS X plugin has selected the Carbon event model then
    // we have to fail. We do not support putting Carbon event model plugins
    // out of process. Note that Carbon is the default model so out of process
    // plugins need to actively negotiate something else in order to work
    // out of process.
    if (EventModel() == NPEventModelCarbon) {
      // Send notification that a plugin tried to negotiate Carbon NPAPI so that
      // users can be notified that restarting the browser in i386 mode may allow
      // them to use the plugin.
      SendNegotiatedCarbon();

      // Fail to instantiate.
      rv = NPERR_MODULE_LOAD_FAILED_ERROR;
    }
#endif

    return rv;
}

int
PluginInstanceChild::GetQuirks()
{
    return PluginModuleChild::GetChrome()->GetQuirks();
}

NPError
PluginInstanceChild::InternalGetNPObjectForValue(NPNVariable aValue,
                                                 NPObject** aObject)
{
    PluginScriptableObjectChild* actor = nullptr;
    NPError result = NPERR_NO_ERROR;

    switch (aValue) {
        case NPNVWindowNPObject:
            if (!(actor = mCachedWindowActor)) {
                PPluginScriptableObjectChild* actorProtocol;
                CallNPN_GetValue_NPNVWindowNPObject(&actorProtocol, &result);
                if (result == NPERR_NO_ERROR) {
                    actor = mCachedWindowActor =
                        static_cast<PluginScriptableObjectChild*>(actorProtocol);
                    NS_ASSERTION(actor, "Null actor!");
                    PluginModuleChild::sBrowserFuncs.retainobject(
                        actor->GetObject(false));
                }
            }
            break;

        case NPNVPluginElementNPObject:
            if (!(actor = mCachedElementActor)) {
                PPluginScriptableObjectChild* actorProtocol;
                CallNPN_GetValue_NPNVPluginElementNPObject(&actorProtocol,
                                                           &result);
                if (result == NPERR_NO_ERROR) {
                    actor = mCachedElementActor =
                        static_cast<PluginScriptableObjectChild*>(actorProtocol);
                    NS_ASSERTION(actor, "Null actor!");
                    PluginModuleChild::sBrowserFuncs.retainobject(
                        actor->GetObject(false));
                }
            }
            break;

        default:
            NS_NOTREACHED("Don't know what to do with this value type!");
    }

#ifdef DEBUG
    {
        NPError currentResult;
        PPluginScriptableObjectChild* currentActor = nullptr;

        switch (aValue) {
            case NPNVWindowNPObject:
                CallNPN_GetValue_NPNVWindowNPObject(&currentActor,
                                                    &currentResult);
                break;
            case NPNVPluginElementNPObject:
                CallNPN_GetValue_NPNVPluginElementNPObject(&currentActor,
                                                           &currentResult);
                break;
            default:
                MOZ_ASSERT(false);
        }

        // Make sure that the current actor returned by the parent matches our
        // cached actor!
        NS_ASSERTION(!currentActor ||
                     static_cast<PluginScriptableObjectChild*>(currentActor) ==
                     actor, "Cached actor is out of date!");
    }
#endif

    if (result != NPERR_NO_ERROR) {
        return result;
    }

    NPObject* object = actor->GetObject(false);
    NS_ASSERTION(object, "Null object?!");

    *aObject = PluginModuleChild::sBrowserFuncs.retainobject(object);
    return NPERR_NO_ERROR;

}

NPError
PluginInstanceChild::NPN_GetValue(NPNVariable aVar,
                                  void* aValue)
{
    PLUGIN_LOG_DEBUG(("%s (aVar=%i)", FULLFUNCTION, (int) aVar));
    AssertPluginThread();

    switch(aVar) {

#if defined(MOZ_X11)
    case NPNVToolkit:
        *((NPNToolkitType*)aValue) = NPNVGtk2;
        return NPERR_NO_ERROR;

    case NPNVxDisplay:
        if (!mWsInfo.display) {
            // We are called before Initialize() so we have to call it now.
           Initialize();
           NS_ASSERTION(mWsInfo.display, "We should have a valid display!");
        }
        *(void **)aValue = mWsInfo.display;
        return NPERR_NO_ERROR;
    
#elif defined(OS_WIN)
    case NPNVToolkit:
        return NPERR_GENERIC_ERROR;
#endif
    case NPNVprivateModeBool: {
        bool v = false;
        NPError result;
        if (!CallNPN_GetValue_NPNVprivateModeBool(&v, &result)) {
            return NPERR_GENERIC_ERROR;
        }
        *static_cast<NPBool*>(aValue) = v;
        return result;
    }

    case NPNVdocumentOrigin: {
        nsCString v;
        NPError result;
        if (!CallNPN_GetValue_NPNVdocumentOrigin(&v, &result)) {
            return NPERR_GENERIC_ERROR;
        }
        if (result == NPERR_NO_ERROR ||
            (GetQuirks() &
                PluginModuleChild::QUIRK_FLASH_RETURN_EMPTY_DOCUMENT_ORIGIN)) {
            *static_cast<char**>(aValue) = ToNewCString(v);
        }
        return result;
    }

    case NPNVWindowNPObject: // Intentional fall-through
    case NPNVPluginElementNPObject: {
        NPObject* object;
        NPError result = InternalGetNPObjectForValue(aVar, &object);
        if (result == NPERR_NO_ERROR) {
            *((NPObject**)aValue) = object;
        }
        return result;
    }

    case NPNVnetscapeWindow: {
#ifdef XP_WIN
        if (mWindow.type == NPWindowTypeDrawable) {
            if (mCachedWinlessPluginHWND) {
              *static_cast<HWND*>(aValue) = mCachedWinlessPluginHWND;
              return NPERR_NO_ERROR;
            }
            NPError result;
            if (!CallNPN_GetValue_NPNVnetscapeWindow(&mCachedWinlessPluginHWND, &result)) {
                return NPERR_GENERIC_ERROR;
            }
            *static_cast<HWND*>(aValue) = mCachedWinlessPluginHWND;
            return result;
        }
        else {
            *static_cast<HWND*>(aValue) = mPluginWindowHWND;
            return NPERR_NO_ERROR;
        }
#elif defined(MOZ_X11)
        NPError result;
        CallNPN_GetValue_NPNVnetscapeWindow(static_cast<XID*>(aValue), &result);
        return result;
#else
        return NPERR_GENERIC_ERROR;
#endif
    }

#ifdef XP_MACOSX
   case NPNVsupportsCoreGraphicsBool: {
        *((NPBool*)aValue) = true;
        return NPERR_NO_ERROR;
    }

    case NPNVsupportsCoreAnimationBool: {
        *((NPBool*)aValue) = true;
        return NPERR_NO_ERROR;
    }

    case NPNVsupportsInvalidatingCoreAnimationBool: {
        *((NPBool*)aValue) = true;
        return NPERR_NO_ERROR;
    }

    case NPNVsupportsCompositingCoreAnimationPluginsBool: {
        *((NPBool*)aValue) = true;
        return NPERR_NO_ERROR;
    }

    case NPNVsupportsCocoaBool: {
        *((NPBool*)aValue) = true;
        return NPERR_NO_ERROR;
    }

#ifndef NP_NO_CARBON
    case NPNVsupportsCarbonBool: {
      *((NPBool*)aValue) = false;
      return NPERR_NO_ERROR;
    }
#endif

    case NPNVsupportsUpdatedCocoaTextInputBool: {
      *static_cast<NPBool*>(aValue) = true;
      return NPERR_NO_ERROR;
    }

#ifndef NP_NO_QUICKDRAW
    case NPNVsupportsQuickDrawBool: {
        *((NPBool*)aValue) = false;
        return NPERR_NO_ERROR;
    }
#endif /* NP_NO_QUICKDRAW */

    case NPNVcontentsScaleFactor: {
        *static_cast<double*>(aValue) = mContentsScaleFactor;
        return NPERR_NO_ERROR;
    }
#endif /* XP_MACOSX */

#ifdef DEBUG
    case NPNVjavascriptEnabledBool:
    case NPNVasdEnabledBool:
    case NPNVisOfflineBool:
    case NPNVSupportsXEmbedBool:
    case NPNVSupportsWindowless:
        NS_NOTREACHED("NPNVariable should be handled in PluginModuleChild.");
#endif

    default:
        MOZ_LOG(GetPluginLog(), LogLevel::Warning,
               ("In PluginInstanceChild::NPN_GetValue: Unhandled NPNVariable %i (%s)",
                (int) aVar, NPNVariableToString(aVar)));
        return NPERR_GENERIC_ERROR;
    }

}

#ifdef MOZ_WIDGET_COCOA
#define DEFAULT_REFRESH_MS 20 // CoreAnimation: 50 FPS

void
CAUpdate(NPP npp, uint32_t timerID) {
    static_cast<PluginInstanceChild*>(npp->ndata)->Invalidate();
}

void
PluginInstanceChild::Invalidate()
{
    NPRect windowRect = {0, 0, uint16_t(mWindow.height),
        uint16_t(mWindow.width)};

    InvalidateRect(&windowRect);
}
#endif

NPError
PluginInstanceChild::NPN_SetValue(NPPVariable aVar, void* aValue)
{
    MOZ_LOG(GetPluginLog(), LogLevel::Debug, ("%s (aVar=%i, aValue=%p)",
                                      FULLFUNCTION, (int) aVar, aValue));

    AssertPluginThread();

    switch (aVar) {
    case NPPVpluginWindowBool: {
        NPError rv;
        bool windowed = (NPBool) (intptr_t) aValue;

        if (!CallNPN_SetValue_NPPVpluginWindow(windowed, &rv))
            return NPERR_GENERIC_ERROR;

        NPWindowType newWindowType = windowed ? NPWindowTypeWindow : NPWindowTypeDrawable;
#ifdef MOZ_WIDGET_GTK
        if (mWindow.type != newWindowType && mWsInfo.display) {
           // plugin type has been changed but we already have a valid display
           // so update it for the recent plugin mode
           if (mXEmbed || !windowed) {
               // Use default GTK display for XEmbed and windowless plugins
               mWsInfo.display = DefaultXDisplay();
           }
           else {
               mWsInfo.display = xt_client_get_display();
           }
        }
#endif
        mWindow.type = newWindowType;
        return rv;
    }

    case NPPVpluginTransparentBool: {
        NPError rv;
        mIsTransparent = (!!aValue);

        if (!CallNPN_SetValue_NPPVpluginTransparent(mIsTransparent, &rv))
            return NPERR_GENERIC_ERROR;

        return rv;
    }

    case NPPVpluginUsesDOMForCursorBool: {
        NPError rv = NPERR_GENERIC_ERROR;
        if (!CallNPN_SetValue_NPPVpluginUsesDOMForCursor((NPBool)(intptr_t)aValue, &rv)) {
            return NPERR_GENERIC_ERROR;
        }
        return rv;
    }

    case NPPVpluginDrawingModel: {
        NPError rv;
        int drawingModel = (int16_t) (intptr_t) aValue;

        if (!CallNPN_SetValue_NPPVpluginDrawingModel(drawingModel, &rv))
            return NPERR_GENERIC_ERROR;

        mDrawingModel = drawingModel;

#ifdef XP_MACOSX
        if (drawingModel == NPDrawingModelCoreAnimation) {
            mCARefreshTimer = ScheduleTimer(DEFAULT_REFRESH_MS, true, CAUpdate);
        }
#endif

        PLUGIN_LOG_DEBUG(("  Plugin requested drawing model id  #%i\n",
            mDrawingModel));

        return rv;
    }

#ifdef XP_MACOSX
    case NPPVpluginEventModel: {
        NPError rv;
        int eventModel = (int16_t) (intptr_t) aValue;

        if (!CallNPN_SetValue_NPPVpluginEventModel(eventModel, &rv))
            return NPERR_GENERIC_ERROR;
#if defined(__i386__)
        mEventModel = static_cast<NPEventModel>(eventModel);
#endif

        PLUGIN_LOG_DEBUG(("  Plugin requested event model id # %i\n",
            eventModel));

        return rv;
    }
#endif

    default:
        MOZ_LOG(GetPluginLog(), LogLevel::Warning,
               ("In PluginInstanceChild::NPN_SetValue: Unhandled NPPVariable %i (%s)",
                (int) aVar, NPPVariableToString(aVar)));
        return NPERR_GENERIC_ERROR;
    }
}

bool
PluginInstanceChild::AnswerNPP_GetValue_NPPVpluginWantsAllNetworkStreams(
    bool* wantsAllStreams, NPError* rv)
{
    AssertPluginThread();

    uint32_t value = 0;
    if (!mPluginIface->getvalue) {
        *rv = NPERR_GENERIC_ERROR;
    }
    else {
        *rv = mPluginIface->getvalue(GetNPP(), NPPVpluginWantsAllNetworkStreams,
                                     &value);
    }
    *wantsAllStreams = value;
    return true;
}

bool
PluginInstanceChild::AnswerNPP_GetValue_NPPVpluginNeedsXEmbed(
    bool* needs, NPError* rv)
{
    AssertPluginThread();

#ifdef MOZ_X11
    // The documentation on the types for many variables in NP(N|P)_GetValue
    // is vague.  Often boolean values are NPBool (1 byte), but
    // https://developer.mozilla.org/en/XEmbed_Extension_for_Mozilla_Plugins
    // treats NPPVpluginNeedsXEmbed as PRBool (int), and
    // on x86/32-bit, flash stores to this using |movl 0x1,&needsXEmbed|.
    // thus we can't use NPBool for needsXEmbed, or the three bytes above
    // it on the stack would get clobbered. so protect with the larger bool.
    int needsXEmbed = 0;
    if (!mPluginIface->getvalue) {
        *rv = NPERR_GENERIC_ERROR;
    }
    else {
        *rv = mPluginIface->getvalue(GetNPP(), NPPVpluginNeedsXEmbed,
                                     &needsXEmbed);
    }
    *needs = needsXEmbed;
    return true;

#else

    NS_RUNTIMEABORT("shouldn't be called on non-X11 platforms");
    return false;               // not reached

#endif
}

bool
PluginInstanceChild::AnswerNPP_GetValue_NPPVpluginScriptableNPObject(
                                          PPluginScriptableObjectChild** aValue,
                                          NPError* aResult)
{
    AssertPluginThread();

    NPObject* object = nullptr;
    NPError result = NPERR_GENERIC_ERROR;
    if (mPluginIface->getvalue) {
        result = mPluginIface->getvalue(GetNPP(), NPPVpluginScriptableNPObject,
                                        &object);
    }
    if (result == NPERR_NO_ERROR && object) {
        PluginScriptableObjectChild* actor = GetActorForNPObject(object);

        // If we get an actor then it has retained. Otherwise we don't need it
        // any longer.
        PluginModuleChild::sBrowserFuncs.releaseobject(object);
        if (actor) {
            *aValue = actor;
            *aResult = NPERR_NO_ERROR;
            return true;
        }

        NS_ERROR("Failed to get actor!");
        result = NPERR_GENERIC_ERROR;
    }
    else {
        result = NPERR_GENERIC_ERROR;
    }

    *aValue = nullptr;
    *aResult = result;
    return true;
}

bool
PluginInstanceChild::AnswerNPP_GetValue_NPPVpluginNativeAccessibleAtkPlugId(
                                          nsCString* aPlugId,
                                          NPError* aResult)
{
    AssertPluginThread();

#if MOZ_ACCESSIBILITY_ATK

    char* plugId = nullptr;
    NPError result = NPERR_GENERIC_ERROR;
    if (mPluginIface->getvalue) {
        result = mPluginIface->getvalue(GetNPP(),
                                        NPPVpluginNativeAccessibleAtkPlugId,
                                        &plugId);
    }

    *aPlugId = nsCString(plugId);
    *aResult = result;
    return true;

#else

    NS_RUNTIMEABORT("shouldn't be called on non-ATK platforms");
    return false;

#endif
}

bool
PluginInstanceChild::AnswerNPP_SetValue_NPNVprivateModeBool(const bool& value,
                                                            NPError* result)
{
    if (!mPluginIface->setvalue) {
        *result = NPERR_GENERIC_ERROR;
        return true;
    }

    NPBool v = value;
    *result = mPluginIface->setvalue(GetNPP(), NPNVprivateModeBool, &v);
    return true;
}

bool
PluginInstanceChild::AnswerNPP_HandleEvent(const NPRemoteEvent& event,
                                           int16_t* handled)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();

#if defined(MOZ_X11) && defined(DEBUG)
    if (GraphicsExpose == event.event.type)
        PLUGIN_LOG_DEBUG(("  received drawable 0x%lx\n",
                          event.event.xgraphicsexpose.drawable));
#endif

#ifdef XP_MACOSX
    // Mac OS X does not define an NPEvent structure. It defines more specific types.
    NPCocoaEvent evcopy = event.event;
    // event.contentsScaleFactor <= 0 is a signal we shouldn't use it,
    // for example when AnswerNPP_HandleEvent() is called from elsewhere
    // in the child process (not via rpc code from the parent process).
    if (event.contentsScaleFactor > 0) {
      mContentsScaleFactor = event.contentsScaleFactor;
    }

    // Make sure we reset mCurrentEvent in case of an exception
    AutoRestore<const NPCocoaEvent*> savePreviousEvent(mCurrentEvent);

    // Track the current event for NPN_PopUpContextMenu.
    mCurrentEvent = &event.event;
#else
    // Make a copy since we may modify values.
    NPEvent evcopy = event.event;
#endif

#ifdef OS_WIN
    // FIXME/bug 567645: temporarily drop the "dummy event" on the floor
    if (WM_NULL == evcopy.event)
        return true;

    // Painting for win32. SharedSurfacePaint handles everything.
    if (mWindow.type == NPWindowTypeDrawable) {
       if (evcopy.event == WM_PAINT) {
          *handled = SharedSurfacePaint(evcopy);
          return true;
       }
       else if (DoublePassRenderingEvent() == evcopy.event) {
            // We'll render to mSharedSurfaceDib first, then render to a cached bitmap
            // we store locally. The two passes are for alpha extraction, so the second
            // pass must be to a flat white surface in order for things to work.
            mAlphaExtract.doublePass = RENDER_BACK_ONE;
            *handled = true;
            return true;
       }
    }
    *handled = WinlessHandleEvent(evcopy);
    return true;
#endif

    // XXX A previous call to mPluginIface->event might block, e.g. right click
    // for context menu. Still, we might get here again, calling into the plugin
    // a second time while it's in the previous call.
    if (!mPluginIface->event)
        *handled = false;
    else
        *handled = mPluginIface->event(&mData, reinterpret_cast<void*>(&evcopy));

#ifdef XP_MACOSX
    // Release any reference counted objects created in the child process.
    if (evcopy.type == NPCocoaEventKeyDown ||
        evcopy.type == NPCocoaEventKeyUp) {
      ::CFRelease((CFStringRef)evcopy.data.key.characters);
      ::CFRelease((CFStringRef)evcopy.data.key.charactersIgnoringModifiers);
    }
    else if (evcopy.type == NPCocoaEventTextInput) {
      ::CFRelease((CFStringRef)evcopy.data.text.text);
    }
#endif

#ifdef MOZ_X11
    if (GraphicsExpose == event.event.type) {
        // Make sure the X server completes the drawing before the parent
        // draws on top and destroys the Drawable.
        //
        // XSync() waits for the X server to complete.  Really this child
        // process does not need to wait; the parent is the process that needs
        // to wait.  A possibly-slightly-better alternative would be to send
        // an X event to the parent that the parent would wait for.
        XSync(mWsInfo.display, False);
    }
#endif

    return true;
}

#ifdef XP_MACOSX

bool
PluginInstanceChild::AnswerNPP_HandleEvent_Shmem(const NPRemoteEvent& event,
                                                 Shmem&& mem,
                                                 int16_t* handled,
                                                 Shmem* rtnmem)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();

    PaintTracker pt;

    NPCocoaEvent evcopy = event.event;
    mContentsScaleFactor = event.contentsScaleFactor;

    if (evcopy.type == NPCocoaEventDrawRect) {
        int scaleFactor = ceil(mContentsScaleFactor);
        if (!mShColorSpace) {
            mShColorSpace = CreateSystemColorSpace();
            if (!mShColorSpace) {
                PLUGIN_LOG_DEBUG(("Could not allocate ColorSpace."));
                *handled = false;
                *rtnmem = mem;
                return true;
            } 
        }
        if (!mShContext) {
            void* cgContextByte = mem.get<char>();
            mShContext = ::CGBitmapContextCreate(cgContextByte, 
                              mWindow.width * scaleFactor,
                              mWindow.height * scaleFactor, 8, 
                              mWindow.width * 4 * scaleFactor, mShColorSpace, 
                              kCGImageAlphaPremultipliedFirst |
                              kCGBitmapByteOrder32Host);
    
            if (!mShContext) {
                PLUGIN_LOG_DEBUG(("Could not allocate CGBitmapContext."));
                *handled = false;
                *rtnmem = mem;
                return true;
            }
        }
        CGRect clearRect = ::CGRectMake(0, 0, mWindow.width, mWindow.height);
        ::CGContextClearRect(mShContext, clearRect);
        evcopy.data.draw.context = mShContext; 
    } else {
        PLUGIN_LOG_DEBUG(("Invalid event type for AnswerNNP_HandleEvent_Shmem."));
        *handled = false;
        *rtnmem = mem;
        return true;
    } 

    if (!mPluginIface->event) {
        *handled = false;
    } else {
        ::CGContextSaveGState(evcopy.data.draw.context);
        *handled = mPluginIface->event(&mData, reinterpret_cast<void*>(&evcopy));
        ::CGContextRestoreGState(evcopy.data.draw.context);
    }

    *rtnmem = mem;
    return true;
}

#else
bool
PluginInstanceChild::AnswerNPP_HandleEvent_Shmem(const NPRemoteEvent& event,
                                                 Shmem&& mem,
                                                 int16_t* handled,
                                                 Shmem* rtnmem)
{
    NS_RUNTIMEABORT("not reached.");
    *rtnmem = mem;
    return true;
}
#endif

#ifdef XP_MACOSX

void CallCGDraw(CGContextRef ref, void* aPluginInstance, nsIntRect aUpdateRect) {
  PluginInstanceChild* pluginInstance = (PluginInstanceChild*)aPluginInstance;

  pluginInstance->CGDraw(ref, aUpdateRect);
}

bool
PluginInstanceChild::CGDraw(CGContextRef ref, nsIntRect aUpdateRect) {

  NPCocoaEvent drawEvent;
  drawEvent.type = NPCocoaEventDrawRect;
  drawEvent.version = 0;
  drawEvent.data.draw.x = aUpdateRect.x;
  drawEvent.data.draw.y = aUpdateRect.y;
  drawEvent.data.draw.width = aUpdateRect.width;
  drawEvent.data.draw.height = aUpdateRect.height;
  drawEvent.data.draw.context = ref;

  NPRemoteEvent remoteDrawEvent = {drawEvent};
  // Signal to AnswerNPP_HandleEvent() not to use this value
  remoteDrawEvent.contentsScaleFactor = -1.0;

  int16_t handled;
  AnswerNPP_HandleEvent(remoteDrawEvent, &handled);
  return handled == true;
}

bool
PluginInstanceChild::AnswerNPP_HandleEvent_IOSurface(const NPRemoteEvent& event,
                                                     const uint32_t &surfaceid,
                                                     int16_t* handled)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();

    PaintTracker pt;

    NPCocoaEvent evcopy = event.event;
    mContentsScaleFactor = event.contentsScaleFactor;
    RefPtr<MacIOSurface> surf = MacIOSurface::LookupSurface(surfaceid,
                                                            mContentsScaleFactor);
    if (!surf) {
        NS_ERROR("Invalid IOSurface.");
        *handled = false;
        return false;
    }

    if (!mCARenderer) {
      mCARenderer = new nsCARenderer();
    }

    if (evcopy.type == NPCocoaEventDrawRect) {
        mCARenderer->AttachIOSurface(surf);
        if (!mCARenderer->isInit()) {
            void *caLayer = nullptr;
            NPError result = mPluginIface->getvalue(GetNPP(), 
                                     NPPVpluginCoreAnimationLayer,
                                     &caLayer);
            
            if (result != NPERR_NO_ERROR || !caLayer) {
                PLUGIN_LOG_DEBUG(("Plugin requested CoreAnimation but did not "
                                  "provide CALayer."));
                *handled = false;
                return false;
            }

            mCARenderer->SetupRenderer(caLayer, mWindow.width, mWindow.height,
                            mContentsScaleFactor,
                            GetQuirks() & PluginModuleChild::QUIRK_ALLOW_OFFLINE_RENDERER ?
                            ALLOW_OFFLINE_RENDERER : DISALLOW_OFFLINE_RENDERER);

            // Flash needs to have the window set again after this step
            if (mPluginIface->setwindow)
                (void) mPluginIface->setwindow(&mData, &mWindow);
        }
    } else {
        PLUGIN_LOG_DEBUG(("Invalid event type for "
                          "AnswerNNP_HandleEvent_IOSurface."));
        *handled = false;
        return false;
    } 

    mCARenderer->Render(mWindow.width, mWindow.height,
                        mContentsScaleFactor, nullptr);

    return true;

}

#else
bool
PluginInstanceChild::AnswerNPP_HandleEvent_IOSurface(const NPRemoteEvent& event,
                                                     const uint32_t &surfaceid,
                                                     int16_t* handled)
{
    NS_RUNTIMEABORT("NPP_HandleEvent_IOSurface is a OSX-only message");
    return false;
}
#endif

bool
PluginInstanceChild::RecvWindowPosChanged(const NPRemoteEvent& event)
{
    NS_ASSERTION(!mLayersRendering && !mPendingPluginCall,
                 "Shouldn't be receiving WindowPosChanged with layer rendering");

#ifdef OS_WIN
    int16_t dontcare;
    return AnswerNPP_HandleEvent(event, &dontcare);
#else
    NS_RUNTIMEABORT("WindowPosChanged is a windows-only message");
    return false;
#endif
}

bool
PluginInstanceChild::RecvContentsScaleFactorChanged(const double& aContentsScaleFactor)
{
#ifdef XP_MACOSX
    mContentsScaleFactor = aContentsScaleFactor;
    if (mShContext) {
        // Release the shared context so that it is reallocated
        // with the new size. 
        ::CGContextRelease(mShContext);
        mShContext = nullptr;
    }
    return true;
#else
    NS_RUNTIMEABORT("ContentsScaleFactorChanged is an OSX-only message");
    return false;
#endif
}

#if defined(MOZ_X11) && defined(XP_UNIX) && !defined(XP_MACOSX)
// Create a new window from NPWindow
bool PluginInstanceChild::CreateWindow(const NPRemoteWindow& aWindow)
{ 
    PLUGIN_LOG_DEBUG(("%s (aWindow=<window: 0x%lx, x: %d, y: %d, width: %d, height: %d>)",
                      FULLFUNCTION,
                      aWindow.window,
                      aWindow.x, aWindow.y,
                      aWindow.width, aWindow.height));

#ifdef MOZ_WIDGET_GTK
    if (mXEmbed) {
        mWindow.window = reinterpret_cast<void*>(aWindow.window);
    }
    else {
        Window browserSocket = (Window)(aWindow.window);
        xt_client_init(&mXtClient, mWsInfo.visual, mWsInfo.colormap, mWsInfo.depth);
        xt_client_create(&mXtClient, browserSocket, mWindow.width, mWindow.height); 
        mWindow.window = (void *)XtWindow(mXtClient.child_widget);
    }  
#else
    mWindow.window = reinterpret_cast<void*>(aWindow.window);
#endif

    return true;
}

// Destroy window
void PluginInstanceChild::DeleteWindow()
{
  PLUGIN_LOG_DEBUG(("%s (aWindow=<window: 0x%lx, x: %d, y: %d, width: %d, height: %d>)",
                    FULLFUNCTION,
                    mWindow.window,
                    mWindow.x, mWindow.y,
                    mWindow.width, mWindow.height));

  if (!mWindow.window)
      return;

#ifdef MOZ_WIDGET_GTK
  if (mXtClient.top_widget) {     
      xt_client_unrealize(&mXtClient);
      xt_client_destroy(&mXtClient); 
      mXtClient.top_widget = nullptr;
  }
#endif

  // We don't have to keep the plug-in window ID any longer.
  mWindow.window = nullptr;
}
#endif

bool
PluginInstanceChild::AnswerNPP_SetWindow(const NPRemoteWindow& aWindow,
                                         NPRemoteWindow* aChildWindowToBeAdopted)
{
    PLUGIN_LOG_DEBUG(("%s (aWindow=<window: 0x%lx, x: %d, y: %d, width: %d, height: %d>)",
                      FULLFUNCTION,
                      aWindow.window,
                      aWindow.x, aWindow.y,
                      aWindow.width, aWindow.height));
    NS_ASSERTION(!mLayersRendering && !mPendingPluginCall,
                 "Shouldn't be receiving NPP_SetWindow with layer rendering");
    AssertPluginThread();

#if defined(MOZ_X11) && defined(XP_UNIX) && !defined(XP_MACOSX)
    NS_ASSERTION(mWsInfo.display, "We should have a valid display!");

    // The minimum info is sent over IPC to allow this
    // code to determine the rest.

    mWindow.x = aWindow.x;
    mWindow.y = aWindow.y;
    mWindow.width = aWindow.width;
    mWindow.height = aWindow.height;
    mWindow.clipRect = aWindow.clipRect;
    mWindow.type = aWindow.type;

    mWsInfo.colormap = aWindow.colormap;
    int depth;
    FindVisualAndDepth(mWsInfo.display, aWindow.visualID,
                       &mWsInfo.visual, &depth);
    mWsInfo.depth = depth;

    if (!mWindow.window && mWindow.type == NPWindowTypeWindow) {
        CreateWindow(aWindow);
    }

#ifdef MOZ_WIDGET_GTK
    if (mXEmbed && gtk_check_version(2,18,7) != nullptr) { // older
        if (aWindow.type == NPWindowTypeWindow) {
            GdkWindow* socket_window = gdk_window_lookup(static_cast<GdkNativeWindow>(aWindow.window));
            if (socket_window) {
                // A GdkWindow for the socket already exists.  Need to
                // workaround https://bugzilla.gnome.org/show_bug.cgi?id=607061
                // See wrap_gtk_plug_embedded in PluginModuleChild.cpp.
                g_object_set_data(G_OBJECT(socket_window),
                                  "moz-existed-before-set-window",
                                  GUINT_TO_POINTER(1));
            }
        }

        if (aWindow.visualID != None
            && gtk_check_version(2, 12, 10) != nullptr) { // older
            // Workaround for a bug in Gtk+ (prior to 2.12.10) where deleting
            // a foreign GdkColormap will also free the XColormap.
            // http://git.gnome.org/browse/gtk+/log/gdk/x11/gdkcolor-x11.c?id=GTK_2_12_10
            GdkVisual *gdkvisual = gdkx_visual_get(aWindow.visualID);
            GdkColormap *gdkcolor =
                gdk_x11_colormap_foreign_new(gdkvisual, aWindow.colormap);

            if (g_object_get_data(G_OBJECT(gdkcolor), "moz-have-extra-ref")) {
                // We already have a ref to keep the object alive.
                g_object_unref(gdkcolor);
            } else {
                // leak and mark as already leaked
                g_object_set_data(G_OBJECT(gdkcolor),
                                  "moz-have-extra-ref", GUINT_TO_POINTER(1));
            }
        }
    }
#endif

    PLUGIN_LOG_DEBUG(
        ("[InstanceChild][%p] Answer_SetWindow w=<x=%d,y=%d, w=%d,h=%d>, clip=<l=%d,t=%d,r=%d,b=%d>",
         this, mWindow.x, mWindow.y, mWindow.width, mWindow.height,
         mWindow.clipRect.left, mWindow.clipRect.top, mWindow.clipRect.right, mWindow.clipRect.bottom));

    if (mPluginIface->setwindow)
        (void) mPluginIface->setwindow(&mData, &mWindow);

#elif defined(OS_WIN)
    switch (aWindow.type) {
      case NPWindowTypeWindow:
      {
          if ((GetQuirks() & PluginModuleChild::QUIRK_QUICKTIME_AVOID_SETWINDOW) &&
              aWindow.width == 0 &&
              aWindow.height == 0) {
            // Skip SetWindow call for hidden QuickTime plugins
            return true;
          }

          if (!CreatePluginWindow())
              return false;

          SizePluginWindow(aWindow.width, aWindow.height);

          // If the window is not our parent set the return child window so that
          // it can be re-parented in the chrome process. Re-parenting now
          // happens there as we might not have sufficient permission.
          // Also, this needs to be after SizePluginWindow because SetWindowPos
          // relies on things that it sets.
          HWND parentWindow = reinterpret_cast<HWND>(aWindow.window);
          if (mPluginParentHWND != parentWindow  && IsWindow(parentWindow)) {
              mPluginParentHWND = parentWindow;
              aChildWindowToBeAdopted->window =
                  reinterpret_cast<uint64_t>(mPluginWindowHWND);
          } else {
              // Now we know that the window has the correct parent we can show
              // it. The actual visibility is controlled by its parent.
              // First time round, these calls are made by our caller after the
              // parent is set.
              ShowWindow(mPluginWindowHWND, SW_SHOWNA);

              // This used to be called in SizePluginWindow, but we need to make
              // sure that mPluginWindowHWND has had it's parent set correctly,
              // otherwise it can cause a focus issue.
              SetWindowPos(mPluginWindowHWND, nullptr, 0, 0, aWindow.width,
                           aWindow.height, SWP_NOZORDER | SWP_NOREPOSITION);
          }

          mWindow.window = (void*)mPluginWindowHWND;
          mWindow.x = aWindow.x;
          mWindow.y = aWindow.y;
          mWindow.width = aWindow.width;
          mWindow.height = aWindow.height;
          mWindow.type = aWindow.type;

          if (mPluginIface->setwindow) {
              SetProp(mPluginWindowHWND, kPluginIgnoreSubclassProperty, (HANDLE)1);
              (void) mPluginIface->setwindow(&mData, &mWindow);
              WNDPROC wndProc = reinterpret_cast<WNDPROC>(
                  GetWindowLongPtr(mPluginWindowHWND, GWLP_WNDPROC));
              if (wndProc != PluginWindowProc) {
                  mPluginWndProc = reinterpret_cast<WNDPROC>(
                      SetWindowLongPtr(mPluginWindowHWND, GWLP_WNDPROC,
                                       reinterpret_cast<LONG_PTR>(PluginWindowProc)));
                  NS_ASSERTION(mPluginWndProc != PluginWindowProc, "WTF?");
              }
              RemoveProp(mPluginWindowHWND, kPluginIgnoreSubclassProperty);
              HookSetWindowLongPtr();
          }
      }
      break;

      case NPWindowTypeDrawable:
          mWindow.type = aWindow.type;
          if (GetQuirks() & PluginModuleChild::QUIRK_WINLESS_TRACKPOPUP_HOOK)
              CreateWinlessPopupSurrogate();
          if (GetQuirks() & PluginModuleChild::QUIRK_FLASH_THROTTLE_WMUSER_EVENTS)
              SetupFlashMsgThrottle();
          return SharedSurfaceSetWindow(aWindow);
      break;

      default:
          NS_NOTREACHED("Bad plugin window type.");
          return false;
      break;
    }

#elif defined(XP_MACOSX)

    mWindow.x = aWindow.x;
    mWindow.y = aWindow.y;
    mWindow.width = aWindow.width;
    mWindow.height = aWindow.height;
    mWindow.clipRect = aWindow.clipRect;
    mWindow.type = aWindow.type;
    mContentsScaleFactor = aWindow.contentsScaleFactor;

    if (mShContext) {
        // Release the shared context so that it is reallocated
        // with the new size. 
        ::CGContextRelease(mShContext);
        mShContext = nullptr;
    }

    if (mPluginIface->setwindow)
        (void) mPluginIface->setwindow(&mData, &mWindow);

#elif defined(ANDROID)
    // TODO: Need Android impl
#elif defined(MOZ_WIDGET_QT)
    // TODO: Need QT-nonX impl
#elif defined(MOZ_WIDGET_UIKIT)
    // Don't care
#else
#  error Implement me for your OS
#endif

    return true;
}

bool
PluginInstanceChild::Initialize()
{
#ifdef MOZ_WIDGET_GTK
    NPError rv;

    if (mWsInfo.display) {
        // Already initialized
        return false;
    }

    // Request for windowless plugins is set in newp(), before this call.
    if (mWindow.type == NPWindowTypeWindow) {
        AnswerNPP_GetValue_NPPVpluginNeedsXEmbed(&mXEmbed, &rv);

        // Set up Xt loop for windowed plugins without XEmbed support
        if (!mXEmbed) {
           xt_client_xloop_create();
        }
    }

    // Use default GTK display for XEmbed and windowless plugins
    if (mXEmbed || mWindow.type != NPWindowTypeWindow) {
        mWsInfo.display = DefaultXDisplay();
    }
    else {
        mWsInfo.display = xt_client_get_display();
    }
#endif 

    return true;
}

#if defined(OS_WIN)

static const TCHAR kWindowClassName[] = TEXT("GeckoPluginWindow");
static const TCHAR kPluginInstanceChildProperty[] = TEXT("PluginInstanceChildProperty");
static const TCHAR kFlashThrottleProperty[] = TEXT("MozillaFlashThrottleProperty");

// static
bool
PluginInstanceChild::RegisterWindowClass()
{
    static bool alreadyRegistered = false;
    if (alreadyRegistered)
        return true;

    alreadyRegistered = true;

    WNDCLASSEX wcex;
    wcex.cbSize         = sizeof(WNDCLASSEX);
    wcex.style          = CS_DBLCLKS;
    wcex.lpfnWndProc    = DummyWindowProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = GetModuleHandle(nullptr);
    wcex.hIcon          = 0;
    wcex.hCursor        = 0;
    wcex.hbrBackground  = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = kWindowClassName;
    wcex.hIconSm        = 0;

    return RegisterClassEx(&wcex) ? true : false;
}

bool
PluginInstanceChild::CreatePluginWindow()
{
    // already initialized
    if (mPluginWindowHWND)
        return true;
        
    if (!RegisterWindowClass())
        return false;

    mPluginWindowHWND =
        CreateWindowEx(WS_EX_LEFT | WS_EX_LTRREADING |
                       WS_EX_NOPARENTNOTIFY | // XXXbent Get rid of this!
                       WS_EX_RIGHTSCROLLBAR,
                       kWindowClassName, 0,
                       WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0,
                       0, 0, nullptr, 0, GetModuleHandle(nullptr), 0);
    if (!mPluginWindowHWND)
        return false;
    if (!SetProp(mPluginWindowHWND, kPluginInstanceChildProperty, this))
        return false;

    // Apparently some plugins require an ASCII WndProc.
    SetWindowLongPtrA(mPluginWindowHWND, GWLP_WNDPROC,
                      reinterpret_cast<LONG_PTR>(DefWindowProcA));

    return true;
}

void
PluginInstanceChild::DestroyPluginWindow()
{
    if (mPluginWindowHWND) {
        // Unsubclass the window.
        WNDPROC wndProc = reinterpret_cast<WNDPROC>(
            GetWindowLongPtr(mPluginWindowHWND, GWLP_WNDPROC));
        // Removed prior to SetWindowLongPtr, see HookSetWindowLongPtr.
        RemoveProp(mPluginWindowHWND, kPluginInstanceChildProperty);
        if (wndProc == PluginWindowProc) {
            NS_ASSERTION(mPluginWndProc, "Should have old proc here!");
            SetWindowLongPtr(mPluginWindowHWND, GWLP_WNDPROC,
                             reinterpret_cast<LONG_PTR>(mPluginWndProc));
            mPluginWndProc = 0;
        }
        DestroyWindow(mPluginWindowHWND);
        mPluginWindowHWND = 0;
    }
}

void
PluginInstanceChild::SizePluginWindow(int width,
                                      int height)
{
    if (mPluginWindowHWND) {
        mPluginSize.x = width;
        mPluginSize.y = height;
    }
}

// See chromium's webplugin_delegate_impl.cc for explanation of this function.
// static
LRESULT CALLBACK
PluginInstanceChild::DummyWindowProc(HWND hWnd,
                                     UINT message,
                                     WPARAM wParam,
                                     LPARAM lParam)
{
    return CallWindowProc(DefWindowProc, hWnd, message, wParam, lParam);
}

// static
LRESULT CALLBACK
PluginInstanceChild::PluginWindowProc(HWND hWnd,
                                      UINT message,
                                      WPARAM wParam,
                                      LPARAM lParam)
{
  return mozilla::CallWindowProcCrashProtected(PluginWindowProcInternal, hWnd, message, wParam, lParam);
}

// static
LRESULT CALLBACK
PluginInstanceChild::PluginWindowProcInternal(HWND hWnd,
                                              UINT message,
                                              WPARAM wParam,
                                              LPARAM lParam)
{
    NS_ASSERTION(!mozilla::ipc::MessageChannel::IsPumpingMessages(),
                 "Failed to prevent a nonqueued message from running!");
    PluginInstanceChild* self = reinterpret_cast<PluginInstanceChild*>(
        GetProp(hWnd, kPluginInstanceChildProperty));
    if (!self) {
        NS_NOTREACHED("Badness!");
        return 0;
    }

    NS_ASSERTION(self->mPluginWindowHWND == hWnd, "Wrong window!");
    NS_ASSERTION(self->mPluginWndProc != PluginWindowProc, "Self-referential windowproc. Infinite recursion will happen soon.");

    // Adobe's shockwave positions the plugin window relative to the browser
    // frame when it initializes. With oopp disabled, this wouldn't have an
    // effect. With oopp, GeckoPluginWindow is a child of the parent plugin
    // window, so the move offsets the child within the parent. Generally
    // we don't want plugins moving or sizing our window, so we prevent these
    // changes here.
    if (message == WM_WINDOWPOSCHANGING) {
      WINDOWPOS* pos = reinterpret_cast<WINDOWPOS*>(lParam);
      if (pos && (!(pos->flags & SWP_NOMOVE) || !(pos->flags & SWP_NOSIZE))) {
        pos->x = pos->y = 0;
        pos->cx = self->mPluginSize.x;
        pos->cy = self->mPluginSize.y;
        LRESULT res = CallWindowProc(self->mPluginWndProc, hWnd, message, wParam,
                                     lParam);
        pos->x = pos->y = 0;
        pos->cx = self->mPluginSize.x;
        pos->cy = self->mPluginSize.y;
        return res;
      }
    }

    // The plugin received keyboard focus, let the parent know so the dom is up to date.
    if (message == WM_MOUSEACTIVATE) {
      self->CallPluginFocusChange(true);
    }

    // Prevent lockups due to plugins making rpc calls when the parent
    // is making a synchronous SendMessage call to the child window. Add
    // more messages as needed.
    if ((InSendMessageEx(nullptr)&(ISMEX_REPLIED|ISMEX_SEND)) == ISMEX_SEND) {
        switch(message) {
            case WM_CHILDACTIVATE:
            case WM_KILLFOCUS:
            ReplyMessage(0);
            break;
        }
    }

    if (message == WM_KILLFOCUS) {
      self->CallPluginFocusChange(false);
    }

    if (message == WM_USER+1 &&
        (self->GetQuirks() & PluginModuleChild::QUIRK_FLASH_THROTTLE_WMUSER_EVENTS)) {
        self->FlashThrottleMessage(hWnd, message, wParam, lParam, true);
        return 0;
    }

    NS_ASSERTION(self->mPluginWndProc != PluginWindowProc,
      "Self-referential windowproc happened inside our hook proc. "
      "Infinite recursion will happen soon.");

    LRESULT res = CallWindowProc(self->mPluginWndProc, hWnd, message, wParam,
                                 lParam);

    // Make sure capture is released by the child on mouse events. Fixes a
    // problem with flash full screen mode mouse input. Appears to be
    // caused by a bug in flash, since we are not setting the capture
    // on the window.
    if (message == WM_LBUTTONDOWN &&
        self->GetQuirks() & PluginModuleChild::QUIRK_FLASH_FIXUP_MOUSE_CAPTURE) {
      wchar_t szClass[26];
      HWND hwnd = GetForegroundWindow();
      if (hwnd && GetClassNameW(hwnd, szClass,
                                sizeof(szClass)/sizeof(char16_t)) &&
          !wcscmp(szClass, kFlashFullscreenClass)) {
        ReleaseCapture();
        SetFocus(hwnd);
      }
    }

    if (message == WM_CLOSE) {
        self->DestroyPluginWindow();
    }

    if (message == WM_NCDESTROY) {
        RemoveProp(hWnd, kPluginInstanceChildProperty);
    }

    return res;
}

/* set window long ptr hook for flash */

/*
 * Flash will reset the subclass of our widget at various times.
 * (Notably when entering and exiting full screen mode.) This
 * occurs independent of the main plugin window event procedure.
 * We trap these subclass calls to prevent our subclass hook from
 * getting dropped.
 * Note, ascii versions can be nixed once flash versions < 10.1
 * are considered obsolete.
 */
 
#ifdef _WIN64
typedef LONG_PTR
  (WINAPI *User32SetWindowLongPtrA)(HWND hWnd,
                                    int nIndex,
                                    LONG_PTR dwNewLong);
typedef LONG_PTR
  (WINAPI *User32SetWindowLongPtrW)(HWND hWnd,
                                    int nIndex,
                                    LONG_PTR dwNewLong);
static User32SetWindowLongPtrA sUser32SetWindowLongAHookStub = nullptr;
static User32SetWindowLongPtrW sUser32SetWindowLongWHookStub = nullptr;
#else
typedef LONG
(WINAPI *User32SetWindowLongA)(HWND hWnd,
                               int nIndex,
                               LONG dwNewLong);
typedef LONG
(WINAPI *User32SetWindowLongW)(HWND hWnd,
                               int nIndex,
                               LONG dwNewLong);
static User32SetWindowLongA sUser32SetWindowLongAHookStub = nullptr;
static User32SetWindowLongW sUser32SetWindowLongWHookStub = nullptr;
#endif

extern LRESULT CALLBACK
NeuteredWindowProc(HWND hwnd,
                   UINT uMsg,
                   WPARAM wParam,
                   LPARAM lParam);

const wchar_t kOldWndProcProp[] = L"MozillaIPCOldWndProc";

// static
bool
PluginInstanceChild::SetWindowLongHookCheck(HWND hWnd,
                                            int nIndex,
                                            LONG_PTR newLong)
{
      // Let this go through if it's not a subclass
  if (nIndex != GWLP_WNDPROC ||
      // if it's not a subclassed plugin window
      !GetProp(hWnd, kPluginInstanceChildProperty) ||
      // if we're not disabled
      GetProp(hWnd, kPluginIgnoreSubclassProperty) ||
      // if the subclass is set to a known procedure
      newLong == reinterpret_cast<LONG_PTR>(PluginWindowProc) ||
      newLong == reinterpret_cast<LONG_PTR>(NeuteredWindowProc) ||
      newLong == reinterpret_cast<LONG_PTR>(DefWindowProcA) ||
      newLong == reinterpret_cast<LONG_PTR>(DefWindowProcW) ||
      // if the subclass is a WindowsMessageLoop subclass restore
      GetProp(hWnd, kOldWndProcProp))
      return true;
  // prevent the subclass
  return false;
}

#ifdef _WIN64
LONG_PTR WINAPI
PluginInstanceChild::SetWindowLongPtrAHook(HWND hWnd,
                                           int nIndex,
                                           LONG_PTR newLong)
#else
LONG WINAPI
PluginInstanceChild::SetWindowLongAHook(HWND hWnd,
                                        int nIndex,
                                        LONG newLong)
#endif
{
    if (SetWindowLongHookCheck(hWnd, nIndex, newLong))
        return sUser32SetWindowLongAHookStub(hWnd, nIndex, newLong);

    // Set flash's new subclass to get the result. 
    LONG_PTR proc = sUser32SetWindowLongAHookStub(hWnd, nIndex, newLong);

    // We already checked this in SetWindowLongHookCheck
    PluginInstanceChild* self = reinterpret_cast<PluginInstanceChild*>(
        GetProp(hWnd, kPluginInstanceChildProperty));

    // Hook our subclass back up, just like we do on setwindow.   
    WNDPROC currentProc =
        reinterpret_cast<WNDPROC>(GetWindowLongPtr(hWnd, GWLP_WNDPROC));
    if (currentProc != PluginWindowProc) {
        self->mPluginWndProc =
            reinterpret_cast<WNDPROC>(sUser32SetWindowLongWHookStub(hWnd, nIndex,
                reinterpret_cast<LONG_PTR>(PluginWindowProc)));
        NS_ASSERTION(self->mPluginWndProc != PluginWindowProc, "Infinite recursion coming up!");
    }
    return proc;
}

#ifdef _WIN64
LONG_PTR WINAPI
PluginInstanceChild::SetWindowLongPtrWHook(HWND hWnd,
                                           int nIndex,
                                           LONG_PTR newLong)
#else
LONG WINAPI
PluginInstanceChild::SetWindowLongWHook(HWND hWnd,
                                        int nIndex,
                                        LONG newLong)
#endif
{
    if (SetWindowLongHookCheck(hWnd, nIndex, newLong))
        return sUser32SetWindowLongWHookStub(hWnd, nIndex, newLong);

    // Set flash's new subclass to get the result. 
    LONG_PTR proc = sUser32SetWindowLongWHookStub(hWnd, nIndex, newLong);

    // We already checked this in SetWindowLongHookCheck
    PluginInstanceChild* self = reinterpret_cast<PluginInstanceChild*>(
        GetProp(hWnd, kPluginInstanceChildProperty));

    // Hook our subclass back up, just like we do on setwindow.   
    WNDPROC currentProc =
        reinterpret_cast<WNDPROC>(GetWindowLongPtr(hWnd, GWLP_WNDPROC));
    if (currentProc != PluginWindowProc) {
        self->mPluginWndProc =
            reinterpret_cast<WNDPROC>(sUser32SetWindowLongWHookStub(hWnd, nIndex,
                reinterpret_cast<LONG_PTR>(PluginWindowProc)));
        NS_ASSERTION(self->mPluginWndProc != PluginWindowProc, "Infinite recursion coming up!");
    }
    return proc;
}

void
PluginInstanceChild::HookSetWindowLongPtr()
{
    if (!(GetQuirks() & PluginModuleChild::QUIRK_FLASH_HOOK_SETLONGPTR))
        return;

    sUser32Intercept.Init("user32.dll");
#ifdef _WIN64
    if (!sUser32SetWindowLongAHookStub)
        sUser32Intercept.AddHook("SetWindowLongPtrA", reinterpret_cast<intptr_t>(SetWindowLongPtrAHook),
                                 (void**) &sUser32SetWindowLongAHookStub);
    if (!sUser32SetWindowLongWHookStub)
        sUser32Intercept.AddHook("SetWindowLongPtrW", reinterpret_cast<intptr_t>(SetWindowLongPtrWHook),
                                 (void**) &sUser32SetWindowLongWHookStub);
#else
    if (!sUser32SetWindowLongAHookStub)
        sUser32Intercept.AddHook("SetWindowLongA", reinterpret_cast<intptr_t>(SetWindowLongAHook),
                                 (void**) &sUser32SetWindowLongAHookStub);
    if (!sUser32SetWindowLongWHookStub)
        sUser32Intercept.AddHook("SetWindowLongW", reinterpret_cast<intptr_t>(SetWindowLongWHook),
                                 (void**) &sUser32SetWindowLongWHookStub);
#endif
}

class SetCaptureHookData
{
public:
    explicit SetCaptureHookData(HWND aHwnd)
        : mHwnd(aHwnd)
        , mHaveRect(false)
    {
        MOZ_ASSERT(aHwnd);
        mHaveRect = !!GetClientRect(aHwnd, &mCaptureRect);
    }

    /**
     * @return true if capture was released
     */
    bool HandleMouseMsg(const MSG& aMsg)
    {
        // If the window belongs to Unity, the mouse button is up, and the mouse
        // has moved outside the client rect of the Unity window, release capture.
        if (aMsg.hwnd != mHwnd || !mHaveRect) {
            return false;
        }
        if (aMsg.message != WM_MOUSEMOVE && aMsg.message != WM_LBUTTONUP) {
            return false;
        }
        if ((aMsg.message == WM_MOUSEMOVE && (aMsg.wParam & MK_LBUTTON))) {
            return false;
        }
        POINT pt = { GET_X_LPARAM(aMsg.lParam), GET_Y_LPARAM(aMsg.lParam) };
        if (PtInRect(&mCaptureRect, pt)) {
            return false;
        }
        return !!ReleaseCapture();
    }

    bool IsUnityLosingCapture(const CWPSTRUCT& aInfo) const
    {
        return aInfo.message == WM_CAPTURECHANGED &&
               aInfo.hwnd == mHwnd;
    }

private:
    HWND mHwnd;
    bool mHaveRect;
    RECT mCaptureRect;
};

static StaticAutoPtr<SetCaptureHookData> sSetCaptureHookData;
typedef HWND (WINAPI* User32SetCapture)(HWND);
static User32SetCapture sUser32SetCaptureHookStub = nullptr;

HWND WINAPI
PluginInstanceChild::SetCaptureHook(HWND aHwnd)
{
    // Don't do anything unless aHwnd belongs to Unity
    wchar_t className[256] = {0};
    int numChars = GetClassNameW(aHwnd, className, ArrayLength(className));
    NS_NAMED_LITERAL_STRING(unityClassName, "Unity.WebPlayer");
    if (numChars == unityClassName.Length() && unityClassName == wwc(className)) {
        sSetCaptureHookData = new SetCaptureHookData(aHwnd);
    }
    return sUser32SetCaptureHookStub(aHwnd);
}

void
PluginInstanceChild::SetUnityHooks()
{
    if (!(GetQuirks() & PluginModuleChild::QUIRK_UNITY_FIXUP_MOUSE_CAPTURE)) {
        return;
    }

    sUser32Intercept.Init("user32.dll");
    if (!sUser32SetCaptureHookStub) {
        sUser32Intercept.AddHook("SetCapture",
                                 reinterpret_cast<intptr_t>(SetCaptureHook),
                                 (void**) &sUser32SetCaptureHookStub);
    }
    if (!mUnityGetMessageHook) {
        mUnityGetMessageHook = SetWindowsHookEx(WH_GETMESSAGE,
                                                &UnityGetMessageHookProc, NULL,
                                                GetCurrentThreadId());
    }
    if (!mUnitySendMessageHook) {
        mUnitySendMessageHook = SetWindowsHookEx(WH_CALLWNDPROC,
                                                 &UnitySendMessageHookProc,
                                                 NULL, GetCurrentThreadId());
    }
}

void
PluginInstanceChild::ClearUnityHooks()
{
    if (mUnityGetMessageHook) {
        UnhookWindowsHookEx(mUnityGetMessageHook);
        mUnityGetMessageHook = NULL;
    }
    if (mUnitySendMessageHook) {
        UnhookWindowsHookEx(mUnitySendMessageHook);
        mUnitySendMessageHook = NULL;
    }
    sSetCaptureHookData = nullptr;
}

LRESULT CALLBACK
PluginInstanceChild::UnityGetMessageHookProc(int aCode, WPARAM aWparam,
                                             LPARAM aLParam)
{
    if (aCode >= 0) {
        MSG* info = reinterpret_cast<MSG*>(aLParam);
        MOZ_ASSERT(info);
        if (sSetCaptureHookData && sSetCaptureHookData->HandleMouseMsg(*info)) {
            sSetCaptureHookData = nullptr;
        }
    }

    return CallNextHookEx(0, aCode, aWparam, aLParam);
}

LRESULT CALLBACK
PluginInstanceChild::UnitySendMessageHookProc(int aCode, WPARAM aWparam,
                                              LPARAM aLParam)
{
    if (aCode >= 0) {
        CWPSTRUCT* info = reinterpret_cast<CWPSTRUCT*>(aLParam);
        MOZ_ASSERT(info);
        if (sSetCaptureHookData &&
            sSetCaptureHookData->IsUnityLosingCapture(*info)) {
            sSetCaptureHookData = nullptr;
        }
    }

    return CallNextHookEx(0, aCode, aWparam, aLParam);
}

/* windowless track popup menu helpers */

BOOL
WINAPI
PluginInstanceChild::TrackPopupHookProc(HMENU hMenu,
                                        UINT uFlags,
                                        int x,
                                        int y,
                                        int nReserved,
                                        HWND hWnd,
                                        CONST RECT *prcRect)
{
  if (!sUser32TrackPopupMenuStub) {
      NS_ERROR("TrackPopupMenu stub isn't set! Badness!");
      return 0;
  }

  // Only change the parent when we know this is a context on the plugin
  // surface within the browser. Prevents resetting the parent on child ui
  // displayed by plugins that have working parent-child relationships.
  wchar_t szClass[21];
  bool haveClass = GetClassNameW(hWnd, szClass, ArrayLength(szClass));
  if (!haveClass || 
      (wcscmp(szClass, L"MozillaWindowClass") &&
       wcscmp(szClass, L"SWFlash_Placeholder"))) {
      // Unrecognized parent
      return sUser32TrackPopupMenuStub(hMenu, uFlags, x, y, nReserved,
                                       hWnd, prcRect);
  }

  // Called on an unexpected event, warn.
  if (!sWinlessPopupSurrogateHWND) {
      NS_WARNING(
          "Untraced TrackPopupHookProc call! Menu might not work right!");
      return sUser32TrackPopupMenuStub(hMenu, uFlags, x, y, nReserved,
                                       hWnd, prcRect);
  }

  HWND surrogateHwnd = sWinlessPopupSurrogateHWND;
  sWinlessPopupSurrogateHWND = nullptr;

  // Popups that don't use TPM_RETURNCMD expect a final command message
  // when an item is selected and the context closes. Since we replace
  // the parent, we need to forward this back to the real parent so it
  // can act on the menu item selected.
  bool isRetCmdCall = (uFlags & TPM_RETURNCMD);

  DWORD res = sUser32TrackPopupMenuStub(hMenu, uFlags|TPM_RETURNCMD, x, y,
                                        nReserved, surrogateHwnd, prcRect);

  if (!isRetCmdCall && res) {
      SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(res, 0), 0);
  }

  return res;
}

void
PluginInstanceChild::InitPopupMenuHook()
{
    if (!(GetQuirks() & PluginModuleChild::QUIRK_WINLESS_TRACKPOPUP_HOOK) ||
        sUser32TrackPopupMenuStub)
        return;

    // Note, once WindowsDllInterceptor is initialized for a module,
    // it remains initialized for that particular module for it's
    // lifetime. Additional instances are needed if other modules need
    // to be hooked.
    if (!sUser32TrackPopupMenuStub) {
        sUser32Intercept.Init("user32.dll");
        sUser32Intercept.AddHook("TrackPopupMenu", reinterpret_cast<intptr_t>(TrackPopupHookProc),
                                 (void**) &sUser32TrackPopupMenuStub);
    }
}

void
PluginInstanceChild::CreateWinlessPopupSurrogate()
{
    // already initialized
    if (mWinlessPopupSurrogateHWND)
        return;

    HWND hwnd = nullptr;
    NPError result;
    if (!CallNPN_GetValue_NPNVnetscapeWindow(&hwnd, &result)) {
        NS_ERROR("CallNPN_GetValue_NPNVnetscapeWindow failed.");
        return;
    }

    mWinlessPopupSurrogateHWND =
        CreateWindowEx(WS_EX_NOPARENTNOTIFY, L"Static", nullptr, WS_CHILD,
                       0, 0, 0, 0, hwnd, 0, GetModuleHandle(nullptr), 0);
    if (!mWinlessPopupSurrogateHWND) {
        NS_ERROR("CreateWindowEx failed for winless placeholder!");
        return;
    }
    return;
}

void
PluginInstanceChild::DestroyWinlessPopupSurrogate()
{
    if (mWinlessPopupSurrogateHWND)
        DestroyWindow(mWinlessPopupSurrogateHWND);
    mWinlessPopupSurrogateHWND = nullptr;
}

int16_t
PluginInstanceChild::WinlessHandleEvent(NPEvent& event)
{
    if (!mPluginIface->event)
        return false;

    // Events that might generate nested event dispatch loops need
    // special handling during delivery.
    int16_t handled;
    
    HWND focusHwnd = nullptr;

    // TrackPopupMenu will fail if the parent window is not associated with
    // our ui thread. So we hook TrackPopupMenu so we can hand in a surrogate
    // parent created in the child process.
    if ((GetQuirks() & PluginModuleChild::QUIRK_WINLESS_TRACKPOPUP_HOOK) && // XXX turn on by default?
          (event.event == WM_RBUTTONDOWN || // flash
           event.event == WM_RBUTTONUP)) {  // silverlight
      sWinlessPopupSurrogateHWND = mWinlessPopupSurrogateHWND;
      
      // A little trick scrounged from chromium's code - set the focus
      // to our surrogate parent so keyboard nav events go to the menu. 
      focusHwnd = SetFocus(mWinlessPopupSurrogateHWND);
    }

    MessageLoop* loop = MessageLoop::current();
    AutoRestore<bool> modalLoop(loop->os_modal_loop());

    handled = mPluginIface->event(&mData, reinterpret_cast<void*>(&event));

    sWinlessPopupSurrogateHWND = nullptr;

    if (IsWindow(focusHwnd)) {
      SetFocus(focusHwnd);
    }

    return handled;
}

/* windowless drawing helpers */

bool
PluginInstanceChild::SharedSurfaceSetWindow(const NPRemoteWindow& aWindow)
{
    // If the surfaceHandle is empty, parent is telling us we can reuse our cached
    // memory surface and hdc. Otherwise, we need to reset, usually due to a
    // expanding plugin port size.
    if (!aWindow.surfaceHandle) {
        if (!mSharedSurfaceDib.IsValid()) {
            return false;
        }
    }
    else {
        // Attach to the new shared surface parent handed us.
        if (NS_FAILED(mSharedSurfaceDib.Attach((SharedDIB::Handle)aWindow.surfaceHandle,
                                               aWindow.width, aWindow.height, false)))
          return false;
        // Free any alpha extraction resources if needed. This will be reset
        // the next time it's used.
        AlphaExtractCacheRelease();
    }
      
    // NPRemoteWindow's origin is the origin of our shared dib.
    mWindow.x      = aWindow.x;
    mWindow.y      = aWindow.y;
    mWindow.width  = aWindow.width;
    mWindow.height = aWindow.height;
    mWindow.type   = aWindow.type;

    mWindow.window = reinterpret_cast<void*>(mSharedSurfaceDib.GetHDC());
    ::SetViewportOrgEx(mSharedSurfaceDib.GetHDC(),
                       -aWindow.x, -aWindow.y, nullptr);

    if (mPluginIface->setwindow)
        mPluginIface->setwindow(&mData, &mWindow);

    return true;
}

void
PluginInstanceChild::SharedSurfaceRelease()
{
    mSharedSurfaceDib.Close();
    AlphaExtractCacheRelease();
}

/* double pass cache buffer - (rarely) used in cases where alpha extraction
 * occurs for windowless plugins. */
 
bool
PluginInstanceChild::AlphaExtractCacheSetup()
{
    AlphaExtractCacheRelease();

    mAlphaExtract.hdc = ::CreateCompatibleDC(nullptr);

    if (!mAlphaExtract.hdc)
        return false;

    BITMAPINFOHEADER bmih;
    memset((void*)&bmih, 0, sizeof(BITMAPINFOHEADER));
    bmih.biSize        = sizeof(BITMAPINFOHEADER);
    bmih.biWidth       = mWindow.width;
    bmih.biHeight      = mWindow.height;
    bmih.biPlanes      = 1;
    bmih.biBitCount    = 32;
    bmih.biCompression = BI_RGB;

    void* ppvBits = nullptr;
    mAlphaExtract.bmp = ::CreateDIBSection(mAlphaExtract.hdc,
                                           (BITMAPINFO*)&bmih,
                                           DIB_RGB_COLORS,
                                           (void**)&ppvBits,
                                           nullptr,
                                           (unsigned long)sizeof(BITMAPINFOHEADER));
    if (!mAlphaExtract.bmp)
      return false;

    DeleteObject(::SelectObject(mAlphaExtract.hdc, mAlphaExtract.bmp));
    return true;
}

void
PluginInstanceChild::AlphaExtractCacheRelease()
{
    if (mAlphaExtract.bmp)
        ::DeleteObject(mAlphaExtract.bmp);

    if (mAlphaExtract.hdc)
        ::DeleteObject(mAlphaExtract.hdc);

    mAlphaExtract.bmp = nullptr;
    mAlphaExtract.hdc = nullptr;
}

void
PluginInstanceChild::UpdatePaintClipRect(RECT* aRect)
{
    if (aRect) {
        // Update the clip rect on our internal hdc
        HRGN clip = ::CreateRectRgnIndirect(aRect);
        ::SelectClipRgn(mSharedSurfaceDib.GetHDC(), clip);
        ::DeleteObject(clip);
    }
}

int16_t
PluginInstanceChild::SharedSurfacePaint(NPEvent& evcopy)
{
    if (!mPluginIface->event)
        return false;

    RECT* pRect = reinterpret_cast<RECT*>(evcopy.lParam);

    switch(mAlphaExtract.doublePass) {
        case RENDER_NATIVE:
            // pass the internal hdc to the plugin
            UpdatePaintClipRect(pRect);
            evcopy.wParam = WPARAM(mSharedSurfaceDib.GetHDC());
            return mPluginIface->event(&mData, reinterpret_cast<void*>(&evcopy));
        break;
        case RENDER_BACK_ONE:
              // Handle a double pass render used in alpha extraction for transparent
              // plugins. (See nsPluginFrame and gfxWindowsNativeDrawing for details.)
              // We render twice, once to the shared dib, and once to a cache which
              // we copy back on a second paint. These paints can't be spread across
              // multiple rpc messages as delays cause animation frame changes.
              if (!mAlphaExtract.bmp && !AlphaExtractCacheSetup()) {
                  mAlphaExtract.doublePass = RENDER_NATIVE;
                  return false;
              }

              // See gfxWindowsNativeDrawing, color order doesn't have to match.
              UpdatePaintClipRect(pRect);
              ::FillRect(mSharedSurfaceDib.GetHDC(), pRect, (HBRUSH)GetStockObject(WHITE_BRUSH));
              evcopy.wParam = WPARAM(mSharedSurfaceDib.GetHDC());
              if (!mPluginIface->event(&mData, reinterpret_cast<void*>(&evcopy))) {
                  mAlphaExtract.doublePass = RENDER_NATIVE;
                  return false;
              }

              // Copy to cache. We render to shared dib so we don't have to call
              // setwindow between calls (flash issue).  
              ::BitBlt(mAlphaExtract.hdc,
                       pRect->left,
                       pRect->top,
                       pRect->right - pRect->left,
                       pRect->bottom - pRect->top,
                       mSharedSurfaceDib.GetHDC(),
                       pRect->left,
                       pRect->top,
                       SRCCOPY);

              ::FillRect(mSharedSurfaceDib.GetHDC(), pRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
              if (!mPluginIface->event(&mData, reinterpret_cast<void*>(&evcopy))) {
                  mAlphaExtract.doublePass = RENDER_NATIVE;
                  return false;
              }
              mAlphaExtract.doublePass = RENDER_BACK_TWO;
              return true;
        break;
        case RENDER_BACK_TWO:
              // copy our cached surface back
              UpdatePaintClipRect(pRect);
              ::BitBlt(mSharedSurfaceDib.GetHDC(),
                       pRect->left,
                       pRect->top,
                       pRect->right - pRect->left,
                       pRect->bottom - pRect->top,
                       mAlphaExtract.hdc,
                       pRect->left,
                       pRect->top,
                       SRCCOPY);
              mAlphaExtract.doublePass = RENDER_NATIVE;
              return true;
        break;
    }
    return false;
}

/* flash msg throttling helpers */

// Flash has the unfortunate habit of flooding dispatch loops with custom
// windowing events they use for timing. We throttle these by dropping the
// delivery priority below any other event, including pending ipc io
// notifications. We do this for both windowed and windowless controls.
// Note flash's windowless msg window can last longer than our instance,
// so we try to unhook when the window is destroyed and in NPP_Destroy.

void
PluginInstanceChild::UnhookWinlessFlashThrottle()
{
  // We may have already unhooked
  if (!mWinlessThrottleOldWndProc)
      return;

  WNDPROC tmpProc = mWinlessThrottleOldWndProc;
  mWinlessThrottleOldWndProc = nullptr;

  NS_ASSERTION(mWinlessHiddenMsgHWND,
               "Missing mWinlessHiddenMsgHWND w/subclass set??");

  // reset the subclass
  SetWindowLongPtr(mWinlessHiddenMsgHWND, GWLP_WNDPROC,
                   reinterpret_cast<LONG_PTR>(tmpProc));

  // Remove our instance prop
  RemoveProp(mWinlessHiddenMsgHWND, kFlashThrottleProperty);
  mWinlessHiddenMsgHWND = nullptr;
}

// static
LRESULT CALLBACK
PluginInstanceChild::WinlessHiddenFlashWndProc(HWND hWnd,
                                               UINT message,
                                               WPARAM wParam,
                                               LPARAM lParam)
{
    PluginInstanceChild* self = reinterpret_cast<PluginInstanceChild*>(
        GetProp(hWnd, kFlashThrottleProperty));
    if (!self) {
        NS_NOTREACHED("Badness!");
        return 0;
    }

    NS_ASSERTION(self->mWinlessThrottleOldWndProc,
                 "Missing subclass procedure!!");

    // Throttle
    if (message == WM_USER+1) {
        self->FlashThrottleMessage(hWnd, message, wParam, lParam, false);
        return 0;
     }

    // Unhook
    if (message == WM_CLOSE || message == WM_NCDESTROY) {
        WNDPROC tmpProc = self->mWinlessThrottleOldWndProc;
        self->UnhookWinlessFlashThrottle();
        LRESULT res = CallWindowProc(tmpProc, hWnd, message, wParam, lParam);
        return res;
    }

    return CallWindowProc(self->mWinlessThrottleOldWndProc,
                          hWnd, message, wParam, lParam);
}

// Enumerate all thread windows looking for flash's hidden message window.
// Once we find it, sub class it so we can throttle user msgs.  
// static
BOOL CALLBACK
PluginInstanceChild::EnumThreadWindowsCallback(HWND hWnd,
                                               LPARAM aParam)
{
    PluginInstanceChild* self = reinterpret_cast<PluginInstanceChild*>(aParam);
    if (!self) {
        NS_NOTREACHED("Enum befuddled!");
        return FALSE;
    }

    wchar_t className[64];
    if (!GetClassNameW(hWnd, className, sizeof(className)/sizeof(char16_t)))
      return TRUE;
    
    if (!wcscmp(className, L"SWFlash_PlaceholderX")) {
        WNDPROC oldWndProc =
            reinterpret_cast<WNDPROC>(GetWindowLongPtr(hWnd, GWLP_WNDPROC));
        // Only set this if we haven't already.
        if (oldWndProc != WinlessHiddenFlashWndProc) {
            if (self->mWinlessThrottleOldWndProc) {
                NS_WARNING("mWinlessThrottleWndProc already set???");
                return FALSE;
            }
            // Subsclass and store self as a property
            self->mWinlessHiddenMsgHWND = hWnd;
            self->mWinlessThrottleOldWndProc =
                reinterpret_cast<WNDPROC>(SetWindowLongPtr(hWnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(WinlessHiddenFlashWndProc)));
            SetProp(hWnd, kFlashThrottleProperty, self);
            NS_ASSERTION(self->mWinlessThrottleOldWndProc,
                         "SetWindowLongPtr failed?!");
        }
        // Return no matter what once we find the right window.
        return FALSE;
    }

    return TRUE;
}


void
PluginInstanceChild::SetupFlashMsgThrottle()
{
    if (mWindow.type == NPWindowTypeDrawable) {
        // Search for the flash hidden message window and subclass it. Only
        // search for flash windows belonging to our ui thread!
        if (mWinlessThrottleOldWndProc)
            return;
        EnumThreadWindows(GetCurrentThreadId(), EnumThreadWindowsCallback,
                          reinterpret_cast<LPARAM>(this));
    }
    else {
        // Already setup through quirks and the subclass.
        return;
    }
}

WNDPROC
PluginInstanceChild::FlashThrottleAsyncMsg::GetProc()
{ 
    if (mInstance) {
        return mWindowed ? mInstance->mPluginWndProc :
                           mInstance->mWinlessThrottleOldWndProc;
    }
    return nullptr;
}
 
void
PluginInstanceChild::FlashThrottleAsyncMsg::Run()
{
    RemoveFromAsyncList();

    // GetProc() checks mInstance, and pulls the procedure from
    // PluginInstanceChild. We don't transport sub-class procedure
    // ptrs around in FlashThrottleAsyncMsg msgs.
    if (!GetProc())
        return;
  
    // deliver the event to flash 
    CallWindowProc(GetProc(), GetWnd(), GetMsg(), GetWParam(), GetLParam());
}

void
PluginInstanceChild::FlashThrottleMessage(HWND aWnd,
                                          UINT aMsg,
                                          WPARAM aWParam,
                                          LPARAM aLParam,
                                          bool isWindowed)
{
    // We reuse ChildAsyncCall so we get the cancelation work
    // that's done in Destroy.
    FlashThrottleAsyncMsg* task = new FlashThrottleAsyncMsg(this,
        aWnd, aMsg, aWParam, aLParam, isWindowed);
    if (!task)
        return; 

    {
        MutexAutoLock lock(mAsyncCallMutex);
        mPendingAsyncCalls.AppendElement(task);
    }
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        task, kFlashWMUSERMessageThrottleDelayMs);
}

#endif // OS_WIN

bool
PluginInstanceChild::AnswerSetPluginFocus()
{
    MOZ_LOG(GetPluginLog(), LogLevel::Debug, ("%s", FULLFUNCTION));

#if defined(OS_WIN)
    // Parent is letting us know the dom set focus to the plugin. Note,
    // focus can change during transit in certain edge cases, for example
    // when a button click brings up a full screen window. Since we send
    // this in response to a WM_SETFOCUS event on our parent, the parent
    // should have focus when we receive this. If not, ignore the call.
    if (::GetFocus() == mPluginWindowHWND ||
        ((GetQuirks() & PluginModuleChild::QUIRK_SILVERLIGHT_FOCUS_CHECK_PARENT) &&
         (::GetFocus() != mPluginParentHWND)))
        return true;
    ::SetFocus(mPluginWindowHWND);
    return true;
#else
    NS_NOTREACHED("PluginInstanceChild::AnswerSetPluginFocus not implemented!");
    return false;
#endif
}

bool
PluginInstanceChild::AnswerUpdateWindow()
{
    MOZ_LOG(GetPluginLog(), LogLevel::Debug, ("%s", FULLFUNCTION));

#if defined(OS_WIN)
    if (mPluginWindowHWND) {
        RECT rect;
        if (GetUpdateRect(GetParent(mPluginWindowHWND), &rect, FALSE)) {
            ::InvalidateRect(mPluginWindowHWND, &rect, FALSE); 
        }
        UpdateWindow(mPluginWindowHWND);
    }
    return true;
#else
    NS_NOTREACHED("PluginInstanceChild::AnswerUpdateWindow not implemented!");
    return false;
#endif
}

bool
PluginInstanceChild::RecvNPP_DidComposite()
{
  if (mPluginIface->didComposite) {
    mPluginIface->didComposite(GetNPP());
  }
  return true;
}

PPluginScriptableObjectChild*
PluginInstanceChild::AllocPPluginScriptableObjectChild()
{
    AssertPluginThread();
    return new PluginScriptableObjectChild(Proxy);
}

bool
PluginInstanceChild::DeallocPPluginScriptableObjectChild(
    PPluginScriptableObjectChild* aObject)
{
    AssertPluginThread();
    delete aObject;
    return true;
}

bool
PluginInstanceChild::RecvPPluginScriptableObjectConstructor(
                                           PPluginScriptableObjectChild* aActor)
{
    AssertPluginThread();

    // This is only called in response to the parent process requesting the
    // creation of an actor. This actor will represent an NPObject that is
    // created by the browser and returned to the plugin.
    PluginScriptableObjectChild* actor =
        static_cast<PluginScriptableObjectChild*>(aActor);
    NS_ASSERTION(!actor->GetObject(false), "Actor already has an object?!");

    actor->InitializeProxy();
    NS_ASSERTION(actor->GetObject(false), "Actor should have an object!");

    return true;
}

bool
PluginInstanceChild::RecvPBrowserStreamConstructor(
    PBrowserStreamChild* aActor,
    const nsCString& url,
    const uint32_t& length,
    const uint32_t& lastmodified,
    PStreamNotifyChild* notifyData,
    const nsCString& headers)
{
    return true;
}

NPError
PluginInstanceChild::DoNPP_NewStream(BrowserStreamChild* actor,
                                     const nsCString& mimeType,
                                     const bool& seekable,
                                     uint16_t* stype)
{
    AssertPluginThread();
    NPError rv = actor->StreamConstructed(mimeType, seekable, stype);
    return rv;
}

bool
PluginInstanceChild::AnswerNPP_NewStream(PBrowserStreamChild* actor,
                                         const nsCString& mimeType,
                                         const bool& seekable,
                                         NPError* rv,
                                         uint16_t* stype)
{
    *rv = DoNPP_NewStream(static_cast<BrowserStreamChild*>(actor), mimeType,
                          seekable, stype);
    return true;
}

class NewStreamAsyncCall : public ChildAsyncCall
{
public:
    NewStreamAsyncCall(PluginInstanceChild* aInstance,
                       BrowserStreamChild* aBrowserStreamChild,
                       const nsCString& aMimeType,
                       const bool aSeekable)
        : ChildAsyncCall(aInstance, nullptr, nullptr)
        , mBrowserStreamChild(aBrowserStreamChild)
        , mMimeType(aMimeType)
        , mSeekable(aSeekable)
    {
    }

    void Run() override
    {
        RemoveFromAsyncList();

        uint16_t stype = NP_NORMAL;
        NPError rv = mInstance->DoNPP_NewStream(mBrowserStreamChild, mMimeType,
                                                mSeekable, &stype);
        DebugOnly<bool> sendOk =
            mBrowserStreamChild->SendAsyncNPP_NewStreamResult(rv, stype);
        MOZ_ASSERT(sendOk);
    }

private:
    BrowserStreamChild* mBrowserStreamChild;
    const nsCString     mMimeType;
    const bool          mSeekable;
};

bool
PluginInstanceChild::RecvAsyncNPP_NewStream(PBrowserStreamChild* actor,
                                            const nsCString& mimeType,
                                            const bool& seekable)
{
    // Reusing ChildAsyncCall so that the task is cancelled properly on Destroy
    BrowserStreamChild* child = static_cast<BrowserStreamChild*>(actor);
    NewStreamAsyncCall* task = new NewStreamAsyncCall(this, child, mimeType,
                                                      seekable);
    PostChildAsyncCall(task);
    return true;
}

PBrowserStreamChild*
PluginInstanceChild::AllocPBrowserStreamChild(const nsCString& url,
                                              const uint32_t& length,
                                              const uint32_t& lastmodified,
                                              PStreamNotifyChild* notifyData,
                                              const nsCString& headers)
{
    AssertPluginThread();
    return new BrowserStreamChild(this, url, length, lastmodified,
                                  static_cast<StreamNotifyChild*>(notifyData),
                                  headers);
}

bool
PluginInstanceChild::DeallocPBrowserStreamChild(PBrowserStreamChild* stream)
{
    AssertPluginThread();
    delete stream;
    return true;
}

PPluginStreamChild*
PluginInstanceChild::AllocPPluginStreamChild(const nsCString& mimeType,
                                             const nsCString& target,
                                             NPError* result)
{
    NS_RUNTIMEABORT("not callable");
    return nullptr;
}

bool
PluginInstanceChild::DeallocPPluginStreamChild(PPluginStreamChild* stream)
{
    AssertPluginThread();
    delete stream;
    return true;
}

PStreamNotifyChild*
PluginInstanceChild::AllocPStreamNotifyChild(const nsCString& url,
                                             const nsCString& target,
                                             const bool& post,
                                             const nsCString& buffer,
                                             const bool& file,
                                             NPError* result)
{
    AssertPluginThread();
    NS_RUNTIMEABORT("not reached");
    return nullptr;
}

void
StreamNotifyChild::ActorDestroy(ActorDestroyReason why)
{
    if (AncestorDeletion == why && mBrowserStream) {
        NS_ERROR("Pending NPP_URLNotify not called when closing an instance.");

        // reclaim responsibility for deleting ourself
        mBrowserStream->mStreamNotify = nullptr;
        mBrowserStream = nullptr;
    }
}


void
StreamNotifyChild::SetAssociatedStream(BrowserStreamChild* bs)
{
    NS_ASSERTION(!mBrowserStream, "Two streams for one streamnotify?");

    mBrowserStream = bs;
}

bool
StreamNotifyChild::Recv__delete__(const NPReason& reason)
{
    AssertPluginThread();

    if (mBrowserStream)
        mBrowserStream->NotifyPending();
    else
        NPP_URLNotify(reason);

    return true;
}

bool
StreamNotifyChild::RecvRedirectNotify(const nsCString& url, const int32_t& status)
{
    // NPP_URLRedirectNotify requires a non-null closure. Since core logic
    // assumes that all out-of-process notify streams have non-null closure
    // data it will assume that the plugin was notified at this point and
    // expect a response otherwise the redirect will hang indefinitely.
    if (!mClosure) {
        SendRedirectNotifyResponse(false);
    }

    PluginInstanceChild* instance = static_cast<PluginInstanceChild*>(Manager());
    if (instance->mPluginIface->urlredirectnotify)
      instance->mPluginIface->urlredirectnotify(instance->GetNPP(), url.get(), status, mClosure);

    return true;
}

void
StreamNotifyChild::NPP_URLNotify(NPReason reason)
{
    PluginInstanceChild* instance = static_cast<PluginInstanceChild*>(Manager());

    if (mClosure)
        instance->mPluginIface->urlnotify(instance->GetNPP(), mURL.get(),
                                          reason, mClosure);
}

bool
PluginInstanceChild::DeallocPStreamNotifyChild(PStreamNotifyChild* notifyData)
{
    AssertPluginThread();

    if (!static_cast<StreamNotifyChild*>(notifyData)->mBrowserStream)
        delete notifyData;
    return true;
}

PluginScriptableObjectChild*
PluginInstanceChild::GetActorForNPObject(NPObject* aObject)
{
    AssertPluginThread();
    NS_ASSERTION(aObject, "Null pointer!");

    if (aObject->_class == PluginScriptableObjectChild::GetClass()) {
        // One of ours! It's a browser-provided object.
        ChildNPObject* object = static_cast<ChildNPObject*>(aObject);
        NS_ASSERTION(object->parent, "Null actor!");
        return object->parent;
    }

    PluginScriptableObjectChild* actor =
        PluginScriptableObjectChild::GetActorForNPObject(aObject);
    if (actor) {
        // Plugin-provided object that we've previously wrapped.
        return actor;
    }

    actor = new PluginScriptableObjectChild(LocalObject);
    if (!SendPPluginScriptableObjectConstructor(actor)) {
        NS_ERROR("Failed to send constructor message!");
        return nullptr;
    }

    actor->InitializeLocal(aObject);
    return actor;
}

NPError
PluginInstanceChild::NPN_NewStream(NPMIMEType aMIMEType, const char* aWindow,
                                   NPStream** aStream)
{
    AssertPluginThread();

    PluginStreamChild* ps = new PluginStreamChild();

    NPError result;
    CallPPluginStreamConstructor(ps, nsDependentCString(aMIMEType),
                                 NullableString(aWindow), &result);
    if (NPERR_NO_ERROR != result) {
        *aStream = nullptr;
        PPluginStreamChild::Call__delete__(ps, NPERR_GENERIC_ERROR, true);
        return result;
    }

    *aStream = &ps->mStream;
    return NPERR_NO_ERROR;
}

void
PluginInstanceChild::NPN_URLRedirectResponse(void* notifyData, NPBool allow)
{
    if (!notifyData) {
        return;
    }

    InfallibleTArray<PStreamNotifyChild*> notifyStreams;
    ManagedPStreamNotifyChild(notifyStreams);
    uint32_t notifyStreamCount = notifyStreams.Length();
    for (uint32_t i = 0; i < notifyStreamCount; i++) {
        StreamNotifyChild* sn = static_cast<StreamNotifyChild*>(notifyStreams[i]);
        if (sn->mClosure == notifyData) {
            sn->SendRedirectNotifyResponse(static_cast<bool>(allow));
            return;
        }
    }
    NS_ASSERTION(false, "Couldn't find stream for redirect response!");
}

void
PluginInstanceChild::DoAsyncRedraw()
{
    {
        MutexAutoLock autoLock(mAsyncInvalidateMutex);
        mAsyncInvalidateTask = nullptr;
    }

    SendRedrawPlugin();
}

bool
PluginInstanceChild::RecvAsyncSetWindow(const gfxSurfaceType& aSurfaceType,
                                        const NPRemoteWindow& aWindow)
{
    AssertPluginThread();

    NS_ASSERTION(!aWindow.window, "Remote window should be null.");

    if (mCurrentAsyncSetWindowTask) {
        mCurrentAsyncSetWindowTask->Cancel();
        mCurrentAsyncSetWindowTask = nullptr;
    }

    // We shouldn't process this now because it may be received within a nested
    // RPC call, and both Flash and Java don't expect to receive setwindow calls
    // at arbitrary times.
    mCurrentAsyncSetWindowTask =
        NewRunnableMethod<PluginInstanceChild,
                          void (PluginInstanceChild::*)(const gfxSurfaceType&, const NPRemoteWindow&, bool),
                          gfxSurfaceType, NPRemoteWindow, bool>
        (this, &PluginInstanceChild::DoAsyncSetWindow,
         aSurfaceType, aWindow, true);
    MessageLoop::current()->PostTask(FROM_HERE, mCurrentAsyncSetWindowTask);

    return true;
}

void
PluginInstanceChild::DoAsyncSetWindow(const gfxSurfaceType& aSurfaceType,
                                      const NPRemoteWindow& aWindow,
                                      bool aIsAsync)
{
    PLUGIN_LOG_DEBUG(
        ("[InstanceChild][%p] AsyncSetWindow to <x=%d,y=%d, w=%d,h=%d>",
         this, aWindow.x, aWindow.y, aWindow.width, aWindow.height));

    AssertPluginThread();
    NS_ASSERTION(!aWindow.window, "Remote window should be null.");
    NS_ASSERTION(!mPendingPluginCall, "Can't do SetWindow during plugin call!");

    if (aIsAsync) {
        if (!mCurrentAsyncSetWindowTask) {
            return;
        }
        mCurrentAsyncSetWindowTask = nullptr;
    }

    mWindow.window = nullptr;
    if (mWindow.width != aWindow.width || mWindow.height != aWindow.height ||
        mWindow.clipRect.top != aWindow.clipRect.top ||
        mWindow.clipRect.left != aWindow.clipRect.left ||
        mWindow.clipRect.bottom != aWindow.clipRect.bottom ||
        mWindow.clipRect.right != aWindow.clipRect.right)
        mAccumulatedInvalidRect = nsIntRect(0, 0, aWindow.width, aWindow.height);

    mWindow.x = aWindow.x;
    mWindow.y = aWindow.y;
    mWindow.width = aWindow.width;
    mWindow.height = aWindow.height;
    mWindow.clipRect = aWindow.clipRect;
    mWindow.type = aWindow.type;
#ifdef XP_MACOSX
    mContentsScaleFactor = aWindow.contentsScaleFactor;
#endif

    if (GetQuirks() & PluginModuleChild::QUIRK_SILVERLIGHT_DEFAULT_TRANSPARENT)
        mIsTransparent = true;

    mLayersRendering = true;
    mSurfaceType = aSurfaceType;
    UpdateWindowAttributes(true);

#ifdef XP_WIN
    if (GetQuirks() & PluginModuleChild::QUIRK_WINLESS_TRACKPOPUP_HOOK)
        CreateWinlessPopupSurrogate();
    if (GetQuirks() & PluginModuleChild::QUIRK_FLASH_THROTTLE_WMUSER_EVENTS)
        SetupFlashMsgThrottle();
#endif

    if (!mAccumulatedInvalidRect.IsEmpty()) {
        AsyncShowPluginFrame();
    }
}

bool
PluginInstanceChild::CreateOptSurface(void)
{
    MOZ_ASSERT(mSurfaceType != gfxSurfaceType::Max,
               "Need a valid surface type here");
    NS_ASSERTION(!mCurrentSurface, "mCurrentSurfaceActor can get out of sync.");

    nsRefPtr<gfxASurface> retsurf;
    // Use an opaque surface unless we're transparent and *don't* have
    // a background to source from.
    gfxImageFormat format =
        (mIsTransparent && !mBackground) ? gfxImageFormat::ARGB32 :
                                           gfxImageFormat::RGB24;

#ifdef MOZ_X11
    Display* dpy = mWsInfo.display;
    Screen* screen = DefaultScreenOfDisplay(dpy);
    if (format == gfxImageFormat::RGB24 &&
        DefaultDepth(dpy, DefaultScreen(dpy)) == 16) {
        format = gfxImageFormat::RGB16_565;
    }

    if (mSurfaceType == gfxSurfaceType::Xlib) {
        if (!mIsTransparent  || mBackground) {
            Visual* defaultVisual = DefaultVisualOfScreen(screen);
            mCurrentSurface =
                gfxXlibSurface::Create(screen, defaultVisual,
                                       gfxIntSize(mWindow.width,
                                                  mWindow.height));
            return mCurrentSurface != nullptr;
        }

        XRenderPictFormat* xfmt = XRenderFindStandardFormat(dpy, PictStandardARGB32);
        if (!xfmt) {
            NS_ERROR("Need X falback surface, but FindRenderFormat failed");
            return false;
        }
        mCurrentSurface =
            gfxXlibSurface::Create(screen, xfmt,
                                   gfxIntSize(mWindow.width,
                                              mWindow.height));
        return mCurrentSurface != nullptr;
    }
#endif

#ifdef XP_WIN
    if (mSurfaceType == gfxSurfaceType::Win32) {
        bool willHaveTransparentPixels = mIsTransparent && !mBackground;

        SharedDIBSurface* s = new SharedDIBSurface();
        if (!s->Create(reinterpret_cast<HDC>(mWindow.window),
                       mWindow.width, mWindow.height,
                       willHaveTransparentPixels))
            return false;

        mCurrentSurface = s;
        return true;
    }

    NS_RUNTIMEABORT("Shared-memory drawing not expected on Windows.");
#endif

    // Make common shmem implementation working for any platform
    mCurrentSurface =
        gfxSharedImageSurface::CreateUnsafe(this, gfxIntSize(mWindow.width, mWindow.height), format);
    return !!mCurrentSurface;
}

bool
PluginInstanceChild::MaybeCreatePlatformHelperSurface(void)
{
    if (!mCurrentSurface) {
        NS_ERROR("Cannot create helper surface without mCurrentSurface");
        return false;
    }

#ifdef MOZ_X11
    bool supportNonDefaultVisual = false;
    Screen* screen = DefaultScreenOfDisplay(mWsInfo.display);
    Visual* defaultVisual = DefaultVisualOfScreen(screen);
    Visual* visual = nullptr;
    Colormap colormap = 0;
    mDoAlphaExtraction = false;
    bool createHelperSurface = false;

    if (mCurrentSurface->GetType() == gfxSurfaceType::Xlib) {
        static_cast<gfxXlibSurface*>(mCurrentSurface.get())->
            GetColormapAndVisual(&colormap, &visual);
        // Create helper surface if layer surface visual not same as default
        // and we don't support non-default visual rendering
        if (!visual || (defaultVisual != visual && !supportNonDefaultVisual)) {
            createHelperSurface = true;
            visual = defaultVisual;
            mDoAlphaExtraction = mIsTransparent;
        }
    } else if (mCurrentSurface->GetType() == gfxSurfaceType::Image) {
        // For image layer surface we should always create helper surface
        createHelperSurface = true;
        // Check if we can create helper surface with non-default visual
        visual = gfxXlibSurface::FindVisual(screen,
            static_cast<gfxImageSurface*>(mCurrentSurface.get())->Format());
        if (!visual || (defaultVisual != visual && !supportNonDefaultVisual)) {
            visual = defaultVisual;
            mDoAlphaExtraction = mIsTransparent;
        }
    }

    if (createHelperSurface) {
        if (!visual) {
            NS_ERROR("Need X falback surface, but visual failed");
            return false;
        }
        mHelperSurface =
            gfxXlibSurface::Create(screen, visual,
                                   mCurrentSurface->GetSize());
        if (!mHelperSurface) {
            NS_WARNING("Fail to create create helper surface");
            return false;
        }
    }
#elif defined(XP_WIN)
    mDoAlphaExtraction = mIsTransparent && !mBackground;
#endif

    return true;
}

bool
PluginInstanceChild::EnsureCurrentBuffer(void)
{
#ifndef XP_DARWIN
    nsIntRect toInvalidate(0, 0, 0, 0);
    gfxIntSize winSize = gfxIntSize(mWindow.width, mWindow.height);

    if (mBackground && mBackground->GetSize() != winSize) {
        // It would be nice to keep the old background here, but doing
        // so can lead to cases in which we permanently keep the old
        // background size.
        mBackground = nullptr;
        toInvalidate.UnionRect(toInvalidate,
                               nsIntRect(0, 0, winSize.width, winSize.height));
    }

    if (mCurrentSurface) {
        gfxIntSize surfSize = mCurrentSurface->GetSize();
        if (winSize != surfSize ||
            (mBackground && !CanPaintOnBackground()) ||
            (mBackground &&
             gfxContentType::COLOR != mCurrentSurface->GetContentType()) ||
            (!mBackground && mIsTransparent &&
             gfxContentType::COLOR == mCurrentSurface->GetContentType())) {
            // Don't try to use an old, invalid DC.
            mWindow.window = nullptr;
            ClearCurrentSurface();
            toInvalidate.UnionRect(toInvalidate,
                                   nsIntRect(0, 0, winSize.width, winSize.height));
        }
    }

    mAccumulatedInvalidRect.UnionRect(mAccumulatedInvalidRect, toInvalidate);

    if (mCurrentSurface) {
        return true;
    }

    if (!CreateOptSurface()) {
        NS_ERROR("Cannot create optimized surface");
        return false;
    }

    if (!MaybeCreatePlatformHelperSurface()) {
        NS_ERROR("Cannot create helper surface");
        return false;
    }

    return true;
#elif defined(XP_MACOSX)

    if (!mDoubleBufferCARenderer.HasCALayer()) {
        void *caLayer = nullptr;
        if (mDrawingModel == NPDrawingModelCoreGraphics) {
            if (!mCGLayer) {
                bool avoidCGCrashes = !nsCocoaFeatures::OnMountainLionOrLater() &&
                  (GetQuirks() & PluginModuleChild::QUIRK_FLASH_AVOID_CGMODE_CRASHES);
                caLayer = mozilla::plugins::PluginUtilsOSX::GetCGLayer(CallCGDraw, this,
                                                                       avoidCGCrashes,
                                                                       mContentsScaleFactor);

                if (!caLayer) {
                    PLUGIN_LOG_DEBUG(("GetCGLayer failed."));
                    return false;
                }
            }
            mCGLayer = caLayer;
        } else {
            NPError result = mPluginIface->getvalue(GetNPP(),
                                     NPPVpluginCoreAnimationLayer,
                                     &caLayer);
            if (result != NPERR_NO_ERROR || !caLayer) {
                PLUGIN_LOG_DEBUG(("Plugin requested CoreAnimation but did not "
                                  "provide CALayer."));
                return false;
            }
        }
        mDoubleBufferCARenderer.SetCALayer(caLayer);
    }

    if (mDoubleBufferCARenderer.HasFrontSurface() &&
        (mDoubleBufferCARenderer.GetFrontSurfaceWidth() != mWindow.width ||
         mDoubleBufferCARenderer.GetFrontSurfaceHeight() != mWindow.height ||
         mDoubleBufferCARenderer.GetContentsScaleFactor() != mContentsScaleFactor)) {
        mDoubleBufferCARenderer.ClearFrontSurface();
    }

    if (!mDoubleBufferCARenderer.HasFrontSurface()) {
        bool allocSurface = mDoubleBufferCARenderer.InitFrontSurface(
                                mWindow.width, mWindow.height, mContentsScaleFactor,
                                GetQuirks() & PluginModuleChild::QUIRK_ALLOW_OFFLINE_RENDERER ?
                                ALLOW_OFFLINE_RENDERER : DISALLOW_OFFLINE_RENDERER);
        if (!allocSurface) {
            PLUGIN_LOG_DEBUG(("Fail to allocate front IOSurface"));
            return false;
        }

        if (mPluginIface->setwindow)
            (void) mPluginIface->setwindow(&mData, &mWindow);

        nsIntRect toInvalidate(0, 0, mWindow.width, mWindow.height);
        mAccumulatedInvalidRect.UnionRect(mAccumulatedInvalidRect, toInvalidate);
    }
#endif
    return true;
}

void
PluginInstanceChild::UpdateWindowAttributes(bool aForceSetWindow)
{
    nsRefPtr<gfxASurface> curSurface = mHelperSurface ? mHelperSurface : mCurrentSurface;
    bool needWindowUpdate = aForceSetWindow;
#ifdef MOZ_X11
    Visual* visual = nullptr;
    Colormap colormap = 0;
    if (curSurface && curSurface->GetType() == gfxSurfaceType::Xlib) {
        static_cast<gfxXlibSurface*>(curSurface.get())->
            GetColormapAndVisual(&colormap, &visual);
        if (visual != mWsInfo.visual || colormap != mWsInfo.colormap) {
            mWsInfo.visual = visual;
            mWsInfo.colormap = colormap;
            needWindowUpdate = true;
        }
    }
#endif // MOZ_X11
#ifdef XP_WIN
    HDC dc = nullptr;

    if (curSurface) {
        if (!SharedDIBSurface::IsSharedDIBSurface(curSurface))
            NS_RUNTIMEABORT("Expected SharedDIBSurface!");

        SharedDIBSurface* dibsurf = static_cast<SharedDIBSurface*>(curSurface.get());
        dc = dibsurf->GetHDC();
    }
    if (mWindow.window != dc) {
        mWindow.window = dc;
        needWindowUpdate = true;
    }
#endif // XP_WIN

    if (!needWindowUpdate) {
        return;
    }

#ifndef XP_MACOSX
    // Adjusting the window isn't needed for OSX
#ifndef XP_WIN
    // On Windows, we translate the device context, in order for the window
    // origin to be correct.
    mWindow.x = mWindow.y = 0;
#endif

    if (IsVisible()) {
        // The clip rect is relative to drawable top-left.
        nsIntRect clipRect;

        // Don't ask the plugin to draw outside the drawable. The clip rect
        // is in plugin coordinates, not window coordinates.
        // This also ensures that the unsigned clip rectangle offsets won't be -ve.
        clipRect.SetRect(0, 0, mWindow.width, mWindow.height);

        mWindow.clipRect.left = 0;
        mWindow.clipRect.top = 0;
        mWindow.clipRect.right = clipRect.XMost();
        mWindow.clipRect.bottom = clipRect.YMost();
    }
#endif // XP_MACOSX

#ifdef XP_WIN
    // Windowless plugins on Windows need a WM_WINDOWPOSCHANGED event to update
    // their location... or at least Flash does: Silverlight uses the
    // window.x/y passed to NPP_SetWindow

    if (mPluginIface->event) {
        WINDOWPOS winpos = {
            0, 0,
            mWindow.x, mWindow.y,
            mWindow.width, mWindow.height,
            0
        };
        NPEvent pluginEvent = {
            WM_WINDOWPOSCHANGED, 0,
            (LPARAM) &winpos
        };
        mPluginIface->event(&mData, &pluginEvent);
    }
#endif

    PLUGIN_LOG_DEBUG(
        ("[InstanceChild][%p] UpdateWindow w=<x=%d,y=%d, w=%d,h=%d>, clip=<l=%d,t=%d,r=%d,b=%d>",
         this, mWindow.x, mWindow.y, mWindow.width, mWindow.height,
         mWindow.clipRect.left, mWindow.clipRect.top, mWindow.clipRect.right, mWindow.clipRect.bottom));

    if (mPluginIface->setwindow) {
        mPluginIface->setwindow(&mData, &mWindow);
    }
}

void
PluginInstanceChild::PaintRectToPlatformSurface(const nsIntRect& aRect,
                                                gfxASurface* aSurface)
{
    UpdateWindowAttributes();

#ifdef MOZ_X11
    {
        NS_ASSERTION(aSurface->GetType() == gfxSurfaceType::Xlib,
                     "Non supported platform surface type");

        NPEvent pluginEvent;
        XGraphicsExposeEvent& exposeEvent = pluginEvent.xgraphicsexpose;
        exposeEvent.type = GraphicsExpose;
        exposeEvent.display = mWsInfo.display;
        exposeEvent.drawable = static_cast<gfxXlibSurface*>(aSurface)->XDrawable();
        exposeEvent.x = aRect.x;
        exposeEvent.y = aRect.y;
        exposeEvent.width = aRect.width;
        exposeEvent.height = aRect.height;
        exposeEvent.count = 0;
        // information not set:
        exposeEvent.serial = 0;
        exposeEvent.send_event = False;
        exposeEvent.major_code = 0;
        exposeEvent.minor_code = 0;
        mPluginIface->event(&mData, reinterpret_cast<void*>(&exposeEvent));
    }
#elif defined(XP_WIN)
    NS_ASSERTION(SharedDIBSurface::IsSharedDIBSurface(aSurface),
                 "Expected (SharedDIB) image surface.");

    // This rect is in the window coordinate space. aRect is in the plugin
    // coordinate space.
    RECT rect = {
        mWindow.x + aRect.x,
        mWindow.y + aRect.y,
        mWindow.x + aRect.XMost(),
        mWindow.y + aRect.YMost()
    };
    NPEvent paintEvent = {
        WM_PAINT,
        uintptr_t(mWindow.window),
        uintptr_t(&rect)
    };

    ::SetViewportOrgEx((HDC) mWindow.window, -mWindow.x, -mWindow.y, nullptr);
    ::SelectClipRgn((HDC) mWindow.window, nullptr);
    ::IntersectClipRect((HDC) mWindow.window, rect.left, rect.top, rect.right, rect.bottom);
    mPluginIface->event(&mData, reinterpret_cast<void*>(&paintEvent));
#else
    NS_RUNTIMEABORT("Surface type not implemented.");
#endif
}

void
PluginInstanceChild::PaintRectToSurface(const nsIntRect& aRect,
                                        gfxASurface* aSurface,
                                        const gfxRGBA& aColor)
{
    // Render using temporary X surface, with copy to image surface
    nsIntRect plPaintRect(aRect);
    nsRefPtr<gfxASurface> renderSurface = aSurface;
#ifdef MOZ_X11
    if (mIsTransparent && (GetQuirks() & PluginModuleChild::QUIRK_FLASH_EXPOSE_COORD_TRANSLATION)) {
        // Work around a bug in Flash up to 10.1 d51 at least, where expose event
        // top left coordinates within the plugin-rect and not at the drawable
        // origin are misinterpreted.  (We can move the top left coordinate
        // provided it is within the clipRect.), see bug 574583
        plPaintRect.SetRect(0, 0, aRect.XMost(), aRect.YMost());
    }
    if (mHelperSurface) {
        // On X11 we can paint to non Xlib surface only with HelperSurface
        renderSurface = mHelperSurface;
    }
#endif

    if (mIsTransparent && !CanPaintOnBackground()) {
        RefPtr<DrawTarget> dt = CreateDrawTargetForSurface(renderSurface);
        gfx::Rect rect(plPaintRect.x, plPaintRect.y,
                       plPaintRect.width, plPaintRect.height);
        // Moz2D treats OP_SOURCE operations as unbounded, so we need to
        // clip to the rect that we want to fill:
        dt->PushClipRect(rect);
        dt->FillRect(rect, ColorPattern(ToColor(aColor)), // aColor is already a device color
                     DrawOptions(1.f, CompositionOp::OP_SOURCE));
        dt->PopClip();
        dt->Flush();
    }

    PaintRectToPlatformSurface(plPaintRect, renderSurface);

    if (renderSurface != aSurface) {
        // Copy helper surface content to target
        RefPtr<DrawTarget> dt = CreateDrawTargetForSurface(aSurface);
        RefPtr<SourceSurface> surface =
            gfxPlatform::GetSourceSurfaceForSurface(dt, renderSurface);
        dt->CopySurface(surface, aRect, aRect.TopLeft());
    }
}

void
PluginInstanceChild::PaintRectWithAlphaExtraction(const nsIntRect& aRect,
                                                  gfxASurface* aSurface)
{
    MOZ_ASSERT(aSurface->GetContentType() == gfxContentType::COLOR_ALPHA,
               "Refusing to pointlessly recover alpha");

    nsIntRect rect(aRect);
    // If |aSurface| can be used to paint and can have alpha values
    // recovered directly to it, do that to save a tmp surface and
    // copy.
    bool useSurfaceSubimageForBlack = false;
    if (gfxSurfaceType::Image == aSurface->GetType()) {
        gfxImageSurface* surfaceAsImage =
            static_cast<gfxImageSurface*>(aSurface);
        useSurfaceSubimageForBlack =
            (surfaceAsImage->Format() == gfxImageFormat::ARGB32);
        // If we're going to use a subimage, nudge the rect so that we
        // can use optimal alpha recovery.  If we're not using a
        // subimage, the temporaries should automatically get
        // fast-path alpha recovery so we don't need to do anything.
        if (useSurfaceSubimageForBlack) {
            rect =
                gfxAlphaRecovery::AlignRectForSubimageRecovery(aRect,
                                                               surfaceAsImage);
        }
    }

    nsRefPtr<gfxImageSurface> whiteImage;
    nsRefPtr<gfxImageSurface> blackImage;
    gfxRect targetRect(rect.x, rect.y, rect.width, rect.height);
    gfxIntSize targetSize(rect.width, rect.height);
    gfxPoint deviceOffset = -targetRect.TopLeft();

    // We always use a temporary "white image"
    whiteImage = new gfxImageSurface(targetSize, gfxImageFormat::RGB24);
    if (whiteImage->CairoStatus()) {
        return;
    }

#ifdef XP_WIN
    // On windows, we need an HDC and so can't paint directly to
    // vanilla image surfaces.  Bifurcate this painting code so that
    // we don't accidentally attempt that.
    if (!SharedDIBSurface::IsSharedDIBSurface(aSurface))
        NS_RUNTIMEABORT("Expected SharedDIBSurface!");

    // Paint the plugin directly onto the target, with a white
    // background and copy the result
    PaintRectToSurface(rect, aSurface, gfxRGBA(1.0, 1.0, 1.0));
    {
        RefPtr<DrawTarget> dt = CreateDrawTargetForSurface(whiteImage);
        RefPtr<SourceSurface> surface =
            gfxPlatform::GetSourceSurfaceForSurface(dt, aSurface);
        dt->CopySurface(surface, rect, IntPoint());
    }

    // Paint the plugin directly onto the target, with a black
    // background
    PaintRectToSurface(rect, aSurface, gfxRGBA(0.0, 0.0, 0.0));

    // Don't copy the result, just extract a subimage so that we can
    // recover alpha directly into the target
    gfxImageSurface *image = static_cast<gfxImageSurface*>(aSurface);
    blackImage = image->GetSubimage(targetRect);

#else
    // Paint onto white background
    whiteImage->SetDeviceOffset(deviceOffset);
    PaintRectToSurface(rect, whiteImage, gfxRGBA(1.0, 1.0, 1.0));

    if (useSurfaceSubimageForBlack) {
        gfxImageSurface *surface = static_cast<gfxImageSurface*>(aSurface);
        blackImage = surface->GetSubimage(targetRect);
    } else {
        blackImage = new gfxImageSurface(targetSize,
                                         gfxImageFormat::ARGB32);
    }

    // Paint onto black background
    blackImage->SetDeviceOffset(deviceOffset);
    PaintRectToSurface(rect, blackImage, gfxRGBA(0.0, 0.0, 0.0));
#endif

    MOZ_ASSERT(whiteImage && blackImage, "Didn't paint enough!");

    // Extract alpha from black and white image and store to black
    // image
    if (!gfxAlphaRecovery::RecoverAlpha(blackImage, whiteImage)) {
        return;
    }

    // If we had to use a temporary black surface, copy the pixels
    // with alpha back to the target
    if (!useSurfaceSubimageForBlack) {
        RefPtr<DrawTarget> dt = CreateDrawTargetForSurface(aSurface);
        RefPtr<SourceSurface> surface =
            gfxPlatform::GetSourceSurfaceForSurface(dt, blackImage);
        dt->CopySurface(surface,
                        IntRect(0, 0, rect.width, rect.height),
                        rect.TopLeft());
    }
}

bool
PluginInstanceChild::CanPaintOnBackground()
{
    return (mBackground &&
            mCurrentSurface &&
            mCurrentSurface->GetSize() == mBackground->GetSize());
}

bool
PluginInstanceChild::ShowPluginFrame()
{
    // mLayersRendering can be false if we somehow get here without
    // receiving AsyncSetWindow() first.  mPendingPluginCall is our
    // re-entrancy guard; we can't paint while nested inside another
    // paint.
    if (!mLayersRendering || mPendingPluginCall) {
        return false;
    }

    AutoRestore<bool> pending(mPendingPluginCall);
    mPendingPluginCall = true;

    bool temporarilyMakeVisible = !IsVisible() && !mHasPainted;
    if (temporarilyMakeVisible && mWindow.width && mWindow.height) {
        mWindow.clipRect.right = mWindow.width;
        mWindow.clipRect.bottom = mWindow.height;
    } else if (!IsVisible()) {
        // If we're not visible, don't bother painting a <0,0,0,0>
        // rect.  If we're eventually made visible, the visibility
        // change will invalidate our window.
        ClearCurrentSurface();
        return true;
    }

    if (!EnsureCurrentBuffer()) {
        return false;
    }

#ifdef MOZ_WIDGET_COCOA
    // We can't use the thebes code with CoreAnimation so we will
    // take a different code path.
    if (mDrawingModel == NPDrawingModelCoreAnimation ||
        mDrawingModel == NPDrawingModelInvalidatingCoreAnimation ||
        mDrawingModel == NPDrawingModelCoreGraphics) {

        if (!IsVisible()) {
            return true;
        }

        if (!mDoubleBufferCARenderer.HasFrontSurface()) {
            NS_ERROR("CARenderer not initialized for rendering");
            return false;
        }

        // Clear accRect here to be able to pass
        // test_invalidate_during_plugin_paint  test
        nsIntRect rect = mAccumulatedInvalidRect;
        mAccumulatedInvalidRect.SetEmpty();

        // Fix up old invalidations that might have been made when our
        // surface was a different size
        rect.IntersectRect(rect,
                          nsIntRect(0, 0, 
                          mDoubleBufferCARenderer.GetFrontSurfaceWidth(), 
                          mDoubleBufferCARenderer.GetFrontSurfaceHeight()));

        if (mDrawingModel == NPDrawingModelCoreGraphics) {
            mozilla::plugins::PluginUtilsOSX::Repaint(mCGLayer, rect);
        }

        mDoubleBufferCARenderer.Render();

        NPRect r = { (uint16_t)rect.y, (uint16_t)rect.x,
                     (uint16_t)rect.YMost(), (uint16_t)rect.XMost() };
        SurfaceDescriptor currSurf;
        currSurf = IOSurfaceDescriptor(mDoubleBufferCARenderer.GetFrontSurfaceID(),
                                       mDoubleBufferCARenderer.GetContentsScaleFactor());

        mHasPainted = true;

        SurfaceDescriptor returnSurf;

        if (!SendShow(r, currSurf, &returnSurf)) {
            return false;
        }

        SwapSurfaces();
        return true;
    } else {
        NS_ERROR("Unsupported drawing model for async layer rendering");
        return false;
    }
#endif

    NS_ASSERTION(mWindow.width == uint32_t(mWindow.clipRect.right - mWindow.clipRect.left) &&
                 mWindow.height == uint32_t(mWindow.clipRect.bottom - mWindow.clipRect.top),
                 "Clip rect should be same size as window when using layers");

    // Clear accRect here to be able to pass
    // test_invalidate_during_plugin_paint  test
    nsIntRect rect = mAccumulatedInvalidRect;
    mAccumulatedInvalidRect.SetEmpty();

    // Fix up old invalidations that might have been made when our
    // surface was a different size
    gfxIntSize surfaceSize = mCurrentSurface->GetSize();
    rect.IntersectRect(rect,
                       nsIntRect(0, 0, surfaceSize.width, surfaceSize.height));

    if (!ReadbackDifferenceRect(rect)) {
        // We couldn't read back the pixels that differ between the
        // current surface and last, so we have to invalidate the
        // entire window.
        rect.SetRect(0, 0, mWindow.width, mWindow.height);
    }

    bool haveTransparentPixels =
        gfxContentType::COLOR_ALPHA == mCurrentSurface->GetContentType();
    PLUGIN_LOG_DEBUG(
        ("[InstanceChild][%p] Painting%s <x=%d,y=%d, w=%d,h=%d> on surface <w=%d,h=%d>",
         this, haveTransparentPixels ? " with alpha" : "",
         rect.x, rect.y, rect.width, rect.height,
         mCurrentSurface->GetSize().width, mCurrentSurface->GetSize().height));

    if (CanPaintOnBackground()) {
        PLUGIN_LOG_DEBUG(("  (on background)"));
        // Source the background pixels ...
        {
            nsRefPtr<gfxASurface> surface =
                mHelperSurface ? mHelperSurface : mCurrentSurface;
            RefPtr<DrawTarget> dt = CreateDrawTargetForSurface(surface);
            RefPtr<SourceSurface> backgroundSurface =
                gfxPlatform::GetSourceSurfaceForSurface(dt, mBackground);
            dt->CopySurface(backgroundSurface, rect, rect.TopLeft());
        }
        // ... and hand off to the plugin
        // BEWARE: mBackground may die during this call
        PaintRectToSurface(rect, mCurrentSurface, gfxRGBA(0.0, 0.0, 0.0, 0.0));
    } else if (!temporarilyMakeVisible && mDoAlphaExtraction) {
        // We don't want to pay the expense of alpha extraction for
        // phony paints.
        PLUGIN_LOG_DEBUG(("  (with alpha recovery)"));
        PaintRectWithAlphaExtraction(rect, mCurrentSurface);
    } else {
        PLUGIN_LOG_DEBUG(("  (onto opaque surface)"));

        // If we're on a platform that needs helper surfaces for
        // plugins, and we're forcing a throwaway paint of a
        // wmode=transparent plugin, then make sure to use the helper
        // surface here.
        nsRefPtr<gfxASurface> target =
            (temporarilyMakeVisible && mHelperSurface) ?
            mHelperSurface : mCurrentSurface;

        PaintRectToSurface(rect, target, gfxRGBA(0.0, 0.0, 0.0, 0.0));
    }
    mHasPainted = true;

    if (temporarilyMakeVisible) {
        mWindow.clipRect.right = mWindow.clipRect.bottom = 0;

        PLUGIN_LOG_DEBUG(
            ("[InstanceChild][%p] Undoing temporary clipping w=<x=%d,y=%d, w=%d,h=%d>, clip=<l=%d,t=%d,r=%d,b=%d>",
             this, mWindow.x, mWindow.y, mWindow.width, mWindow.height,
             mWindow.clipRect.left, mWindow.clipRect.top, mWindow.clipRect.right, mWindow.clipRect.bottom));

        if (mPluginIface->setwindow) {
            mPluginIface->setwindow(&mData, &mWindow);
        }

        // Skip forwarding the results of the phony paint to the
        // browser.  We may have painted a transparent plugin using
        // the opaque-plugin path, which can result in wrong pixels.
        // We also don't want to pay the expense of forwarding the
        // surface for plugins that might really be invisible.
        mAccumulatedInvalidRect.SetRect(0, 0, mWindow.width, mWindow.height);
        return true;
    }

    NPRect r = { (uint16_t)rect.y, (uint16_t)rect.x,
                 (uint16_t)rect.YMost(), (uint16_t)rect.XMost() };
    SurfaceDescriptor currSurf;
#ifdef MOZ_X11
    if (mCurrentSurface->GetType() == gfxSurfaceType::Xlib) {
        gfxXlibSurface *xsurf = static_cast<gfxXlibSurface*>(mCurrentSurface.get());
        currSurf = SurfaceDescriptorX11(xsurf);
        // Need to sync all pending x-paint requests
        // before giving drawable to another process
        XSync(mWsInfo.display, False);
    } else
#endif
#ifdef XP_WIN
    if (SharedDIBSurface::IsSharedDIBSurface(mCurrentSurface)) {
        SharedDIBSurface* s = static_cast<SharedDIBSurface*>(mCurrentSurface.get());
        if (!mCurrentSurfaceActor) {
            base::SharedMemoryHandle handle = nullptr;
            s->ShareToProcess(OtherPid(), &handle);

            mCurrentSurfaceActor =
                SendPPluginSurfaceConstructor(handle,
                                              mCurrentSurface->GetSize(),
                                              haveTransparentPixels);
        }
        currSurf = mCurrentSurfaceActor;
        s->Flush();
    } else
#endif
    if (gfxSharedImageSurface::IsSharedImage(mCurrentSurface)) {
        currSurf = static_cast<gfxSharedImageSurface*>(mCurrentSurface.get())->GetShmem();
    } else {
        NS_RUNTIMEABORT("Surface type is not remotable");
        return false;
    }

    // Unused, except to possibly return a shmem to us
    SurfaceDescriptor returnSurf;

    if (!SendShow(r, currSurf, &returnSurf)) {
        return false;
    }

    SwapSurfaces();
    mSurfaceDifferenceRect = rect;
    return true;
}

bool
PluginInstanceChild::ReadbackDifferenceRect(const nsIntRect& rect)
{
    if (!mBackSurface)
        return false;

    // We can read safely from XSurface,SharedDIBSurface and Unsafe SharedMemory,
    // because PluginHost is not able to modify that surface
#if defined(MOZ_X11)
    if (mBackSurface->GetType() != gfxSurfaceType::Xlib &&
        !gfxSharedImageSurface::IsSharedImage(mBackSurface))
        return false;
#elif defined(XP_WIN)
    if (!SharedDIBSurface::IsSharedDIBSurface(mBackSurface))
        return false;
#else
    return false;
#endif

    if (mCurrentSurface->GetContentType() != mBackSurface->GetContentType())
        return false;

    if (mSurfaceDifferenceRect.IsEmpty())
        return true;

    PLUGIN_LOG_DEBUG(
        ("[InstanceChild][%p] Reading back part of <x=%d,y=%d, w=%d,h=%d>",
         this, mSurfaceDifferenceRect.x, mSurfaceDifferenceRect.y,
         mSurfaceDifferenceRect.width, mSurfaceDifferenceRect.height));

    // Read back previous content
    RefPtr<DrawTarget> dt = CreateDrawTargetForSurface(mCurrentSurface);
    RefPtr<SourceSurface> source =
        gfxPlatform::GetSourceSurfaceForSurface(dt, mBackSurface);
    // Subtract from mSurfaceDifferenceRect area which is overlapping with rect
    nsIntRegion result;
    result.Sub(mSurfaceDifferenceRect, nsIntRegion(rect));
    nsIntRegionRectIterator iter(result);
    const nsIntRect* r;
    while ((r = iter.Next()) != nullptr) {
        dt->CopySurface(source, *r, r->TopLeft());
    }

    return true;
}

void
PluginInstanceChild::InvalidateRectDelayed(void)
{
    if (!mCurrentInvalidateTask) {
        return;
    }

    mCurrentInvalidateTask = nullptr;
    if (mAccumulatedInvalidRect.IsEmpty()) {
        return;
    }

    if (!ShowPluginFrame()) {
        AsyncShowPluginFrame();
    }
}

void
PluginInstanceChild::AsyncShowPluginFrame(void)
{
    if (mCurrentInvalidateTask) {
        return;
    }

    mCurrentInvalidateTask =
        NewRunnableMethod(this, &PluginInstanceChild::InvalidateRectDelayed);
    MessageLoop::current()->PostTask(FROM_HERE, mCurrentInvalidateTask);
}

void
PluginInstanceChild::InvalidateRect(NPRect* aInvalidRect)
{
    NS_ASSERTION(aInvalidRect, "Null pointer!");

#ifdef OS_WIN
    // Invalidate and draw locally for windowed plugins.
    if (mWindow.type == NPWindowTypeWindow) {
      NS_ASSERTION(IsWindow(mPluginWindowHWND), "Bad window?!");
      RECT rect = { aInvalidRect->left, aInvalidRect->top,
                    aInvalidRect->right, aInvalidRect->bottom };
      ::InvalidateRect(mPluginWindowHWND, &rect, FALSE);
      return;
    }
#endif

    if (mLayersRendering) {
        nsIntRect r(aInvalidRect->left, aInvalidRect->top,
                    aInvalidRect->right - aInvalidRect->left,
                    aInvalidRect->bottom - aInvalidRect->top);

        mAccumulatedInvalidRect.UnionRect(r, mAccumulatedInvalidRect);
        // If we are able to paint and invalidate sent, then reset
        // accumulated rectangle
        AsyncShowPluginFrame();
        return;
    }

    // If we were going to use layers rendering but it's not set up
    // yet, and the plugin happens to call this first, we'll forward
    // the invalidation to the browser.  It's unclear whether
    // non-layers plugins need this rect forwarded when their window
    // width or height is 0, which it would be for layers plugins
    // before their first SetWindow().
    SendNPN_InvalidateRect(*aInvalidRect);
}

bool
PluginInstanceChild::RecvUpdateBackground(const SurfaceDescriptor& aBackground,
                                          const nsIntRect& aRect)
{
    MOZ_ASSERT(mIsTransparent, "Only transparent plugins use backgrounds");

    if (!mBackground) {
        // XXX refactor me
        switch (aBackground.type()) {
#ifdef MOZ_X11
        case SurfaceDescriptor::TSurfaceDescriptorX11: {
            mBackground = aBackground.get_SurfaceDescriptorX11().OpenForeign();
            break;
        }
#endif
        case SurfaceDescriptor::TShmem: {
            mBackground = gfxSharedImageSurface::Open(aBackground.get_Shmem());
            break;
        }
        default:
            NS_RUNTIMEABORT("Unexpected background surface descriptor");
        }

        if (!mBackground) {
            return false;
        }

        gfxIntSize bgSize = mBackground->GetSize();
        mAccumulatedInvalidRect.UnionRect(mAccumulatedInvalidRect,
                                          nsIntRect(0, 0, bgSize.width, bgSize.height));
        AsyncShowPluginFrame();
        return true;
    }

    // XXX refactor me
    mAccumulatedInvalidRect.UnionRect(aRect, mAccumulatedInvalidRect);

    // This must be asynchronous, because we may be nested within RPC messages
    // which do not expect to receiving paint events.
    AsyncShowPluginFrame();

    return true;
}

PPluginBackgroundDestroyerChild*
PluginInstanceChild::AllocPPluginBackgroundDestroyerChild()
{
    return new PluginBackgroundDestroyerChild();
}

bool
PluginInstanceChild::RecvPPluginBackgroundDestroyerConstructor(
    PPluginBackgroundDestroyerChild* aActor)
{
    // Our background changed, so we have to invalidate the area
    // painted with the old background.  If the background was
    // destroyed because we have a new background, then we expect to
    // be notified of that "soon", before processing the asynchronous
    // invalidation here.  If we're *not* getting a new background,
    // our current front surface is stale and we want to repaint
    // "soon" so that we can hand the browser back a surface with
    // alpha values.  (We should be notified of that invalidation soon
    // too, but we don't assume that here.)
    if (mBackground) {
        gfxIntSize bgsize = mBackground->GetSize();
        mAccumulatedInvalidRect.UnionRect(
            nsIntRect(0, 0, bgsize.width, bgsize.height), mAccumulatedInvalidRect);

        // NB: we don't have to XSync here because only ShowPluginFrame()
        // uses mBackground, and it always XSyncs after finishing.
        mBackground = nullptr;
        AsyncShowPluginFrame();
    }

    return PPluginBackgroundDestroyerChild::Send__delete__(aActor);
}

bool
PluginInstanceChild::DeallocPPluginBackgroundDestroyerChild(
    PPluginBackgroundDestroyerChild* aActor)
{
    delete aActor;
    return true;
}

uint32_t
PluginInstanceChild::ScheduleTimer(uint32_t interval, bool repeat,
                                   TimerFunc func)
{
    ChildTimer* t = new ChildTimer(this, interval, repeat, func);
    if (0 == t->ID()) {
        delete t;
        return 0;
    }

    mTimers.AppendElement(t);
    return t->ID();
}

void
PluginInstanceChild::UnscheduleTimer(uint32_t id)
{
    if (0 == id)
        return;

    mTimers.RemoveElement(id, ChildTimer::IDComparator());
}

void
PluginInstanceChild::AsyncCall(PluginThreadCallback aFunc, void* aUserData)
{
    ChildAsyncCall* task = new ChildAsyncCall(this, aFunc, aUserData);
    PostChildAsyncCall(task);
}

void
PluginInstanceChild::PostChildAsyncCall(ChildAsyncCall* aTask)
{
    {
        MutexAutoLock lock(mAsyncCallMutex);
        mPendingAsyncCalls.AppendElement(aTask);
    }
    ProcessChild::message_loop()->PostTask(FROM_HERE, aTask);
}

void
PluginInstanceChild::SwapSurfaces()
{
    nsRefPtr<gfxASurface> tmpsurf = mCurrentSurface;
#ifdef XP_WIN
    PPluginSurfaceChild* tmpactor = mCurrentSurfaceActor;
#endif

    mCurrentSurface = mBackSurface;
#ifdef XP_WIN
    mCurrentSurfaceActor = mBackSurfaceActor;
#endif

    mBackSurface = tmpsurf;
#ifdef XP_WIN
    mBackSurfaceActor = tmpactor;
#endif

#ifdef MOZ_WIDGET_COCOA
    mDoubleBufferCARenderer.SwapSurfaces();

    // Outdated back surface... not usable anymore due to changed plugin size.
    // Dropping obsolete surface
    if (mDoubleBufferCARenderer.HasFrontSurface() && 
        mDoubleBufferCARenderer.HasBackSurface() &&
        (mDoubleBufferCARenderer.GetFrontSurfaceWidth() != 
            mDoubleBufferCARenderer.GetBackSurfaceWidth() ||
        mDoubleBufferCARenderer.GetFrontSurfaceHeight() != 
            mDoubleBufferCARenderer.GetBackSurfaceHeight() ||
        mDoubleBufferCARenderer.GetFrontSurfaceContentsScaleFactor() != 
            mDoubleBufferCARenderer.GetBackSurfaceContentsScaleFactor())) {

        mDoubleBufferCARenderer.ClearFrontSurface();
    }
#else
    if (mCurrentSurface && mBackSurface &&
        (mCurrentSurface->GetSize() != mBackSurface->GetSize() ||
         mCurrentSurface->GetContentType() != mBackSurface->GetContentType())) {
        ClearCurrentSurface();
    }
#endif
}

void
PluginInstanceChild::ClearCurrentSurface()
{
    mCurrentSurface = nullptr;
#ifdef MOZ_WIDGET_COCOA
    if (mDoubleBufferCARenderer.HasFrontSurface()) {
        mDoubleBufferCARenderer.ClearFrontSurface();
    }
#endif
#ifdef XP_WIN
    if (mCurrentSurfaceActor) {
        PPluginSurfaceChild::Send__delete__(mCurrentSurfaceActor);
        mCurrentSurfaceActor = nullptr;
    }
#endif
    mHelperSurface = nullptr;
}

void
PluginInstanceChild::ClearAllSurfaces()
{
    if (mBackSurface) {
        // Get last surface back, and drop it
        SurfaceDescriptor temp = null_t();
        NPRect r = { 0, 0, 1, 1 };
        SendShow(r, temp, &temp);
    }

    if (gfxSharedImageSurface::IsSharedImage(mCurrentSurface))
        DeallocShmem(static_cast<gfxSharedImageSurface*>(mCurrentSurface.get())->GetShmem());
    if (gfxSharedImageSurface::IsSharedImage(mBackSurface))
        DeallocShmem(static_cast<gfxSharedImageSurface*>(mBackSurface.get())->GetShmem());
    mCurrentSurface = nullptr;
    mBackSurface = nullptr;

#ifdef XP_WIN
    if (mCurrentSurfaceActor) {
        PPluginSurfaceChild::Send__delete__(mCurrentSurfaceActor);
        mCurrentSurfaceActor = nullptr;
    }
    if (mBackSurfaceActor) {
        PPluginSurfaceChild::Send__delete__(mBackSurfaceActor);
        mBackSurfaceActor = nullptr;
    }
#endif

#ifdef MOZ_WIDGET_COCOA
    if (mDoubleBufferCARenderer.HasBackSurface()) {
        // Get last surface back, and drop it
        SurfaceDescriptor temp = null_t();
        NPRect r = { 0, 0, 1, 1 };
        SendShow(r, temp, &temp);
    }

    if (mCGLayer) {
        mozilla::plugins::PluginUtilsOSX::ReleaseCGLayer(mCGLayer);
        mCGLayer = nullptr;
    }

    mDoubleBufferCARenderer.ClearFrontSurface();
    mDoubleBufferCARenderer.ClearBackSurface();
#endif
}

static void
InvalidateObjects(nsTHashtable<DeletingObjectEntry>& aEntries)
{
    for (auto iter = aEntries.Iter(); !iter.Done(); iter.Next()) {
        DeletingObjectEntry* e = iter.Get();
        NPObject* o = e->GetKey();
        if (!e->mDeleted && o->_class && o->_class->invalidate) {
            o->_class->invalidate(o);
        }
    }
}

static void
DeleteObjects(nsTHashtable<DeletingObjectEntry>& aEntries)
{
    for (auto iter = aEntries.Iter(); !iter.Done(); iter.Next()) {
        DeletingObjectEntry* e = iter.Get();
        NPObject* o = e->GetKey();
        if (!e->mDeleted) {
            e->mDeleted = true;

#ifdef NS_BUILD_REFCNT_LOGGING
            {
                int32_t refcnt = o->referenceCount;
                while (refcnt) {
                    --refcnt;
                    NS_LOG_RELEASE(o, refcnt, "NPObject");
                }
            }
#endif

            PluginModuleChild::DeallocNPObject(o);
        }
    }
}

void
PluginInstanceChild::Destroy()
{
    if (mDestroyed) {
        return;
    }
    mDestroyed = true;

#if defined(OS_WIN)
    SetProp(mPluginWindowHWND, kPluginIgnoreSubclassProperty, (HANDLE)1);
#endif

    InfallibleTArray<PBrowserStreamChild*> streams;
    ManagedPBrowserStreamChild(streams);

    // First make sure none of these streams become deleted
    for (uint32_t i = 0; i < streams.Length(); ) {
        if (static_cast<BrowserStreamChild*>(streams[i])->InstanceDying())
            ++i;
        else
            streams.RemoveElementAt(i);
    }
    for (uint32_t i = 0; i < streams.Length(); ++i)
        static_cast<BrowserStreamChild*>(streams[i])->FinishDelivery();

    mTimers.Clear();

    // NPP_Destroy() should be a synchronization point for plugin threads
    // calling NPN_AsyncCall: after this function returns, they are no longer
    // allowed to make async calls on this instance.
    static_cast<PluginModuleChild *>(Manager())->NPP_Destroy(this);
    mData.ndata = 0;

    if (mCurrentInvalidateTask) {
        mCurrentInvalidateTask->Cancel();
        mCurrentInvalidateTask = nullptr;
    }
    if (mCurrentAsyncSetWindowTask) {
        mCurrentAsyncSetWindowTask->Cancel();
        mCurrentAsyncSetWindowTask = nullptr;
    }
    {
        MutexAutoLock autoLock(mAsyncInvalidateMutex);
        if (mAsyncInvalidateTask) {
            mAsyncInvalidateTask->Cancel();
            mAsyncInvalidateTask = nullptr;
        }
    }

    ClearAllSurfaces();

    mDeletingHash = new nsTHashtable<DeletingObjectEntry>;
    PluginScriptableObjectChild::NotifyOfInstanceShutdown(this);

    InvalidateObjects(*mDeletingHash);
    DeleteObjects(*mDeletingHash);

    // Null out our cached actors as they should have been killed in the
    // PluginInstanceDestroyed call above.
    mCachedWindowActor = nullptr;
    mCachedElementActor = nullptr;

#if defined(OS_WIN)
    SharedSurfaceRelease();
    DestroyWinlessPopupSurrogate();
    UnhookWinlessFlashThrottle();
    DestroyPluginWindow();
#endif

    // Pending async calls are discarded, not delivered. This matches the
    // in-process behavior.
    for (uint32_t i = 0; i < mPendingAsyncCalls.Length(); ++i)
        mPendingAsyncCalls[i]->Cancel();

    mPendingAsyncCalls.Clear();
    
#ifdef MOZ_WIDGET_GTK
    if (mWindow.type == NPWindowTypeWindow && !mXEmbed) {
      xt_client_xloop_destroy();
    }
#endif
#if defined(MOZ_X11) && defined(XP_UNIX) && !defined(XP_MACOSX)
    DeleteWindow();
#endif
}

bool
PluginInstanceChild::AnswerNPP_Destroy(NPError* aResult)
{
    PLUGIN_LOG_DEBUG_METHOD;
    AssertPluginThread();
    *aResult = NPERR_NO_ERROR;

    Destroy();

    return true;
}

void
PluginInstanceChild::ActorDestroy(ActorDestroyReason why)
{
    Destroy();
}
