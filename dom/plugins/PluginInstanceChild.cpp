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

using namespace mozilla::plugins;

#ifdef MOZ_WIDGET_GTK2

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include "gtk2xtbin.h"

#elif defined(OS_WIN)

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
                                           PPluginScriptableObjectChild** value,
                                           NPError* result)
{
    AssertPluginThread();

    NPObject* object;
    *result = mPluginIface->getvalue(GetNPP(), NPPVpluginScriptableNPObject,
                                     &object);
    if (*result != NPERR_NO_ERROR) {
        return true;
    }

    PluginScriptableObjectChild* actor = GetActorForNPObject(object);

    // If we get an actor then it has retained. Otherwise we don't need it any
    // longer.
    PluginModuleChild::sBrowserFuncs.releaseobject(object);

    if (!actor) {
        *result = NPERR_GENERIC_ERROR;
        return true;
    }

    *value = actor;
    return true;
}

bool
PluginInstanceChild::AnswerNPP_HandleEvent(const NPRemoteEvent& event,
                                           int16_t* handled)
{
    _MOZ_LOG(__FUNCTION__);
    AssertPluginThread();

#if defined(OS_LINUX) && defined(DEBUG_cjones)
    if (GraphicsExpose == event.type)
        printf("  received drawable 0x%lx\n",
               event.xgraphicsexpose.drawable);
#endif

    // plugins might be fooling with these, make a copy
    NPEvent evcopy = event.event;
    *handled = mPluginIface->event(&mData, reinterpret_cast<void*>(&evcopy));

#ifdef MOZ_X11
    if (GraphicsExpose == event.type) {
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

#else
#  error Implement me for your OS
#endif

    return true;
}

bool
PluginInstanceChild::Initialize()
{
#if defined(OS_WIN)
    if (!CreatePluginWindow())
        return false;
#endif

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
    if (!RegisterWindowClass())
        return false;

    if (!mPluginWindowHWND) {
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
    }

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
        LONG style = GetWindowLongPtr(mPluginWindowHWND, GWL_STYLE);
        style |= WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
        SetWindowLongPtr(mPluginWindowHWND, GWL_STYLE, style);
        SetParent(mPluginWindowHWND, hWndParent);
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
PluginInstanceChild::DeallocPStreamNotify(PStreamNotifyChild* notifyData,
                                          const NPReason& reason)
{
    AssertPluginThread();

    StreamNotifyChild* sn = static_cast<StreamNotifyChild*>(notifyData);
    mPluginIface->urlnotify(&mData, sn->mURL.get(), reason, sn->mClosure);
    delete sn;

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
