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

#include "PluginInstanceChild.h"
#include "PluginModuleChild.h"
#include "BrowserStreamChild.h"
#include "PluginStreamChild.h"
#include "StreamNotifyChild.h"

#include "mozilla/ipc/SyncChannel.h"

using namespace mozilla::plugins;

#ifdef MOZ_WIDGET_GTK2

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include "gtk2xtbin.h"

#elif defined(OS_WIN)
using mozilla::gfx::SharedDIB;

#include <windows.h>

#endif

PluginInstanceChild::PluginInstanceChild(const NPPluginFuncs* aPluginIface) :
        mPluginIface(aPluginIface)
#if defined(OS_WIN)
        , mPluginWindowHWND(0)
        , mPluginWndProc(0)
        , mPluginParentHWND(0)
#endif
    {
        memset(&mWindow, 0, sizeof(mWindow));
        mData.ndata = (void*) this;
#if defined(MOZ_X11) && defined(XP_UNIX) && !defined(XP_MACOSX)
        mWindow.ws_info = &mWsInfo;
        memset(&mWsInfo, 0, sizeof(mWsInfo));
#  ifdef MOZ_WIDGET_GTK2
        mWsInfo.display = GDK_DISPLAY();
#  endif
#endif
    }

PluginInstanceChild::~PluginInstanceChild()
{
#if defined(OS_WIN)
  DestroyPluginWindow();
#endif
}

NPError
PluginInstanceChild::NPN_GetValue(NPNVariable aVar,
                                  void* aValue)
{
    printf ("[PluginInstanceChild] NPN_GetValue(%s)\n",
            NPNVariableToString(aVar));
    AssertPluginThread();

    switch(aVar) {

    case NPNVSupportsWindowless:
#if defined(OS_LINUX) || defined(OS_WIN)
        *((NPBool*)aValue) = true;
#else
        *((NPBool*)aValue) = false;
#endif
        return NPERR_NO_ERROR;

#if defined(OS_LINUX)
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

    case NPNVWindowNPObject: {
        PPluginScriptableObjectChild* actor;
        NPError result;
        if (!CallNPN_GetValue_NPNVWindowNPObject(&actor, &result)) {
            NS_WARNING("Failed to send message!");
            return NPERR_GENERIC_ERROR;
        }

        if (result != NPERR_NO_ERROR) {
            return result;
        }

        NS_ASSERTION(actor, "Null actor!");

        NPObject* object =
            static_cast<PluginScriptableObjectChild*>(actor)->GetObject();
        NS_ASSERTION(object, "Null object?!");

        PluginModuleChild::sBrowserFuncs.retainobject(object);
        *((NPObject**)aValue) = object;
        return NPERR_NO_ERROR;
    }

    case NPNVPluginElementNPObject: {
        PPluginScriptableObjectChild* actor;
        NPError result;
        if (!CallNPN_GetValue_NPNVPluginElementNPObject(&actor, &result)) {
            NS_WARNING("Failed to send message!");
            return NPERR_GENERIC_ERROR;
        }

        if (result != NPERR_NO_ERROR) {
            return result;
        }

        NS_ASSERTION(actor, "Null actor!");

        NPObject* object =
            static_cast<PluginScriptableObjectChild*>(actor)->GetObject();
        NS_ASSERTION(object, "Null object?!");

        PluginModuleChild::sBrowserFuncs.retainobject(object);
        *((NPObject**)aValue) = object;
        return NPERR_NO_ERROR;
    }

    default:
        printf("  unhandled var %s\n", NPNVariableToString(aVar));
        return NPERR_GENERIC_ERROR;
    }

}


NPError
PluginInstanceChild::NPN_SetValue(NPPVariable aVar, void* aValue)
{
    printf ("[PluginInstanceChild] NPN_SetValue(%s, %ld)\n",
            NPPVariableToString(aVar), reinterpret_cast<intptr_t>(aValue));
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
        bool transparent = (NPBool) (intptr_t) aValue;

        if (!CallNPN_SetValue_NPPVpluginTransparent(transparent, &rv))
            return NPERR_GENERIC_ERROR;

        return rv;
    }

    default:
        printf("  unhandled var %s\n", NPPVariableToString(aVar));
        return NPERR_GENERIC_ERROR;
    }
}


bool
PluginInstanceChild::AnswerNPP_GetValue_NPPVpluginWindow(
    bool* windowed, NPError* rv)
{
    AssertPluginThread();

    NPBool isWindowed;
    *rv = mPluginIface->getvalue(GetNPP(), NPPVpluginWindowBool,
                                 reinterpret_cast<void*>(&isWindowed));
    *windowed = isWindowed;
    return true;
}

bool
PluginInstanceChild::AnswerNPP_GetValue_NPPVpluginTransparent(
    bool* transparent, NPError* rv)
{
    AssertPluginThread();

    NPBool isTransparent;
    *rv = mPluginIface->getvalue(GetNPP(), NPPVpluginTransparentBool,
                                 reinterpret_cast<void*>(&isTransparent));
    *transparent = isTransparent;
    return true;
}

bool
PluginInstanceChild::AnswerNPP_GetValue_NPPVpluginNeedsXEmbed(
    bool* needs, NPError* rv)
{
    AssertPluginThread();

#ifdef OS_LINUX

    NPBool needsXEmbed;
    *rv = mPluginIface->getvalue(GetNPP(), NPPVpluginNeedsXEmbed,
                                 reinterpret_cast<void*>(&needsXEmbed));
    *needs = needsXEmbed;
    return true;

#else

    NS_RUNTIMEABORT("shouldn't be called on non-linux platforms");
    return false;               // not reached

#endif
}

bool
PluginInstanceChild::AnswerNPP_GetValue_NPPVpluginScriptableNPObject(
                                          PPluginScriptableObjectChild** aValue,
                                          NPError* aResult)
{
    AssertPluginThread();

    NPObject* object;
    NPError result = mPluginIface->getvalue(GetNPP(),
                                            NPPVpluginScriptableNPObject,
                                            &object);
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

    *aValue = nsnull;
    *aResult = result;
    return true;
}

bool
PluginInstanceChild::AnswerNPP_HandleEvent(const NPRemoteEvent& event,
                                           int16_t* handled)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

#if defined(OS_LINUX) && defined(DEBUG)
    if (GraphicsExpose == event.event.type)
        printf("  received drawable 0x%lx\n",
               event.event.xgraphicsexpose.drawable);
#endif

    // Make a copy since we may modify values.
    NPEvent evcopy = event.event;

#ifdef OS_WIN
    // Setup the shared dib for painting and update evcopy.
    if (NPWindowTypeDrawable == mWindow.type && WM_PAINT == evcopy.event)
        SharedSurfaceBeforePaint(evcopy);
#endif

    *handled = mPluginIface->event(&mData, reinterpret_cast<void*>(&evcopy));

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
PluginInstanceChild::AnswerNPP_SetWindow(const NPRemoteWindow& aWindow,
                                         NPError* rv)
{
    printf("[PluginInstanceChild] NPP_SetWindow(0x%lx, %d, %d, %d x %d)\n",
           aWindow.window,
           aWindow.x, aWindow.y,
           aWindow.width, aWindow.height);
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

    *rv = mPluginIface->setwindow(&mData, &mWindow);

#elif defined(OS_WIN)
    switch (aWindow.type) {
      case NPWindowTypeWindow:
      {
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

          *rv = mPluginIface->setwindow(&mData, &mWindow);
          if (*rv == NPERR_NO_ERROR) {
              WNDPROC wndProc = reinterpret_cast<WNDPROC>(
                  GetWindowLongPtr(mPluginWindowHWND, GWLP_WNDPROC));
              if (wndProc != PluginWindowProc) {
                  mPluginWndProc = reinterpret_cast<WNDPROC>(
                      SetWindowLongPtr(mPluginWindowHWND, GWLP_WNDPROC,
                                       reinterpret_cast<LONG>(PluginWindowProc)));
              }
          }
      }
      break;

      case NPWindowTypeDrawable:
          return SharedSurfaceSetWindow(aWindow, rv);
      break;

      default:
          NS_NOTREACHED("Bad plugin window type.");
          return false;
      break;
    }

#elif defined(OS_MACOSX)
#  warning This is only a stub implementation IMPLEMENT ME

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

void
PluginInstanceChild::Destroy()
{
    // Copy the actors here so we don't enumerate a mutating array.
    nsAutoTArray<PluginScriptableObjectChild*, 10> objects;
    PRUint32 count = mScriptableObjects.Length();
    for (PRUint32 index = 0; index < count; index++) {
        objects.AppendElement(mScriptableObjects[index]);
    }

    count = objects.Length();
    for (PRUint32 index = 0; index < count; index++) {
        PluginScriptableObjectChild*& actor = objects[index];
        NPObject* object = actor->GetObject();
        if (object->_class == PluginScriptableObjectChild::GetClass()) {
          PluginScriptableObjectChild::ScriptableInvalidate(object);
        }
    }

#if defined(OS_WIN)
    SharedSurfaceRelease();
#endif
}

#if defined(OS_WIN)

static const TCHAR kWindowClassName[] = TEXT("GeckoPluginWindow");
static const TCHAR kPluginInstanceChildProperty[] = TEXT("PluginInstanceChildProperty");

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
                      reinterpret_cast<LONG>(DefWindowProcA));

    return true;
}

void
PluginInstanceChild::DestroyPluginWindow()
{
    if (mPluginWindowHWND) {
        // Unsubclass the window.
        WNDPROC wndProc = reinterpret_cast<WNDPROC>(
            GetWindowLongPtr(mPluginWindowHWND, GWLP_WNDPROC));
        if (wndProc == PluginWindowProc) {
            NS_ASSERTION(mPluginWndProc, "Should have old proc here!");
            SetWindowLongPtr(mPluginWindowHWND, GWLP_WNDPROC,
                             reinterpret_cast<LONG>(mPluginWndProc));
            mPluginWndProc = 0;
        }

        RemoveProp(mPluginWindowHWND, kPluginInstanceChildProperty);
        DestroyWindow(mPluginWindowHWND);
        mPluginWindowHWND = 0;
    }
}

void
PluginInstanceChild::ReparentPluginWindow(HWND hWndParent)
{
    if (hWndParent != mPluginParentHWND && IsWindow(hWndParent)) {
        // Fix the child window's style to be a child window.
        LONG style = GetWindowLongPtr(mPluginWindowHWND, GWL_STYLE);
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
    NS_ASSERTION(!mozilla::ipc::SyncChannel::IsPumpingMessages(),
                 "Failed to prevent a nonqueued message from running!");

    PluginInstanceChild* self = reinterpret_cast<PluginInstanceChild*>(
        GetProp(hWnd, kPluginInstanceChildProperty));
    if (!self) {
        NS_NOTREACHED("Badness!");
        return 0;
    }

    NS_ASSERTION(self->mPluginWindowHWND == hWnd, "Wrong window!");

    LRESULT res = CallWindowProc(self->mPluginWndProc, hWnd, message, wParam,
                                 lParam);

    if (message == WM_CLOSE)
        self->DestroyPluginWindow();

    if (message == WM_NCDESTROY)
        RemoveProp(hWnd, kPluginInstanceChildProperty);

    return res;
}

/* windowless drawing helpers */

bool
PluginInstanceChild::SharedSurfaceSetWindow(const NPRemoteWindow& aWindow,
                                            NPError* rv)
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
                                               aWindow.width, aWindow.height, 32)))
          return false;
    }
      
    // NPRemoteWindow's origin is the origin of our shared dib.
    mWindow.x      = 0;
    mWindow.y      = 0;
    mWindow.width  = aWindow.width;
    mWindow.height = aWindow.height;
    mWindow.type   = aWindow.type;

    mWindow.window = reinterpret_cast<void*>(mSharedSurfaceDib.GetHDC());
    *rv = mPluginIface->setwindow(&mData, &mWindow);

    return true;
}

void
PluginInstanceChild::SharedSurfaceRelease()
{
    mSharedSurfaceDib.Close();
}

void
PluginInstanceChild::SharedSurfaceBeforePaint(NPEvent& evcopy)
{
    // Update the clip rect on our internal hdc
    RECT* pRect = reinterpret_cast<RECT*>(evcopy.lParam);
    if (pRect) {
      HRGN clip = ::CreateRectRgnIndirect(pRect);
      ::SelectClipRgn(mSharedSurfaceDib.GetHDC(), clip);
      ::DeleteObject(clip);
    }
    // pass the internal hdc to the plugin
    evcopy.wParam = WPARAM(mSharedSurfaceDib.GetHDC());
}

#endif // OS_WIN

PPluginScriptableObjectChild*
PluginInstanceChild::AllocPPluginScriptableObject()
{
    AssertPluginThread();

    nsAutoPtr<PluginScriptableObjectChild>* object =
        mScriptableObjects.AppendElement();
    NS_ENSURE_TRUE(object, nsnull);

    *object = new PluginScriptableObjectChild();
    NS_ENSURE_TRUE(*object, nsnull);

    return object->get();
}

bool
PluginInstanceChild::DeallocPPluginScriptableObject(
                                          PPluginScriptableObjectChild* aObject)
{
    AssertPluginThread();

    PluginScriptableObjectChild* object =
        reinterpret_cast<PluginScriptableObjectChild*>(aObject);

    NPObject* npobject = object->GetObject();
    if (npobject &&
        npobject->_class != PluginScriptableObjectChild::GetClass()) {
        PluginModuleChild::current()->UnregisterNPObject(npobject);
    }

    PRUint32 count = mScriptableObjects.Length();
    for (PRUint32 index = 0; index < count; index++) {
        if (mScriptableObjects[index] == object) {
            mScriptableObjects.RemoveElementAt(index);
            return true;
        }
    }
    NS_NOTREACHED("An actor we don't know about?!");
    return false;
}

bool
PluginInstanceChild::AnswerPPluginScriptableObjectConstructor(
                                           PPluginScriptableObjectChild* aActor)
{
    AssertPluginThread();

    // This is only called in response to the parent process requesting the
    // creation of an actor. This actor will represent an NPObject that is
    // created by the browser and returned to the plugin.
    NPClass* npclass =
        const_cast<NPClass*>(PluginScriptableObjectChild::GetClass());

    ChildNPObject* object = reinterpret_cast<ChildNPObject*>(
        PluginModuleChild::sBrowserFuncs.createobject(GetNPP(), npclass));
    if (!object) {
        NS_WARNING("Failed to create NPObject!");
        return false;
    }

    PluginScriptableObjectChild* actor =
        static_cast<PluginScriptableObjectChild*>(aActor);
    actor->Initialize(const_cast<PluginInstanceChild*>(this), object);

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
    return new BrowserStreamChild(this, url, length, lastmodified, notifyData,
                                  headers, mimeType, seekable, rv, stype);
}

bool
PluginInstanceChild::AnswerPBrowserStreamDestructor(PBrowserStreamChild* stream,
                                                    const NPError& reason,
                                                    const bool& artificial)
{
    AssertPluginThread();
    if (!artificial)
        static_cast<BrowserStreamChild*>(stream)->NPP_DestroyStream(reason);
    return true;
}

bool
PluginInstanceChild::DeallocPBrowserStream(PBrowserStreamChild* stream,
                                           const NPError& reason,
                                           const bool& artificial)
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
PluginInstanceChild::AnswerPPluginStreamDestructor(PPluginStreamChild* stream,
                                                   const NPReason& reason,
                                                   const bool& artificial)
{
    AssertPluginThread();
    if (!artificial) {
        static_cast<PluginStreamChild*>(stream)->NPP_DestroyStream(reason);
    }
    return true;
}

bool
PluginInstanceChild::DeallocPPluginStream(PPluginStreamChild* stream,
                                          const NPError& reason,
                                          const bool& artificial)
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

bool
PluginInstanceChild::AnswerPStreamNotifyDestructor(PStreamNotifyChild* notifyData,
                                                   const NPReason& reason)
{
    AssertPluginThread();

    StreamNotifyChild* sn = static_cast<StreamNotifyChild*>(notifyData);
    if (sn->mClosure)
        mPluginIface->urlnotify(&mData, sn->mURL.get(), reason, sn->mClosure);

    return true;
}

bool
PluginInstanceChild::DeallocPStreamNotify(PStreamNotifyChild* notifyData,
                                          const NPReason& reason)
{
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

  actor = reinterpret_cast<PluginScriptableObjectChild*>(
      CallPPluginScriptableObjectConstructor());
  NS_ENSURE_TRUE(actor, nsnull);

  actor->Initialize(this, aObject);

#ifdef DEBUG
  bool ok =
#endif
  PluginModuleChild::current()->RegisterNPObject(aObject, actor);
  NS_ASSERTION(ok, "Out of memory?");

  return actor;
}

NPError
PluginInstanceChild::NPN_NewStream(NPMIMEType aMIMEType, const char* aWindow,
                                   NPStream** aStream)
{
    AssertPluginThread();

    PluginStreamChild* ps = new PluginStreamChild(this);

    NPError result;
    CallPPluginStreamConstructor(ps, nsDependentCString(aMIMEType),
                                 NullableString(aWindow), &result);
    if (NPERR_NO_ERROR != result) {
        *aStream = NULL;
        CallPPluginStreamDestructor(ps, NPERR_GENERIC_ERROR, true);
        return result;
    }

    *aStream = &ps->mStream;
    return NPERR_NO_ERROR;
}

bool
PluginInstanceChild::InternalInvalidateRect(NPRect* aInvalidRect)
{
    NS_ASSERTION(aInvalidRect, "Null pointer!");

#ifdef OS_WIN
    // Invalidate and draw locally for windowed plugins.
    if (mWindow.type == NPWindowTypeWindow) {
      NS_ASSERTION(IsWindow(mPluginWindowHWND), "Bad window?!");
      RECT rect = { aInvalidRect->left, aInvalidRect->top,
                    aInvalidRect->right, aInvalidRect->bottom };
      InvalidateRect(mPluginWindowHWND, &rect, FALSE);
      return false;
    }
    // Windowless need the invalidation to propegate to parent
    // triggering wm_paint handle event calls.
    return true;
#endif

    // Windowless plugins must return true!
    return false;
}
