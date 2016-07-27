/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"
#include <stdint.h> // for intptr_t

#include "mozilla/BasicEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "PluginInstanceParent.h"
#include "BrowserStreamParent.h"
#include "PluginAsyncSurrogate.h"
#include "PluginBackgroundDestroyer.h"
#include "PluginModuleParent.h"
#include "PluginStreamParent.h"
#include "StreamNotifyParent.h"
#include "npfunctions.h"
#include "nsAutoPtr.h"
#include "gfxASurface.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxSharedImageSurface.h"
#include "nsNetUtil.h"
#include "nsNPAPIPluginInstance.h"
#include "nsPluginInstanceOwner.h"
#include "nsFocusManager.h"
#include "nsIDOMElement.h"
#ifdef MOZ_X11
#include "gfxXlibSurface.h"
#endif
#include "gfxContext.h"
#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "Layers.h"
#include "ImageContainer.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "gfxPrefs.h"
#include "LayersLogging.h"
#include "mozilla/layers/TextureWrapperImage.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"
#include "mozilla/layers/ImageBridgeChild.h"
#if defined(XP_WIN)
# include "mozilla/layers/D3D11ShareHandleImage.h"
# include "mozilla/gfx/DeviceManagerD3D11.h"
# include "mozilla/layers/TextureD3D11.h"
#endif

#ifdef XP_MACOSX
#include "MacIOSurfaceImage.h"
#endif

#if defined(OS_WIN)
#include <windowsx.h>
#include "gfxWindowsPlatform.h"
#include "mozilla/plugins/PluginSurfaceParent.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsIWidget.h"
#include "nsPluginNativeWindow.h"
#include "PluginQuirks.h"
extern const wchar_t* kFlashFullscreenClass;
#elif defined(MOZ_WIDGET_GTK)
#include "mozilla/dom/ContentChild.h"
#include <gdk/gdk.h>
#elif defined(XP_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#endif // defined(XP_MACOSX)

using namespace mozilla::plugins;
using namespace mozilla::layers;
using namespace mozilla::gl;

void
StreamNotifyParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // Implement me! Bug 1005162
}

bool
StreamNotifyParent::RecvRedirectNotifyResponse(const bool& allow)
{
  PluginInstanceParent* instance = static_cast<PluginInstanceParent*>(Manager());
  instance->mNPNIface->urlredirectresponse(instance->mNPP, this, static_cast<NPBool>(allow));
  return true;
}

#if defined(XP_WIN)
namespace mozilla {
namespace plugins {
/**
 * e10s specific, used in cross referencing hwnds with plugin instances so we
 * can access methods here from PluginWidgetChild.
 */
static nsClassHashtable<nsVoidPtrHashKey, PluginInstanceParent>* sPluginInstanceList;

// static
PluginInstanceParent*
PluginInstanceParent::LookupPluginInstanceByID(uintptr_t aId)
{
    MOZ_ASSERT(NS_IsMainThread());
    if (sPluginInstanceList) {
        return sPluginInstanceList->Get((void*)aId);
    }
    return nullptr;
}
}
}
#endif

PluginInstanceParent::PluginInstanceParent(PluginModuleParent* parent,
                                           NPP npp,
                                           const nsCString& aMimeType,
                                           const NPNetscapeFuncs* npniface)
    : mParent(parent)
    , mSurrogate(PluginAsyncSurrogate::Cast(npp))
    , mUseSurrogate(true)
    , mNPP(npp)
    , mNPNIface(npniface)
    , mWindowType(NPWindowTypeWindow)
    , mDrawingModel(kDefaultDrawingModel)
    , mLastRecordedDrawingModel(-1)
    , mFrameID(0)
#if defined(OS_WIN)
    , mPluginHWND(nullptr)
    , mChildPluginHWND(nullptr)
    , mChildPluginsParentHWND(nullptr)
    , mPluginWndProc(nullptr)
    , mNestedEventState(false)
#endif // defined(XP_WIN)
#if defined(XP_MACOSX)
    , mShWidth(0)
    , mShHeight(0)
    , mShColorSpace(nullptr)
#endif
{
#if defined(OS_WIN)
    if (!sPluginInstanceList) {
        sPluginInstanceList = new nsClassHashtable<nsVoidPtrHashKey, PluginInstanceParent>();
    }
#endif
}

PluginInstanceParent::~PluginInstanceParent()
{
    if (mNPP)
        mNPP->pdata = nullptr;

#if defined(OS_WIN)
    NS_ASSERTION(!(mPluginHWND || mPluginWndProc),
        "Subclass was not reset correctly before the dtor was reached!");
#endif
#if defined(MOZ_WIDGET_COCOA)
    if (mShWidth != 0 && mShHeight != 0) {
        DeallocShmem(mShSurface);
    }
    if (mShColorSpace)
        ::CGColorSpaceRelease(mShColorSpace);
#endif
}

bool
PluginInstanceParent::InitMetadata(const nsACString& aMimeType,
                                   const nsACString& aSrcAttribute)
{
    if (aSrcAttribute.IsEmpty()) {
        return false;
    }
    // Ensure that the src attribute is absolute
    RefPtr<nsPluginInstanceOwner> owner = GetOwner();
    if (!owner) {
        return false;
    }
    nsCOMPtr<nsIURI> baseUri(owner->GetBaseURI());
    return NS_SUCCEEDED(NS_MakeAbsoluteURI(mSrcAttribute, aSrcAttribute, baseUri));
}

void
PluginInstanceParent::ActorDestroy(ActorDestroyReason why)
{
#if defined(OS_WIN)
    if (why == AbnormalShutdown) {
        // If the plugin process crashes, this is the only
        // chance we get to destroy resources.
        UnsubclassPluginWindow();
    }
#endif
    if (mFrontSurface) {
        mFrontSurface = nullptr;
        if (mImageContainer) {
            mImageContainer->ClearAllImages();
        }
#ifdef MOZ_X11
        FinishX(DefaultXDisplay());
#endif
    }
    if (IsUsingDirectDrawing() && mImageContainer) {
        mImageContainer->ClearAllImages();
    }
}

NPError
PluginInstanceParent::Destroy()
{
    NPError retval;
    {   // Scope for timer
        Telemetry::AutoTimer<Telemetry::BLOCKED_ON_PLUGIN_INSTANCE_DESTROY_MS>
            timer(Module()->GetHistogramKey());
        if (!CallNPP_Destroy(&retval)) {
            retval = NPERR_GENERIC_ERROR;
        }
    }

#if defined(OS_WIN)
    UnsubclassPluginWindow();
#endif

    return retval;
}

bool
PluginInstanceParent::IsUsingDirectDrawing()
{
    return IsDrawingModelDirect(mDrawingModel);
}

PBrowserStreamParent*
PluginInstanceParent::AllocPBrowserStreamParent(const nsCString& url,
                                                const uint32_t& length,
                                                const uint32_t& lastmodified,
                                                PStreamNotifyParent* notifyData,
                                                const nsCString& headers)
{
    NS_RUNTIMEABORT("Not reachable");
    return nullptr;
}

bool
PluginInstanceParent::DeallocPBrowserStreamParent(PBrowserStreamParent* stream)
{
    delete stream;
    return true;
}

PPluginStreamParent*
PluginInstanceParent::AllocPPluginStreamParent(const nsCString& mimeType,
                                               const nsCString& target,
                                               NPError* result)
{
    return new PluginStreamParent(this, mimeType, target, result);
}

bool
PluginInstanceParent::DeallocPPluginStreamParent(PPluginStreamParent* stream)
{
    delete stream;
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
#elif defined(XP_DARWIN)
    intptr_t id;
#elif defined(ANDROID)
    // TODO: Need Android impl
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

    *aValue = nullptr;
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
PluginInstanceParent::AnswerNPN_GetValue_DrawingModelSupport(const NPNVariable& model, bool* value)
{
    *value = false;
    return true;
}

bool
PluginInstanceParent::AnswerNPN_GetValue_NPNVdocumentOrigin(nsCString* value,
                                                            NPError* result)
{
    void *v = nullptr;
    *result = mNPNIface->getvalue(mNPP, NPNVdocumentOrigin, &v);
    if (*result == NPERR_NO_ERROR && v) {
        value->Adopt(static_cast<char*>(v));
    }
    return true;
}

static inline bool
AllowDirectBitmapSurfaceDrawing()
{
    if (!gfxPrefs::PluginAsyncDrawingEnabled()) {
        return false;
    }
    return gfxPlatform::GetPlatform()->SupportsPluginDirectBitmapDrawing();
}

static inline bool
AllowDirectDXGISurfaceDrawing()
{
    if (!gfxPrefs::PluginAsyncDrawingEnabled()) {
        return false;
    }
#if defined(XP_WIN)
    return gfxWindowsPlatform::GetPlatform()->SupportsPluginDirectDXGIDrawing();
#else
    return false;
#endif
}

bool
PluginInstanceParent::AnswerNPN_GetValue_SupportsAsyncBitmapSurface(bool* value)
{
    *value = AllowDirectBitmapSurfaceDrawing();
    return true;
}

bool
PluginInstanceParent::AnswerNPN_GetValue_SupportsAsyncDXGISurface(bool* value)
{
    *value = AllowDirectDXGISurfaceDrawing();
    return true;
}

bool
PluginInstanceParent::AnswerNPN_GetValue_PreferredDXGIAdapter(DxgiAdapterDesc* aOutDesc)
{
    PodZero(aOutDesc);
#ifdef XP_WIN
    if (!AllowDirectDXGISurfaceDrawing()) {
        return false;
    }

    RefPtr<ID3D11Device> device = DeviceManagerD3D11::Get()->GetContentDevice();
    if (!device) {
        return false;
    }

    RefPtr<IDXGIDevice> dxgi;
    if (FAILED(device->QueryInterface(__uuidof(IDXGIDevice), getter_AddRefs(dxgi))) || !dxgi) {
        return false;
    }
    RefPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgi->GetAdapter(getter_AddRefs(adapter))) || !adapter) {
        return false;
    }

    DXGI_ADAPTER_DESC desc;
    if (FAILED(adapter->GetDesc(&desc))) {
         return false;
    }

    *aOutDesc = DxgiAdapterDesc::From(desc);
#endif
    return true;
}

bool
PluginInstanceParent::AnswerNPN_SetValue_NPPVpluginWindow(
    const bool& windowed, NPError* result)
{
    // Yes, we are passing a boolean as a void*.  We have to cast to intptr_t
    // first to avoid gcc warnings about casting to a pointer from a
    // non-pointer-sized integer.
    *result = mNPNIface->setvalue(mNPP, NPPVpluginWindowBool,
                                  (void*)(intptr_t)windowed);
    return true;
}

bool
PluginInstanceParent::AnswerNPN_SetValue_NPPVpluginTransparent(
    const bool& transparent, NPError* result)
{
    *result = mNPNIface->setvalue(mNPP, NPPVpluginTransparentBool,
                                  (void*)(intptr_t)transparent);
    return true;
}

bool
PluginInstanceParent::AnswerNPN_SetValue_NPPVpluginUsesDOMForCursor(
    const bool& useDOMForCursor, NPError* result)
{
    *result = mNPNIface->setvalue(mNPP, NPPVpluginUsesDOMForCursorBool,
                                  (void*)(intptr_t)useDOMForCursor);
    return true;
}

bool
PluginInstanceParent::AnswerNPN_SetValue_NPPVpluginDrawingModel(
    const int& drawingModel, NPError* result)
{
    bool allowed = false;

    switch (drawingModel) {
#if defined(XP_MACOSX)
        case NPDrawingModelCoreAnimation:
        case NPDrawingModelInvalidatingCoreAnimation:
        case NPDrawingModelOpenGL:
        case NPDrawingModelCoreGraphics:
            allowed = true;
            break;
#elif defined(XP_WIN)
        case NPDrawingModelSyncWin:
            allowed = true;
            break;
        case NPDrawingModelAsyncWindowsDXGISurface:
            allowed = AllowDirectDXGISurfaceDrawing();
            break;
#elif defined(MOZ_X11)
        case NPDrawingModelSyncX:
            allowed = true;
            break;
#endif
        case NPDrawingModelAsyncBitmapSurface:
            allowed = AllowDirectBitmapSurfaceDrawing();
            break;
        default:
            allowed = false;
            break;
    }

    if (!allowed) {
        *result = NPERR_GENERIC_ERROR;
        return true;
    }

    mDrawingModel = drawingModel;

    int requestModel = drawingModel;

#ifdef XP_MACOSX
    if (drawingModel == NPDrawingModelCoreAnimation ||
        drawingModel == NPDrawingModelInvalidatingCoreAnimation) {
        // We need to request CoreGraphics otherwise
        // the nsPluginFrame will try to draw a CALayer
        // that can not be shared across process.
        requestModel = NPDrawingModelCoreGraphics;
    }
#endif

    *result = mNPNIface->setvalue(mNPP, NPPVpluginDrawingModel,
                                  (void*)(intptr_t)requestModel);

    return true;
}

bool
PluginInstanceParent::AnswerNPN_SetValue_NPPVpluginEventModel(
    const int& eventModel, NPError* result)
{
#ifdef XP_MACOSX
    *result = mNPNIface->setvalue(mNPP, NPPVpluginEventModel,
                                  (void*)(intptr_t)eventModel);
    return true;
#else
    *result = NPERR_GENERIC_ERROR;
    return true;
#endif
}

bool
PluginInstanceParent::AnswerNPN_SetValue_NPPVpluginIsPlayingAudio(
    const bool& isAudioPlaying, NPError* result)
{
    *result = mNPNIface->setvalue(mNPP, NPPVpluginIsPlayingAudio,
                                  (void*)(intptr_t)isAudioPlaying);
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
PluginInstanceParent::AllocPStreamNotifyParent(const nsCString& url,
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

    if (streamDestroyed) {
        // If the stream was destroyed, we must return an error code in the
        // constructor.
        *result = NPERR_GENERIC_ERROR;
    }
    else {
        static_cast<StreamNotifyParent*>(actor)->ClearDestructionFlag();
        if (*result != NPERR_NO_ERROR)
            return PStreamNotifyParent::Send__delete__(actor,
                                                       NPERR_GENERIC_ERROR);
    }

    return true;
}

bool
PluginInstanceParent::DeallocPStreamNotifyParent(PStreamNotifyParent* notifyData)
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

static inline NPRect
IntRectToNPRect(const gfx::IntRect& rect)
{
    NPRect r;
    r.left = rect.x;
    r.top = rect.y;
    r.right = rect.x + rect.width;
    r.bottom = rect.y + rect.height;
    return r;
}

bool
PluginInstanceParent::RecvRevokeCurrentDirectSurface()
{
    ImageContainer *container = GetImageContainer();
    if (!container) {
        return true;
    }

    container->ClearAllImages();

    PLUGIN_LOG_DEBUG(("   (RecvRevokeCurrentDirectSurface)"));
    return true;
}

bool
PluginInstanceParent::RecvInitDXGISurface(const gfx::SurfaceFormat& format,
                                           const gfx::IntSize& size,
                                           WindowsHandle* outHandle,
                                           NPError* outError)
{
    *outHandle = 0;
    *outError = NPERR_GENERIC_ERROR;

#if defined(XP_WIN)
    if (format != SurfaceFormat::B8G8R8A8 && format != SurfaceFormat::B8G8R8X8) {
        *outError = NPERR_INVALID_PARAM;
        return true;
    }
    if (size.width <= 0 || size.height <= 0) {
        *outError = NPERR_INVALID_PARAM;
        return true;
    }

    ImageContainer *container = GetImageContainer();
    if (!container) {
        return true;
    }

    ImageBridgeChild* forwarder = ImageBridgeChild::GetSingleton();
    if (!forwarder) {
        return true;
    }

    RefPtr<ID3D11Device> d3d11 = DeviceManagerD3D11::Get()->GetContentDevice();
    if (!d3d11) {
        return true;
    }

    // Create the texture we'll give to the plugin process.
    HANDLE sharedHandle = 0;
    RefPtr<ID3D11Texture2D> back;
    {
        CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, size.width, size.height, 1, 1);
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        if (FAILED(d3d11->CreateTexture2D(&desc, nullptr, getter_AddRefs(back))) || !back) {
            return true;
        }

        RefPtr<IDXGIResource> resource;
        if (FAILED(back->QueryInterface(IID_IDXGIResource, getter_AddRefs(resource))) || !resource) {
            return true;
        }
        if (FAILED(resource->GetSharedHandle(&sharedHandle) || !sharedHandle)) {
            return true;
        }
    }

    RefPtr<D3D11SurfaceHolder> holder = new D3D11SurfaceHolder(back, format, size);
    mD3D11Surfaces.Put(reinterpret_cast<void*>(sharedHandle), holder);

    *outHandle = reinterpret_cast<uintptr_t>(sharedHandle);
    *outError = NPERR_NO_ERROR;
#endif
    return true;
}

bool
PluginInstanceParent::RecvFinalizeDXGISurface(const WindowsHandle& handle)
{
#if defined(XP_WIN)
    mD3D11Surfaces.Remove(reinterpret_cast<void*>(handle));
#endif
    return true;
}

bool
PluginInstanceParent::RecvShowDirectBitmap(Shmem&& buffer,
                                           const SurfaceFormat& format,
                                           const uint32_t& stride,
                                           const IntSize& size,
                                           const IntRect& dirty)
{
    // Validate format.
    if (format != SurfaceFormat::B8G8R8A8 && format != SurfaceFormat::B8G8R8X8) {
        MOZ_ASSERT_UNREACHABLE("bad format type");
        return false;
    }
    if (size.width <= 0 || size.height <= 0) {
        MOZ_ASSERT_UNREACHABLE("bad image size");
        return false;
    }
    if (mDrawingModel != NPDrawingModelAsyncBitmapSurface) {
        MOZ_ASSERT_UNREACHABLE("plugin did not set a bitmap drawing model");
        return false;
    }

    // Validate buffer and size.
    CheckedInt<uint32_t> nbytes = CheckedInt<uint32_t>(uint32_t(size.height)) * stride;
    if (!nbytes.isValid() || nbytes.value() != buffer.Size<uint8_t>()) {
        MOZ_ASSERT_UNREACHABLE("bad shmem size");
        return false;
    }

    ImageContainer* container = GetImageContainer();
    if (!container) {
        return false;
    }

    RefPtr<gfx::DataSourceSurface> source =
        gfx::Factory::CreateWrappingDataSourceSurface(buffer.get<uint8_t>(), stride, size, format);
    if (!source) {
        return false;
    }

    // Allocate a texture for the compositor.
    RefPtr<TextureClientRecycleAllocator> allocator = mParent->EnsureTextureAllocator();
    RefPtr<TextureClient> texture = allocator->CreateOrRecycle(
        format, size, BackendSelector::Content,
        TextureFlags::NO_FLAGS,
        ALLOC_FOR_OUT_OF_BAND_CONTENT);
    if (!texture) {
        NS_WARNING("Could not allocate a TextureClient for plugin!");
        return false;
    }

    // Upload the plugin buffer.
    {
        TextureClientAutoLock autoLock(texture, OpenMode::OPEN_WRITE_ONLY);
        if (!autoLock.Succeeded()) {
            return false;
        }
        texture->UpdateFromSurface(source);
    }

    // Wrap the texture in an image and ship it off.
    RefPtr<TextureWrapperImage> image =
        new TextureWrapperImage(texture, gfx::IntRect(gfx::IntPoint(0, 0), size));
    SetCurrentImage(image);

    PLUGIN_LOG_DEBUG(("   (RecvShowDirectBitmap received shmem=%p stride=%d size=%s dirty=%s)",
        buffer.get<unsigned char>(), stride, Stringify(size).c_str(), Stringify(dirty).c_str()));
    return true;
}

void
PluginInstanceParent::SetCurrentImage(Image* aImage)
{
    MOZ_ASSERT(IsUsingDirectDrawing());
    ImageContainer::NonOwningImage holder(aImage);
    holder.mFrameID = ++mFrameID;

    AutoTArray<ImageContainer::NonOwningImage,1> imageList;
    imageList.AppendElement(holder);
    mImageContainer->SetCurrentImages(imageList);

    // Invalidate our area in the page so the image gets flushed.
    gfx::IntRect rect = aImage->GetPictureRect();
    NPRect nprect = {uint16_t(rect.x), uint16_t(rect.y), uint16_t(rect.width), uint16_t(rect.height)};
    RecvNPN_InvalidateRect(nprect);

    RecordDrawingModel();
}

bool
PluginInstanceParent::RecvShowDirectDXGISurface(const WindowsHandle& handle,
                                                 const gfx::IntRect& dirty)
{
#if defined(XP_WIN)
    RefPtr<D3D11SurfaceHolder> surface;
    if (!mD3D11Surfaces.Get(reinterpret_cast<void*>(handle), getter_AddRefs(surface))) {
        return false;
    }
    if (!surface->IsValid()) {
        return false;
    }

    ImageContainer* container = GetImageContainer();
    if (!container) {
        return false;
    }

    RefPtr<TextureClientRecycleAllocator> allocator = mParent->EnsureTextureAllocator();
    RefPtr<TextureClient> texture = allocator->CreateOrRecycle(
        surface->GetFormat(), surface->GetSize(),
        BackendSelector::Content,
        TextureFlags::NO_FLAGS,
        ALLOC_FOR_OUT_OF_BAND_CONTENT);
    if (!texture) {
        NS_WARNING("Could not allocate a TextureClient for plugin!");
        return false;
    }

    surface->CopyToTextureClient(texture);

    gfx::IntSize size(surface->GetSize());
    gfx::IntRect pictureRect(gfx::IntPoint(0, 0), size);

    // Wrap the texture in an image and ship it off.
    RefPtr<TextureWrapperImage> image = new TextureWrapperImage(texture, pictureRect);
    SetCurrentImage(image);

    PLUGIN_LOG_DEBUG(("   (RecvShowDirect3D10Surface received handle=%p rect=%s)",
        reinterpret_cast<void*>(handle), Stringify(dirty).c_str()));
    return true;
#else
    return false;
#endif
}

bool
PluginInstanceParent::RecvShow(const NPRect& updatedRect,
                               const SurfaceDescriptor& newSurface,
                               SurfaceDescriptor* prevSurface)
{
    PLUGIN_LOG_DEBUG(
        ("[InstanceParent][%p] RecvShow for <x=%d,y=%d, w=%d,h=%d>",
         this, updatedRect.left, updatedRect.top,
         updatedRect.right - updatedRect.left,
         updatedRect.bottom - updatedRect.top));

    MOZ_ASSERT(!IsUsingDirectDrawing());

    // XXXjwatt rewrite to use Moz2D
    RefPtr<gfxASurface> surface;
    if (newSurface.type() == SurfaceDescriptor::TShmem) {
        if (!newSurface.get_Shmem().IsReadable()) {
            NS_WARNING("back surface not readable");
            return false;
        }
        surface = gfxSharedImageSurface::Open(newSurface.get_Shmem());
    }
#ifdef XP_MACOSX
    else if (newSurface.type() == SurfaceDescriptor::TIOSurfaceDescriptor) {
        IOSurfaceDescriptor iodesc = newSurface.get_IOSurfaceDescriptor();

        RefPtr<MacIOSurface> newIOSurface =
          MacIOSurface::LookupSurface(iodesc.surfaceId(),
                                      iodesc.contentsScaleFactor());

        if (!newIOSurface) {
            NS_WARNING("Got bad IOSurfaceDescriptor in RecvShow");
            return false;
        }

        if (mFrontIOSurface)
            *prevSurface = IOSurfaceDescriptor(mFrontIOSurface->GetIOSurfaceID(),
                                               mFrontIOSurface->GetContentsScaleFactor());
        else
            *prevSurface = null_t();

        mFrontIOSurface = newIOSurface;

        RecvNPN_InvalidateRect(updatedRect);

        PLUGIN_LOG_DEBUG(("   (RecvShow invalidated for surface %p)",
                          mFrontSurface.get()));

        return true;
    }
#endif
#ifdef MOZ_X11
    else if (newSurface.type() == SurfaceDescriptor::TSurfaceDescriptorX11) {
        surface = newSurface.get_SurfaceDescriptorX11().OpenForeign();
    }
#endif
#ifdef XP_WIN
    else if (newSurface.type() == SurfaceDescriptor::TPPluginSurfaceParent) {
        PluginSurfaceParent* s =
            static_cast<PluginSurfaceParent*>(newSurface.get_PPluginSurfaceParent());
        surface = s->Surface();
    }
#endif

    if (mFrontSurface) {
        // This is the "old front buffer" we're about to hand back to
        // the plugin.  We might still have drawing operations
        // referencing it.
#ifdef MOZ_X11
        if (mFrontSurface->GetType() == gfxSurfaceType::Xlib) {
            // Finish with the surface and XSync here to ensure the server has
            // finished operations on the surface before the plugin starts
            // scribbling on it again, or worse, destroys it.
            mFrontSurface->Finish();
            FinishX(DefaultXDisplay());
        } else
#endif
        {
            mFrontSurface->Flush();
        }
    }

    if (mFrontSurface && gfxSharedImageSurface::IsSharedImage(mFrontSurface))
        *prevSurface = static_cast<gfxSharedImageSurface*>(mFrontSurface.get())->GetShmem();
    else
        *prevSurface = null_t();

    if (surface) {
        // Notify the cairo backend that this surface has changed behind
        // its back.
        gfxRect ur(updatedRect.left, updatedRect.top,
                   updatedRect.right - updatedRect.left,
                   updatedRect.bottom - updatedRect.top);
        surface->MarkDirty(ur);

        RefPtr<gfx::SourceSurface> sourceSurface =
            gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(nullptr, surface);
        RefPtr<SourceSurfaceImage> image = new SourceSurfaceImage(surface->GetSize(), sourceSurface);

        AutoTArray<ImageContainer::NonOwningImage,1> imageList;
        imageList.AppendElement(
            ImageContainer::NonOwningImage(image));

        ImageContainer *container = GetImageContainer();
        container->SetCurrentImages(imageList);
    }
    else if (mImageContainer) {
        mImageContainer->ClearAllImages();
    }

    mFrontSurface = surface;
    RecvNPN_InvalidateRect(updatedRect);

    PLUGIN_LOG_DEBUG(("   (RecvShow invalidated for surface %p)",
                      mFrontSurface.get()));

    RecordDrawingModel();
    return true;
}

nsresult
PluginInstanceParent::AsyncSetWindow(NPWindow* aWindow)
{
    NPRemoteWindow window;
    mWindowType = aWindow->type;
    window.window = reinterpret_cast<uint64_t>(aWindow->window);
    window.x = aWindow->x;
    window.y = aWindow->y;
    window.width = aWindow->width;
    window.height = aWindow->height;
    window.clipRect = aWindow->clipRect;
    window.type = aWindow->type;
#ifdef XP_MACOSX
    double scaleFactor = 1.0;
    mNPNIface->getvalue(mNPP, NPNVcontentsScaleFactor, &scaleFactor);
    window.contentsScaleFactor = scaleFactor;
#endif

#if defined(OS_WIN)
    MaybeCreateChildPopupSurrogate();
#endif

    if (!SendAsyncSetWindow(gfxPlatform::GetPlatform()->ScreenReferenceSurface()->GetType(),
                            window))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

nsresult
PluginInstanceParent::GetImageContainer(ImageContainer** aContainer)
{
    if (IsUsingDirectDrawing()) {
        // Use the image container created by the most recent direct surface
        // call, if any. We don't create one if no surfaces were presented
        // yet.
        ImageContainer *container = mImageContainer;
        NS_IF_ADDREF(container);
        *aContainer = container;
        return NS_OK;
    }

#ifdef XP_MACOSX
    MacIOSurface* ioSurface = nullptr;

    if (mFrontIOSurface) {
      ioSurface = mFrontIOSurface;
    } else if (mIOSurface) {
      ioSurface = mIOSurface;
    }

    if (!mFrontSurface && !ioSurface)
#else
    if (!mFrontSurface)
#endif
        return NS_ERROR_NOT_AVAILABLE;

    ImageContainer *container = GetImageContainer();

    if (!container) {
        return NS_ERROR_FAILURE;
    }

#ifdef XP_MACOSX
    if (ioSurface) {
        RefPtr<Image> image = new MacIOSurfaceImage(ioSurface);
        container->SetCurrentImageInTransaction(image);

        NS_IF_ADDREF(container);
        *aContainer = container;
        return NS_OK;
    }
#endif

    NS_IF_ADDREF(container);
    *aContainer = container;
    return NS_OK;
}

nsresult
PluginInstanceParent::GetImageSize(nsIntSize* aSize)
{
    if (IsUsingDirectDrawing()) {
        if (!mImageContainer) {
            return NS_ERROR_NOT_AVAILABLE;
        }
        *aSize = mImageContainer->GetCurrentSize();
        return NS_OK;
    }

    if (mFrontSurface) {
        mozilla::gfx::IntSize size = mFrontSurface->GetSize();
        *aSize = nsIntSize(size.width, size.height);
        return NS_OK;
    }

#ifdef XP_MACOSX
    if (mFrontIOSurface) {
        *aSize = nsIntSize(mFrontIOSurface->GetWidth(), mFrontIOSurface->GetHeight());
        return NS_OK;
    } else if (mIOSurface) {
        *aSize = nsIntSize(mIOSurface->GetWidth(), mIOSurface->GetHeight());
        return NS_OK;
    }
#endif

    return NS_ERROR_NOT_AVAILABLE;
}

void
PluginInstanceParent::DidComposite()
{
    if (!IsUsingDirectDrawing()) {
        return;
    }
    Unused << SendNPP_DidComposite();
}

#ifdef XP_MACOSX
nsresult
PluginInstanceParent::IsRemoteDrawingCoreAnimation(bool *aDrawing)
{
    *aDrawing = (NPDrawingModelCoreAnimation == (NPDrawingModel)mDrawingModel ||
                 NPDrawingModelInvalidatingCoreAnimation == (NPDrawingModel)mDrawingModel);
    return NS_OK;
}

nsresult
PluginInstanceParent::ContentsScaleFactorChanged(double aContentsScaleFactor)
{
    bool rv = SendContentsScaleFactorChanged(aContentsScaleFactor);
    return rv ? NS_OK : NS_ERROR_FAILURE;
}
#endif // #ifdef XP_MACOSX

nsresult
PluginInstanceParent::SetBackgroundUnknown()
{
    PLUGIN_LOG_DEBUG(("[InstanceParent][%p] SetBackgroundUnknown", this));

    if (mBackground) {
        DestroyBackground();
        MOZ_ASSERT(!mBackground, "Background not destroyed");
    }

    return NS_OK;
}

nsresult
PluginInstanceParent::BeginUpdateBackground(const nsIntRect& aRect,
                                            DrawTarget** aDrawTarget)
{
    PLUGIN_LOG_DEBUG(
        ("[InstanceParent][%p] BeginUpdateBackground for <x=%d,y=%d, w=%d,h=%d>",
         this, aRect.x, aRect.y, aRect.width, aRect.height));

    if (!mBackground) {
        // XXX if we failed to create a background surface on one
        // update, there's no guarantee that later updates will be for
        // the entire background area until successful.  We might want
        // to fix that eventually.
        MOZ_ASSERT(aRect.TopLeft() == nsIntPoint(0, 0),
                   "Expecting rect for whole frame");
        if (!CreateBackground(aRect.Size())) {
            *aDrawTarget = nullptr;
            return NS_OK;
        }
    }

    mozilla::gfx::IntSize sz = mBackground->GetSize();
#ifdef DEBUG
    MOZ_ASSERT(nsIntRect(0, 0, sz.width, sz.height).Contains(aRect),
               "Update outside of background area");
#endif

    RefPtr<gfx::DrawTarget> dt = gfxPlatform::GetPlatform()->
      CreateDrawTargetForSurface(mBackground, gfx::IntSize(sz.width, sz.height));
    dt.forget(aDrawTarget);

    return NS_OK;
}

nsresult
PluginInstanceParent::EndUpdateBackground(const nsIntRect& aRect)
{
    PLUGIN_LOG_DEBUG(
        ("[InstanceParent][%p] EndUpdateBackground for <x=%d,y=%d, w=%d,h=%d>",
         this, aRect.x, aRect.y, aRect.width, aRect.height));

#ifdef MOZ_X11
    // Have to XSync here to avoid the plugin trying to draw with this
    // surface racing with its creation in the X server.  We also want
    // to avoid the plugin drawing onto stale pixels, then handing us
    // back a front surface from those pixels that we might
    // recomposite for "a while" until the next update.  This XSync
    // still doesn't guarantee that the plugin draws onto a consistent
    // view of its background, but it does mean that the plugin is
    // drawing onto pixels no older than those in the latest
    // EndUpdateBackground().
    XSync(DefaultXDisplay(), False);
#endif

    Unused << SendUpdateBackground(BackgroundDescriptor(), aRect);

    return NS_OK;
}

#if defined(XP_WIN)
nsresult
PluginInstanceParent::SetScrollCaptureId(uint64_t aScrollCaptureId)
{
  if (aScrollCaptureId == ImageContainer::sInvalidAsyncContainerId) {
    return NS_ERROR_FAILURE;
  }

  mImageContainer = new ImageContainer(aScrollCaptureId);
  return NS_OK;
}

nsresult
PluginInstanceParent::GetScrollCaptureContainer(ImageContainer** aContainer)
{
  if (!aContainer || !mImageContainer) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<ImageContainer> container = GetImageContainer();
  container.forget(aContainer);

  return NS_OK;
}
#endif // XP_WIN

PluginAsyncSurrogate*
PluginInstanceParent::GetAsyncSurrogate()
{
    return mSurrogate;
}

bool
PluginInstanceParent::CreateBackground(const nsIntSize& aSize)
{
    MOZ_ASSERT(!mBackground, "Already have a background");

    // XXX refactor me

#if defined(MOZ_X11)
    Screen* screen = DefaultScreenOfDisplay(DefaultXDisplay());
    Visual* visual = DefaultVisualOfScreen(screen);
    mBackground = gfxXlibSurface::Create(screen, visual,
                                         mozilla::gfx::IntSize(aSize.width, aSize.height));
    return !!mBackground;

#elif defined(XP_WIN)
    // We have chosen to create an unsafe surface in which the plugin
    // can read from the region while we're writing to it.
    mBackground =
        gfxSharedImageSurface::CreateUnsafe(
            this,
            mozilla::gfx::IntSize(aSize.width, aSize.height),
            mozilla::gfx::SurfaceFormat::X8R8G8B8_UINT32);
    return !!mBackground;
#else
    return false;
#endif
}

void
PluginInstanceParent::DestroyBackground()
{
    if (!mBackground) {
        return;
    }

    // Relinquish ownership of |mBackground| to its destroyer
    PPluginBackgroundDestroyerParent* pbd =
        new PluginBackgroundDestroyerParent(mBackground);
    mBackground = nullptr;

    // If this fails, there's no problem: |bd| will be destroyed along
    // with the old background surface.
    Unused << SendPPluginBackgroundDestroyerConstructor(pbd);
}

mozilla::plugins::SurfaceDescriptor
PluginInstanceParent::BackgroundDescriptor()
{
    MOZ_ASSERT(mBackground, "Need a background here");

    // XXX refactor me

#ifdef MOZ_X11
    gfxXlibSurface* xsurf = static_cast<gfxXlibSurface*>(mBackground.get());
    return SurfaceDescriptorX11(xsurf);
#endif

#ifdef XP_WIN
    MOZ_ASSERT(gfxSharedImageSurface::IsSharedImage(mBackground),
               "Expected shared image surface");
    gfxSharedImageSurface* shmem =
        static_cast<gfxSharedImageSurface*>(mBackground.get());
    return shmem->GetShmem();
#endif

    // If this is ever used, which it shouldn't be, it will trigger a
    // hard assertion in IPDL-generated code.
    return mozilla::plugins::SurfaceDescriptor();
}

ImageContainer*
PluginInstanceParent::GetImageContainer()
{
  if (mImageContainer) {
    return mImageContainer;
  }

  if (IsUsingDirectDrawing()) {
      mImageContainer = LayerManager::CreateImageContainer(ImageContainer::ASYNCHRONOUS);
  } else {
      mImageContainer = LayerManager::CreateImageContainer();
  }
  return mImageContainer;
}

PPluginBackgroundDestroyerParent*
PluginInstanceParent::AllocPPluginBackgroundDestroyerParent()
{
    NS_RUNTIMEABORT("'Power-user' ctor is used exclusively");
    return nullptr;
}

bool
PluginInstanceParent::DeallocPPluginBackgroundDestroyerParent(
    PPluginBackgroundDestroyerParent* aActor)
{
    delete aActor;
    return true;
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
        MaybeCreateChildPopupSurrogate();
    } else {
        SubclassPluginWindow(reinterpret_cast<HWND>(aWindow->window));

        // Skip SetWindow call for hidden QuickTime plugins.
        if ((mParent->GetQuirks() & QUIRK_QUICKTIME_AVOID_SETWINDOW) &&
            aWindow->width == 0 && aWindow->height == 0) {
            return NPERR_NO_ERROR;
        }

        window.window = reinterpret_cast<uint64_t>(aWindow->window);
        window.x = aWindow->x;
        window.y = aWindow->y;
        window.width = aWindow->width;
        window.height = aWindow->height;
        window.type = aWindow->type;

        // On Windows we need to create and set the parent before we set the
        // window on the plugin, or keyboard interaction will not work.
        if (!MaybeCreateAndParentChildPluginWindow()) {
            return NPERR_GENERIC_ERROR;
        }
    }
#else
    window.window = reinterpret_cast<uint64_t>(aWindow->window);
    window.x = aWindow->x;
    window.y = aWindow->y;
    window.width = aWindow->width;
    window.height = aWindow->height;
    window.clipRect = aWindow->clipRect; // MacOS specific
    window.type = aWindow->type;
#endif

#if defined(XP_MACOSX)
    double floatScaleFactor = 1.0;
    mNPNIface->getvalue(mNPP, NPNVcontentsScaleFactor, &floatScaleFactor);
    int scaleFactor = ceil(floatScaleFactor);
    window.contentsScaleFactor = floatScaleFactor;

    if (mShWidth != window.width * scaleFactor || mShHeight != window.height * scaleFactor) {
        if (mDrawingModel == NPDrawingModelCoreAnimation ||
            mDrawingModel == NPDrawingModelInvalidatingCoreAnimation) {
            mIOSurface = MacIOSurface::CreateIOSurface(window.width, window.height,
                                                       floatScaleFactor);
        } else if (uint32_t(mShWidth * mShHeight) !=
                   window.width * scaleFactor * window.height * scaleFactor) {
            if (mShWidth != 0 && mShHeight != 0) {
                DeallocShmem(mShSurface);
                mShWidth = 0;
                mShHeight = 0;
            }

            if (window.width != 0 && window.height != 0) {
                if (!AllocShmem(window.width * scaleFactor * window.height*4 * scaleFactor,
                                SharedMemory::TYPE_BASIC, &mShSurface)) {
                    PLUGIN_LOG_DEBUG(("Shared memory could not be allocated."));
                    return NPERR_GENERIC_ERROR;
                }
            }
        }
        mShWidth = window.width * scaleFactor;
        mShHeight = window.height * scaleFactor;
    }
#endif

#if defined(MOZ_X11) && defined(XP_UNIX) && !defined(XP_MACOSX)
    const NPSetWindowCallbackStruct* ws_info =
      static_cast<NPSetWindowCallbackStruct*>(aWindow->ws_info);
    window.visualID = ws_info->visual ? ws_info->visual->visualid : 0;
    window.colormap = ws_info->colormap;
#endif

    if (!CallNPP_SetWindow(window)) {
        return NPERR_GENERIC_ERROR;
    }

    RecordDrawingModel();
    return NPERR_NO_ERROR;
}

NPError
PluginInstanceParent::NPP_GetValue(NPPVariable aVariable,
                                   void* _retval)
{
    switch (aVariable) {

    case NPPVpluginWantsAllNetworkStreams: {
        bool wantsAllStreams;
        NPError rv;

        if (!CallNPP_GetValue_NPPVpluginWantsAllNetworkStreams(&wantsAllStreams, &rv)) {
            return NPERR_GENERIC_ERROR;
        }

        if (NPERR_NO_ERROR != rv) {
            return rv;
        }

        (*(NPBool*)_retval) = wantsAllStreams;
        return NPERR_NO_ERROR;
    }

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

#ifdef MOZ_ACCESSIBILITY_ATK
    case NPPVpluginNativeAccessibleAtkPlugId: {
        nsCString plugId;
        NPError rv;
        if (!CallNPP_GetValue_NPPVpluginNativeAccessibleAtkPlugId(&plugId, &rv)) {
            return NPERR_GENERIC_ERROR;
        }

        if (NPERR_NO_ERROR != rv) {
            return rv;
        }

        (*(nsCString*)_retval) = plugId;
        return NPERR_NO_ERROR;
    }
#endif

    default:
        MOZ_LOG(GetPluginLog(), LogLevel::Warning,
               ("In PluginInstanceParent::NPP_GetValue: Unhandled NPPVariable %i (%s)",
                (int) aVariable, NPPVariableToString(aVariable)));
        return NPERR_GENERIC_ERROR;
    }
}

NPError
PluginInstanceParent::NPP_SetValue(NPNVariable variable, void* value)
{
    NPError result;
    switch (variable) {
    case NPNVprivateModeBool:
        if (!CallNPP_SetValue_NPNVprivateModeBool(*static_cast<NPBool*>(value),
                                                  &result))
            return NPERR_GENERIC_ERROR;

        return result;

    case NPNVmuteAudioBool:
        if (!CallNPP_SetValue_NPNVmuteAudioBool(*static_cast<NPBool*>(value),
                                                &result))
            return NPERR_GENERIC_ERROR;

        return result;

    case NPNVCSSZoomFactor:
        if (!CallNPP_SetValue_NPNVCSSZoomFactor(*static_cast<double*>(value),
                                                &result))
            return NPERR_GENERIC_ERROR;

        return result;

    default:
        NS_ERROR("Unhandled NPNVariable in NPP_SetValue");
        MOZ_LOG(GetPluginLog(), LogLevel::Warning,
               ("In PluginInstanceParent::NPP_SetValue: Unhandled NPNVariable %i (%s)",
                (int) variable, NPNVariableToString(variable)));
        return NPERR_GENERIC_ERROR;
    }
}

void
PluginInstanceParent::NPP_URLRedirectNotify(const char* url, int32_t status,
                                            void* notifyData)
{
  if (!notifyData)
    return;

  PStreamNotifyParent* streamNotify = static_cast<PStreamNotifyParent*>(notifyData);
  Unused << streamNotify->SendRedirectNotify(NullableString(url), status);
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
#if defined(XP_MACOSX)
    double scaleFactor = 1.0;
    mNPNIface->getvalue(mNPP, NPNVcontentsScaleFactor, &scaleFactor);
    npremoteevent.contentsScaleFactor = scaleFactor;
#endif
    int16_t handled = 0;

#if defined(OS_WIN)
    if (mWindowType == NPWindowTypeDrawable) {
        switch (npevent->event) {
            case WM_KILLFOCUS:
            {
              // When the user selects fullscreen mode in Flash video players,
              // WM_KILLFOCUS will be delayed by deferred event processing:
              // WM_LBUTTONUP results in a call to CreateWindow within Flash,
              // which fires WM_KILLFOCUS. Delayed delivery causes Flash to
              // misinterpret the event, dropping back out of fullscreen. Trap
              // this event and drop it.
              wchar_t szClass[26];
              HWND hwnd = GetForegroundWindow();
              if (hwnd && hwnd != mPluginHWND &&
                  GetClassNameW(hwnd, szClass,
                                sizeof(szClass)/sizeof(char16_t)) &&
                  !wcscmp(szClass, kFlashFullscreenClass)) {
                  return 0;
              }
            }
            break;

            case WM_WINDOWPOSCHANGED:
            {
                // We send this in nsPluginFrame just before painting
                return SendWindowPosChanged(npremoteevent);
            }

            case WM_IME_STARTCOMPOSITION:
            case WM_IME_COMPOSITION:
            case WM_IME_ENDCOMPOSITION:
                if (!(mParent->GetQuirks() & QUIRK_WINLESS_HOOK_IME)) {
                  // IME message will be posted on allowed plugins only such as
                  // Flash.  Because if we cannot know that plugin can handle
                  // IME correctly.
                  return 0;
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
        FinishX(DefaultXDisplay());

        return CallPaint(npremoteevent, &handled) ? handled : 0;

    case ButtonPress:
        // Release any active pointer grab so that the plugin X client can
        // grab the pointer if it wishes.
        Display *dpy = DefaultXDisplay();
#  ifdef MOZ_WIDGET_GTK
        // GDK attempts to (asynchronously) track whether there is an active
        // grab so ungrab through GDK.
        //
        // This call needs to occur in the same process that receives the event in
        // the first place (chrome process)
        if (XRE_IsContentProcess()) {
          dom::ContentChild* cp = dom::ContentChild::GetSingleton();
          cp->SendUngrabPointer(npevent->xbutton.time);
        } else {
          gdk_pointer_ungrab(npevent->xbutton.time);
        }
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
            if (cgContext) {
                nsCARenderer::DrawSurfaceToCGContext(cgContext, mIOSurface,
                                                     mShColorSpace,
                                                     npevent->data.draw.x,
                                                     npevent->data.draw.y,
                                                     npevent->data.draw.width,
                                                     npevent->data.draw.height);
            }
            return true;
        } else if (mFrontIOSurface) {
            CGContextRef cgContext = npevent->data.draw.context;
            if (!mShColorSpace) {
                mShColorSpace = CreateSystemColorSpace();
            }
            if (!mShColorSpace) {
                PLUGIN_LOG_DEBUG(("Could not allocate ColorSpace."));
                return false;
            }
            if (cgContext) {
                nsCARenderer::DrawSurfaceToCGContext(cgContext, mFrontIOSurface,
                                                     mShColorSpace,
                                                     npevent->data.draw.x,
                                                     npevent->data.draw.y,
                                                     npevent->data.draw.width,
                                                     npevent->data.draw.height);
            }
            return true;
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

    if (!SendPBrowserStreamConstructor(bs,
                                       NullableString(stream->url),
                                       stream->end,
                                       stream->lastmodified,
                                       static_cast<PStreamNotifyParent*>(stream->notifyData),
                                       NullableString(stream->headers))) {
        return NPERR_GENERIC_ERROR;
    }

    Telemetry::AutoTimer<Telemetry::BLOCKED_ON_PLUGIN_STREAM_INIT_MS>
        timer(Module()->GetHistogramKey());

    NPError err = NPERR_NO_ERROR;
    if (mParent->IsStartingAsync()) {
        MOZ_ASSERT(mSurrogate);
        mSurrogate->AsyncCallDeparting();
        if (SendAsyncNPP_NewStream(bs, NullableString(type), seekable)) {
            *stype = nsPluginStreamListenerPeer::STREAM_TYPE_UNKNOWN;
        } else {
            err = NPERR_GENERIC_ERROR;
        }
    } else {
        bs->SetAlive();
        if (!CallNPP_NewStream(bs, NullableString(type), seekable, &err, stype)) {
            err = NPERR_GENERIC_ERROR;
        }
        if (NPERR_NO_ERROR != err) {
            Unused << PBrowserStreamParent::Send__delete__(bs);
        }
    }

    return err;
}

NPError
PluginInstanceParent::NPP_DestroyStream(NPStream* stream, NPReason reason)
{
    PLUGIN_LOG_DEBUG(("%s (stream=%p, reason=%i)",
                      FULLFUNCTION, (void*) stream, (int) reason));

    AStream* s = static_cast<AStream*>(stream->pdata);
    if (!s) {
        // The stream has already been deleted by other means.
        // With async plugin init this could happen if async NPP_NewStream
        // returns an error code.
        return NPERR_NO_ERROR;
    }
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
PluginInstanceParent::AllocPPluginScriptableObjectParent()
{
    return new PluginScriptableObjectParent(Proxy);
}

bool
PluginInstanceParent::DeallocPPluginScriptableObjectParent(
                                         PPluginScriptableObjectParent* aObject)
{
    PluginScriptableObjectParent* actor =
        static_cast<PluginScriptableObjectParent*>(aObject);

    NPObject* object = actor->GetObject(false);
    if (object) {
        NS_ASSERTION(mScriptableObjects.Get(object, nullptr),
                     "NPObject not in the hash!");
        mScriptableObjects.Remove(object);
    }
#ifdef DEBUG
    else {
        for (auto iter = mScriptableObjects.Iter(); !iter.Done(); iter.Next()) {
            NS_ASSERTION(actor != iter.UserData(),
                         "Actor in the hash with a null NPObject!");
        }
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
    Unused << PStreamNotifyParent::Send__delete__(streamNotify, reason);
}

bool
PluginInstanceParent::RegisterNPObjectForActor(
                                           NPObject* aObject,
                                           PluginScriptableObjectParent* aActor)
{
    NS_ASSERTION(aObject && aActor, "Null pointers!");
    NS_ASSERTION(!mScriptableObjects.Get(aObject, nullptr), "Duplicate entry!");
    mScriptableObjects.Put(aObject, aActor);
    return true;
}

void
PluginInstanceParent::UnregisterNPObject(NPObject* aObject)
{
    NS_ASSERTION(aObject, "Null pointer!");
    NS_ASSERTION(mScriptableObjects.Get(aObject, nullptr), "Unknown entry!");
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
    if (!SendPPluginScriptableObjectConstructor(actor)) {
        NS_WARNING("Failed to send constructor message!");
        return nullptr;
    }

    actor->InitializeLocal(aObject);
    return actor;
}

PPluginSurfaceParent*
PluginInstanceParent::AllocPPluginSurfaceParent(const WindowsSharedMemoryHandle& handle,
                                                const mozilla::gfx::IntSize& size,
                                                const bool& transparent)
{
#ifdef XP_WIN
    return new PluginSurfaceParent(handle, size, transparent);
#else
    NS_ERROR("This shouldn't be called!");
    return nullptr;
#endif
}

bool
PluginInstanceParent::DeallocPPluginSurfaceParent(PPluginSurfaceParent* s)
{
#ifdef XP_WIN
    delete s;
    return true;
#else
    return false;
#endif
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
                                      ignoreDestX ? nullptr : destX,
                                      ignoreDestY ? nullptr : destY,
                                      destSpace);

    return true;
}

bool
PluginInstanceParent::RecvRedrawPlugin()
{
    nsNPAPIPluginInstance *inst = static_cast<nsNPAPIPluginInstance*>(mNPP->ndata);
    if (!inst) {
        return false;
    }

    inst->RedrawPlugin();
    return true;
}

bool
PluginInstanceParent::RecvNegotiatedCarbon()
{
    nsNPAPIPluginInstance *inst = static_cast<nsNPAPIPluginInstance*>(mNPP->ndata);
    if (!inst) {
        return false;
    }
    inst->CarbonNPAPIFailure();
    return true;
}

nsPluginInstanceOwner*
PluginInstanceParent::GetOwner()
{
    nsNPAPIPluginInstance* inst = static_cast<nsNPAPIPluginInstance*>(mNPP->ndata);
    if (!inst) {
        return nullptr;
    }
    return inst->GetOwner();
}

bool
PluginInstanceParent::RecvAsyncNPP_NewResult(const NPError& aResult)
{
    // NB: mUseSurrogate must be cleared before doing anything else, especially
    //     calling NPP_SetWindow!
    mUseSurrogate = false;

    mSurrogate->AsyncCallArriving();
    if (aResult == NPERR_NO_ERROR) {
        mSurrogate->SetAcceptingCalls(true);
    }

    // It is possible for a plugin instance to outlive its owner (eg. When a
    // PluginDestructionGuard was on the stack at the time the owner was being
    // destroyed). We need to handle that case.
    nsPluginInstanceOwner* owner = GetOwner();
    if (!owner) {
        // We can't do anything at this point, just return. Any pending browser
        // streams will be cleaned up when the plugin instance is destroyed.
        return true;
    }

    if (aResult != NPERR_NO_ERROR) {
        mSurrogate->NotifyAsyncInitFailed();
        return true;
    }

    // Now we need to do a bunch of exciting post-NPP_New housekeeping.
    owner->NotifyHostCreateWidget();

    MOZ_ASSERT(mSurrogate);
    mSurrogate->OnInstanceCreated(this);

    return true;
}

bool
PluginInstanceParent::RecvSetNetscapeWindowAsParent(const NativeWindowHandle& childWindow)
{
#if defined(XP_WIN)
    nsPluginInstanceOwner* owner = GetOwner();
    if (!owner || NS_FAILED(owner->SetNetscapeWindowAsParent(childWindow))) {
        NS_WARNING("Failed to set Netscape window as parent.");
    }

    return true;
#else
    NS_NOTREACHED("PluginInstanceParent::RecvSetNetscapeWindowAsParent not implemented!");
    return false;
#endif
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
    here. We then post a custom event to widget/windows/nswindow
    which fires off a gui event letting the browser know.
*/

static const wchar_t kPluginInstanceParentProperty[] =
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
        Unused << self->CallSetPluginFocus();
        break;

        case WM_CLOSE:
        self->UnsubclassPluginWindow();
        break;
    }

    if (self->mPluginWndProc == PluginWindowHookProc) {
      NS_NOTREACHED(
        "PluginWindowHookProc invoking mPluginWndProc w/"
        "mPluginWndProc == PluginWindowHookProc????");
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return ::CallWindowProc(self->mPluginWndProc, hWnd, message, wParam,
                            lParam);
}

void
PluginInstanceParent::SubclassPluginWindow(HWND aWnd)
{
    if ((aWnd && mPluginHWND == aWnd) || (!aWnd && mPluginHWND)) {
        return;
    }

    if (XRE_IsContentProcess()) {
        if (!aWnd) {
            NS_WARNING("PluginInstanceParent::SubclassPluginWindow unexpected null window");
            return;
        }
        mPluginHWND = aWnd; // now a remote window, we can't subclass this
        mPluginWndProc = nullptr;
        // Note sPluginInstanceList wil delete 'this' if we do not remove
        // it on shutdown.
        sPluginInstanceList->Put((void*)mPluginHWND, this);
        return;
    }

    NS_ASSERTION(!(mPluginHWND && aWnd != mPluginHWND),
        "PluginInstanceParent::SubclassPluginWindow hwnd is not our window!");

    mPluginHWND = aWnd;
    mPluginWndProc =
        (WNDPROC)::SetWindowLongPtrA(mPluginHWND, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(PluginWindowHookProc));
    DebugOnly<bool> bRes = ::SetPropW(mPluginHWND, kPluginInstanceParentProperty, this);
    NS_ASSERTION(mPluginWndProc,
        "PluginInstanceParent::SubclassPluginWindow failed to set subclass!");
    NS_ASSERTION(bRes,
        "PluginInstanceParent::SubclassPluginWindow failed to set prop!");
}

void
PluginInstanceParent::UnsubclassPluginWindow()
{
    if (XRE_IsContentProcess()) {
        if (mPluginHWND) {
            // Remove 'this' from the plugin list safely
            nsAutoPtr<PluginInstanceParent> tmp;
            MOZ_ASSERT(sPluginInstanceList);
            sPluginInstanceList->RemoveAndForget((void*)mPluginHWND, tmp);
            tmp.forget();
            if (!sPluginInstanceList->Count()) {
                delete sPluginInstanceList;
                sPluginInstanceList = nullptr;
            }
        }
        mPluginHWND = nullptr;
        return;
    }

    if (mPluginHWND && mPluginWndProc) {
        ::SetWindowLongPtrA(mPluginHWND, GWLP_WNDPROC,
                            reinterpret_cast<LONG_PTR>(mPluginWndProc));

        ::RemovePropW(mPluginHWND, kPluginInstanceParentProperty);

        mPluginWndProc = nullptr;
        mPluginHWND = nullptr;
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

bool
PluginInstanceParent::MaybeCreateAndParentChildPluginWindow()
{
    // On Windows we need to create and set the parent before we set the
    // window on the plugin, or keyboard interaction will not work.
    if (!mChildPluginHWND) {
        if (!CallCreateChildPluginWindow(&mChildPluginHWND) ||
            !mChildPluginHWND) {
            return false;
        }
    }

    // It's not clear if the parent window would ever change, but when this
    // was done in the NPAPI child it used to allow for this.
    if (mPluginHWND == mChildPluginsParentHWND) {
        return true;
    }

    nsPluginInstanceOwner* owner = GetOwner();
    if (!owner) {
        // We can't reparent without an owner, the plugin is probably shutting
        // down, just return true to allow any calls to continue.
        return true;
    }

    // Note that this call will probably cause a sync native message to the
    // process that owns the child window.
    owner->SetWidgetWindowAsParent(mChildPluginHWND);
    mChildPluginsParentHWND = mPluginHWND;
    return true;
}

void
PluginInstanceParent::MaybeCreateChildPopupSurrogate()
{
    // Already created or not required for this plugin.
    if (mChildPluginHWND || mWindowType != NPWindowTypeDrawable ||
        !(mParent->GetQuirks() & QUIRK_WINLESS_TRACKPOPUP_HOOK)) {
        return;
    }

    // We need to pass the netscape window down to be cached as part of the call
    // to create the surrogate, because the reparenting of the surrogate in the
    // main process can cause sync Windows messages to the plugin process, which
    // then cause sync messages from the plugin child for the netscape window
    // which causes a deadlock.
    NativeWindowHandle netscapeWindow;
    NPError result = mNPNIface->getvalue(mNPP, NPNVnetscapeWindow,
                                         &netscapeWindow);
    if (NPERR_NO_ERROR != result) {
        NS_WARNING("Can't get netscape window to pass to plugin child.");
        return;
    }

    if (!SendCreateChildPopupSurrogate(netscapeWindow)) {
        NS_WARNING("Failed to create popup surrogate in child.");
    }
}

#endif // defined(OS_WIN)

bool
PluginInstanceParent::AnswerPluginFocusChange(const bool& gotFocus)
{
    PLUGIN_LOG_DEBUG(("%s", FULLFUNCTION));

    // Currently only in use on windows - an event we receive from the child
    // when it's plugin window (or one of it's children) receives keyboard
    // focus. We detect this and forward a notification here so we can update
    // focus.
#if defined(OS_WIN)
    if (gotFocus) {
      nsPluginInstanceOwner* owner = GetOwner();
      if (owner) {
        nsIFocusManager* fm = nsFocusManager::GetFocusManager();
        nsCOMPtr<nsIDOMElement> element;
        owner->GetDOMElement(getter_AddRefs(element));
        if (fm && element) {
          fm->SetFocus(element, 0);
        }
      }
    }
    return true;
#else
    NS_NOTREACHED("PluginInstanceParent::AnswerPluginFocusChange not implemented!");
    return false;
#endif
}

PluginInstanceParent*
PluginInstanceParent::Cast(NPP aInstance, PluginAsyncSurrogate** aSurrogate)
{
    PluginDataResolver* resolver =
        static_cast<PluginDataResolver*>(aInstance->pdata);

    // If the plugin crashed and the PluginInstanceParent was deleted,
    // aInstance->pdata will be nullptr.
    if (!resolver) {
        return nullptr;
    }

    PluginInstanceParent* instancePtr = resolver->GetInstance();

    if (instancePtr && aInstance != instancePtr->mNPP) {
        NS_RUNTIMEABORT("Corrupted plugin data.");
    }

    if (aSurrogate) {
        *aSurrogate = resolver->GetAsyncSurrogate();
    }

    return instancePtr;
}

bool
PluginInstanceParent::RecvGetCompositionString(const uint32_t& aIndex,
                                               nsTArray<uint8_t>* aDist,
                                               int32_t* aLength)
{
#if defined(OS_WIN)
    nsPluginInstanceOwner* owner = GetOwner();
    if (!owner) {
        *aLength = IMM_ERROR_GENERAL;
        return true;
    }

    if (!owner->GetCompositionString(aIndex, aDist, aLength)) {
        *aLength = IMM_ERROR_NODATA;
    }
#endif
    return true;
}

bool
PluginInstanceParent::RecvSetCandidateWindow(
    const mozilla::widget::CandidateWindowPosition& aPosition)
{
#if defined(OS_WIN)
    nsPluginInstanceOwner* owner = GetOwner();
    if (owner) {
        owner->SetCandidateWindow(aPosition);
    }
#endif
    return true;
}

bool
PluginInstanceParent::RecvRequestCommitOrCancel(const bool& aCommitted)
{
#if defined(OS_WIN)
    nsPluginInstanceOwner* owner = GetOwner();
    if (owner) {
        owner->RequestCommitOrCancel(aCommitted);
    }
#endif
    return true;
}

nsresult
PluginInstanceParent::HandledWindowedPluginKeyEvent(
                        const NativeEventData& aKeyEventData,
                        bool aIsConsumed)
{
    if (NS_WARN_IF(!SendHandledWindowedPluginKeyEvent(aKeyEventData,
                                                      aIsConsumed))) {
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

bool
PluginInstanceParent::RecvOnWindowedPluginKeyEvent(
                        const NativeEventData& aKeyEventData)
{
    nsPluginInstanceOwner* owner = GetOwner();
    if (NS_WARN_IF(!owner)) {
        // Notifies the plugin process of the key event being not consumed
        // by us.
        HandledWindowedPluginKeyEvent(aKeyEventData, false);
        return true;
    }
    owner->OnWindowedPluginKeyEvent(aKeyEventData);
    return true;
}

void
PluginInstanceParent::RecordDrawingModel()
{
    int mode = -1;
    switch (mWindowType) {
    case NPWindowTypeWindow:
        // We use 0=windowed since there is no specific NPDrawingModel value.
        mode = 0;
        break;
    case NPWindowTypeDrawable:
        mode = mDrawingModel + 1;
        break;
    default:
        MOZ_ASSERT_UNREACHABLE("bad window type");
        return;
    }

    if (mode == mLastRecordedDrawingModel) {
        return;
    }
    MOZ_ASSERT(mode >= 0);

    Telemetry::Accumulate(Telemetry::PLUGIN_DRAWING_MODEL, mode);
    mLastRecordedDrawingModel = mode;
}
