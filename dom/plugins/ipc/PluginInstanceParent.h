/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_plugins_PluginInstanceParent_h
#define dom_plugins_PluginInstanceParent_h 1

#include "mozilla/plugins/PPluginInstanceParent.h"
#include "mozilla/plugins/PluginScriptableObjectParent.h"
#if defined(OS_WIN)
#include "mozilla/gfx/SharedDIBWin.h"
#include <d3d10_1.h>
#include "nsRefPtrHashtable.h"
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
    AnswerNPN_GetValue_DrawingModelSupport(const NPNVariable& model, bool* value);
  
    virtual bool
    AnswerNPN_GetValue_NPNVdocumentOrigin(nsCString* value, NPError* result);

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
                                              OptionalShmem *remoteImageData,
                                              CrossProcessMutexHandle *mutex,
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

    virtual bool
    AnswerNPN_InitAsyncSurface(const gfxIntSize& size,
                               const NPImageFormat& format,
                               NPRemoteAsyncSurface* surfData,
                               bool* result);

    virtual bool
    RecvRedrawPlugin();

    NS_OVERRIDE virtual bool
    RecvNegotiatedCarbon();

    virtual bool RecvReleaseDXGISharedSurface(const DXGISharedSurfaceHandle &aHandle);

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

    nsresult AsyncSetWindow(NPWindow* window);
    nsresult GetImageContainer(mozilla::layers::ImageContainer** aContainer);
    nsresult GetImageSize(nsIntSize* aSize);
#ifdef XP_MACOSX
    nsresult IsRemoteDrawingCoreAnimation(bool *aDrawing);
#endif
    nsresult SetBackgroundUnknown();
    nsresult BeginUpdateBackground(const nsIntRect& aRect,
                                   gfxContext** aCtx);
    nsresult EndUpdateBackground(gfxContext* aCtx,
                                 const nsIntRect& aRect);
#if defined(MOZ_WIDGET_QT) && (MOZ_PLATFORM_MAEMO == 6)
    nsresult HandleGUIEvent(const nsGUIEvent& anEvent, bool* handled);
#endif

    void DidComposite() { SendNPP_DidComposite(); }

private:
    // Create an appropriate platform surface for a background of size
    // |aSize|.  Return true if successful.
    bool CreateBackground(const nsIntSize& aSize);
    void DestroyBackground();
    SurfaceDescriptor BackgroundDescriptor() /*const*/;

    typedef mozilla::layers::ImageContainer ImageContainer;
    ImageContainer *GetImageContainer();

    NS_OVERRIDE
    virtual PPluginBackgroundDestroyerParent*
    AllocPPluginBackgroundDestroyer();

    NS_OVERRIDE
    virtual bool
    DeallocPPluginBackgroundDestroyer(PPluginBackgroundDestroyerParent* aActor);

    bool InternalGetValueForNPObject(NPNVariable aVariable,
                                     PPluginScriptableObjectParent** aValue,
                                     NPError* aResult);

    bool IsAsyncDrawing();

private:
    PluginModuleParent* mParent;
    NPP mNPP;
    const NPNetscapeFuncs* mNPNIface;
    NPWindowType mWindowType;
    Shmem mRemoteImageDataShmem;
    nsAutoPtr<CrossProcessMutex> mRemoteImageDataMutex;
    int16_t            mDrawingModel;
    nsAutoPtr<mozilla::layers::CompositionNotifySink> mNotifySink;

    nsDataHashtable<nsPtrHashKey<NPObject>, PluginScriptableObjectParent*> mScriptableObjects;

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

    // This will automatically release the textures when this object goes away.
    nsRefPtrHashtable<nsPtrHashKey<void>, ID3D10Texture2D> mTextureMap;
#endif // defined(XP_WIN)
#if defined(MOZ_WIDGET_COCOA)
private:
    Shmem                  mShSurface; 
    uint16_t               mShWidth;
    uint16_t               mShHeight;
    CGColorSpaceRef        mShColorSpace;
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
    
    nsRefPtr<ImageContainer> mImageContainer;
};


} // namespace plugins
} // namespace mozilla

#endif // ifndef dom_plugins_PluginInstanceParent_h
