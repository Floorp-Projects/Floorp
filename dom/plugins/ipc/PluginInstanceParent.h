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
#include "mozilla/gfx/QuartzSupport.h"
#endif

#include "npfunctions.h"
#include "nsAutoPtr.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsRect.h"
#include "PluginDataResolver.h"

#include "mozilla/unused.h"

class gfxASurface;
class gfxContext;
class nsPluginInstanceOwner;

namespace mozilla {
namespace layers {
class ImageContainer;
} // namespace layers
namespace plugins {

class PBrowserStreamParent;
class PluginModuleParent;

class PluginInstanceParent : public PPluginInstanceParent
                           , public PluginDataResolver
{
    friend class PluginModuleParent;
    friend class BrowserStreamParent;
    friend class PluginStreamParent;
    friend class StreamNotifyParent;

#if defined(XP_WIN)
public:
    /**
     * Helper method for looking up instances based on a supplied id.
     */
    static PluginInstanceParent*
    LookupPluginInstanceByID(uintptr_t aId);
#endif // defined(XP_WIN)

public:
    PluginInstanceParent(PluginModuleParent* parent,
                         NPP npp,
                         const nsCString& mimeType,
                         const NPNetscapeFuncs* npniface);

    virtual ~PluginInstanceParent();

    bool InitMetadata(const nsACString& aMimeType,
                      const nsACString& aSrcAttribute);
    NPError Destroy();

    virtual void ActorDestroy(ActorDestroyReason why) override;

    virtual PPluginScriptableObjectParent*
    AllocPPluginScriptableObjectParent() override;

    virtual bool
    RecvPPluginScriptableObjectConstructor(PPluginScriptableObjectParent* aActor) override;

    virtual bool
    DeallocPPluginScriptableObjectParent(PPluginScriptableObjectParent* aObject) override;
    virtual PBrowserStreamParent*
    AllocPBrowserStreamParent(const nsCString& url,
                              const uint32_t& length,
                              const uint32_t& lastmodified,
                              PStreamNotifyParent* notifyData,
                              const nsCString& headers) override;
    virtual bool
    DeallocPBrowserStreamParent(PBrowserStreamParent* stream) override;

    virtual PPluginStreamParent*
    AllocPPluginStreamParent(const nsCString& mimeType,
                             const nsCString& target,
                             NPError* result) override;
    virtual bool
    DeallocPPluginStreamParent(PPluginStreamParent* stream) override;

    virtual bool
    AnswerNPN_GetValue_NPNVnetscapeWindow(NativeWindowHandle* value,
                                          NPError* result) override;
    virtual bool
    AnswerNPN_GetValue_NPNVWindowNPObject(
                                       PPluginScriptableObjectParent** value,
                                       NPError* result) override;
    virtual bool
    AnswerNPN_GetValue_NPNVPluginElementNPObject(
                                       PPluginScriptableObjectParent** value,
                                       NPError* result) override;
    virtual bool
    AnswerNPN_GetValue_NPNVprivateModeBool(bool* value, NPError* result) override;

    virtual bool
    AnswerNPN_GetValue_DrawingModelSupport(const NPNVariable& model, bool* value) override;
  
    virtual bool
    AnswerNPN_GetValue_NPNVdocumentOrigin(nsCString* value, NPError* result) override;

    virtual bool
    AnswerNPN_SetValue_NPPVpluginWindow(const bool& windowed, NPError* result) override;
    virtual bool
    AnswerNPN_SetValue_NPPVpluginTransparent(const bool& transparent,
                                             NPError* result) override;
    virtual bool
    AnswerNPN_SetValue_NPPVpluginUsesDOMForCursor(const bool& useDOMForCursor,
                                                  NPError* result) override;
    virtual bool
    AnswerNPN_SetValue_NPPVpluginDrawingModel(const int& drawingModel,
                                              NPError* result) override;
    virtual bool
    AnswerNPN_SetValue_NPPVpluginEventModel(const int& eventModel,
                                             NPError* result) override;
    virtual bool
    AnswerNPN_SetValue_NPPVpluginIsPlayingAudio(const bool& isAudioPlaying,
                                                NPError* result) override;

    virtual bool
    AnswerNPN_GetURL(const nsCString& url, const nsCString& target,
                     NPError *result) override;

    virtual bool
    AnswerNPN_PostURL(const nsCString& url, const nsCString& target,
                      const nsCString& buffer, const bool& file,
                      NPError* result) override;

    virtual PStreamNotifyParent*
    AllocPStreamNotifyParent(const nsCString& url, const nsCString& target,
                             const bool& post, const nsCString& buffer,
                             const bool& file,
                             NPError* result) override;

    virtual bool
    AnswerPStreamNotifyConstructor(PStreamNotifyParent* actor,
                                   const nsCString& url,
                                   const nsCString& target,
                                   const bool& post, const nsCString& buffer,
                                   const bool& file,
                                   NPError* result) override;

    virtual bool
    DeallocPStreamNotifyParent(PStreamNotifyParent* notifyData) override;

    virtual bool
    RecvNPN_InvalidateRect(const NPRect& rect) override;

    // Async rendering
    virtual bool
    RecvShow(const NPRect& updatedRect,
             const SurfaceDescriptor& newSurface,
             SurfaceDescriptor* prevSurface) override;

    virtual PPluginSurfaceParent*
    AllocPPluginSurfaceParent(const WindowsSharedMemoryHandle& handle,
                              const mozilla::gfx::IntSize& size,
                              const bool& transparent) override;

    virtual bool
    DeallocPPluginSurfaceParent(PPluginSurfaceParent* s) override;

    virtual bool
    AnswerNPN_PushPopupsEnabledState(const bool& aState) override;

    virtual bool
    AnswerNPN_PopPopupsEnabledState() override;

    virtual bool
    AnswerNPN_GetValueForURL(const NPNURLVariable& variable,
                             const nsCString& url,
                             nsCString* value, NPError* result) override;

    virtual bool
    AnswerNPN_SetValueForURL(const NPNURLVariable& variable,
                             const nsCString& url,
                             const nsCString& value, NPError* result) override;

    virtual bool
    AnswerNPN_GetAuthenticationInfo(const nsCString& protocol,
                                    const nsCString& host,
                                    const int32_t& port,
                                    const nsCString& scheme,
                                    const nsCString& realm,
                                    nsCString* username,
                                    nsCString* password,
                                    NPError* result) override;

    virtual bool
    AnswerNPN_ConvertPoint(const double& sourceX,
                           const bool&   ignoreDestX,
                           const double& sourceY,
                           const bool&   ignoreDestY,
                           const NPCoordinateSpace& sourceSpace,
                           const NPCoordinateSpace& destSpace,
                           double *destX,
                           double *destY,
                           bool *result) override;

    virtual bool
    RecvRedrawPlugin() override;

    virtual bool
    RecvNegotiatedCarbon() override;

    virtual bool
    RecvAsyncNPP_NewResult(const NPError& aResult) override;

    virtual bool
    RecvSetNetscapeWindowAsParent(const NativeWindowHandle& childWindow) override;

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

    bool
    UseSurrogate() const
    {
        return mUseSurrogate;
    }

    void
    GetSrcAttribute(nsACString& aOutput) const
    {
        aOutput = mSrcAttribute;
    }

    /**
     * This function tells us whether this plugin instance would have been
     * whitelisted for Shumway if Shumway had been enabled. This is being used
     * for the purpose of gathering telemetry on Flash hangs that could
     * potentially be avoided by using Shumway instead.
     */
    bool
    IsWhitelistedForShumway() const
    {
        return mIsWhitelistedForShumway;
    }

    virtual bool
    AnswerPluginFocusChange(const bool& gotFocus) override;

    nsresult AsyncSetWindow(NPWindow* window);
    nsresult GetImageContainer(mozilla::layers::ImageContainer** aContainer);
    nsresult GetImageSize(nsIntSize* aSize);
#ifdef XP_MACOSX
    nsresult IsRemoteDrawingCoreAnimation(bool *aDrawing);
    nsresult ContentsScaleFactorChanged(double aContentsScaleFactor);
#endif
    nsresult SetBackgroundUnknown();
    nsresult BeginUpdateBackground(const nsIntRect& aRect,
                                   gfxContext** aCtx);
    nsresult EndUpdateBackground(gfxContext* aCtx,
                                 const nsIntRect& aRect);
    void DidComposite() { unused << SendNPP_DidComposite(); }

    virtual PluginAsyncSurrogate* GetAsyncSurrogate() override;

    virtual PluginInstanceParent* GetInstance() override { return this; }

    static PluginInstanceParent* Cast(NPP instance,
                                      PluginAsyncSurrogate** aSurrogate = nullptr);

private:
    // Create an appropriate platform surface for a background of size
    // |aSize|.  Return true if successful.
    bool CreateBackground(const nsIntSize& aSize);
    void DestroyBackground();
    SurfaceDescriptor BackgroundDescriptor() /*const*/;

    typedef mozilla::layers::ImageContainer ImageContainer;
    ImageContainer *GetImageContainer();

    virtual PPluginBackgroundDestroyerParent*
    AllocPPluginBackgroundDestroyerParent() override;

    virtual bool
    DeallocPPluginBackgroundDestroyerParent(PPluginBackgroundDestroyerParent* aActor) override;

    bool InternalGetValueForNPObject(NPNVariable aVariable,
                                     PPluginScriptableObjectParent** aValue,
                                     NPError* aResult);

    nsPluginInstanceOwner* GetOwner();

private:
    PluginModuleParent* mParent;
    nsRefPtr<PluginAsyncSurrogate> mSurrogate;
    bool mUseSurrogate;
    NPP mNPP;
    const NPNetscapeFuncs* mNPNIface;
    nsCString mSrcAttribute;
    bool mIsWhitelistedForShumway;
    NPWindowType mWindowType;
    int16_t            mDrawingModel;

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

    bool MaybeCreateAndParentChildPluginWindow();
    void MaybeCreateChildPopupSurrogate();

private:
    gfx::SharedDIBWin  mSharedSurfaceDib;
    nsIntRect          mPluginPort;
    nsIntRect          mSharedSize;
    HWND               mPluginHWND;
    // This is used for the normal child plugin HWND for windowed plugins and,
    // if needed, also the child popup surrogate HWND for windowless plugins.
    HWND               mChildPluginHWND;
    HWND               mChildPluginsParentHWND;
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
    RefPtr<MacIOSurface> mIOSurface;
    RefPtr<MacIOSurface> mFrontIOSurface;
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
