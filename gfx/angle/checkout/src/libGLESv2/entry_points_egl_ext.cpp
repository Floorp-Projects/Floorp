//
// Copyright(c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// entry_points_ext.cpp : Implements the EGL extension entry points.

#include "libGLESv2/entry_points_egl_ext.h"
#include "libGLESv2/global_state.h"

#include "libANGLE/Context.h"
#include "libANGLE/Device.h"
#include "libANGLE/Display.h"
#include "libANGLE/Stream.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Thread.h"
#include "libANGLE/validationEGL.h"

#include "common/debug.h"

namespace egl
{

// EGL_ANGLE_query_surface_pointer
EGLBoolean EGLAPIENTRY QuerySurfacePointerANGLE(EGLDisplay dpy,
                                                EGLSurface surface,
                                                EGLint attribute,
                                                void **value)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLint attribute = %d, void "
        "**value = 0x%0.8p)",
        dpy, surface, attribute, value);
    Thread *thread = GetCurrentThread();

    Display *display    = static_cast<Display *>(dpy);
    Surface *eglSurface = static_cast<Surface *>(surface);

    Error error = ValidateSurface(display, eglSurface);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglQuerySurfacePointerANGLE",
                         GetSurfaceIfValid(display, eglSurface));
        return EGL_FALSE;
    }

    if (!display->getExtensions().querySurfacePointer)
    {
        thread->setSuccess();
        return EGL_FALSE;
    }

    if (surface == EGL_NO_SURFACE)
    {
        thread->setError(EglBadSurface(), GetDebug(), "eglQuerySurfacePointerANGLE",
                         GetSurfaceIfValid(display, eglSurface));
        return EGL_FALSE;
    }

    // validate the attribute parameter
    switch (attribute)
    {
        case EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE:
            if (!display->getExtensions().surfaceD3DTexture2DShareHandle)
            {
                thread->setError(EglBadAttribute(), GetDebug(), "eglQuerySurfacePointerANGLE",
                                 GetSurfaceIfValid(display, eglSurface));
                return EGL_FALSE;
            }
            break;
        case EGL_DXGI_KEYED_MUTEX_ANGLE:
            if (!display->getExtensions().keyedMutex)
            {
                thread->setError(EglBadAttribute(), GetDebug(), "eglQuerySurfacePointerANGLE",
                                 GetSurfaceIfValid(display, eglSurface));
                return EGL_FALSE;
            }
            break;
        default:
            thread->setError(EglBadAttribute(), GetDebug(), "eglQuerySurfacePointerANGLE",
                             GetSurfaceIfValid(display, eglSurface));
            return EGL_FALSE;
    }

    error = eglSurface->querySurfacePointerANGLE(attribute, value);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglQuerySurfacePointerANGLE",
                         GetSurfaceIfValid(display, eglSurface));
        return EGL_FALSE;
    }

    thread->setSuccess();
    return EGL_TRUE;
}

// EGL_NV_post_sub_buffer
EGLBoolean EGLAPIENTRY
PostSubBufferNV(EGLDisplay dpy, EGLSurface surface, EGLint x, EGLint y, EGLint width, EGLint height)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLint x = %d, EGLint y = %d, "
        "EGLint width = %d, EGLint height = %d)",
        dpy, surface, x, y, width, height);
    Thread *thread      = GetCurrentThread();
    Display *display    = static_cast<Display *>(dpy);
    Surface *eglSurface = static_cast<Surface *>(surface);

    if (x < 0 || y < 0 || width < 0 || height < 0)
    {
        thread->setError(EglBadParameter(), GetDebug(), "eglPostSubBufferNV",
                         GetSurfaceIfValid(display, eglSurface));
        return EGL_FALSE;
    }

    Error error = ValidateSurface(display, eglSurface);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglPostSubBufferNV",
                         GetSurfaceIfValid(display, eglSurface));
        return EGL_FALSE;
    }

    if (display->testDeviceLost())
    {
        thread->setError(EglContextLost(), GetDebug(), "eglPostSubBufferNV",
                         GetSurfaceIfValid(display, eglSurface));
        return EGL_FALSE;
    }

    if (surface == EGL_NO_SURFACE)
    {
        thread->setError(EglBadSurface(), GetDebug(), "eglPostSubBufferNV",
                         GetSurfaceIfValid(display, eglSurface));
        return EGL_FALSE;
    }

    if (!display->getExtensions().postSubBuffer)
    {
        // Spec is not clear about how this should be handled.
        thread->setSuccess();
        return EGL_TRUE;
    }

    // TODO(jmadill): Validate Surface is bound to the thread.
    error = eglSurface->postSubBuffer(thread->getContext(), x, y, width, height);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglPostSubBufferNV",
                         GetSurfaceIfValid(display, eglSurface));
        return EGL_FALSE;
    }

    thread->setSuccess();
    return EGL_TRUE;
}

// EGL_EXT_platform_base
EGLDisplay EGLAPIENTRY GetPlatformDisplayEXT(EGLenum platform,
                                             void *native_display,
                                             const EGLint *attrib_list)
{
    EVENT(
        "(EGLenum platform = %d, void* native_display = 0x%0.8p, const EGLint* attrib_list = "
        "0x%0.8p)",
        platform, native_display, attrib_list);
    Thread *thread = GetCurrentThread();

    Error err = ValidateGetPlatformDisplayEXT(platform, native_display, attrib_list);
    thread->setError(err, GetDebug(), "eglGetPlatformDisplayEXT", GetThreadIfValid(thread));
    if (err.isError())
    {
        return EGL_NO_DISPLAY;
    }

    const auto &attribMap = AttributeMap::CreateFromIntArray(attrib_list);
    if (platform == EGL_PLATFORM_ANGLE_ANGLE)
    {
        return Display::GetDisplayFromNativeDisplay(
            gl::bitCast<EGLNativeDisplayType>(native_display), attribMap);
    }
    else if (platform == EGL_PLATFORM_DEVICE_EXT)
    {
        Device *eglDevice = static_cast<Device *>(native_display);
        return Display::GetDisplayFromDevice(eglDevice, attribMap);
    }
    else
    {
        UNREACHABLE();
        return EGL_NO_DISPLAY;
    }
}

EGLSurface EGLAPIENTRY CreatePlatformWindowSurfaceEXT(EGLDisplay dpy,
                                                      EGLConfig config,
                                                      void *native_window,
                                                      const EGLint *attrib_list)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLConfig config = 0x%0.8p, void *native_window = 0x%0.8p, "
        "const EGLint *attrib_list = 0x%0.8p)",
        dpy, config, native_window, attrib_list);
    Thread *thread = GetCurrentThread();

    Display *display        = static_cast<Display *>(dpy);
    Config *configuration   = static_cast<Config *>(config);
    AttributeMap attributes = AttributeMap::CreateFromIntArray(attrib_list);

    ANGLE_EGL_TRY_RETURN(
        thread,
        ValidateCreatePlatformWindowSurfaceEXT(display, configuration, native_window, attributes),
        "eglCreatePlatformWindowSurfaceEXT", GetDisplayIfValid(display), EGL_NO_SURFACE);

    thread->setError(EglBadDisplay() << "CreatePlatformWindowSurfaceEXT unimplemented.", GetDebug(),
                     "eglCreatePlatformWindowSurfaceEXT", GetDisplayIfValid(display));
    return EGL_NO_SURFACE;
}

EGLSurface EGLAPIENTRY CreatePlatformPixmapSurfaceEXT(EGLDisplay dpy,
                                                      EGLConfig config,
                                                      void *native_pixmap,
                                                      const EGLint *attrib_list)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLConfig config = 0x%0.8p, void *native_pixmap = 0x%0.8p, "
        "const EGLint *attrib_list = 0x%0.8p)",
        dpy, config, native_pixmap, attrib_list);
    Thread *thread = GetCurrentThread();

    Display *display        = static_cast<Display *>(dpy);
    Config *configuration   = static_cast<Config *>(config);
    AttributeMap attributes = AttributeMap::CreateFromIntArray(attrib_list);

    ANGLE_EGL_TRY_RETURN(
        thread,
        ValidateCreatePlatformPixmapSurfaceEXT(display, configuration, native_pixmap, attributes),
        "eglCreatePlatformPixmapSurfaceEXT", GetDisplayIfValid(display), EGL_NO_SURFACE);

    thread->setError(EglBadDisplay() << "CreatePlatformPixmapSurfaceEXT unimplemented.", GetDebug(),
                     "eglCreatePlatformPixmapSurfaceEXT", GetDisplayIfValid(display));
    return EGL_NO_SURFACE;
}

// EGL_EXT_device_query
EGLBoolean EGLAPIENTRY QueryDeviceAttribEXT(EGLDeviceEXT device, EGLint attribute, EGLAttrib *value)
{
    EVENT("(EGLDeviceEXT device = 0x%0.8p, EGLint attribute = %d, EGLAttrib *value = 0x%0.8p)",
          device, attribute, value);
    Thread *thread = GetCurrentThread();

    Device *dev = static_cast<Device *>(device);

    Error error = ValidateDevice(dev);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglQueryDeviceAttribEXT", GetDeviceIfValid(dev));
        return EGL_FALSE;
    }

    // If the device was created by (and is owned by) a display, and that display doesn't support
    // device querying, then this call should fail
    Display *owningDisplay = dev->getOwningDisplay();
    if (owningDisplay != nullptr && !owningDisplay->getExtensions().deviceQuery)
    {
        thread->setError(EglBadAccess() << "Device wasn't created using eglCreateDeviceANGLE, "
                                           "and the Display that created it doesn't support "
                                           "device querying",
                         GetDebug(), "eglQueryDeviceAttribEXT", GetDeviceIfValid(dev));
        return EGL_FALSE;
    }

    // validate the attribute parameter
    switch (attribute)
    {
        case EGL_D3D11_DEVICE_ANGLE:
        case EGL_D3D9_DEVICE_ANGLE:
            if (!dev->getExtensions().deviceD3D || dev->getType() != attribute)
            {
                thread->setError(EglBadAttribute(), GetDebug(), "eglQueryDeviceAttribEXT",
                                 GetDeviceIfValid(dev));
                return EGL_FALSE;
            }
            error = dev->getDevice(value);
            if (error.isError())
            {
                thread->setError(error, GetDebug(), "eglQueryDeviceAttribEXT",
                                 GetDeviceIfValid(dev));
                return EGL_FALSE;
            }
            break;
        default:
            thread->setError(EglBadAttribute(), GetDebug(), "eglQueryDeviceAttribEXT",
                             GetDeviceIfValid(dev));
            return EGL_FALSE;
    }

    thread->setSuccess();
    return EGL_TRUE;
}

// EGL_EXT_device_query
const char *EGLAPIENTRY QueryDeviceStringEXT(EGLDeviceEXT device, EGLint name)
{
    EVENT("(EGLDeviceEXT device = 0x%0.8p, EGLint name = %d)", device, name);
    Thread *thread = GetCurrentThread();

    Device *dev = static_cast<Device *>(device);

    Error error = ValidateDevice(dev);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglQueryDeviceStringEXT", GetDeviceIfValid(dev));
        return EGL_FALSE;
    }

    const char *result;
    switch (name)
    {
        case EGL_EXTENSIONS:
            result = dev->getExtensionString().c_str();
            break;
        default:
            thread->setError(EglBadDevice(), GetDebug(), "eglQueryDeviceStringEXT",
                             GetDeviceIfValid(dev));
            return nullptr;
    }

    thread->setSuccess();
    return result;
}

// EGL_EXT_device_query
EGLBoolean EGLAPIENTRY QueryDisplayAttribEXT(EGLDisplay dpy, EGLint attribute, EGLAttrib *value)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLint attribute = %d, EGLAttrib *value = 0x%0.8p)", dpy,
          attribute, value);
    Thread *thread = GetCurrentThread();

    Display *display = static_cast<Display *>(dpy);

    Error error = ValidateDisplay(display);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglQueryDisplayAttribEXT", GetDisplayIfValid(display));
        return EGL_FALSE;
    }

    if (!display->getExtensions().deviceQuery)
    {
        thread->setError(EglBadAccess(), GetDebug(), "eglQueryDisplayAttribEXT",
                         GetDisplayIfValid(display));
        return EGL_FALSE;
    }

    // validate the attribute parameter
    switch (attribute)
    {
        case EGL_DEVICE_EXT:
            *value = reinterpret_cast<EGLAttrib>(display->getDevice());
            break;

        default:
            thread->setError(EglBadAttribute(), GetDebug(), "eglQueryDisplayAttribEXT",
                             GetDisplayIfValid(display));
            return EGL_FALSE;
    }

    thread->setSuccess();
    return EGL_TRUE;
}

ANGLE_EXPORT EGLImageKHR EGLAPIENTRY CreateImageKHR(EGLDisplay dpy,
                                                    EGLContext ctx,
                                                    EGLenum target,
                                                    EGLClientBuffer buffer,
                                                    const EGLint *attrib_list)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLContext ctx = 0x%0.8p, EGLenum target = 0x%X, "
        "EGLClientBuffer buffer = 0x%0.8p, const EGLAttrib *attrib_list = 0x%0.8p)",
        dpy, ctx, target, buffer, attrib_list);
    Thread *thread = GetCurrentThread();

    Display *display        = static_cast<Display *>(dpy);
    gl::Context *context    = static_cast<gl::Context *>(ctx);
    AttributeMap attributes = AttributeMap::CreateFromIntArray(attrib_list);

    Error error = ValidateCreateImageKHR(display, context, target, buffer, attributes);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglCreateImageKHR", GetDisplayIfValid(display));
        return EGL_NO_IMAGE;
    }

    Image *image = nullptr;
    error        = display->createImage(context, target, buffer, attributes, &image);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglCreateImageKHR", GetDisplayIfValid(display));
        return EGL_NO_IMAGE;
    }

    thread->setSuccess();
    return static_cast<EGLImage>(image);
}

ANGLE_EXPORT EGLBoolean EGLAPIENTRY DestroyImageKHR(EGLDisplay dpy, EGLImageKHR image)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLImage image = 0x%0.8p)", dpy, image);
    Thread *thread = GetCurrentThread();

    Display *display = static_cast<Display *>(dpy);
    Image *img       = static_cast<Image *>(image);

    Error error = ValidateDestroyImageKHR(display, img);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglDestroyImageKHR", GetImageIfValid(display, img));
        return EGL_FALSE;
    }

    display->destroyImage(img);

    thread->setSuccess();
    return EGL_TRUE;
}

ANGLE_EXPORT EGLDeviceEXT EGLAPIENTRY CreateDeviceANGLE(EGLint device_type,
                                                        void *native_device,
                                                        const EGLAttrib *attrib_list)
{
    EVENT(
        "(EGLint device_type = %d, void* native_device = 0x%0.8p, const EGLAttrib* attrib_list = "
        "0x%0.8p)",
        device_type, native_device, attrib_list);
    Thread *thread = GetCurrentThread();

    Error error = ValidateCreateDeviceANGLE(device_type, native_device, attrib_list);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglCreateDeviceANGLE", GetThreadIfValid(thread));
        return EGL_NO_DEVICE_EXT;
    }

    Device *device = nullptr;
    error          = Device::CreateDevice(device_type, native_device, &device);
    if (error.isError())
    {
        ASSERT(device == nullptr);
        thread->setError(error, GetDebug(), "eglCreateDeviceANGLE", GetThreadIfValid(thread));
        return EGL_NO_DEVICE_EXT;
    }

    thread->setSuccess();
    return device;
}

ANGLE_EXPORT EGLBoolean EGLAPIENTRY ReleaseDeviceANGLE(EGLDeviceEXT device)
{
    EVENT("(EGLDeviceEXT device = 0x%0.8p)", device);
    Thread *thread = GetCurrentThread();

    Device *dev = static_cast<Device *>(device);

    Error error = ValidateReleaseDeviceANGLE(dev);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglReleaseDeviceANGLE", GetDeviceIfValid(dev));
        return EGL_FALSE;
    }

    SafeDelete(dev);

    thread->setSuccess();
    return EGL_TRUE;
}

// EGL_KHR_stream
EGLStreamKHR EGLAPIENTRY CreateStreamKHR(EGLDisplay dpy, const EGLint *attrib_list)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, const EGLAttrib* attrib_list = 0x%0.8p)", dpy, attrib_list);
    Thread *thread = GetCurrentThread();

    Display *display        = static_cast<Display *>(dpy);
    AttributeMap attributes = AttributeMap::CreateFromIntArray(attrib_list);

    Error error = ValidateCreateStreamKHR(display, attributes);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglCreateStreamKHR", GetDisplayIfValid(display));
        return EGL_NO_STREAM_KHR;
    }

    Stream *stream;
    error = display->createStream(attributes, &stream);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglCreateStreamKHR", GetDisplayIfValid(display));
        return EGL_NO_STREAM_KHR;
    }

    thread->setSuccess();
    return static_cast<EGLStreamKHR>(stream);
}

EGLBoolean EGLAPIENTRY DestroyStreamKHR(EGLDisplay dpy, EGLStreamKHR stream)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLStreamKHR = 0x%0.8p)", dpy, stream);
    Thread *thread = GetCurrentThread();

    Display *display     = static_cast<Display *>(dpy);
    Stream *streamObject = static_cast<Stream *>(stream);

    Error error = ValidateDestroyStreamKHR(display, streamObject);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglDestroyStreamKHR",
                         GetStreamIfValid(display, streamObject));
        return EGL_FALSE;
    }

    display->destroyStream(streamObject);

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY StreamAttribKHR(EGLDisplay dpy,
                                       EGLStreamKHR stream,
                                       EGLenum attribute,
                                       EGLint value)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLStreamKHR stream = 0x%0.8p, EGLenum attribute = 0x%X, "
        "EGLint value = 0x%X)",
        dpy, stream, attribute, value);
    Thread *thread = GetCurrentThread();

    Display *display     = static_cast<Display *>(dpy);
    Stream *streamObject = static_cast<Stream *>(stream);

    Error error = ValidateStreamAttribKHR(display, streamObject, attribute, value);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglStreamAttribKHR",
                         GetStreamIfValid(display, streamObject));
        return EGL_FALSE;
    }

    switch (attribute)
    {
        case EGL_CONSUMER_LATENCY_USEC_KHR:
            streamObject->setConsumerLatency(value);
            break;
        case EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR:
            streamObject->setConsumerAcquireTimeout(value);
            break;
        default:
            UNREACHABLE();
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY QueryStreamKHR(EGLDisplay dpy,
                                      EGLStreamKHR stream,
                                      EGLenum attribute,
                                      EGLint *value)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLStreamKHR stream = 0x%0.8p, EGLenum attribute = 0x%X, "
        "EGLint value = 0x%0.8p)",
        dpy, stream, attribute, value);
    Thread *thread = GetCurrentThread();

    Display *display     = static_cast<Display *>(dpy);
    Stream *streamObject = static_cast<Stream *>(stream);

    Error error = ValidateQueryStreamKHR(display, streamObject, attribute, value);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglQueryStreamKHR",
                         GetStreamIfValid(display, streamObject));
        return EGL_FALSE;
    }

    switch (attribute)
    {
        case EGL_STREAM_STATE_KHR:
            *value = streamObject->getState();
            break;
        case EGL_CONSUMER_LATENCY_USEC_KHR:
            *value = streamObject->getConsumerLatency();
            break;
        case EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR:
            *value = streamObject->getConsumerAcquireTimeout();
            break;
        default:
            UNREACHABLE();
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY QueryStreamu64KHR(EGLDisplay dpy,
                                         EGLStreamKHR stream,
                                         EGLenum attribute,
                                         EGLuint64KHR *value)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLStreamKHR stream = 0x%0.8p, EGLenum attribute = 0x%X, "
        "EGLuint64KHR value = 0x%0.8p)",
        dpy, stream, attribute, value);
    Thread *thread = GetCurrentThread();

    Display *display     = static_cast<Display *>(dpy);
    Stream *streamObject = static_cast<Stream *>(stream);

    Error error = ValidateQueryStreamu64KHR(display, streamObject, attribute, value);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglQueryStreamu64KHR",
                         GetStreamIfValid(display, streamObject));
        return EGL_FALSE;
    }

    switch (attribute)
    {
        case EGL_PRODUCER_FRAME_KHR:
            *value = streamObject->getProducerFrame();
            break;
        case EGL_CONSUMER_FRAME_KHR:
            *value = streamObject->getConsumerFrame();
            break;
        default:
            UNREACHABLE();
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY StreamConsumerGLTextureExternalKHR(EGLDisplay dpy, EGLStreamKHR stream)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLStreamKHR = 0x%0.8p)", dpy, stream);
    Thread *thread = GetCurrentThread();

    Display *display     = static_cast<Display *>(dpy);
    Stream *streamObject = static_cast<Stream *>(stream);
    gl::Context *context = gl::GetValidGlobalContext();

    Error error = ValidateStreamConsumerGLTextureExternalKHR(display, context, streamObject);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglStreamConsumerGLTextureExternalKHR",
                         GetStreamIfValid(display, streamObject));
        return EGL_FALSE;
    }

    error = streamObject->createConsumerGLTextureExternal(AttributeMap(), context);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglStreamConsumerGLTextureExternalKHR",
                         GetStreamIfValid(display, streamObject));
        return EGL_FALSE;
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY StreamConsumerAcquireKHR(EGLDisplay dpy, EGLStreamKHR stream)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLStreamKHR = 0x%0.8p)", dpy, stream);
    Thread *thread = GetCurrentThread();

    Display *display     = static_cast<Display *>(dpy);
    Stream *streamObject = static_cast<Stream *>(stream);
    gl::Context *context = gl::GetValidGlobalContext();

    Error error = ValidateStreamConsumerAcquireKHR(display, context, streamObject);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglStreamConsumerAcquireKHR",
                         GetStreamIfValid(display, streamObject));
        return EGL_FALSE;
    }

    error = streamObject->consumerAcquire(context);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglStreamConsumerAcquireKHR",
                         GetStreamIfValid(display, streamObject));
        return EGL_FALSE;
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY StreamConsumerReleaseKHR(EGLDisplay dpy, EGLStreamKHR stream)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLStreamKHR = 0x%0.8p)", dpy, stream);
    Thread *thread = GetCurrentThread();

    Display *display     = static_cast<Display *>(dpy);
    Stream *streamObject = static_cast<Stream *>(stream);
    gl::Context *context = gl::GetValidGlobalContext();

    Error error = ValidateStreamConsumerReleaseKHR(display, context, streamObject);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglSStreamConsumerReleaseKHR",
                         GetStreamIfValid(display, streamObject));
        return EGL_FALSE;
    }

    error = streamObject->consumerRelease(context);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglStreamConsumerReleaseKHR",
                         GetStreamIfValid(display, streamObject));
        return EGL_FALSE;
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY StreamConsumerGLTextureExternalAttribsNV(EGLDisplay dpy,
                                                                EGLStreamKHR stream,
                                                                const EGLAttrib *attrib_list)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLStreamKHR stream = 0x%0.8p, EGLAttrib attrib_list = 0x%0.8p",
        dpy, stream, attrib_list);
    Thread *thread = GetCurrentThread();

    Display *display        = static_cast<Display *>(dpy);
    Stream *streamObject    = static_cast<Stream *>(stream);
    gl::Context *context    = gl::GetValidGlobalContext();
    AttributeMap attributes = AttributeMap::CreateFromAttribArray(attrib_list);

    Error error = ValidateStreamConsumerGLTextureExternalAttribsNV(display, context, streamObject,
                                                                   attributes);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglStreamConsumerGLTextureExternalAttribsNV",
                         GetStreamIfValid(display, streamObject));
        return EGL_FALSE;
    }

    error = streamObject->createConsumerGLTextureExternal(attributes, context);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglStreamConsumerGLTextureExternalAttribsNV",
                         GetStreamIfValid(display, streamObject));
        return EGL_FALSE;
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY CreateStreamProducerD3DTextureANGLE(EGLDisplay dpy,
                                                           EGLStreamKHR stream,
                                                           const EGLAttrib *attrib_list)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLStreamKHR stream = 0x%0.8p, EGLAttrib attrib_list = 0x%0.8p",
        dpy, stream, attrib_list);
    Thread *thread = GetCurrentThread();

    Display *display        = static_cast<Display *>(dpy);
    Stream *streamObject    = static_cast<Stream *>(stream);
    AttributeMap attributes = AttributeMap::CreateFromAttribArray(attrib_list);

    Error error = ValidateCreateStreamProducerD3DTextureANGLE(display, streamObject, attributes);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglCreateStreamProducerD3DTextureANGLE",
                         GetStreamIfValid(display, streamObject));
        return EGL_FALSE;
    }

    error = streamObject->createProducerD3D11Texture(attributes);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglCreateStreamProducerD3DTextureANGLE",
                         GetStreamIfValid(display, streamObject));
        return EGL_FALSE;
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY StreamPostD3DTextureANGLE(EGLDisplay dpy,
                                                 EGLStreamKHR stream,
                                                 void *texture,
                                                 const EGLAttrib *attrib_list)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLStreamKHR stream = 0x%0.8p, void* texture = 0x%0.8p, "
        "EGLAttrib attrib_list = 0x%0.8p",
        dpy, stream, texture, attrib_list);
    Thread *thread = GetCurrentThread();

    Display *display        = static_cast<Display *>(dpy);
    Stream *streamObject    = static_cast<Stream *>(stream);
    AttributeMap attributes = AttributeMap::CreateFromAttribArray(attrib_list);

    Error error = ValidateStreamPostD3DTextureANGLE(display, streamObject, texture, attributes);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglStreamPostD3DTextureANGLE",
                         GetStreamIfValid(display, streamObject));
        return EGL_FALSE;
    }

    error = streamObject->postD3D11Texture(texture, attributes);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglStreamPostD3DTextureANGLE",
                         GetStreamIfValid(display, streamObject));
        return EGL_FALSE;
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY GetSyncValuesCHROMIUM(EGLDisplay dpy,
                                             EGLSurface surface,
                                             EGLuint64KHR *ust,
                                             EGLuint64KHR *msc,
                                             EGLuint64KHR *sbc)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLuint64KHR* ust = 0x%0.8p, "
        "EGLuint64KHR* msc = 0x%0.8p, EGLuint64KHR* sbc = 0x%0.8p",
        dpy, surface, ust, msc, sbc);
    Thread *thread = GetCurrentThread();

    Display *display    = static_cast<Display *>(dpy);
    Surface *eglSurface = static_cast<Surface *>(surface);

    Error error = ValidateGetSyncValuesCHROMIUM(display, eglSurface, ust, msc, sbc);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglGetSyncValuesCHROMIUM",
                         GetSurfaceIfValid(display, eglSurface));
        return EGL_FALSE;
    }

    error = eglSurface->getSyncValues(ust, msc, sbc);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglGetSyncValuesCHROMIUM",
                         GetSurfaceIfValid(display, eglSurface));
        return EGL_FALSE;
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY SwapBuffersWithDamageKHR(EGLDisplay dpy,
                                                EGLSurface surface,
                                                EGLint *rects,
                                                EGLint n_rects)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLint *rects = 0x%0.8p, EGLint "
        "n_rects = %d)",
        dpy, surface, rects, n_rects);
    Thread *thread = GetCurrentThread();

    Display *display    = static_cast<Display *>(dpy);
    Surface *eglSurface = static_cast<Surface *>(surface);

    Error error = ValidateSwapBuffersWithDamageKHR(display, eglSurface, rects, n_rects);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglSwapBuffersWithDamageEXT",
                         GetSurfaceIfValid(display, eglSurface));
        return EGL_FALSE;
    }

    error = eglSurface->swapWithDamage(thread->getContext(), rects, n_rects);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglSwapBuffersWithDamageEXT",
                         GetSurfaceIfValid(display, eglSurface));
        return EGL_FALSE;
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLBoolean EGLAPIENTRY PresentationTimeANDROID(EGLDisplay dpy,
                                               EGLSurface surface,
                                               EGLnsecsANDROID time)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLSurface surface = 0x%0.8p, EGLnsecsANDROID time = %d)",
          dpy, surface, time);
    Thread *thread = GetCurrentThread();

    Display *display    = static_cast<Display *>(dpy);
    Surface *eglSurface = static_cast<Surface *>(surface);

    ANGLE_EGL_TRY_RETURN(thread, ValidatePresentationTimeANDROID(display, eglSurface, time),
                         "eglPresentationTimeANDROID", GetSurfaceIfValid(display, eglSurface),
                         EGL_FALSE);
    ANGLE_EGL_TRY_RETURN(thread, eglSurface->setPresentationTime(time),
                         "eglPresentationTimeANDROID", GetSurfaceIfValid(display, eglSurface),
                         EGL_FALSE);

    return EGL_TRUE;
}

EGLint EGLAPIENTRY ProgramCacheGetAttribANGLE(EGLDisplay dpy, EGLenum attrib)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLenum attrib = 0x%X)", dpy, attrib);

    Display *display = static_cast<Display *>(dpy);
    Thread *thread   = GetCurrentThread();

    ANGLE_EGL_TRY_RETURN(thread, ValidateProgramCacheGetAttribANGLE(display, attrib),
                         "eglProgramCacheGetAttribANGLE", GetDisplayIfValid(display), 0);

    thread->setSuccess();
    return display->programCacheGetAttrib(attrib);
}

void EGLAPIENTRY ProgramCacheQueryANGLE(EGLDisplay dpy,
                                        EGLint index,
                                        void *key,
                                        EGLint *keysize,
                                        void *binary,
                                        EGLint *binarysize)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, EGLint index = %d, void *key = 0x%0.8p, EGLint *keysize = "
        "0x%0.8p, void *binary = 0x%0.8p, EGLint *size = 0x%0.8p)",
        dpy, index, key, keysize, binary, binarysize);

    Display *display = static_cast<Display *>(dpy);
    Thread *thread   = GetCurrentThread();

    ANGLE_EGL_TRY(thread,
                  ValidateProgramCacheQueryANGLE(display, index, key, keysize, binary, binarysize),
                  "eglProgramCacheQueryANGLE", GetDisplayIfValid(display));

    ANGLE_EGL_TRY(thread, display->programCacheQuery(index, key, keysize, binary, binarysize),
                  "eglProgramCacheQueryANGLE", GetDisplayIfValid(display));

    thread->setSuccess();
}

void EGLAPIENTRY ProgramCachePopulateANGLE(EGLDisplay dpy,
                                           const void *key,
                                           EGLint keysize,
                                           const void *binary,
                                           EGLint binarysize)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8p, void *key = 0x%0.8p, EGLint keysize = %d, void *binary = "
        "0x%0.8p, EGLint *size = 0x%0.8p)",
        dpy, key, keysize, binary, binarysize);

    Display *display = static_cast<Display *>(dpy);
    Thread *thread   = GetCurrentThread();

    ANGLE_EGL_TRY(thread,
                  ValidateProgramCachePopulateANGLE(display, key, keysize, binary, binarysize),
                  "eglProgramCachePopulateANGLE", GetDisplayIfValid(display));

    ANGLE_EGL_TRY(thread, display->programCachePopulate(key, keysize, binary, binarysize),
                  "eglProgramCachePopulateANGLE", GetDisplayIfValid(display));

    thread->setSuccess();
}

EGLint EGLAPIENTRY ProgramCacheResizeANGLE(EGLDisplay dpy, EGLint limit, EGLenum mode)
{
    EVENT("(EGLDisplay dpy = 0x%0.8p, EGLint limit = %d, EGLenum mode = 0x%X)", dpy, limit, mode);

    Display *display = static_cast<Display *>(dpy);
    Thread *thread   = GetCurrentThread();

    ANGLE_EGL_TRY_RETURN(thread, ValidateProgramCacheResizeANGLE(display, limit, mode),
                         "eglProgramCacheResizeANGLE", GetDisplayIfValid(display), 0);

    thread->setSuccess();
    return display->programCacheResize(limit, mode);
}

EGLint EGLAPIENTRY DebugMessageControlKHR(EGLDEBUGPROCKHR callback, const EGLAttrib *attrib_list)
{
    EVENT("(EGLDEBUGPROCKHR callback = 0x%0.8p, EGLAttrib attrib_list = 0x%0.8p)", callback,
          attrib_list);

    Thread *thread = GetCurrentThread();

    AttributeMap attributes = AttributeMap::CreateFromAttribArray(attrib_list);

    Error error = ValidateDebugMessageControlKHR(callback, attributes);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglDebugMessageControlKHR", nullptr);
        return error.getCode();
    }

    Debug *debug = GetDebug();
    debug->setCallback(callback, attributes);

    thread->setSuccess();
    return EGL_SUCCESS;
}

EGLBoolean EGLAPIENTRY QueryDebugKHR(EGLint attribute, EGLAttrib *value)
{
    EVENT("(EGLint attribute = 0x%X, EGLAttrib* value = 0x%0.8p)", attribute, value);

    Thread *thread = GetCurrentThread();

    Error error = ValidateQueryDebugKHR(attribute, value);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglQueryDebugKHR", nullptr);
        return EGL_FALSE;
    }

    Debug *debug = GetDebug();
    switch (attribute)
    {
        case EGL_DEBUG_MSG_CRITICAL_KHR:
        case EGL_DEBUG_MSG_ERROR_KHR:
        case EGL_DEBUG_MSG_WARN_KHR:
        case EGL_DEBUG_MSG_INFO_KHR:
            *value = debug->isMessageTypeEnabled(FromEGLenum<MessageType>(attribute)) ? EGL_TRUE
                                                                                      : EGL_FALSE;
            break;
        case EGL_DEBUG_CALLBACK_KHR:
            *value = reinterpret_cast<EGLAttrib>(debug->getCallback());
            break;

        default:
            UNREACHABLE();
    }

    thread->setSuccess();
    return EGL_TRUE;
}

EGLint EGLAPIENTRY LabelObjectKHR(EGLDisplay dpy,
                                  EGLenum objectType,
                                  EGLObjectKHR object,
                                  EGLLabelKHR label)
{
    EVENT(
        "(EGLDisplay dpy = 0x%0.8pf, EGLenum objectType = 0x%X, EGLObjectKHR object = 0x%0.8p, "
        "EGLLabelKHR label = 0x%0.8p)",
        dpy, objectType, object, label);

    Display *display = static_cast<Display *>(dpy);
    Thread *thread   = GetCurrentThread();

    ObjectType objectTypePacked = FromEGLenum<ObjectType>(objectType);
    Error error = ValidateLabelObjectKHR(thread, display, objectTypePacked, object, label);
    if (error.isError())
    {
        thread->setError(error, GetDebug(), "eglLabelObjectKHR",
                         GetLabeledObjectIfValid(thread, display, objectTypePacked, object));
        return error.getCode();
    }

    LabeledObject *labeledObject =
        GetLabeledObjectIfValid(thread, display, objectTypePacked, object);
    ASSERT(labeledObject != nullptr);
    labeledObject->setLabel(label);

    thread->setSuccess();
    return EGL_SUCCESS;
}

}  // namespace egl
