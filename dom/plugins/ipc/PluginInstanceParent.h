/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_plugins_PluginInstanceParent_h
#define dom_plugins_PluginInstanceParent_h 1

#include "mozilla/plugins/PPluginInstanceParent.h"
#include "mozilla/plugins/PluginScriptableObjectParent.h"
#if defined(OS_WIN)
#  include "mozilla/gfx/SharedDIBWin.h"
#  include <d3d10_1.h>
#  include "nsRefPtrHashtable.h"
#  include "mozilla/layers/LayersSurfaces.h"
#elif defined(MOZ_WIDGET_COCOA)
#  include "mozilla/gfx/QuartzSupport.h"
#endif

#include "npfunctions.h"
#include "nsTHashMap.h"
#include "nsHashKeys.h"
#include "nsRect.h"

#include "mozilla/Unused.h"
#include "mozilla/EventForwards.h"

class gfxASurface;
class gfxContext;
class nsPluginInstanceOwner;

namespace mozilla {
namespace layers {
class Image;
class ImageContainer;
class TextureClientRecycleAllocator;
}  // namespace layers
namespace plugins {

class PBrowserStreamParent;
class PluginModuleParent;
class D3D11SurfaceHolder;

class PluginInstanceParent : public PPluginInstanceParent {
  friend class PluginModuleParent;
  friend class BrowserStreamParent;
  friend class StreamNotifyParent;
  friend class PPluginInstanceParent;

#if defined(XP_WIN)
 public:
  /**
   * Helper method for looking up instances based on a supplied id.
   */
  static PluginInstanceParent* LookupPluginInstanceByID(uintptr_t aId);
#endif  // defined(XP_WIN)

 public:
  typedef mozilla::gfx::DrawTarget DrawTarget;

  PluginInstanceParent(PluginModuleParent* parent, NPP npp,
                       const nsCString& mimeType,
                       const NPNetscapeFuncs* npniface);

  virtual ~PluginInstanceParent();

  bool InitMetadata(const nsACString& aMimeType,
                    const nsACString& aSrcAttribute);
  NPError Destroy();

  virtual void ActorDestroy(ActorDestroyReason why) override;

  PPluginScriptableObjectParent* AllocPPluginScriptableObjectParent();

  virtual mozilla::ipc::IPCResult RecvPPluginScriptableObjectConstructor(
      PPluginScriptableObjectParent* aActor) override;

  bool DeallocPPluginScriptableObjectParent(
      PPluginScriptableObjectParent* aObject);
  PBrowserStreamParent* AllocPBrowserStreamParent(
      const nsCString& url, const uint32_t& length,
      const uint32_t& lastmodified, PStreamNotifyParent* notifyData,
      const nsCString& headers);
  bool DeallocPBrowserStreamParent(PBrowserStreamParent* stream);

  mozilla::ipc::IPCResult AnswerNPN_GetValue_NPNVnetscapeWindow(
      NativeWindowHandle* value, NPError* result);
  mozilla::ipc::IPCResult AnswerNPN_GetValue_NPNVWindowNPObject(
      PPluginScriptableObjectParent** value, NPError* result);
  mozilla::ipc::IPCResult AnswerNPN_GetValue_NPNVPluginElementNPObject(
      PPluginScriptableObjectParent** value, NPError* result);
  mozilla::ipc::IPCResult AnswerNPN_GetValue_NPNVprivateModeBool(
      bool* value, NPError* result);

  mozilla::ipc::IPCResult AnswerNPN_GetValue_DrawingModelSupport(
      const NPNVariable& model, bool* value);

  mozilla::ipc::IPCResult AnswerNPN_GetValue_NPNVdocumentOrigin(
      nsCString* value, NPError* result);

  mozilla::ipc::IPCResult AnswerNPN_GetValue_SupportsAsyncBitmapSurface(
      bool* value);

  static bool SupportsPluginDirectDXGISurfaceDrawing();
  mozilla::ipc::IPCResult AnswerNPN_GetValue_SupportsAsyncDXGISurface(
      bool* value) {
    *value = SupportsPluginDirectDXGISurfaceDrawing();
    return IPC_OK();
  }

  mozilla::ipc::IPCResult AnswerNPN_GetValue_PreferredDXGIAdapter(
      DxgiAdapterDesc* desc);

  mozilla::ipc::IPCResult AnswerNPN_SetValue_NPPVpluginWindow(
      const bool& windowed, NPError* result);
  mozilla::ipc::IPCResult AnswerNPN_SetValue_NPPVpluginTransparent(
      const bool& transparent, NPError* result);
  mozilla::ipc::IPCResult AnswerNPN_SetValue_NPPVpluginUsesDOMForCursor(
      const bool& useDOMForCursor, NPError* result);
  mozilla::ipc::IPCResult AnswerNPN_SetValue_NPPVpluginDrawingModel(
      const int& drawingModel, NPError* result);
  mozilla::ipc::IPCResult AnswerNPN_SetValue_NPPVpluginEventModel(
      const int& eventModel, NPError* result);
  mozilla::ipc::IPCResult AnswerNPN_SetValue_NPPVpluginIsPlayingAudio(
      const bool& isAudioPlaying, NPError* result);

  mozilla::ipc::IPCResult AnswerNPN_GetURL(const nsCString& url,
                                           const nsCString& target,
                                           NPError* result);

  mozilla::ipc::IPCResult AnswerNPN_PostURL(const nsCString& url,
                                            const nsCString& target,
                                            const nsCString& buffer,
                                            const bool& file, NPError* result);

  PStreamNotifyParent* AllocPStreamNotifyParent(
      const nsCString& url, const nsCString& target, const bool& post,
      const nsCString& buffer, const bool& file, NPError* result);

  virtual mozilla::ipc::IPCResult AnswerPStreamNotifyConstructor(
      PStreamNotifyParent* actor, const nsCString& url, const nsCString& target,
      const bool& post, const nsCString& buffer, const bool& file,
      NPError* result) override;

  bool DeallocPStreamNotifyParent(PStreamNotifyParent* notifyData);

  mozilla::ipc::IPCResult RecvNPN_InvalidateRect(const NPRect& rect);

  mozilla::ipc::IPCResult RecvRevokeCurrentDirectSurface();

  /**
   * Windows async plugin rendering uses DXGI surface objects, entirely
   * maintained by the compositor process, to back the rendering of plugins.
   * The expected mechanics are:
   * - The PluginInstanceChild (PIC) in the plugin process sends
   *   InitDXGISurface to us, the content process' PluginInstanceParent (PIP).
   * - The PIP uses the ImageBridge to tell the compositor to create 2
   *   surfaces -- one to be written to by the plugin process and another
   *   to be displayed by the compositor.  The PIP returns the plugin
   *   surface to the PIC.
   * - The PIC repeatedly issues ShowDirectDXGISurface calls to tell the
   *   PIP to blit the plugin surface to the display surface.  These
   *   requests are forwarded to the compositor via the ImageBridge.
   * - The PIC sends FinalizeDXGISurface tio the PIP when it no longer needs
   *   the surface.  The PIP then tells the compositor to destroy the
   *   plugin surface.  After this, the PIP will tell the compositor to
   *   also destroy the display surface as soon as the ImageBridge says it
   *   no longer needs it.
   */
  mozilla::ipc::IPCResult RecvInitDXGISurface(const gfx::SurfaceFormat& format,
                                              const gfx::IntSize& size,
                                              WindowsHandle* outHandle,
                                              NPError* outError);
  mozilla::ipc::IPCResult RecvShowDirectDXGISurface(const WindowsHandle& handle,
                                                    const gfx::IntRect& rect);

  mozilla::ipc::IPCResult RecvFinalizeDXGISurface(const WindowsHandle& handle);

  // --

  mozilla::ipc::IPCResult RecvShowDirectBitmap(Shmem&& buffer,
                                               const gfx::SurfaceFormat& format,
                                               const uint32_t& stride,
                                               const gfx::IntSize& size,
                                               const gfx::IntRect& dirty);

  mozilla::ipc::IPCResult RecvShow(const NPRect& updatedRect,
                                   const SurfaceDescriptor& newSurface,
                                   SurfaceDescriptor* prevSurface);

  PPluginSurfaceParent* AllocPPluginSurfaceParent(
      const WindowsSharedMemoryHandle& handle,
      const mozilla::gfx::IntSize& size, const bool& transparent);

  bool DeallocPPluginSurfaceParent(PPluginSurfaceParent* s);

  mozilla::ipc::IPCResult AnswerNPN_PushPopupsEnabledState(const bool& aState);

  mozilla::ipc::IPCResult AnswerNPN_PopPopupsEnabledState();

  mozilla::ipc::IPCResult AnswerNPN_GetValueForURL(
      const NPNURLVariable& variable, const nsCString& url, nsCString* value,
      NPError* result);

  mozilla::ipc::IPCResult AnswerNPN_SetValueForURL(
      const NPNURLVariable& variable, const nsCString& url,
      const nsCString& value, NPError* result);

  mozilla::ipc::IPCResult AnswerNPN_ConvertPoint(
      const double& sourceX, const bool& ignoreDestX, const double& sourceY,
      const bool& ignoreDestY, const NPCoordinateSpace& sourceSpace,
      const NPCoordinateSpace& destSpace, double* destX, double* destY,
      bool* result);

  mozilla::ipc::IPCResult RecvRedrawPlugin();

  mozilla::ipc::IPCResult RecvSetNetscapeWindowAsParent(
      const NativeWindowHandle& childWindow);

  NPError NPP_SetWindow(const NPWindow* aWindow);

  NPError NPP_GetValue(NPPVariable variable, void* retval);
  NPError NPP_SetValue(NPNVariable variable, void* value);

  void NPP_URLRedirectNotify(const char* url, int32_t status, void* notifyData);

  NPError NPP_NewStream(NPMIMEType type, NPStream* stream, NPBool seekable,
                        uint16_t* stype);
  NPError NPP_DestroyStream(NPStream* stream, NPReason reason);

  void NPP_Print(NPPrint* platformPrint);

  int16_t NPP_HandleEvent(void* event);

  void NPP_URLNotify(const char* url, NPReason reason, void* notifyData);

  PluginModuleParent* Module() { return mParent; }

  const NPNetscapeFuncs* GetNPNIface() { return mNPNIface; }

  bool RegisterNPObjectForActor(NPObject* aObject,
                                PluginScriptableObjectParent* aActor);

  void UnregisterNPObject(NPObject* aObject);

  PluginScriptableObjectParent* GetActorForNPObject(NPObject* aObject);

  NPP GetNPP() { return mNPP; }

  void GetSrcAttribute(nsACString& aOutput) const { aOutput = mSrcAttribute; }

  MOZ_CAN_RUN_SCRIPT_BOUNDARY mozilla::ipc::IPCResult AnswerPluginFocusChange(
      const bool& gotFocus);

  nsresult AsyncSetWindow(NPWindow* window);
  nsresult GetImageContainer(mozilla::layers::ImageContainer** aContainer);
  nsresult GetImageSize(nsIntSize* aSize);
#ifdef XP_MACOSX
  nsresult IsRemoteDrawingCoreAnimation(bool* aDrawing);
#endif
#if defined(XP_MACOSX) || defined(XP_WIN)
  nsresult ContentsScaleFactorChanged(double aContentsScaleFactor);
#endif
  nsresult SetBackgroundUnknown();
  nsresult BeginUpdateBackground(const nsIntRect& aRect,
                                 DrawTarget** aDrawTarget);
  nsresult EndUpdateBackground(const nsIntRect& aRect);
#if defined(XP_WIN)
  nsresult SetScrollCaptureId(uint64_t aScrollCaptureId);
  nsresult GetScrollCaptureContainer(
      mozilla::layers::ImageContainer** aContainer);
#endif
  void DidComposite();

  bool IsUsingDirectDrawing();

  static PluginInstanceParent* Cast(NPP instance);

  // for IME hook
  mozilla::ipc::IPCResult RecvGetCompositionString(const uint32_t& aIndex,
                                                   nsTArray<uint8_t>* aBuffer,
                                                   int32_t* aLength);
  mozilla::ipc::IPCResult RecvRequestCommitOrCancel(const bool& aCommitted);

 private:
  // Create an appropriate platform surface for a background of size
  // |aSize|.  Return true if successful.
  bool CreateBackground(const nsIntSize& aSize);
  void DestroyBackground();
  SurfaceDescriptor BackgroundDescriptor() /*const*/;

  typedef mozilla::layers::ImageContainer ImageContainer;
  ImageContainer* GetImageContainer();

  PPluginBackgroundDestroyerParent* AllocPPluginBackgroundDestroyerParent();

  bool DeallocPPluginBackgroundDestroyerParent(
      PPluginBackgroundDestroyerParent* aActor);

  bool InternalGetValueForNPObject(NPNVariable aVariable,
                                   PPluginScriptableObjectParent** aValue,
                                   NPError* aResult);

  nsPluginInstanceOwner* GetOwner();

  void SetCurrentImage(layers::Image* aImage);

  // Update Telemetry with the current drawing model.
  void RecordDrawingModel();

 private:
  PluginModuleParent* mParent;
  NPP mNPP;
  const NPNetscapeFuncs* mNPNIface;
  nsCString mSrcAttribute;
  NPWindowType mWindowType;
  int16_t mDrawingModel;

  // Since plugins may request different drawing models to find a compatible
  // one, we only record the drawing model after a SetWindow call and if the
  // drawing model has changed.
  int mLastRecordedDrawingModel;

  nsTHashMap<nsPtrHashKey<NPObject>, PluginScriptableObjectParent*>
      mScriptableObjects;

  // This is used to tell the compositor that it should invalidate the
  // ImageLayer.
  uint32_t mFrameID;

#if defined(XP_WIN)
  // Note: DXGI 1.1 surface handles are global across all processes, and are not
  // marshaled. As long as we haven't freed a texture its handle should be valid
  // as a unique cross-process identifier for the texture.
  nsRefPtrHashtable<nsPtrHashKey<void>, D3D11SurfaceHolder> mD3D11Surfaces;
#endif

#if defined(OS_WIN)
 private:
  // Used in handling parent/child forwarding of events.
  static LRESULT CALLBACK PluginWindowHookProc(HWND hWnd, UINT message,
                                               WPARAM wParam, LPARAM lParam);
  void SubclassPluginWindow(HWND aWnd);
  void UnsubclassPluginWindow();

  bool MaybeCreateAndParentChildPluginWindow();
  void MaybeCreateChildPopupSurrogate();

 private:
  nsIntRect mPluginPort;
  nsIntRect mSharedSize;
  HWND mPluginHWND;
  // This is used for the normal child plugin HWND for windowed plugins and,
  // if needed, also the child popup surrogate HWND for windowless plugins.
  HWND mChildPluginHWND;
  HWND mChildPluginsParentHWND;
  WNDPROC mPluginWndProc;

  struct AsyncSurfaceInfo {
    layers::SurfaceDescriptorPlugin mSD;
    gfx::IntSize mSize;
  };
  // Key is plugin surface's texture's handle
  HashMap<WindowsHandle, AsyncSurfaceInfo> mAsyncSurfaceMap;
#endif  // defined(OS_WIN)

#if defined(MOZ_WIDGET_COCOA)
 private:
  Shmem mShSurface;
  uint16_t mShWidth;
  uint16_t mShHeight;
  CGColorSpaceRef mShColorSpace;
  RefPtr<MacIOSurface> mIOSurface;
  RefPtr<MacIOSurface> mFrontIOSurface;
#endif  // definied(MOZ_WIDGET_COCOA)

  // ObjectFrame layer wrapper
  RefPtr<gfxASurface> mFrontSurface;
  // For windowless+transparent instances, this surface contains a
  // "pretty recent" copy of the pixels under its <object> frame.
  // On the plugin side, we use this surface to avoid doing alpha
  // recovery when possible.  This surface is created and owned by
  // the browser, but a "read-only" reference is sent to the plugin.
  //
  // We have explicitly chosen not to provide any guarantees about
  // the consistency of the pixels in |mBackground|.  A plugin may
  // be able to observe partial updates to the background.
  RefPtr<gfxASurface> mBackground;

  RefPtr<ImageContainer> mImageContainer;
};

}  // namespace plugins
}  // namespace mozilla

#endif  // ifndef dom_plugins_PluginInstanceParent_h
