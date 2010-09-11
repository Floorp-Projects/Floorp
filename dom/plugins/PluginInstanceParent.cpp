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

#include "PluginInstanceParent.h"

#include "BrowserStreamParent.h"
#include "PluginModuleParent.h"
#include "PluginStreamParent.h"
#include "StreamNotifyParent.h"
#include "npfunctions.h"
#include "nsAutoPtr.h"
#include "mozilla/unused.h"

#if defined(OS_WIN)
#include <windowsx.h>

// Plugin focus event for widget.
extern const PRUnichar* kOOPPPluginFocusEventId;
UINT gOOPPPluginFocusEvent =
    RegisterWindowMessage(kOOPPPluginFocusEventId);
extern const PRUnichar* kFlashFullscreenClass;
UINT gOOPPSpinNativeLoopEvent =
    RegisterWindowMessage(L"SyncChannel Spin Inner Loop Message");
UINT gOOPPStopNativeLoopEvent =
    RegisterWindowMessage(L"SyncChannel Stop Inner Loop Message");
#elif defined(MOZ_WIDGET_GTK2)
#include <gdk/gdk.h>
#elif defined(XP_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#endif // defined(XP_MACOSX)

using namespace mozilla::plugins;

PluginInstanceParent::PluginInstanceParent(PluginModuleParent* parent,
                                           NPP npp,
                                           const nsCString& aMimeType,
                                           const NPNetscapeFuncs* npniface)
  : mParent(parent)
    , mNPP(npp)
    , mNPNIface(npniface)
    , mWindowType(NPWindowTypeWindow)
#if defined(OS_WIN)
    , mPluginHWND(NULL)
    , mPluginWndProc(NULL)
    , mNestedEventState(false)
#endif // defined(XP_WIN)
    , mQuirks(0)
#if defined(XP_MACOSX)
    , mShWidth(0)
    , mShHeight(0)
    , mShColorSpace(nsnull)
    , mDrawingModel(NPDrawingModelCoreGraphics)
    , mIOSurface(nsnull)
#endif
{
    InitQuirksModes(aMimeType);
}

void
PluginInstanceParent::InitQuirksModes(const nsCString& aMimeType)
{
#ifdef OS_MACOSX
    NS_NAMED_LITERAL_CSTRING(flash, "application/x-shockwave-flash");
    // Flash sends us Invalidate events so we will use those
    // instead of the refresh timer.
    if (!FindInReadable(flash, aMimeType)) {
        mQuirks |= COREANIMATION_REFRESH_TIMER;
    }
#endif
}

PluginInstanceParent::~PluginInstanceParent()
{
    if (mNPP)
        mNPP->pdata = NULL;

#if defined(OS_WIN)
    NS_ASSERTION(!(mPluginHWND || mPluginWndProc),
        "Subclass was not reset correctly before the dtor was reached!");
#endif
#if defined(OS_MACOSX)
    if (mShWidth != 0 && mShHeight != 0) {
        DeallocShmem(mShSurface);
    }
    if (mShColorSpace)
        ::CGColorSpaceRelease(mShColorSpace);
    if (mIOSurface)
        delete mIOSurface;
    if (mDrawingModel == NPDrawingModelCoreAnimation) {
        mParent->RemoveFromRefreshTimer(this);
    }
#endif
}

bool
PluginInstanceParent::Init()
{
    return !!mScriptableObjects.Init();
}

namespace {

PLDHashOperator
ActorCollect(const void* aKey,
             PluginScriptableObjectParent* aData,
             void* aUserData)
{
    nsTArray<PluginScriptableObjectParent*>* objects =
        reinterpret_cast<nsTArray<PluginScriptableObjectParent*>*>(aUserData);
    return objects->AppendElement(aData) ? PL_DHASH_NEXT : PL_DHASH_STOP;
}

} // anonymous namespace

void
PluginInstanceParent::ActorDestroy(ActorDestroyReason why)
{
#if defined(OS_WIN)
    if (why == AbnormalShutdown) {
        // If the plugin process crashes, this is the only
        // chance we get to destroy resources.
        SharedSurfaceRelease();
        UnsubclassPluginWindow();
    }
#endif
}

NPError
PluginInstanceParent::Destroy()
{
    NPError retval;
    if (!CallNPP_Destroy(&retval))
        retval = NPERR_GENERIC_ERROR;

#if defined(OS_WIN)
    SharedSurfaceRelease();
    UnsubclassPluginWindow();
#endif

    return retval;
}

PBrowserStreamParent*
PluginInstanceParent::AllocPBrowserStream(const nsCString& url,
                                          const uint32_t& length,
                                          const uint32_t& lastmodified,
                                          PStreamNotifyParent* notifyData,
                                          const nsCString& headers,
                                          const nsCString& mimeType,
                                          const bool& seekable,
                                          NPError* rv,
                                          uint16_t *stype)
{
    NS_RUNTIMEABORT("Not reachable");
    return NULL;
}

bool
PluginInstanceParent::DeallocPBrowserStream(PBrowserStreamParent* stream)
{
    delete stream;
    return true;
}

PPluginStreamParent*
PluginInstanceParent::AllocPPluginStream(const nsCString& mimeType,
                                         const nsCString& target,
                                         NPError* result)
{
    return new PluginStreamParent(this, mimeType, target, result);
}

bool
PluginInstanceParent::DeallocPPluginStream(PPluginStreamParent* stream)
{
    delete stream;
    return true;
}

bool
PluginInstanceParent::AnswerNPN_GetValue_NPNVjavascriptEnabledBool(
                                                       bool* value,
                                                       NPError* result)
{
    NPBool v;
    *result = mNPNIface->getvalue(mNPP, NPNVjavascriptEnabledBool, &v);
    *value = v;
    return true;
}

bool
PluginInstanceParent::AnswerNPN_GetValue_NPNVisOfflineBool(bool* value,
                                                           NPError* result)
{
    NPBool v;
    *result = mNPNIface->getvalue(mNPP, NPNVisOfflineBool, &v);
    *value = v;
    return true;
}

bool
PluginInstanceParent::AnswerNPN_GetValue_NPNVnetscapeWindow(NativeWindowHandle* value,
                                                            NPError* result)
{
#ifdef XP_WIN
    HWND id;
#elif defined(MOZ_X11)
    XID id;
#elif defined(XP_MACOSX)
    intptr_t id;
#elif defined(ANDROID)
#warning Need Android impl
    int id;
#else
#warning Implement me
#endif

    *result = mNPNIface->getvalue(mNPP, NPNVnetscapeWindow, &id);
    *value = id;
    return true;
}

bool
PluginInstanceParent::InternalGetValueForNPObject(
                                         NPNVariable aVariable,
                                         PPluginScriptableObjectParent** aValue,
                                         NPError* aResult)
{
    NPObject* npobject;
    NPError result = mNPNIface->getvalue(mNPP, aVariable, (void*)&npobject);
    if (result == NPERR_NO_ERROR) {
        NS_ASSERTION(npobject, "Shouldn't return null and NPERR_NO_ERROR!");

        PluginScriptableObjectParent* actor = GetActorForNPObject(npobject);
        mNPNIface->releaseobject(npobject);
        if (actor) {
            *aValue = actor;
            *aResult = NPERR_NO_ERROR;
            return true;
        }

        NS_ERROR("Failed to get actor!");
        result = NPERR_GENERIC_ERROR;
    }

    *aValue = nsnull;
    *aResult = result;
    return true;
}

bool
PluginInstanceParent::AnswerNPN_GetValue_NPNVWindowNPObject(
                                         PPluginScriptableObjectParent** aValue,
                                         NPError* aResult)
{
    return InternalGetValueForNPObject(NPNVWindowNPObject, aValue, aResult);
}

bool
PluginInstanceParent::AnswerNPN_GetValue_NPNVPluginElementNPObject(
                                         PPluginScriptableObjectParent** aValue,
                                         NPError* aResult)
{
    return InternalGetValueForNPObject(NPNVPluginElementNPObject, aValue,
                                       aResult);
}

bool
PluginInstanceParent::AnswerNPN_GetValue_NPNVprivateModeBool(bool* value,
                                                             NPError* result)
{
    NPBool v;
    *result = mNPNIface->getvalue(mNPP, NPNVprivateModeBool, &v);
    *value = v;
    return true;
}

bool
PluginInstanceParent::AnswerNPN_SetValue_NPPVpluginWindow(
    const bool& windowed, NPError* result)
{
    NPBool isWindowed = windowed;
    *result = mNPNIface->setvalue(mNPP, NPPVpluginWindowBool,
                                  (void*)isWindowed);
    return true;
}

bool
PluginInstanceParent::AnswerNPN_SetValue_NPPVpluginTransparent(
    const bool& transparent, NPError* result)
{
    NPBool isTransparent = transparent;
    *result = mNPNIface->setvalue(mNPP, NPPVpluginTransparentBool,
                                  (void*)isTransparent);
    return true;
}

bool
PluginInstanceParent::AnswerNPN_SetValue_NPPVpluginDrawingModel(
    const int& drawingModel, NPError* result)
{
#ifdef XP_MACOSX
    if (drawingModel == NPDrawingModelCoreAnimation ||
        drawingModel == NPDrawingModelInvalidatingCoreAnimation) {
        // We need to request CoreGraphics otherwise
        // the nsObjectFrame will try to draw a CALayer
        // that can not be shared across process.
        mDrawingModel = drawingModel;
        *result = mNPNIface->setvalue(mNPP, NPPVpluginDrawingModel,
                                  (void*)NPDrawingModelCoreGraphics);
        if (drawingModel == NPDrawingModelCoreAnimation &&
            mQuirks & COREANIMATION_REFRESH_TIMER) {
            mParent->AddToRefreshTimer(this);
        }
    } else {
        mDrawingModel = drawingModel;
        *result = mNPNIface->setvalue(mNPP, NPPVpluginDrawingModel,
                                  (void*)drawingModel);
    }
    return true;
#else
    *result = NPERR_GENERIC_ERROR;
    return true;
#endif
}

bool
PluginInstanceParent::AnswerNPN_SetValue_NPPVpluginEventModel(
    const int& eventModel, NPError* result)
{
#ifdef XP_MACOSX
    *result = mNPNIface->setvalue(mNPP, NPPVpluginEventModel,
                                  (void*)eventModel);
    return true;
#else
    *result = NPERR_GENERIC_ERROR;
    return true;
#endif
}

bool
PluginInstanceParent::AnswerNPN_GetURL(const nsCString& url,
                                       const nsCString& target,
                                       NPError* result)
{
    *result = mNPNIface->geturl(mNPP,
                                NullableStringGet(url),
                                NullableStringGet(target));
    return true;
}

bool
PluginInstanceParent::AnswerNPN_PostURL(const nsCString& url,
                                        const nsCString& target,
                                        const nsCString& buffer,
                                        const bool& file,
                                        NPError* result)
{
    *result = mNPNIface->posturl(mNPP, url.get(), NullableStringGet(target),
                                 buffer.Length(), buffer.get(), file);
    return true;
}

PStreamNotifyParent*
PluginInstanceParent::AllocPStreamNotify(const nsCString& url,
                                         const nsCString& target,
                                         const bool& post,
                                         const nsCString& buffer,
                                         const bool& file,
                                         NPError* result)
{
    return new StreamNotifyParent();
}

bool
PluginInstanceParent::AnswerPStreamNotifyConstructor(PStreamNotifyParent* actor,
                                                     const nsCString& url,
                                                     const nsCString& target,
                                                     const bool& post,
                                                     const nsCString& buffer,
                                                     const bool& file,
                                                     NPError* result)
{
    bool streamDestroyed = false;
    static_cast<StreamNotifyParent*>(actor)->
        SetDestructionFlag(&streamDestroyed);

    if (!post) {
        *result = mNPNIface->geturlnotify(mNPP,
                                          NullableStringGet(url),
                                          NullableStringGet(target),
                                          actor);
    }
    else {
        *result = mNPNIface->posturlnotify(mNPP,
                                           NullableStringGet(url),
                                           NullableStringGet(target),
                                           buffer.Length(),
                                           NullableStringGet(buffer),
                                           file, actor);
    }

    if (!streamDestroyed) {
        static_cast<StreamNotifyParent*>(actor)->ClearDestructionFlag();
        if (*result != NPERR_NO_ERROR)
            return PStreamNotifyParent::Send__delete__(actor,
                                                       NPERR_GENERIC_ERROR);
    }

    return true;
}

bool
PluginInstanceParent::DeallocPStreamNotify(PStreamNotifyParent* notifyData)
{
    delete notifyData;
    return true;
}

bool
PluginInstanceParent::RecvNPN_InvalidateRect(const NPRect& rect)
{
    mNPNIface->invalidaterect(mNPP, const_cast<NPRect*>(&rect));
    return true;
}

nsresult
PluginInstanceParent::AsyncSetWindow(NPWindow* aWindow)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
PluginInstanceParent::NotifyPainted(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
PluginInstanceParent::GetSurface(gfxASurface** aSurface)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NPError
PluginInstanceParent::NPP_SetWindow(const NPWindow* aWindow)
{
    PLUGIN_LOG_DEBUG(("%s (aWindow=%p)", FULLFUNCTION, (void*) aWindow));

    NS_ENSURE_TRUE(aWindow, NPERR_GENERIC_ERROR);

    NPRemoteWindow window;
    mWindowType = aWindow->type;

#if defined(OS_WIN)
    // On windowless controls, reset the shared memory surface as needed.
    if (mWindowType == NPWindowTypeDrawable) {
        // SharedSurfaceSetWindow will take care of NPRemoteWindow.
        if (!SharedSurfaceSetWindow(aWindow, window)) {
          return NPERR_OUT_OF_MEMORY_ERROR;
        }
    }
    else {
        SubclassPluginWindow(reinterpret_cast<HWND>(aWindow->window));

        window.window = reinterpret_cast<unsigned long>(aWindow->window);
        window.x = aWindow->x;
        window.y = aWindow->y;
        window.width = aWindow->width;
        window.height = aWindow->height;
        window.type = aWindow->type;
    }
#else
    window.window = reinterpret_cast<unsigned long>(aWindow->window);
    window.x = aWindow->x;
    window.y = aWindow->y;
    window.width = aWindow->width;
    window.height = aWindow->height;
    window.clipRect = aWindow->clipRect; // MacOS specific
    window.type = aWindow->type;
#endif

#if defined(XP_MACOSX)
    if (mShWidth != window.width || mShHeight != window.height) {
        if (mDrawingModel == NPDrawingModelCoreAnimation || 
            mDrawingModel == NPDrawingModelInvalidatingCoreAnimation) {
            if (mIOSurface) {
                delete mIOSurface;
            }
            mIOSurface = nsIOSurface::CreateIOSurface(window.width, window.height);
        } else if (mShWidth * mShHeight != window.width * window.height) {
            if (mShWidth != 0 && mShHeight != 0) {
                DeallocShmem(mShSurface);
                mShWidth = 0;
                mShHeight = 0;
            }

            if (window.width != 0 && window.height != 0) {
                if (!AllocShmem(window.width * window.height*4, 
                                SharedMemory::TYPE_BASIC, &mShSurface)) {
                    PLUGIN_LOG_DEBUG(("Shared memory could not be allocated."));
                    return NPERR_GENERIC_ERROR;
                } 
            }
        }
        mShWidth = window.width;
        mShHeight = window.height;
    }
#endif

#if defined(MOZ_X11) && defined(XP_UNIX) && !defined(XP_MACOSX)
    const NPSetWindowCallbackStruct* ws_info =
      static_cast<NPSetWindowCallbackStruct*>(aWindow->ws_info);
    window.visualID = ws_info->visual ? ws_info->visual->visualid : None;
    window.colormap = ws_info->colormap;
#endif

    if (!CallNPP_SetWindow(window))
        return NPERR_GENERIC_ERROR;

    return NPERR_NO_ERROR;
}

NPError
PluginInstanceParent::NPP_GetValue(NPPVariable aVariable,
                                   void* _retval)
{
    switch (aVariable) {

#ifdef MOZ_X11
    case NPPVpluginNeedsXEmbed: {
        bool needsXEmbed;
        NPError rv;

        if (!CallNPP_GetValue_NPPVpluginNeedsXEmbed(&needsXEmbed, &rv)) {
            return NPERR_GENERIC_ERROR;
        }

        if (NPERR_NO_ERROR != rv) {
            return rv;
        }

        (*(NPBool*)_retval) = needsXEmbed;
        return NPERR_NO_ERROR;
    }
#endif

    case NPPVpluginScriptableNPObject: {
        PPluginScriptableObjectParent* actor;
        NPError rv;
        if (!CallNPP_GetValue_NPPVpluginScriptableNPObject(&actor, &rv)) {
            return NPERR_GENERIC_ERROR;
        }

        if (NPERR_NO_ERROR != rv) {
            return rv;
        }

        if (!actor) {
            NS_ERROR("NPPVpluginScriptableNPObject succeeded but null.");
            return NPERR_GENERIC_ERROR;
        }

        const NPNetscapeFuncs* npn = mParent->GetNetscapeFuncs();
        if (!npn) {
            NS_WARNING("No netscape functions?!");
            return NPERR_GENERIC_ERROR;
        }

        NPObject* object =
            static_cast<PluginScriptableObjectParent*>(actor)->GetObject(true);
        NS_ASSERTION(object, "This shouldn't ever be null!");

        (*(NPObject**)_retval) = npn->retainobject(object);
        return NPERR_NO_ERROR;
    }

    default:
        PR_LOG(gPluginLog, PR_LOG_WARNING,
               ("In PluginInstanceParent::NPP_GetValue: Unhandled NPPVariable %i (%s)",
                (int) aVariable, NPPVariableToString(aVariable)));
        return NPERR_GENERIC_ERROR;
    }
}

NPError
PluginInstanceParent::NPP_SetValue(NPNVariable variable, void* value)
{
    switch (variable) {
    case NPNVprivateModeBool:
        NPError result;
        if (!CallNPP_SetValue_NPNVprivateModeBool(*static_cast<NPBool*>(value),
                                                  &result))
            return NPERR_GENERIC_ERROR;

        return result;

    default:
        NS_ERROR("Unhandled NPNVariable in NPP_SetValue");
        PR_LOG(gPluginLog, PR_LOG_WARNING,
               ("In PluginInstanceParent::NPP_SetValue: Unhandled NPNVariable %i (%s)",
                (int) variable, NPNVariableToString(variable)));
        return NPERR_GENERIC_ERROR;
    }
}

int16_t
PluginInstanceParent::NPP_HandleEvent(void* event)
{
    PLUGIN_LOG_DEBUG_FUNCTION;

#if defined(XP_MACOSX)
    NPCocoaEvent* npevent = reinterpret_cast<NPCocoaEvent*>(event);
#else
    NPEvent* npevent = reinterpret_cast<NPEvent*>(event);
#endif
    NPRemoteEvent npremoteevent;
    npremoteevent.event = *npevent;
    int16_t handled = 0;

#if defined(OS_WIN)
    if (mWindowType == NPWindowTypeDrawable) {
        if (DoublePassRenderingEvent() == npevent->event) {
            CallPaint(npremoteevent, &handled);
            return handled;
        }

        switch (npevent->event) {
            case WM_PAINT:
            {
                RECT rect;
                SharedSurfaceBeforePaint(rect, npremoteevent);
                CallPaint(npremoteevent, &handled);
                SharedSurfaceAfterPaint(npevent);
                return handled;
            }
            break;

            case WM_KILLFOCUS:
            {
              // When the user selects fullscreen mode in Flash video players,
              // WM_KILLFOCUS will be delayed by deferred event processing:
              // WM_LBUTTONUP results in a call to CreateWindow within Flash,
              // which fires WM_KILLFOCUS. Delayed delivery causes Flash to
              // misinterpret the event, dropping back out of fullscreen. Trap
              // this event and drop it.
              PRUnichar szClass[26];
              HWND hwnd = GetForegroundWindow();
              if (hwnd && hwnd != mPluginHWND &&
                  GetClassNameW(hwnd, szClass,
                                sizeof(szClass)/sizeof(PRUnichar)) &&
                  !wcscmp(szClass, kFlashFullscreenClass)) {
                  return 0;
              }
            }
            break;

            case WM_WINDOWPOSCHANGED:
            {
                // We send this in nsObjectFrame just before painting
                SendWindowPosChanged(npremoteevent);
                // nsObjectFrame doesn't care whether we handle this
                // or not, just returning 1 for good hygiene
                return 1;
            }
            break;
        }
    }
#endif

#if defined(MOZ_X11)
    switch (npevent->type) {
    case GraphicsExpose:
        PLUGIN_LOG_DEBUG(("  schlepping drawable 0x%lx across the pipe\n",
                          npevent->xgraphicsexpose.drawable));
        // Make sure the X server has created the Drawable and completes any
        // drawing before the plugin draws on top.
        //
        // XSync() waits for the X server to complete.  Really this parent
        // process does not need to wait; the child is the process that needs
        // to wait.  A possibly-slightly-better alternative would be to send
        // an X event to the child that the child would wait for.
        XSync(DefaultXDisplay(), False);

        return CallPaint(npremoteevent, &handled) ? handled : 0;

    case ButtonPress:
        // Release any active pointer grab so that the plugin X client can
        // grab the pointer if it wishes.
        Display *dpy = DefaultXDisplay();
#  ifdef MOZ_WIDGET_GTK2
        // GDK attempts to (asynchronously) track whether there is an active
        // grab so ungrab through GDK.
        gdk_pointer_ungrab(npevent->xbutton.time);
#  else
        XUngrabPointer(dpy, npevent->xbutton.time);
#  endif
        // Wait for the ungrab to complete.
        XSync(dpy, False);
        break;
    }
#endif

#ifdef XP_MACOSX
    if (npevent->type == NPCocoaEventDrawRect) {
        if (mDrawingModel == NPDrawingModelCoreAnimation ||
            mDrawingModel == NPDrawingModelInvalidatingCoreAnimation) {
            if (!mIOSurface) {
                NS_ERROR("No IOSurface allocated.");
                return false;
            }
            if (!CallNPP_HandleEvent_IOSurface(npremoteevent, 
                                               mIOSurface->GetIOSurfaceID(), 
                                               &handled)) 
                return false; // no good way to handle errors here...

            CGContextRef cgContext = npevent->data.draw.context;
            if (!mShColorSpace) {
                mShColorSpace = CreateSystemColorSpace();
            }
            if (!mShColorSpace) {
                PLUGIN_LOG_DEBUG(("Could not allocate ColorSpace."));
                return false;
            } 
            nsCARenderer::DrawSurfaceToCGContext(cgContext, mIOSurface, 
                                                 mShColorSpace,
                                                 npevent->data.draw.x,
                                                 npevent->data.draw.y,
                                                 npevent->data.draw.width,
                                                 npevent->data.draw.height);
            return false;
        } else {
            if (mShWidth == 0 && mShHeight == 0) {
                PLUGIN_LOG_DEBUG(("NPCocoaEventDrawRect on window of size 0."));
                return false;
            }
            if (!mShSurface.IsReadable()) {
                PLUGIN_LOG_DEBUG(("Shmem is not readable."));
                return false;
            }

            if (!CallNPP_HandleEvent_Shmem(npremoteevent, mShSurface, 
                                           &handled, &mShSurface)) 
                return false; // no good way to handle errors here...

            if (!mShSurface.IsReadable()) {
                PLUGIN_LOG_DEBUG(("Shmem not returned. Either the plugin crashed "
                                  "or we have a bug."));
                return false;
            }

            char* shContextByte = mShSurface.get<char>();

            if (!mShColorSpace) {
                mShColorSpace = CreateSystemColorSpace();
            }
            if (!mShColorSpace) {
                PLUGIN_LOG_DEBUG(("Could not allocate ColorSpace."));
                return false;
            } 
            CGContextRef shContext = ::CGBitmapContextCreate(shContextByte, 
                                    mShWidth, mShHeight, 8, 
                                    mShWidth*4, mShColorSpace, 
                                    kCGImageAlphaPremultipliedFirst | 
                                    kCGBitmapByteOrder32Host);
            if (!shContext) {
                PLUGIN_LOG_DEBUG(("Could not allocate CGBitmapContext."));
                return false;
            }

            CGImageRef shImage = ::CGBitmapContextCreateImage(shContext);
            if (shImage) {
                CGContextRef cgContext = npevent->data.draw.context;
     
                ::CGContextDrawImage(cgContext, 
                                     CGRectMake(0,0,mShWidth,mShHeight), 
                                     shImage);
                ::CGImageRelease(shImage);
            } else {
                ::CGContextRelease(shContext);
                return false;
            }
            ::CGContextRelease(shContext);
            return true;
        }
    }
#endif

    if (!CallNPP_HandleEvent(npremoteevent, &handled))
        return 0; // no good way to handle errors here...

    return handled;
}

NPError
PluginInstanceParent::NPP_NewStream(NPMIMEType type, NPStream* stream,
                                    NPBool seekable, uint16_t* stype)
{
    PLUGIN_LOG_DEBUG(("%s (type=%s, stream=%p, seekable=%i)",
                      FULLFUNCTION, (char*) type, (void*) stream, (int) seekable));

    BrowserStreamParent* bs = new BrowserStreamParent(this, stream);

    NPError err;
    if (!CallPBrowserStreamConstructor(bs,
                                       NullableString(stream->url),
                                       stream->end,
                                       stream->lastmodified,
                                       static_cast<PStreamNotifyParent*>(stream->notifyData),
                                       NullableString(stream->headers),
                                       NullableString(type), seekable,
                                       &err, stype))
        return NPERR_GENERIC_ERROR;

    if (NPERR_NO_ERROR != err)
        unused << PBrowserStreamParent::Send__delete__(bs);

    return err;
}

NPError
PluginInstanceParent::NPP_DestroyStream(NPStream* stream, NPReason reason)
{
    PLUGIN_LOG_DEBUG(("%s (stream=%p, reason=%i)",
                      FULLFUNCTION, (void*) stream, (int) reason));

    AStream* s = static_cast<AStream*>(stream->pdata);
    if (s->IsBrowserStream()) {
        BrowserStreamParent* sp =
            static_cast<BrowserStreamParent*>(s);
        if (sp->mNPP != this)
            NS_RUNTIMEABORT("Mismatched plugin data");

        sp->NPP_DestroyStream(reason);
        return NPERR_NO_ERROR;
    }
    else {
        PluginStreamParent* sp =
            static_cast<PluginStreamParent*>(s);
        if (sp->mInstance != this)
            NS_RUNTIMEABORT("Mismatched plugin data");

        return PPluginStreamParent::Call__delete__(sp, reason, false) ?
            NPERR_NO_ERROR : NPERR_GENERIC_ERROR;
    }
}

void
PluginInstanceParent::NPP_Print(NPPrint* platformPrint)
{
    // TODO: implement me
    NS_ERROR("Not implemented");
}

PPluginScriptableObjectParent*
PluginInstanceParent::AllocPPluginScriptableObject()
{
    return new PluginScriptableObjectParent(Proxy);
}

#ifdef DEBUG
namespace {

struct ActorSearchData
{
    PluginScriptableObjectParent* actor;
    bool found;
};

PLDHashOperator
ActorSearch(const void* aKey,
            PluginScriptableObjectParent* aData,
            void* aUserData)
{
    ActorSearchData* asd = reinterpret_cast<ActorSearchData*>(aUserData);
    if (asd->actor == aData) {
        asd->found = true;
        return PL_DHASH_STOP;
    }
    return PL_DHASH_NEXT;
}

} // anonymous namespace
#endif // DEBUG

bool
PluginInstanceParent::DeallocPPluginScriptableObject(
                                         PPluginScriptableObjectParent* aObject)
{
    PluginScriptableObjectParent* actor =
        static_cast<PluginScriptableObjectParent*>(aObject);

    NPObject* object = actor->GetObject(false);
    if (object) {
        NS_ASSERTION(mScriptableObjects.Get(object, nsnull),
                     "NPObject not in the hash!");
        mScriptableObjects.Remove(object);
    }
#ifdef DEBUG
    else {
        ActorSearchData asd = { actor, false };
        mScriptableObjects.EnumerateRead(ActorSearch, &asd);
        NS_ASSERTION(!asd.found, "Actor in the hash with a null NPObject!");
    }
#endif

    delete actor;
    return true;
}

bool
PluginInstanceParent::RecvPPluginScriptableObjectConstructor(
                                          PPluginScriptableObjectParent* aActor)
{
    // This is only called in response to the child process requesting the
    // creation of an actor. This actor will represent an NPObject that is
    // created by the plugin and returned to the browser.
    PluginScriptableObjectParent* actor =
        static_cast<PluginScriptableObjectParent*>(aActor);
    NS_ASSERTION(!actor->GetObject(false), "Actor already has an object?!");

    actor->InitializeProxy();
    NS_ASSERTION(actor->GetObject(false), "Actor should have an object!");

    return true;
}

void
PluginInstanceParent::NPP_URLNotify(const char* url, NPReason reason,
                                    void* notifyData)
{
    PLUGIN_LOG_DEBUG(("%s (%s, %i, %p)",
                      FULLFUNCTION, url, (int) reason, notifyData));

    PStreamNotifyParent* streamNotify =
        static_cast<PStreamNotifyParent*>(notifyData);
    unused << PStreamNotifyParent::Send__delete__(streamNotify, reason);
}

bool
PluginInstanceParent::RegisterNPObjectForActor(
                                           NPObject* aObject,
                                           PluginScriptableObjectParent* aActor)
{
    NS_ASSERTION(aObject && aActor, "Null pointers!");
    NS_ASSERTION(mScriptableObjects.IsInitialized(), "Hash not initialized!");
    NS_ASSERTION(!mScriptableObjects.Get(aObject, nsnull), "Duplicate entry!");
    return !!mScriptableObjects.Put(aObject, aActor);
}

void
PluginInstanceParent::UnregisterNPObject(NPObject* aObject)
{
    NS_ASSERTION(aObject, "Null pointer!");
    NS_ASSERTION(mScriptableObjects.IsInitialized(), "Hash not initialized!");
    NS_ASSERTION(mScriptableObjects.Get(aObject, nsnull), "Unknown entry!");
    mScriptableObjects.Remove(aObject);
}

PluginScriptableObjectParent*
PluginInstanceParent::GetActorForNPObject(NPObject* aObject)
{
    NS_ASSERTION(aObject, "Null pointer!");

    if (aObject->_class == PluginScriptableObjectParent::GetClass()) {
        // One of ours!
        ParentNPObject* object = static_cast<ParentNPObject*>(aObject);
        NS_ASSERTION(object->parent, "Null actor!");
        return object->parent;
    }

    PluginScriptableObjectParent* actor;
    if (mScriptableObjects.Get(aObject, &actor)) {
        return actor;
    }

    actor = new PluginScriptableObjectParent(LocalObject);
    if (!actor) {
        NS_ERROR("Out of memory!");
        return nsnull;
    }

    if (!SendPPluginScriptableObjectConstructor(actor)) {
        NS_WARNING("Failed to send constructor message!");
        return nsnull;
    }

    actor->InitializeLocal(aObject);
    return actor;
}

bool
PluginInstanceParent::AnswerNPN_PushPopupsEnabledState(const bool& aState)
{
    mNPNIface->pushpopupsenabledstate(mNPP, aState ? 1 : 0);
    return true;
}

bool
PluginInstanceParent::AnswerNPN_PopPopupsEnabledState()
{
    mNPNIface->poppopupsenabledstate(mNPP);
    return true;
}

bool
PluginInstanceParent::AnswerNPN_GetValueForURL(const NPNURLVariable& variable,
                                               const nsCString& url,
                                               nsCString* value,
                                               NPError* result)
{
    char* v;
    uint32_t len;

    *result = mNPNIface->getvalueforurl(mNPP, (NPNURLVariable) variable,
                                        url.get(), &v, &len);
    if (NPERR_NO_ERROR == *result)
        value->Adopt(v, len);

    return true;
}

bool
PluginInstanceParent::AnswerNPN_SetValueForURL(const NPNURLVariable& variable,
                                               const nsCString& url,
                                               const nsCString& value,
                                               NPError* result)
{
    *result = mNPNIface->setvalueforurl(mNPP, (NPNURLVariable) variable,
                                        url.get(), value.get(),
                                        value.Length());
    return true;
}

bool
PluginInstanceParent::AnswerNPN_GetAuthenticationInfo(const nsCString& protocol,
                                                      const nsCString& host,
                                                      const int32_t& port,
                                                      const nsCString& scheme,
                                                      const nsCString& realm,
                                                      nsCString* username,
                                                      nsCString* password,
                                                      NPError* result)
{
    char* u;
    uint32_t ulen;
    char* p;
    uint32_t plen;

    *result = mNPNIface->getauthenticationinfo(mNPP, protocol.get(),
                                               host.get(), port,
                                               scheme.get(), realm.get(),
                                               &u, &ulen, &p, &plen);
    if (NPERR_NO_ERROR == *result) {
        username->Adopt(u, ulen);
        password->Adopt(p, plen);
    }
    return true;
}

bool
PluginInstanceParent::AnswerNPN_ConvertPoint(const double& sourceX,
                                             const bool&   ignoreDestX,
                                             const double& sourceY,
                                             const bool&   ignoreDestY,
                                             const NPCoordinateSpace& sourceSpace,
                                             const NPCoordinateSpace& destSpace,
                                             double *destX,
                                             double *destY,
                                             bool *result)
{
    *result = mNPNIface->convertpoint(mNPP, sourceX, sourceY, sourceSpace,
                                      ignoreDestX ? nsnull : destX,
                                      ignoreDestY ? nsnull : destY,
                                      destSpace);

    return true;
}

#if defined(OS_WIN)

/*
  plugin focus changes between processes

  focus from dom -> child:
    Focus manager calls on widget to set the focus on the window.
    We pick up the resulting wm_setfocus event here, and forward
    that over ipc to the child which calls set focus on itself. 

  focus from child -> focus manager:
    Child picks up the local wm_setfocus and sends it via ipc over
    here. We then post a custom event to widget/src/windows/nswindow
    which fires off a gui event letting the browser know.
*/

static const PRUnichar kPluginInstanceParentProperty[] =
                         L"PluginInstanceParentProperty";

// static
LRESULT CALLBACK
PluginInstanceParent::PluginWindowHookProc(HWND hWnd,
                                           UINT message,
                                           WPARAM wParam,
                                           LPARAM lParam)
{
    PluginInstanceParent* self = reinterpret_cast<PluginInstanceParent*>(
        ::GetPropW(hWnd, kPluginInstanceParentProperty));
    if (!self) {
        NS_NOTREACHED("PluginInstanceParent::PluginWindowHookProc null this ptr!");
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    NS_ASSERTION(self->mPluginHWND == hWnd, "Wrong window!");

    switch (message) {
        case WM_SETFOCUS:
        // Let the child plugin window know it should take focus.
        self->CallSetPluginFocus();
        break;

        case WM_CLOSE:
        self->UnsubclassPluginWindow();
        break;
    }

    return ::CallWindowProc(self->mPluginWndProc, hWnd, message, wParam,
                            lParam);
}

void
PluginInstanceParent::SubclassPluginWindow(HWND aWnd)
{
    NS_ASSERTION(!(mPluginHWND && aWnd != mPluginHWND),
      "PluginInstanceParent::SubclassPluginWindow hwnd is not our window!");

    if (!mPluginHWND) {
        mPluginHWND = aWnd;
        mPluginWndProc = 
            (WNDPROC)::SetWindowLongPtrA(mPluginHWND, GWLP_WNDPROC,
                         reinterpret_cast<LONG_PTR>(PluginWindowHookProc));
        bool bRes = ::SetPropW(mPluginHWND, kPluginInstanceParentProperty, this);
        NS_ASSERTION(mPluginWndProc,
          "PluginInstanceParent::SubclassPluginWindow failed to set subclass!");
        NS_ASSERTION(bRes,
          "PluginInstanceParent::SubclassPluginWindow failed to set prop!");
   }
}

void
PluginInstanceParent::UnsubclassPluginWindow()
{
    if (mPluginHWND && mPluginWndProc) {
        ::SetWindowLongPtrA(mPluginHWND, GWLP_WNDPROC,
                            reinterpret_cast<LONG_PTR>(mPluginWndProc));

        ::RemovePropW(mPluginHWND, kPluginInstanceParentProperty);

        mPluginWndProc = NULL;
        mPluginHWND = NULL;
    }
}

/* windowless drawing helpers */

/*
 * Origin info:
 *
 * windowless, offscreen:
 *
 * WM_WINDOWPOSCHANGED: origin is relative to container 
 * setwindow: origin is 0,0
 * WM_PAINT: origin is 0,0
 *
 * windowless, native:
 *
 * WM_WINDOWPOSCHANGED: origin is relative to container 
 * setwindow: origin is relative to container
 * WM_PAINT: origin is relative to container
 *
 * PluginInstanceParent:
 *
 * painting: mPluginPort (nsIntRect, saved in SetWindow)
 */

void
PluginInstanceParent::SharedSurfaceRelease()
{
    mSharedSurfaceDib.Close();
}

bool
PluginInstanceParent::SharedSurfaceSetWindow(const NPWindow* aWindow,
                                             NPRemoteWindow& aRemoteWindow)
{
    aRemoteWindow.window = nsnull;
    aRemoteWindow.x      = 0;
    aRemoteWindow.y      = 0;
    aRemoteWindow.width  = aWindow->width;
    aRemoteWindow.height = aWindow->height;
    aRemoteWindow.type   = aWindow->type;

    nsIntRect newPort(aWindow->x, aWindow->y, aWindow->width, aWindow->height);

    // save the the rect location within the browser window.
    mPluginPort = newPort;

    // move the port to our shared surface origin
    newPort.MoveTo(0,0);

    // check to see if we have the room in shared surface
    if (mSharedSurfaceDib.IsValid() && mSharedSize.Contains(newPort)) {
      // ok to paint
      aRemoteWindow.surfaceHandle = 0;
      return true;
    }

    // allocate a new shared surface
    SharedSurfaceRelease();
    if (NS_FAILED(mSharedSurfaceDib.Create(reinterpret_cast<HDC>(aWindow->window),
                                           newPort.width, newPort.height, 32)))
      return false;

    // save the new shared surface size we just allocated
    mSharedSize = newPort;

    base::SharedMemoryHandle handle;
    if (NS_FAILED(mSharedSurfaceDib.ShareToProcess(mParent->ChildProcessHandle(), &handle)))
      return false;

    aRemoteWindow.surfaceHandle = handle;

    return true;
}

void
PluginInstanceParent::SharedSurfaceBeforePaint(RECT& rect,
                                               NPRemoteEvent& npremoteevent)
{
    RECT* dr = (RECT*)npremoteevent.event.lParam;
    HDC parentHdc = (HDC)npremoteevent.event.wParam;

    nsIntRect dirtyRect(dr->left, dr->top, dr->right-dr->left, dr->bottom-dr->top);
    dirtyRect.MoveBy(-mPluginPort.x, -mPluginPort.y); // should always be smaller than dirtyRect

    ::BitBlt(mSharedSurfaceDib.GetHDC(),
             dirtyRect.x,
             dirtyRect.y,
             dirtyRect.width,
             dirtyRect.height,
             parentHdc,
             dr->left,
             dr->top,
             SRCCOPY);

    // setup the translated dirty rect we'll send to the child
    rect.left   = dirtyRect.x;
    rect.top    = dirtyRect.y;
    rect.right  = dirtyRect.x + dirtyRect.width;
    rect.bottom = dirtyRect.y + dirtyRect.height;

    npremoteevent.event.wParam = WPARAM(0);
    npremoteevent.event.lParam = LPARAM(&rect);
}

void
PluginInstanceParent::SharedSurfaceAfterPaint(NPEvent* npevent)
{
    RECT* dr = (RECT*)npevent->lParam;
    HDC parentHdc = (HDC)npevent->wParam;

    nsIntRect dirtyRect(dr->left, dr->top, dr->right-dr->left, dr->bottom-dr->top);
    dirtyRect.MoveBy(-mPluginPort.x, -mPluginPort.y);

    // src copy the shared dib into the parent surface we are handed.
    ::BitBlt(parentHdc,
             dr->left,
             dr->top,
             dirtyRect.width,
             dirtyRect.height,
             mSharedSurfaceDib.GetHDC(),
             dirtyRect.x,
             dirtyRect.y,
             SRCCOPY);
}

#endif // defined(OS_WIN)

bool
PluginInstanceParent::AnswerPluginFocusChange(const bool& gotFocus)
{
    PLUGIN_LOG_DEBUG(("%s", FULLFUNCTION));

    // Currently only in use on windows - an rpc event we receive from the
    // child when it's plugin window (or one of it's children) receives keyboard
    // focus. We forward the event down to widget so the dom/focus manager can
    // be updated.
#if defined(OS_WIN)
    ::SendMessage(mPluginHWND, gOOPPPluginFocusEvent, gotFocus ? 1 : 0, 0);
    return true;
#else
    NS_NOTREACHED("PluginInstanceParent::AnswerPluginFocusChange not implemented!");
    return false;
#endif
}

#ifdef OS_MACOSX
void
PluginInstanceParent::Invalidate()
{
    NPRect windowRect = {0, 0, mShWidth, mShHeight};
    RecvNPN_InvalidateRect(windowRect);
}
#endif
