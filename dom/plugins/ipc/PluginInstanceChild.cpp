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
 *   Chris Jones <jones.chris.g@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jim Mathies <jmathies@mozilla.com>
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

#include "PluginBackgroundDestroyer.h"
#include "PluginInstanceChild.h"
#include "PluginModuleChild.h"
#include "BrowserStreamChild.h"
#include "PluginStreamChild.h"
#include "StreamNotifyChild.h"
#include "PluginProcessChild.h"
#include "gfxASurface.h"
#include "gfxContext.h"
#ifdef MOZ_X11
#include "gfxXlibSurface.h"
#endif
#ifdef XP_WIN
#include "mozilla/gfx/SharedDIBSurface.h"
#include "nsCrashOnException.h"
extern const PRUnichar* kFlashFullscreenClass;
using mozilla::gfx::SharedDIBSurface;
#endif
#include "gfxSharedImageSurface.h"
#include "gfxUtils.h"
#include "gfxAlphaRecovery.h"

#include "mozilla/ipc/SyncChannel.h"
#include "mozilla/AutoRestore.h"

using mozilla::ipc::ProcessChild;
using namespace mozilla::plugins;

#ifdef MOZ_WIDGET_GTK2

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include "gtk2xtbin.h"

#elif defined(MOZ_WIDGET_QT)
#include <QX11Info>
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
static HWND sWinlessPopupSurrogateHWND = NULL;
static User32TrackPopupMenu sUser32TrackPopupMenuStub = NULL;

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
#include "PluginUtilsOSX.h"
#endif // defined(XP_MACOSX)

template<>
struct RunnableMethodTraits<PluginInstanceChild>
{
    static void RetainCallee(PluginInstanceChild* obj) { }
    static void ReleaseCallee(PluginInstanceChild* obj) { }
};

PluginInstanceChild::PluginInstanceChild(const NPPluginFuncs* aPluginIface)
    : mPluginIface(aPluginIface)
    , mCachedWindowActor(nsnull)
    , mCachedElementActor(nsnull)
#if defined(OS_WIN)
    , mPluginWindowHWND(0)
    , mPluginWndProc(0)
    , mPluginParentHWND(0)
    , mCachedWinlessPluginHWND(0)
    , mWinlessPopupSurrogateHWND(0)
    , mWinlessThrottleOldWndProc(0)
    , mWinlessHiddenMsgHWND(0)
#endif // OS_WIN
    , mAsyncCallMutex("PluginInstanceChild::mAsyncCallMutex")
#if defined(MOZ_WIDGET_COCOA)
#if defined(__i386__)
    , mEventModel(NPEventModelCarbon)
#endif
    , mShColorSpace(nsnull)
    , mShContext(nsnull)
    , mDrawingModel(NPDrawingModelCoreGraphics)
    , mCGLayer(nsnull)
    , mCurrentEvent(nsnull)
#endif
    , mLayersRendering(false)
#ifdef XP_WIN
    , mCurrentSurfaceActor(NULL)
    , mBackSurfaceActor(NULL)
#endif
    , mAccumulatedInvalidRect(0,0,0,0)
    , mIsTransparent(false)
    , mSurfaceType(gfxASurface::SurfaceTypeMax)
    , mCurrentInvalidateTask(nsnull)
    , mCurrentAsyncSetWindowTask(nsnull)
    , mPendingPluginCall(false)
    , mDoAlphaExtraction(false)
    , mHasPainted(false)
    , mSurfaceDifferenceRect(0,0,0,0)
#if (MOZ_PLATFORM_MAEMO == 5) || (MOZ_PLATFORM_MAEMO == 6)
    , mMaemoImageRendering(PR_TRUE)
#endif
{
    memset(&mWindow, 0, sizeof(mWindow));
    mData.ndata = (void*) this;
    mData.pdata = nsnull;
#if defined(MOZ_X11) && defined(XP_UNIX) && !defined(XP_MACOSX)
    mWindow.ws_info = &mWsInfo;
    memset(&mWsInfo, 0, sizeof(mWsInfo));
    mWsInfo.display = DefaultXDisplay();
#endif // MOZ_X11 && XP_UNIX && !XP_MACOSX
#if defined(OS_WIN)
    memset(&mAlphaExtract, 0, sizeof(mAlphaExtract));
#endif // OS_WIN
#if defined(OS_WIN)
    InitPopupMenuHook();
#endif // OS_WIN
}

PluginInstanceChild::~PluginInstanceChild()
{
#if defined(OS_WIN)
    NS_ASSERTION(!mPluginWindowHWND, "Destroying PluginInstanceChild without NPP_Destroy?");
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
#endif
}

int
PluginInstanceChild::GetQuirks()
{
    return PluginModuleChild::current()->GetQuirks();
}

NPError
PluginInstanceChild::InternalGetNPObjectForValue(NPNVariable aValue,
                                                 NPObject** aObject)
{
    PluginScriptableObjectChild* actor;
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
        PPluginScriptableObjectChild* currentActor;

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
                NS_NOTREACHED("Don't know what to do with this value type!");
        }

        // Make sure that the current actor returned by the parent matches our
        // cached actor!
        NS_ASSERTION(static_cast<PluginScriptableObjectChild*>(currentActor) ==
                     actor, "Cached actor is out of date!");
        NS_ASSERTION(currentResult == result, "Results don't match?!");
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

    case NPNVSupportsWindowless:
#if defined(OS_LINUX) || defined(MOZ_X11) || defined(OS_WIN)
        *((NPBool*)aValue) = true;
#else
        *((NPBool*)aValue) = false;
#endif
        return NPERR_NO_ERROR;

#if (MOZ_PLATFORM_MAEMO == 5) || (MOZ_PLATFORM_MAEMO == 6)
    case NPNVSupportsWindowlessLocal: {
#ifdef MOZ_WIDGET_QT
        const char *graphicsSystem = PR_GetEnv("MOZ_QT_GRAPHICSSYSTEM");
        // we should set local rendering to false in order to render X-Plugin
        // there is no possibility to change it later on maemo5 platform
        mMaemoImageRendering = (!(graphicsSystem && !strcmp(graphicsSystem, "native")));
#endif
        *((NPBool*)aValue) = mMaemoImageRendering;
        return NPERR_NO_ERROR;
    }
#endif
#if defined(MOZ_X11)
    case NPNVSupportsXEmbedBool:
        *((NPBool*)aValue) = true;
        return NPERR_NO_ERROR;

    case NPNVToolkit:
        *((NPNToolkitType*)aValue) = NPNVGtk2;
        return NPERR_NO_ERROR;

#elif defined(OS_WIN)
    case NPNVToolkit:
        return NPERR_GENERIC_ERROR;
#endif
    case NPNVjavascriptEnabledBool: {
        bool v = false;
        NPError result;
        if (!CallNPN_GetValue_NPNVjavascriptEnabledBool(&v, &result)) {
            return NPERR_GENERIC_ERROR;
        }
        *static_cast<NPBool*>(aValue) = v;
        return result;
    }

    case NPNVisOfflineBool: {
        bool v = false;
        NPError result;
        if (!CallNPN_GetValue_NPNVisOfflineBool(&v, &result)) {
            return NPERR_GENERIC_ERROR;
        }
        *static_cast<NPBool*>(aValue) = v;
        return result;
    }

    case NPNVprivateModeBool: {
        bool v = false;
        NPError result;
        if (!CallNPN_GetValue_NPNVprivateModeBool(&v, &result)) {
            return NPERR_GENERIC_ERROR;
        }
        *static_cast<NPBool*>(aValue) = v;
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
#endif /* XP_MACOSX */

    default:
        PR_LOG(gPluginLog, PR_LOG_WARNING,
               ("In PluginInstanceChild::NPN_GetValue: Unhandled NPNVariable %i (%s)",
                (int) aVar, NPNVariableToString(aVar)));
        return NPERR_GENERIC_ERROR;
    }

}


NPError
PluginInstanceChild::NPN_SetValue(NPPVariable aVar, void* aValue)
{
    PR_LOG(gPluginLog, PR_LOG_DEBUG, ("%s (aVar=%i, aValue=%p)",
                                      FULLFUNCTION, (int) aVar, aValue));

    AssertPluginThread();

    switch (aVar) {
    case NPPVpluginWindowBool: {
        NPError rv;
        bool windowed = (NPBool) (intptr_t) aValue;

        if (!CallNPN_SetValue_NPPVpluginWindow(windowed, &rv))
            return NPERR_GENERIC_ERROR;

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

#ifdef XP_MACOSX
    case NPPVpluginDrawingModel: {
        NPError rv;
        int drawingModel = (int16) (intptr_t) aValue;

        if (!CallNPN_SetValue_NPPVpluginDrawingModel(drawingModel, &rv))
            return NPERR_GENERIC_ERROR;
        mDrawingModel = drawingModel;

        PLUGIN_LOG_DEBUG(("  Plugin requested drawing model id  #%i\n",
            mDrawingModel));

        return rv;
    }

    case NPPVpluginEventModel: {
        NPError rv;
        int eventModel = (int16) (intptr_t) aValue;

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
        PR_LOG(gPluginLog, PR_LOG_WARNING,
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

    PRBool value = 0;
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
    // it on the stack would get clobbered. so protect with the larger PRBool.
    PRBool needsXEmbed = 0;
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

    NPObject* object = nsnull;
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

    *aValue = nsnull;
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

    char* plugId = NULL;
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
                                                 Shmem& mem,
                                                 int16_t* handled,
                                                 Shmem* rtnmem)
{
    PLUGIN_LOG_DEBUG_FUNCTION;
    AssertPluginThread();

    PaintTracker pt;

    NPCocoaEvent evcopy = event.event;

    if (evcopy.type == NPCocoaEventDrawRect) {
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
                              mWindow.width, mWindow.height, 8, 
                              mWindow.width * 4, mShColorSpace, 
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
                                                 Shmem& mem,
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
    nsRefPtr<nsIOSurface> surf = nsIOSurface::LookupSurface(surfaceid);
    if (!surf) {
        NS_ERROR("Invalid IOSurface.");
        *handled = false;
        return false;
    }

    if (evcopy.type == NPCocoaEventDrawRect) {
        mCARenderer.AttachIOSurface(surf);
        if (!mCARenderer.isInit()) {
            void *caLayer = nsnull;
            NPError result = mPluginIface->getvalue(GetNPP(), 
                                     NPPVpluginCoreAnimationLayer,
                                     &caLayer);
            
            if (result != NPERR_NO_ERROR || !caLayer) {
                PLUGIN_LOG_DEBUG(("Plugin requested CoreAnimation but did not "
                                  "provide CALayer."));
                *handled = false;
                return false;
            }

            mCARenderer.SetupRenderer(caLayer, mWindow.width, mWindow.height);

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

    mCARenderer.Render(mWindow.width, mWindow.height, nsnull);

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


#if defined(MOZ_X11) && defined(XP_UNIX) && !defined(XP_MACOSX)
static bool
XVisualIDToInfo(Display* aDisplay, VisualID aVisualID,
                Visual** aVisual, unsigned int* aDepth)
{
    if (aVisualID == None) {
        *aVisual = NULL;
        *aDepth = 0;
        return true;
    }

    const Screen* screen = DefaultScreenOfDisplay(aDisplay);

    for (int d = 0; d < screen->ndepths; d++) {
        Depth *d_info = &screen->depths[d];
        for (int v = 0; v < d_info->nvisuals; v++) {
            Visual* visual = &d_info->visuals[v];
            if (visual->visualid == aVisualID) {
                *aVisual = visual;
                *aDepth = d_info->depth;
                return true;
            }
        }
    }

    NS_ERROR("VisualID not on Screen.");
    return false;
}
#endif

bool
PluginInstanceChild::AnswerNPP_SetWindow(const NPRemoteWindow& aWindow)
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
    // The minimum info is sent over IPC to allow this
    // code to determine the rest.

    mWindow.window = reinterpret_cast<void*>(aWindow.window);
    mWindow.x = aWindow.x;
    mWindow.y = aWindow.y;
    mWindow.width = aWindow.width;
    mWindow.height = aWindow.height;
    mWindow.clipRect = aWindow.clipRect;
    mWindow.type = aWindow.type;

    mWsInfo.colormap = aWindow.colormap;
    if (!XVisualIDToInfo(mWsInfo.display, aWindow.visualID,
                         &mWsInfo.visual, &mWsInfo.depth))
        return false;

#ifdef MOZ_WIDGET_GTK2
    if (gtk_check_version(2,18,7) != NULL) { // older
        if (aWindow.type == NPWindowTypeWindow) {
            GdkWindow* socket_window = gdk_window_lookup(aWindow.window);
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
            && gtk_check_version(2, 12, 10) != NULL) { // older
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

          ReparentPluginWindow((HWND)aWindow.window);
          SizePluginWindow(aWindow.width, aWindow.height);

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

    if (mShContext) {
        // Release the shared context so that it is reallocated
        // with the new size. 
        ::CGContextRelease(mShContext);
        mShContext = nsnull;
    }

    if (mPluginIface->setwindow)
        (void) mPluginIface->setwindow(&mData, &mWindow);

#elif defined(ANDROID)
#  warning Need Android impl
#elif defined(MOZ_WIDGET_QT)
#  warning Need QT-nonX impl
#else
#  error Implement me for your OS
#endif

    return true;
}

bool
PluginInstanceChild::Initialize()
{
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
    wcex.hInstance      = GetModuleHandle(NULL);
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
                       0, 0, NULL, 0, GetModuleHandle(NULL), 0);
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
PluginInstanceChild::ReparentPluginWindow(HWND hWndParent)
{
    if (hWndParent != mPluginParentHWND && IsWindow(hWndParent)) {
        // Fix the child window's style to be a child window.
        LONG_PTR style = GetWindowLongPtr(mPluginWindowHWND, GWL_STYLE);
        style |= WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
        style &= ~WS_POPUP;
        SetWindowLongPtr(mPluginWindowHWND, GWL_STYLE, style);

        // Do the reparenting.
        SetParent(mPluginWindowHWND, hWndParent);

        // Make sure we're visible.
        ShowWindow(mPluginWindowHWND, SW_SHOWNA);
    }
    mPluginParentHWND = hWndParent;
}

void
PluginInstanceChild::SizePluginWindow(int width,
                                      int height)
{
    if (mPluginWindowHWND) {
        mPluginSize.x = width;
        mPluginSize.y = height;
        SetWindowPos(mPluginWindowHWND, NULL, 0, 0, width, height,
                     SWP_NOZORDER | SWP_NOREPOSITION);
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
    NS_ASSERTION(!mozilla::ipc::SyncChannel::IsPumpingMessages(),
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
    if (message == WM_MOUSEACTIVATE)
      self->CallPluginFocusChange(true);

    // Prevent lockups due to plugins making rpc calls when the parent
    // is making a synchronous SendMessage call to the child window. Add
    // more messages as needed.
    if ((InSendMessageEx(NULL)&(ISMEX_REPLIED|ISMEX_SEND)) == ISMEX_SEND) {
        switch(message) {
            case WM_KILLFOCUS:
            ReplyMessage(0);
            break;
        }
    }

    if (message == WM_KILLFOCUS)
      self->CallPluginFocusChange(false);

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
      PRUnichar szClass[26];
      HWND hwnd = GetForegroundWindow();
      if (hwnd && GetClassNameW(hwnd, szClass,
                                sizeof(szClass)/sizeof(PRUnichar)) &&
          !wcscmp(szClass, kFlashFullscreenClass)) {
        ReleaseCapture();
        SetFocus(hwnd);
      }
    }

    if (message == WM_CLOSE)
        self->DestroyPluginWindow();

    if (message == WM_NCDESTROY)
        RemoveProp(hWnd, kPluginInstanceChildProperty);

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
static User32SetWindowLongPtrA sUser32SetWindowLongAHookStub = NULL;
static User32SetWindowLongPtrW sUser32SetWindowLongWHookStub = NULL;
#else
typedef LONG
(WINAPI *User32SetWindowLongA)(HWND hWnd,
                               int nIndex,
                               LONG dwNewLong);
typedef LONG
(WINAPI *User32SetWindowLongW)(HWND hWnd,
                               int nIndex,
                               LONG dwNewLong);
static User32SetWindowLongA sUser32SetWindowLongAHookStub = NULL;
static User32SetWindowLongW sUser32SetWindowLongWHookStub = NULL;
#endif

extern LRESULT CALLBACK
NeuteredWindowProc(HWND hwnd,
                   UINT uMsg,
                   WPARAM wParam,
                   LPARAM lParam);

const wchar_t kOldWndProcProp[] = L"MozillaIPCOldWndProc";

// static
PRBool
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
      return PR_TRUE;
  // prevent the subclass
  return PR_FALSE;
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
    sUser32Intercept.AddHook("SetWindowLongPtrA", reinterpret_cast<intptr_t>(SetWindowLongPtrAHook),
                             (void**) &sUser32SetWindowLongAHookStub);
    sUser32Intercept.AddHook("SetWindowLongPtrW", reinterpret_cast<intptr_t>(SetWindowLongPtrWHook),
                             (void**) &sUser32SetWindowLongWHookStub);
#else
    sUser32Intercept.AddHook("SetWindowLongA", reinterpret_cast<intptr_t>(SetWindowLongAHook),
                             (void**) &sUser32SetWindowLongAHookStub);
    sUser32Intercept.AddHook("SetWindowLongW", reinterpret_cast<intptr_t>(SetWindowLongWHook),
                             (void**) &sUser32SetWindowLongWHookStub);
#endif
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
  PRUnichar szClass[21];
  bool haveClass = GetClassNameW(hWnd, szClass, NS_ARRAY_LENGTH(szClass));
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
  sWinlessPopupSurrogateHWND = NULL;

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
    sUser32Intercept.Init("user32.dll");
    sUser32Intercept.AddHook("TrackPopupMenu", reinterpret_cast<intptr_t>(TrackPopupHookProc),
                             (void**) &sUser32TrackPopupMenuStub);
}

void
PluginInstanceChild::CreateWinlessPopupSurrogate()
{
    // already initialized
    if (mWinlessPopupSurrogateHWND)
        return;

    HWND hwnd = NULL;
    NPError result;
    if (!CallNPN_GetValue_NPNVnetscapeWindow(&hwnd, &result)) {
        NS_ERROR("CallNPN_GetValue_NPNVnetscapeWindow failed.");
        return;
    }

    mWinlessPopupSurrogateHWND =
        CreateWindowEx(WS_EX_NOPARENTNOTIFY, L"Static", NULL, WS_CHILD, 0, 0,
                       0, 0, hwnd, 0, GetModuleHandle(NULL), 0);
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
    mWinlessPopupSurrogateHWND = NULL;
}

int16_t
PluginInstanceChild::WinlessHandleEvent(NPEvent& event)
{
    if (!mPluginIface->event)
        return false;

    // Events that might generate nested event dispatch loops need
    // special handling during delivery.
    int16_t handled;
    
    HWND focusHwnd = NULL;

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

    sWinlessPopupSurrogateHWND = NULL;

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
    ::SetViewportOrgEx(mSharedSurfaceDib.GetHDC(), -aWindow.x, -aWindow.y, NULL);

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

    mAlphaExtract.hdc = ::CreateCompatibleDC(NULL);

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

    void* ppvBits = nsnull;
    mAlphaExtract.bmp = ::CreateDIBSection(mAlphaExtract.hdc,
                                           (BITMAPINFO*)&bmih,
                                           DIB_RGB_COLORS,
                                           (void**)&ppvBits,
                                           NULL,
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

    mAlphaExtract.bmp = NULL;
    mAlphaExtract.hdc = NULL;
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
              // plugins. (See nsObjectFrame and gfxWindowsNativeDrawing for details.)
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
  mWinlessThrottleOldWndProc = nsnull;

  NS_ASSERTION(mWinlessHiddenMsgHWND,
               "Missing mWinlessHiddenMsgHWND w/subclass set??");

  // reset the subclass
  SetWindowLongPtr(mWinlessHiddenMsgHWND, GWLP_WNDPROC,
                   reinterpret_cast<LONG_PTR>(tmpProc));

  // Remove our instance prop
  RemoveProp(mWinlessHiddenMsgHWND, kFlashThrottleProperty);
  mWinlessHiddenMsgHWND = nsnull;
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

    PRUnichar className[64];
    if (!GetClassNameW(hWnd, className, sizeof(className)/sizeof(PRUnichar)))
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
    return nsnull;
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
    PR_LOG(gPluginLog, PR_LOG_DEBUG, ("%s", FULLFUNCTION));

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
    PR_LOG(gPluginLog, PR_LOG_DEBUG, ("%s", FULLFUNCTION));

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

PPluginScriptableObjectChild*
PluginInstanceChild::AllocPPluginScriptableObject()
{
    AssertPluginThread();
    return new PluginScriptableObjectChild(Proxy);
}

bool
PluginInstanceChild::DeallocPPluginScriptableObject(
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
PluginInstanceChild::AnswerPBrowserStreamConstructor(
    PBrowserStreamChild* aActor,
    const nsCString& url,
    const uint32_t& length,
    const uint32_t& lastmodified,
    PStreamNotifyChild* notifyData,
    const nsCString& headers,
    const nsCString& mimeType,
    const bool& seekable,
    NPError* rv,
    uint16_t* stype)
{
    AssertPluginThread();
    *rv = static_cast<BrowserStreamChild*>(aActor)
          ->StreamConstructed(mimeType, seekable, stype);
    return true;
}

PBrowserStreamChild*
PluginInstanceChild::AllocPBrowserStream(const nsCString& url,
                                         const uint32_t& length,
                                         const uint32_t& lastmodified,
                                         PStreamNotifyChild* notifyData,
                                         const nsCString& headers,
                                         const nsCString& mimeType,
                                         const bool& seekable,
                                         NPError* rv,
                                         uint16_t *stype)
{
    AssertPluginThread();
    return new BrowserStreamChild(this, url, length, lastmodified,
                                  static_cast<StreamNotifyChild*>(notifyData),
                                  headers, mimeType, seekable, rv, stype);
}

bool
PluginInstanceChild::DeallocPBrowserStream(PBrowserStreamChild* stream)
{
    AssertPluginThread();
    delete stream;
    return true;
}

PPluginStreamChild*
PluginInstanceChild::AllocPPluginStream(const nsCString& mimeType,
                                        const nsCString& target,
                                        NPError* result)
{
    NS_RUNTIMEABORT("not callable");
    return NULL;
}

bool
PluginInstanceChild::DeallocPPluginStream(PPluginStreamChild* stream)
{
    AssertPluginThread();
    delete stream;
    return true;
}

PStreamNotifyChild*
PluginInstanceChild::AllocPStreamNotify(const nsCString& url,
                                        const nsCString& target,
                                        const bool& post,
                                        const nsCString& buffer,
                                        const bool& file,
                                        NPError* result)
{
    AssertPluginThread();
    NS_RUNTIMEABORT("not reached");
    return NULL;
}

void
StreamNotifyChild::ActorDestroy(ActorDestroyReason why)
{
    if (AncestorDeletion == why && mBrowserStream) {
        NS_ERROR("Pending NPP_URLNotify not called when closing an instance.");

        // reclaim responsibility for deleting ourself
        mBrowserStream->mStreamNotify = NULL;
        mBrowserStream = NULL;
    }
}


void
StreamNotifyChild::SetAssociatedStream(BrowserStreamChild* bs)
{
    NS_ASSERTION(bs, "Shouldn't be null");
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
PluginInstanceChild::DeallocPStreamNotify(PStreamNotifyChild* notifyData)
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
        PluginModuleChild::current()->GetActorForNPObject(aObject);
    if (actor) {
        // Plugin-provided object that we've previously wrapped.
        return actor;
    }

    actor = new PluginScriptableObjectChild(LocalObject);
    if (!SendPPluginScriptableObjectConstructor(actor)) {
        NS_ERROR("Failed to send constructor message!");
        return nsnull;
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
        *aStream = NULL;
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
    PRUint32 notifyStreamCount = notifyStreams.Length();
    for (PRUint32 i = 0; i < notifyStreamCount; i++) {
        StreamNotifyChild* sn = static_cast<StreamNotifyChild*>(notifyStreams[i]);
        if (sn->mClosure == notifyData) {
            sn->SendRedirectNotifyResponse(static_cast<bool>(allow));
            return;
        }
    }
    NS_ASSERTION(PR_FALSE, "Couldn't find stream for redirect response!");
}

bool
PluginInstanceChild::RecvAsyncSetWindow(const gfxSurfaceType& aSurfaceType,
                                        const NPRemoteWindow& aWindow)
{
    AssertPluginThread();

    NS_ASSERTION(!aWindow.window, "Remote window should be null.");

    if (mCurrentAsyncSetWindowTask) {
        mCurrentAsyncSetWindowTask->Cancel();
        mCurrentAsyncSetWindowTask = nsnull;
    }

    if (mPendingPluginCall) {
        // We shouldn't process this now. Run it later.
        mCurrentAsyncSetWindowTask =
            NewRunnableMethod<PluginInstanceChild,
                              void (PluginInstanceChild::*)(const gfxSurfaceType&, const NPRemoteWindow&, bool),
                              gfxSurfaceType, NPRemoteWindow, bool>
                (this, &PluginInstanceChild::DoAsyncSetWindow,
                 aSurfaceType, aWindow, true);
        MessageLoop::current()->PostTask(FROM_HERE, mCurrentAsyncSetWindowTask);
    } else {
        DoAsyncSetWindow(aSurfaceType, aWindow, false);
    }

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
        mCurrentAsyncSetWindowTask = nsnull;
    }

    mWindow.window = NULL;
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

static inline gfxRect
GfxFromNsRect(const nsIntRect& aRect)
{
    return gfxRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

bool
PluginInstanceChild::CreateOptSurface(void)
{
    NS_ABORT_IF_FALSE(mSurfaceType != gfxASurface::SurfaceTypeMax,
                      "Need a valid surface type here");
    NS_ASSERTION(!mCurrentSurface, "mCurrentSurfaceActor can get out of sync.");

    nsRefPtr<gfxASurface> retsurf;
    // Use an opaque surface unless we're transparent and *don't* have
    // a background to source from.
    gfxASurface::gfxImageFormat format =
        (mIsTransparent && !mBackground) ? gfxASurface::ImageFormatARGB32 :
                                           gfxASurface::ImageFormatRGB24;

#ifdef MOZ_X11
#if (MOZ_PLATFORM_MAEMO == 5) || (MOZ_PLATFORM_MAEMO == 6)
    // On Maemo 5, we must send the Visibility event to activate the plugin
    if (mMaemoImageRendering) {
        NPEvent pluginEvent;
        XVisibilityEvent& visibilityEvent = pluginEvent.xvisibility;
        visibilityEvent.type = VisibilityNotify;
        visibilityEvent.display = 0;
        visibilityEvent.state = VisibilityUnobscured;
        mPluginIface->event(&mData, reinterpret_cast<void*>(&pluginEvent));
    }
#endif
    Display* dpy = mWsInfo.display;
    Screen* screen = DefaultScreenOfDisplay(dpy);
    if (format == gfxASurface::ImageFormatRGB24 &&
        DefaultDepth(dpy, DefaultScreen(dpy)) == 16) {
        format = gfxASurface::ImageFormatRGB16_565;
    }

    if (mSurfaceType == gfxASurface::SurfaceTypeXlib) {
        if (!mIsTransparent  || mBackground) {
            Visual* defaultVisual = DefaultVisualOfScreen(screen);
            mCurrentSurface =
                gfxXlibSurface::Create(screen, defaultVisual,
                                       gfxIntSize(mWindow.width,
                                                  mWindow.height));
            return mCurrentSurface != nsnull;
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
        return mCurrentSurface != nsnull;
    }
#endif

#ifdef XP_WIN
    if (mSurfaceType == gfxASurface::SurfaceTypeWin32 ||
        mSurfaceType == gfxASurface::SurfaceTypeD2D) {
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

#ifdef MOZ_PLATFORM_MAEMO
    // On maemo plugins support non-default visual rendering
    bool supportNonDefaultVisual = true;
#else
    bool supportNonDefaultVisual = false;
#endif
#ifdef MOZ_X11
    Screen* screen = DefaultScreenOfDisplay(mWsInfo.display);
    Visual* defaultVisual = DefaultVisualOfScreen(screen);
    Visual* visual = nsnull;
    Colormap colormap = 0;
    mDoAlphaExtraction = false;
    bool createHelperSurface = false;

    if (mCurrentSurface->GetType() == gfxASurface::SurfaceTypeXlib) {
        static_cast<gfxXlibSurface*>(mCurrentSurface.get())->
            GetColormapAndVisual(&colormap, &visual);
        // Create helper surface if layer surface visual not same as default
        // and we don't support non-default visual rendering
        if (!visual || (defaultVisual != visual && !supportNonDefaultVisual)) {
            createHelperSurface = true;
            visual = defaultVisual;
            mDoAlphaExtraction = mIsTransparent;
        }
    } else if (mCurrentSurface->GetType() == gfxASurface::SurfaceTypeImage) {
#if (MOZ_PLATFORM_MAEMO == 5) || (MOZ_PLATFORM_MAEMO == 6)
        if (mMaemoImageRendering) {
            // No helper surface needed, when mMaemoImageRendering is TRUE.
            // we can rendering directly into image memory
            // with NPImageExpose Maemo5 NPAPI
            return PR_TRUE;
        }
#endif
        // For image layer surface we should always create helper surface
        createHelperSurface = true;
        // Check if we can create helper surface with non-default visual
        visual = gfxXlibSurface::FindVisual(screen,
            static_cast<gfxImageSurface*>(mCurrentSurface.get())->Format());
        if (visual && defaultVisual != visual && !supportNonDefaultVisual) {
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
#ifndef XP_MACOSX
    nsIntRect toInvalidate(0, 0, 0, 0);
    gfxIntSize winSize = gfxIntSize(mWindow.width, mWindow.height);

    if (mBackground && mBackground->GetSize() != winSize) {
        // It would be nice to keep the old background here, but doing
        // so can lead to cases in which we permanently keep the old
        // background size.
        mBackground = nsnull;
        toInvalidate.UnionRect(toInvalidate,
                               nsIntRect(0, 0, winSize.width, winSize.height));
    }

    if (mCurrentSurface) {
        gfxIntSize surfSize = mCurrentSurface->GetSize();
        if (winSize != surfSize ||
            (mBackground && !CanPaintOnBackground()) ||
            (mBackground &&
             gfxASurface::CONTENT_COLOR != mCurrentSurface->GetContentType()) ||
            (!mBackground && mIsTransparent &&
             gfxASurface::CONTENT_COLOR == mCurrentSurface->GetContentType())) {
            // Don't try to use an old, invalid DC.
            mWindow.window = nsnull;
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
#else // XP_MACOSX

    if (!mDoubleBufferCARenderer.HasCALayer()) {
        void *caLayer = nsnull;
        if (mDrawingModel == NPDrawingModelCoreGraphics) {
            if (!mCGLayer) {
                caLayer = mozilla::plugins::PluginUtilsOSX::GetCGLayer(CallCGDraw, this);

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
         mDoubleBufferCARenderer.GetFrontSurfaceHeight() != mWindow.height) ) {
        mDoubleBufferCARenderer.ClearFrontSurface();
    }

    if (!mDoubleBufferCARenderer.HasFrontSurface()) {
        bool allocSurface = mDoubleBufferCARenderer.InitFrontSurface(mWindow.width, 
                                                           mWindow.height);
        if (!allocSurface) {
            PLUGIN_LOG_DEBUG(("Fail to allocate front IOSurface"));
            return false;
        }

        if (mPluginIface->setwindow)
            (void) mPluginIface->setwindow(&mData, &mWindow);

        nsIntRect toInvalidate(0, 0, mWindow.width, mWindow.height);
        mAccumulatedInvalidRect.UnionRect(mAccumulatedInvalidRect, toInvalidate);
    }
  
    return true;
#endif
}

void
PluginInstanceChild::UpdateWindowAttributes(bool aForceSetWindow)
{
    nsRefPtr<gfxASurface> curSurface = mHelperSurface ? mHelperSurface : mCurrentSurface;
    bool needWindowUpdate = aForceSetWindow;
#ifdef MOZ_X11
    Visual* visual = nsnull;
    Colormap colormap = 0;
    if (curSurface && curSurface->GetType() == gfxASurface::SurfaceTypeXlib) {
        static_cast<gfxXlibSurface*>(curSurface.get())->
            GetColormapAndVisual(&colormap, &visual);
        if (visual != mWsInfo.visual || colormap != mWsInfo.colormap) {
            mWsInfo.visual = visual;
            mWsInfo.colormap = colormap;
            needWindowUpdate = true;
        }
    }
#if (MOZ_PLATFORM_MAEMO == 5) || (MOZ_PLATFORM_MAEMO == 6)
    else if (curSurface && curSurface->GetType() == gfxASurface::SurfaceTypeImage
             && mMaemoImageRendering) {
        // For maemo5 we need to setup window/colormap to 0
        // and specify depth of image surface
        gfxImageSurface* img = static_cast<gfxImageSurface*>(curSurface.get());
        if (mWindow.window ||
            mWsInfo.depth != gfxUtils::ImageFormatToDepth(img->Format()) ||
            mWsInfo.colormap) {
            mWindow.window = nsnull;
            mWsInfo.depth = gfxUtils::ImageFormatToDepth(img->Format());
            mWsInfo.colormap = 0;
            needWindowUpdate = true;
        }
    }
#endif // MAEMO
#endif // MOZ_X11
#ifdef XP_WIN
    HDC dc = NULL;

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
#if (MOZ_PLATFORM_MAEMO == 5) || (MOZ_PLATFORM_MAEMO == 6)
    // On maemo5 we do support Image rendering NPAPI
    if (mMaemoImageRendering &&
        aSurface->GetType() == gfxASurface::SurfaceTypeImage) {
        aSurface->Flush();
        gfxImageSurface* image = static_cast<gfxImageSurface*>(aSurface);
        NPImageExpose imgExp;
        imgExp.depth = gfxUtils::ImageFormatToDepth(image->Format());
        imgExp.x = aRect.x;
        imgExp.y = aRect.y;
        imgExp.width = aRect.width;
        imgExp.height = aRect.height;
        imgExp.stride = image->Stride();
        imgExp.data = (char *)image->Data();
        imgExp.dataSize.width = image->Width();
        imgExp.dataSize.height = image->Height();
        imgExp.translateX = 0;
        imgExp.translateY = 0;
        imgExp.scaleX = 1;
        imgExp.scaleY = 1;
        NPEvent pluginEvent;
        XGraphicsExposeEvent& exposeEvent = pluginEvent.xgraphicsexpose;
        exposeEvent.type = GraphicsExpose;
        exposeEvent.display = 0;
        // Store imageExpose structure pointer as drawable member
        exposeEvent.drawable = (Drawable)&imgExp;
        exposeEvent.x = imgExp.x;
        exposeEvent.y = imgExp.y;
        exposeEvent.width = imgExp.width;
        exposeEvent.height = imgExp.height;
        exposeEvent.count = 0;
        // information not set:
        exposeEvent.serial = 0;
        exposeEvent.send_event = False;
        exposeEvent.major_code = 0;
        exposeEvent.minor_code = 0;
        mPluginIface->event(&mData, reinterpret_cast<void*>(&exposeEvent));
    } else
#endif
    {
        NS_ASSERTION(aSurface->GetType() == gfxASurface::SurfaceTypeXlib,
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

    ::SetViewportOrgEx((HDC) mWindow.window, -mWindow.x, -mWindow.y, NULL);
    ::SelectClipRgn((HDC) mWindow.window, NULL);
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
#if (MOZ_PLATFORM_MAEMO == 5) || (MOZ_PLATFORM_MAEMO == 6)
        // Don't use mHelperSurface if surface is image and mMaemoImageRendering is TRUE
        if (!mMaemoImageRendering ||
            renderSurface->GetType() != gfxASurface::SurfaceTypeImage)
#endif
        renderSurface = mHelperSurface;
    }
#endif

    if (aColor.a > 0.0) {
       // Clear surface content for transparent rendering
       nsRefPtr<gfxContext> ctx = new gfxContext(renderSurface);
       ctx->SetColor(aColor);
       ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
       ctx->Rectangle(GfxFromNsRect(plPaintRect));
       ctx->Fill();
    }

    PaintRectToPlatformSurface(plPaintRect, renderSurface);

    if (renderSurface != aSurface) {
        // Copy helper surface content to target
        nsRefPtr<gfxContext> ctx = new gfxContext(aSurface);
        ctx->SetSource(renderSurface);
        ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
        ctx->Rectangle(GfxFromNsRect(aRect));
        ctx->Fill();
    }
}

void
PluginInstanceChild::PaintRectWithAlphaExtraction(const nsIntRect& aRect,
                                                  gfxASurface* aSurface)
{
    NS_ABORT_IF_FALSE(aSurface->GetContentType() == gfxASurface::CONTENT_COLOR_ALPHA,
                      "Refusing to pointlessly recover alpha");

    nsIntRect rect(aRect);
    // If |aSurface| can be used to paint and can have alpha values
    // recovered directly to it, do that to save a tmp surface and
    // copy.
    bool useSurfaceSubimageForBlack = false;
    if (gfxASurface::SurfaceTypeImage == aSurface->GetType()) {
        gfxImageSurface* surfaceAsImage =
            static_cast<gfxImageSurface*>(aSurface);
        useSurfaceSubimageForBlack =
            (surfaceAsImage->Format() == gfxASurface::ImageFormatARGB32);
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
    whiteImage = new gfxImageSurface(targetSize, gfxASurface::ImageFormatRGB24);
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
        gfxRect copyRect(gfxPoint(0, 0), targetRect.Size());
        nsRefPtr<gfxContext> ctx = new gfxContext(whiteImage);
        ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
        ctx->SetSource(aSurface, deviceOffset);
        ctx->Rectangle(copyRect);
        ctx->Fill();
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
                                         gfxASurface::ImageFormatARGB32);
    }

    // Paint onto black background
    blackImage->SetDeviceOffset(deviceOffset);
    PaintRectToSurface(rect, blackImage, gfxRGBA(0.0, 0.0, 0.0));
#endif

    NS_ABORT_IF_FALSE(whiteImage && blackImage, "Didn't paint enough!");

    // Extract alpha from black and white image and store to black
    // image
    if (!gfxAlphaRecovery::RecoverAlpha(blackImage, whiteImage)) {
        return;
    }

    // If we had to use a temporary black surface, copy the pixels
    // with alpha back to the target
    if (!useSurfaceSubimageForBlack) {
        nsRefPtr<gfxContext> ctx = new gfxContext(aSurface);
        ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
        ctx->SetSource(blackImage);
        ctx->Rectangle(targetRect);
        ctx->Fill();
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
        currSurf = IOSurfaceDescriptor(mDoubleBufferCARenderer.GetFrontSurfaceID());

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

    NS_ASSERTION(mWindow.width == (mWindow.clipRect.right - mWindow.clipRect.left) &&
                 mWindow.height == (mWindow.clipRect.bottom - mWindow.clipRect.top),
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
        gfxASurface::CONTENT_COLOR_ALPHA == mCurrentSurface->GetContentType();
    PLUGIN_LOG_DEBUG(
        ("[InstanceChild][%p] Painting%s <x=%d,y=%d, w=%d,h=%d> on surface <w=%d,h=%d>",
         this, haveTransparentPixels ? " with alpha" : "",
         rect.x, rect.y, rect.width, rect.height,
         mCurrentSurface->GetSize().width, mCurrentSurface->GetSize().height));

    if (CanPaintOnBackground()) {
        PLUGIN_LOG_DEBUG(("  (on background)"));
        // Source the background pixels ...
        {
            nsRefPtr<gfxContext> ctx =
                new gfxContext(mHelperSurface ? mHelperSurface : mCurrentSurface);
            ctx->SetSource(mBackground);
            ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
            ctx->Rectangle(gfxRect(rect.x, rect.y, rect.width, rect.height));
            ctx->Fill();
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
    if (mCurrentSurface->GetType() == gfxASurface::SurfaceTypeXlib) {
        gfxXlibSurface *xsurf = static_cast<gfxXlibSurface*>(mCurrentSurface.get());
        currSurf = SurfaceDescriptorX11(xsurf->XDrawable(), xsurf->XRenderFormat()->id,
                                        mCurrentSurface->GetSize());
        // Need to sync all pending x-paint requests
        // before giving drawable to another process
        XSync(mWsInfo.display, False);
    } else
#endif
#ifdef XP_WIN
    if (SharedDIBSurface::IsSharedDIBSurface(mCurrentSurface)) {
        SharedDIBSurface* s = static_cast<SharedDIBSurface*>(mCurrentSurface.get());
        if (!mCurrentSurfaceActor) {
            base::SharedMemoryHandle handle = NULL;
            s->ShareToProcess(PluginModuleChild::current()->OtherProcess(), &handle);

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
    if (mBackSurface->GetType() != gfxASurface::SurfaceTypeXlib &&
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
    nsRefPtr<gfxContext> ctx = new gfxContext(mCurrentSurface);
    ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
    ctx->SetSource(mBackSurface);
    // Subtract from mSurfaceDifferenceRect area which is overlapping with rect
    nsIntRegion result;
    result.Sub(mSurfaceDifferenceRect, nsIntRegion(rect));
    nsIntRegionRectIterator iter(result);
    const nsIntRect* r;
    while ((r = iter.Next()) != nsnull) {
        ctx->Rectangle(GfxFromNsRect(*r));
    }
    ctx->Fill();

    return true;
}

void
PluginInstanceChild::InvalidateRectDelayed(void)
{
    if (!mCurrentInvalidateTask) {
        return;
    }

    mCurrentInvalidateTask = nsnull;
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
    NS_ABORT_IF_FALSE(mIsTransparent, "Only transparent plugins use backgrounds");

    if (!mBackground) {
        // XXX refactor me
        switch (aBackground.type()) {
#ifdef MOZ_X11
        case SurfaceDescriptor::TSurfaceDescriptorX11: {
            SurfaceDescriptorX11 xdesc = aBackground.get_SurfaceDescriptorX11();
            XRenderPictFormat pf;
            pf.id = xdesc.xrenderPictID();
            XRenderPictFormat *incFormat =
                XRenderFindFormat(DefaultXDisplay(), PictFormatID, &pf, 0);
            mBackground =
                new gfxXlibSurface(DefaultScreenOfDisplay(DefaultXDisplay()),
                                   xdesc.XID(), incFormat, xdesc.size());
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

    // The browser is limping along with a stale copy of our pixels.
    // Try to repaint ASAP.  This will ClearCurrentBackground() if we
    // needed it.
    if (!ShowPluginFrame()) {
        NS_WARNING("Couldn't immediately repaint plugin instance");
        AsyncShowPluginFrame();
    }

    return true;
}

PPluginBackgroundDestroyerChild*
PluginInstanceChild::AllocPPluginBackgroundDestroyer()
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
        mBackground = nsnull;
        AsyncShowPluginFrame();
    }

    return PPluginBackgroundDestroyerChild::Send__delete__(aActor);
}

bool
PluginInstanceChild::DeallocPPluginBackgroundDestroyer(
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

    {
        MutexAutoLock lock(mAsyncCallMutex);
        mPendingAsyncCalls.AppendElement(task);
    }
    ProcessChild::message_loop()->PostTask(FROM_HERE, task);
}

static PLDHashOperator
InvalidateObject(DeletingObjectEntry* e, void* userArg)
{
    NPObject* o = e->GetKey();
    if (!e->mDeleted && o->_class && o->_class->invalidate)
        o->_class->invalidate(o);

    return PL_DHASH_NEXT;
}

static PLDHashOperator
DeleteObject(DeletingObjectEntry* e, void* userArg)
{
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

    return PL_DHASH_NEXT;
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
            mDoubleBufferCARenderer.GetBackSurfaceHeight())) {

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
    mCurrentSurface = nsnull;
#ifdef MOZ_WIDGET_COCOA
    if (mDoubleBufferCARenderer.HasFrontSurface()) {
        mDoubleBufferCARenderer.ClearFrontSurface();
    }
#endif
#ifdef XP_WIN
    if (mCurrentSurfaceActor) {
        PPluginSurfaceChild::Send__delete__(mCurrentSurfaceActor);
        mCurrentSurfaceActor = NULL;
    }
#endif
    mHelperSurface = nsnull;
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
    mCurrentSurface = nsnull;
    mBackSurface = nsnull;

#ifdef XP_WIN
    if (mCurrentSurfaceActor) {
        PPluginSurfaceChild::Send__delete__(mCurrentSurfaceActor);
        mCurrentSurfaceActor = NULL;
    }
    if (mBackSurfaceActor) {
        PPluginSurfaceChild::Send__delete__(mBackSurfaceActor);
        mBackSurfaceActor = NULL;
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
        mCGLayer = nsnull;
    }

    mDoubleBufferCARenderer.ClearFrontSurface();
    mDoubleBufferCARenderer.ClearBackSurface();
#endif
}

bool
PluginInstanceChild::AnswerNPP_Destroy(NPError* aResult)
{
    PLUGIN_LOG_DEBUG_METHOD;
    AssertPluginThread();

#if defined(OS_WIN)
    SetProp(mPluginWindowHWND, kPluginIgnoreSubclassProperty, (HANDLE)1);
#endif

    InfallibleTArray<PBrowserStreamChild*> streams;
    ManagedPBrowserStreamChild(streams);

    // First make sure none of these streams become deleted
    for (PRUint32 i = 0; i < streams.Length(); ) {
        if (static_cast<BrowserStreamChild*>(streams[i])->InstanceDying())
            ++i;
        else
            streams.RemoveElementAt(i);
    }
    for (PRUint32 i = 0; i < streams.Length(); ++i)
        static_cast<BrowserStreamChild*>(streams[i])->FinishDelivery();

    mTimers.Clear();

    // NPP_Destroy() should be a synchronization point for plugin threads
    // calling NPN_AsyncCall: after this function returns, they are no longer
    // allowed to make async calls on this instance.
    PluginModuleChild::current()->NPP_Destroy(this);
    mData.ndata = 0;

    if (mCurrentInvalidateTask) {
        mCurrentInvalidateTask->Cancel();
        mCurrentInvalidateTask = nsnull;
    }
    if (mCurrentAsyncSetWindowTask) {
        mCurrentAsyncSetWindowTask->Cancel();
        mCurrentAsyncSetWindowTask = nsnull;
    }

    ClearAllSurfaces();

    mDeletingHash = new nsTHashtable<DeletingObjectEntry>;
    mDeletingHash->Init();
    PluginModuleChild::current()->FindNPObjectsForInstance(this);

    mDeletingHash->EnumerateEntries(InvalidateObject, NULL);
    mDeletingHash->EnumerateEntries(DeleteObject, NULL);

    // Null out our cached actors as they should have been killed in the
    // PluginInstanceDestroyed call above.
    mCachedWindowActor = nsnull;
    mCachedElementActor = nsnull;

#if defined(OS_WIN)
    SharedSurfaceRelease();
    DestroyWinlessPopupSurrogate();
    UnhookWinlessFlashThrottle();
    DestroyPluginWindow();
#endif

    // Pending async calls are discarded, not delivered. This matches the
    // in-process behavior.
    for (PRUint32 i = 0; i < mPendingAsyncCalls.Length(); ++i)
        mPendingAsyncCalls[i]->Cancel();

    mPendingAsyncCalls.Clear();

    return true;
}
