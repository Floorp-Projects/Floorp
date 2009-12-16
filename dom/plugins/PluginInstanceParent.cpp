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

#if defined(OS_WIN)
#include <windowsx.h>
#endif

using namespace mozilla::plugins;

PluginInstanceParent::PluginInstanceParent(PluginModuleParent* parent,
                                           NPP npp,
                                           const NPNetscapeFuncs* npniface)
  : mParent(parent),
    mNPP(npp),
    mNPNIface(npniface),
    mWindowType(NPWindowTypeWindow)
{
}

PluginInstanceParent::~PluginInstanceParent()
{
    if (mNPP)
        mNPP->pdata = NULL;
}

void
PluginInstanceParent::Destroy()
{
    // Copy the actors here so we don't enumerate a mutating array.
    nsAutoTArray<PluginScriptableObjectParent*, 10> objects;
    PRUint32 count = mScriptableObjects.Length();
    for (PRUint32 index = 0; index < count; index++) {
        objects.AppendElement(mScriptableObjects[index]);
    }

    count = objects.Length();
    for (PRUint32 index = 0; index < count; index++) {
        NPObject* object = objects[index]->GetObject();
        if (object->_class == PluginScriptableObjectParent::GetClass()) {
          PluginScriptableObjectParent::ScriptableInvalidate(object);
        }
    }

#if defined(OS_WIN)
    SharedSurfaceRelease();
#endif
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

    if (*result != NPERR_NO_ERROR)
        PStreamNotifyParent::Call__delete__(actor, NPERR_GENERIC_ERROR);

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

NPError
PluginInstanceParent::NPP_SetWindow(const NPWindow* aWindow)
{
    _MOZ_LOG(__FUNCTION__);
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

#if defined(MOZ_X11) && defined(XP_UNIX) && !defined(XP_MACOSX)
    const NPSetWindowCallbackStruct* ws_info =
      static_cast<NPSetWindowCallbackStruct*>(aWindow->ws_info);
    window.visualID = ws_info->visual ? ws_info->visual->visualid : None;
    window.colormap = ws_info->colormap;
#endif

    NPError prv;
    if (!CallNPP_SetWindow(window, &prv))
        return NPERR_GENERIC_ERROR;
    return prv;
}

NPError
PluginInstanceParent::NPP_GetValue(NPPVariable aVariable,
                                   void* _retval)
{
    printf("[PluginInstanceParent] NPP_GetValue(%s)\n",
           NPPVariableToString(aVariable));

    switch (aVariable) {

    case NPPVpluginWindowBool: {
        bool windowed;
        NPError rv;

        if (!CallNPP_GetValue_NPPVpluginWindow(&windowed, &rv)) {
            return NPERR_GENERIC_ERROR;
        }

        if (NPERR_NO_ERROR != rv) {
            return rv;
        }

        (*(NPBool*)_retval) = windowed;
        return NPERR_NO_ERROR;
    }

    case NPPVpluginTransparentBool: {
        bool transparent;
        NPError rv;

        if (!CallNPP_GetValue_NPPVpluginTransparent(&transparent, &rv)) {
            return NPERR_GENERIC_ERROR;
        }

        if (NPERR_NO_ERROR != rv) {
            return rv;
        }

        (*(NPBool*)_retval) = transparent;
        return NPERR_NO_ERROR;
    }

#ifdef OS_LINUX
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
            static_cast<PluginScriptableObjectParent*>(actor)->GetObject();
        NS_ASSERTION(object, "This shouldn't ever be null!");

        (*(NPObject**)_retval) = npn->retainobject(object);
        return NPERR_NO_ERROR;
    }

    default:
        printf("  unhandled var %s\n", NPPVariableToString(aVariable));
        return NPERR_GENERIC_ERROR;
    }
}

int16_t
PluginInstanceParent::NPP_HandleEvent(void* event)
{
    _MOZ_LOG(__FUNCTION__);

    NPEvent* npevent = reinterpret_cast<NPEvent*>(event);
    NPRemoteEvent npremoteevent;
    npremoteevent.event = *npevent;
    int16_t handled;

#if defined(OS_WIN)
    RECT rect;
    if (mWindowType == NPWindowTypeDrawable) {
        switch(npevent->event) {
            case WM_PAINT:
            {
                RECT rect;
                SharedSurfaceBeforePaint(rect, npremoteevent);
                if (!CallNPP_HandleEvent(npremoteevent, &handled))
                    return 0;
                if (handled)
                    SharedSurfaceAfterPaint(npevent);
            }
            break;
            case WM_WINDOWPOSCHANGED:
                SharedSurfaceSetOrigin(npremoteevent);
                if (!CallNPP_HandleEvent(npremoteevent, &handled))
                    return 0;
            break;
            case WM_MOUSEMOVE:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_MBUTTONUP:
            case WM_RBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case WM_MBUTTONDBLCLK:
            case WM_RBUTTONDBLCLK:
            {
                // Received mouse events have an origin at the position of the plugin rect
                // in the page. However, when rendering to the shared dib, the rect origin
                // changes to 0,0 via the WM_WINDOWPOSCHANGED event. In this case we need to
                // translate these coords to the proper location.
                nsPoint pt(GET_X_LPARAM(npremoteevent.event.lParam), GET_Y_LPARAM(npremoteevent.event.lParam));
                pt.MoveBy(-mPluginPosOrigin.x, -mPluginPosOrigin.y);
                npremoteevent.event.lParam = MAKELPARAM(pt.x, pt.y);
                if (!CallNPP_HandleEvent(npremoteevent, &handled))
                    return 0;
            }
            default:
                if (!CallNPP_HandleEvent(npremoteevent, &handled))
                    return 0;
            break;
        }
    }
    else {
        if (!CallNPP_HandleEvent(npremoteevent, &handled))
            return 0;
    }
#endif

#if defined(MOZ_X11)
    if (GraphicsExpose == npevent->type) {
        printf("  schlepping drawable 0x%lx across the pipe\n",
               npevent->xgraphicsexpose.drawable);
        // Make sure the X server has created the Drawable and completes any
        // drawing before the plugin draws on top.
        //
        // XSync() waits for the X server to complete.  Really this parent
        // process does not need to wait; the child is the process that needs
        // to wait.  A possibly-slightly-better alternative would be to send
        // an X event to the child that the child would wait for.
#  ifdef MOZ_WIDGET_GTK2
        XSync(GDK_DISPLAY(), False);
#  endif
    }

    if (!CallNPP_HandleEvent(npremoteevent, &handled))
        return 0; // no good way to handle errors here...
#endif

    return handled;
}

NPError
PluginInstanceParent::NPP_NewStream(NPMIMEType type, NPStream* stream,
                                    NPBool seekable, uint16_t* stype)
{
    _MOZ_LOG(__FUNCTION__);

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
        PBrowserStreamParent::Call__delete__(bs, NPERR_GENERIC_ERROR, true);

    return err;
}

NPError
PluginInstanceParent::NPP_DestroyStream(NPStream* stream, NPReason reason)
{
    _MOZ_LOG(__FUNCTION__);

    AStream* s = static_cast<AStream*>(stream->pdata);
    if (s->IsBrowserStream()) {
        BrowserStreamParent* sp =
            static_cast<BrowserStreamParent*>(s);
        if (sp->mNPP != this)
            NS_RUNTIMEABORT("Mismatched plugin data");

        PBrowserStreamParent::Call__delete__(sp, reason, false);
        return NPERR_NO_ERROR;
    }
    else {
        PluginStreamParent* sp =
            static_cast<PluginStreamParent*>(s);
        if (sp->mInstance != this)
            NS_RUNTIMEABORT("Mismatched plugin data");

        PPluginStreamParent::Call__delete__(sp, reason, false);
        return NPERR_NO_ERROR;
    }
}

PPluginScriptableObjectParent*
PluginInstanceParent::AllocPPluginScriptableObject()
{
    nsAutoPtr<PluginScriptableObjectParent>* object =
        mScriptableObjects.AppendElement();
    NS_ENSURE_TRUE(object, nsnull);

    *object = new PluginScriptableObjectParent();
    NS_ENSURE_TRUE(*object, nsnull);

    return object->get();
}

bool
PluginInstanceParent::DeallocPPluginScriptableObject(
                                         PPluginScriptableObjectParent* aObject)
{
    PluginScriptableObjectParent* object =
        reinterpret_cast<PluginScriptableObjectParent*>(aObject);

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
PluginInstanceParent::AnswerPPluginScriptableObjectConstructor(
                                          PPluginScriptableObjectParent* aActor)
{
    // This is only called in response to the child process requesting the
    // creation of an actor. This actor will represent an NPObject that is
    // created by the plugin and returned to the browser.
    const NPNetscapeFuncs* npn = mParent->GetNetscapeFuncs();
    if (!npn) {
        NS_WARNING("No netscape function pointers?!");
        return false;
    }

    NPClass* npclass =
        const_cast<NPClass*>(PluginScriptableObjectParent::GetClass());

    ParentNPObject* object = reinterpret_cast<ParentNPObject*>(
        npn->createobject(mNPP, npclass));
    if (!object) {
        NS_WARNING("Failed to create NPObject!");
        return false;
    }

    static_cast<PluginScriptableObjectParent*>(aActor)->Initialize(
        const_cast<PluginInstanceParent*>(this), object);
    return true;
}

void
PluginInstanceParent::NPP_URLNotify(const char* url, NPReason reason,
                                    void* notifyData)
{
    _MOZ_LOG(__FUNCTION__);

    PStreamNotifyParent* streamNotify =
        static_cast<PStreamNotifyParent*>(notifyData);
    PStreamNotifyParent::Call__delete__(streamNotify, reason);
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

    PRUint32 count = mScriptableObjects.Length();
    for (PRUint32 index = 0; index < count; index++) {
        nsAutoPtr<PluginScriptableObjectParent>& actor =
            mScriptableObjects[index];
        if (actor->GetObject() == aObject) {
            return actor;
        }
    }

    PluginScriptableObjectParent* actor =
        static_cast<PluginScriptableObjectParent*>(
            CallPPluginScriptableObjectConstructor());
    NS_ENSURE_TRUE(actor, nsnull);

    actor->Initialize(const_cast<PluginInstanceParent*>(this), aObject);
    return actor;
}

bool
PluginInstanceParent::AnswerNPN_PushPopupsEnabledState(const bool& aState,
                                                       bool* aSuccess)
{
    *aSuccess = mNPNIface->pushpopupsenabledstate(mNPP, aState ? 1 : 0);
    return true;
}

bool
PluginInstanceParent::AnswerNPN_PopPopupsEnabledState(bool* aSuccess)
{
    *aSuccess = mNPNIface->poppopupsenabledstate(mNPP);
    return true;
}

#if defined(OS_WIN)

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
 * event translation: mPluginPosOrigin (nsIntPoint, saved in SetOrigin)
 */

void
PluginInstanceParent::SharedSurfaceSetOrigin(NPRemoteEvent& npremoteevent)
{
    WINDOWPOS* winpos = (WINDOWPOS*)npremoteevent.event.lParam;

    // save the origin, we'll use this to translate input coordinates
    mPluginPosOrigin.x = winpos->x;
    mPluginPosOrigin.y = winpos->y;

    // Reset to the offscreen dib origin
    winpos->x  = 0;
    winpos->y  = 0;
}

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
