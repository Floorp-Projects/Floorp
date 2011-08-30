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

#ifndef dom_plugins_PluginInstanceParent_h
#define dom_plugins_PluginInstanceParent_h 1

#include "mozilla/plugins/PPluginInstanceParent.h"
#include "mozilla/plugins/PluginScriptableObjectParent.h"
#if defined(OS_WIN)
#include "mozilla/gfx/SharedDIBWin.h"
#elif defined(MOZ_WIDGET_COCOA)
#include "nsCoreAnimationSupport.h"
#endif

#include "npfunctions.h"
#include "nsAutoPtr.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsRect.h"
#include "gfxASurface.h"
#include "ImageLayers.h"
#ifdef MOZ_X11
class gfxXlibSurface;
#endif
#include "nsGUIEvent.h"

namespace mozilla {
namespace plugins {

class PBrowserStreamParent;
class PluginModuleParent;

class PluginInstanceParent : public PPluginInstanceParent
{
    friend class PluginModuleParent;
    friend class BrowserStreamParent;
    friend class PluginStreamParent;
    friend class StreamNotifyParent;

public:
    PluginInstanceParent(PluginModuleParent* parent,
                         NPP npp,
                         const nsCString& mimeType,
                         const NPNetscapeFuncs* npniface);

    virtual ~PluginInstanceParent();

    bool Init();
    NPError Destroy();

    NS_OVERRIDE virtual void ActorDestroy(ActorDestroyReason why);

    virtual PPluginScriptableObjectParent*
    AllocPPluginScriptableObject();

    NS_OVERRIDE virtual bool
    RecvPPluginScriptableObjectConstructor(PPluginScriptableObjectParent* aActor);

    virtual bool
    DeallocPPluginScriptableObject(PPluginScriptableObjectParent* aObject);
    virtual PBrowserStreamParent*
    AllocPBrowserStream(const nsCString& url,
                        const uint32_t& length,
                        const uint32_t& lastmodified,
                        PStreamNotifyParent* notifyData,
                        const nsCString& headers,
                        const nsCString& mimeType,
                        const bool& seekable,
                        NPError* rv,
                        uint16_t *stype);
    virtual bool
    DeallocPBrowserStream(PBrowserStreamParent* stream);

    virtual PPluginStreamParent*
    AllocPPluginStream(const nsCString& mimeType,
                       const nsCString& target,
                       NPError* result);
    virtual bool
    DeallocPPluginStream(PPluginStreamParent* stream);

    virtual bool
    AnswerNPN_GetValue_NPNVjavascriptEnabledBool(bool* value, NPError* result);
    virtual bool
    AnswerNPN_GetValue_NPNVisOfflineBool(bool* value, NPError* result);
    virtual bool
    AnswerNPN_GetValue_NPNVnetscapeWindow(NativeWindowHandle* value,
                                          NPError* result);
    virtual bool
    AnswerNPN_GetValue_NPNVWindowNPObject(
                                       PPluginScriptableObjectParent** value,
                                       NPError* result);
    virtual bool
    AnswerNPN_GetValue_NPNVPluginElementNPObject(
                                       PPluginScriptableObjectParent** value,
                                       NPError* result);
    virtual bool
    AnswerNPN_GetValue_NPNVprivateModeBool(bool* value, NPError* result);

    virtual bool
    AnswerNPN_SetValue_NPPVpluginWindow(const bool& windowed, NPError* result);
    virtual bool
    AnswerNPN_SetValue_NPPVpluginTransparent(const bool& transparent,
                                             NPError* result);
    virtual bool
    AnswerNPN_SetValue_NPPVpluginUsesDOMForCursor(const bool& useDOMForCursor,
                                                  NPError* result);
    virtual bool
    AnswerNPN_SetValue_NPPVpluginDrawingModel(const int& drawingModel,
                                             NPError* result);
    virtual bool
    AnswerNPN_SetValue_NPPVpluginEventModel(const int& eventModel,
                                             NPError* result);

    virtual bool
    AnswerNPN_GetURL(const nsCString& url, const nsCString& target,
                     NPError *result);

    virtual bool
    AnswerNPN_PostURL(const nsCString& url, const nsCString& target,
                      const nsCString& buffer, const bool& file,
                      NPError* result);

    virtual PStreamNotifyParent*
    AllocPStreamNotify(const nsCString& url, const nsCString& target,
                       const bool& post, const nsCString& buffer,
                       const bool& file,
                       NPError* result);

    NS_OVERRIDE virtual bool
    AnswerPStreamNotifyConstructor(PStreamNotifyParent* actor,
                                   const nsCString& url,
                                   const nsCString& target,
                                   const bool& post, const nsCString& buffer,
                                   const bool& file,
                                   NPError* result);

    virtual bool
    DeallocPStreamNotify(PStreamNotifyParent* notifyData);

    virtual bool
    RecvNPN_InvalidateRect(const NPRect& rect);

    // Async rendering
    virtual bool
    RecvShow(const NPRect& updatedRect,
             const SurfaceDescriptor& newSurface,
             SurfaceDescriptor* prevSurface);

    virtual PPluginSurfaceParent*
    AllocPPluginSurface(const WindowsSharedMemoryHandle& handle,
                        const gfxIntSize& size,
                        const bool& transparent);

    virtual bool
    DeallocPPluginSurface(PPluginSurfaceParent* s);

    virtual bool
    AnswerNPN_PushPopupsEnabledState(const bool& aState);

    virtual bool
    AnswerNPN_PopPopupsEnabledState();

    NS_OVERRIDE virtual bool
    AnswerNPN_GetValueForURL(const NPNURLVariable& variable,
                             const nsCString& url,
                             nsCString* value, NPError* result);

    NS_OVERRIDE virtual bool
    AnswerNPN_SetValueForURL(const NPNURLVariable& variable,
                             const nsCString& url,
                             const nsCString& value, NPError* result);

    NS_OVERRIDE virtual bool
    AnswerNPN_GetAuthenticationInfo(const nsCString& protocol,
                                    const nsCString& host,
                                    const int32_t& port,
                                    const nsCString& scheme,
                                    const nsCString& realm,
                                    nsCString* username,
                                    nsCString* password,
                                    NPError* result);

    NS_OVERRIDE virtual bool
    AnswerNPN_ConvertPoint(const double& sourceX,
                           const bool&   ignoreDestX,
                           const double& sourceY,
                           const bool&   ignoreDestY,
                           const NPCoordinateSpace& sourceSpace,
                           const NPCoordinateSpace& destSpace,
                           double *destX,
                           double *destY,
                           bool *result);

    NS_OVERRIDE virtual bool
    RecvNegotiatedCarbon();

    NPError NPP_SetWindow(const NPWindow* aWindow);

    NPError NPP_GetValue(NPPVariable variable, void* retval);
    NPError NPP_SetValue(NPNVariable variable, void* value);

    void NPP_URLRedirectNotify(const char* url, int32_t status,
                               void* notifyData);

    NPError NPP_NewStream(NPMIMEType type, NPStream* stream,
                          NPBool seekable, uint16_t* stype);
    NPError NPP_DestroyStream(NPStream* stream, NPReason reason);

    void NPP_Print(NPPrint* platformPrint);

    int16_t NPP_HandleEvent(void* event);

    void NPP_URLNotify(const char* url, NPReason reason, void* notifyData);

    PluginModuleParent* Module()
    {
        return mParent;
    }

    const NPNetscapeFuncs* GetNPNIface()
    {
        return mNPNIface;
    }

    bool
    RegisterNPObjectForActor(NPObject* aObject,
                             PluginScriptableObjectParent* aActor);

    void
    UnregisterNPObject(NPObject* aObject);

    PluginScriptableObjectParent*
    GetActorForNPObject(NPObject* aObject);

    NPP
    GetNPP()
    {
      return mNPP;
    }

    virtual bool
    AnswerPluginFocusChange(const bool& gotFocus);

#ifdef MOZ_WIDGET_COCOA
    void Invalidate();
#endif // definied(MOZ_WIDGET_COCOA)

    nsresult AsyncSetWindow(NPWindow* window);
    nsresult GetImage(mozilla::layers::ImageContainer* aContainer, mozilla::layers::Image** aImage);
    nsresult GetImageSize(nsIntSize* aSize);
#ifdef XP_MACOSX
    nsresult IsRemoteDrawingCoreAnimation(PRBool *aDrawing);
#endif
    nsresult SetBackgroundUnknown();
    nsresult BeginUpdateBackground(const nsIntRect& aRect,
                                   gfxContext** aCtx);
    nsresult EndUpdateBackground(gfxContext* aCtx,
                                 const nsIntRect& aRect);
#if defined(MOZ_WIDGET_QT) && (MOZ_PLATFORM_MAEMO == 6)
    nsresult HandleGUIEvent(const nsGUIEvent& anEvent, bool* handled);
#endif

private:
    // Create an appropriate platform surface for a background of size
    // |aSize|.  Return true if successful.
    bool CreateBackground(const nsIntSize& aSize);
    void DestroyBackground();
    SurfaceDescriptor BackgroundDescriptor() /*const*/;

    NS_OVERRIDE
    virtual PPluginBackgroundDestroyerParent*
    AllocPPluginBackgroundDestroyer();

    NS_OVERRIDE
    virtual bool
    DeallocPPluginBackgroundDestroyer(PPluginBackgroundDestroyerParent* aActor);

    // Quirks mode support for various plugin mime types
    enum PluginQuirks {
        // OSX: Don't use the refresh timer for plug-ins
        // using this quirk. These plug-in most have another
        // way to refresh the window.
        COREANIMATION_REFRESH_TIMER = 1,
    };

    void InitQuirksModes(const nsCString& aMimeType);

    bool InternalGetValueForNPObject(NPNVariable aVariable,
                                     PPluginScriptableObjectParent** aValue,
                                     NPError* aResult);

private:
    PluginModuleParent* mParent;
    NPP mNPP;
    const NPNetscapeFuncs* mNPNIface;
    NPWindowType mWindowType;
    int mQuirks;

    nsDataHashtable<nsVoidPtrHashKey, PluginScriptableObjectParent*> mScriptableObjects;

#if defined(OS_WIN)
private:
    // Used in rendering windowless plugins in other processes.
    bool SharedSurfaceSetWindow(const NPWindow* aWindow, NPRemoteWindow& aRemoteWindow);
    void SharedSurfaceBeforePaint(RECT &rect, NPRemoteEvent& npremoteevent);
    void SharedSurfaceAfterPaint(NPEvent* npevent);
    void SharedSurfaceRelease();
    // Used in handling parent/child forwarding of events.
    static LRESULT CALLBACK PluginWindowHookProc(HWND hWnd, UINT message,
                                                 WPARAM wParam, LPARAM lParam);
    void SubclassPluginWindow(HWND aWnd);
    void UnsubclassPluginWindow();

private:
    gfx::SharedDIBWin  mSharedSurfaceDib;
    nsIntRect          mPluginPort;
    nsIntRect          mSharedSize;
    HWND               mPluginHWND;
    WNDPROC            mPluginWndProc;
    bool               mNestedEventState;
#endif // defined(XP_WIN)
#if defined(MOZ_WIDGET_COCOA)
private:
    Shmem                  mShSurface; 
    uint16_t               mShWidth;
    uint16_t               mShHeight;
    CGColorSpaceRef        mShColorSpace;
    int16_t                mDrawingModel;
    nsRefPtr<nsIOSurface> mIOSurface;
    nsRefPtr<nsIOSurface> mFrontIOSurface;
#endif // definied(MOZ_WIDGET_COCOA)

    // ObjectFrame layer wrapper
    nsRefPtr<gfxASurface>    mFrontSurface;
    // For windowless+transparent instances, this surface contains a
    // "pretty recent" copy of the pixels under its <object> frame.
    // On the plugin side, we use this surface to avoid doing alpha
    // recovery when possible.  This surface is created and owned by
    // the browser, but a "read-only" reference is sent to the plugin.
    //
    // We have explicitly chosen not to provide any guarantees about
    // the consistency of the pixels in |mBackground|.  A plugin may
    // be able to observe partial updates to the background.
    nsRefPtr<gfxASurface>    mBackground;
};


} // namespace plugins
} // namespace mozilla

#endif // ifndef dom_plugins_PluginInstanceParent_h
