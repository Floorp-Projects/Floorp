/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLBlitHelper.h"

#include <d3d11.h>

#include "GLContext.h"
#include "GLLibraryEGL.h"
#include "GPUVideoImage.h"
#include "ScopedGLHelpers.h"

#include "mozilla/layers/D3D11YCbCrImage.h"
#include "mozilla/layers/TextureD3D11.h"

namespace mozilla {
namespace gl {

static EGLStreamKHR
StreamFromD3DTexture(ID3D11Texture2D* const texD3D, const EGLAttrib* const postAttribs)
{
    auto* egl = gl::GLLibraryEGL::Get();
    if (!egl->IsExtensionSupported(GLLibraryEGL::NV_stream_consumer_gltexture_yuv) ||
        !egl->IsExtensionSupported(GLLibraryEGL::ANGLE_stream_producer_d3d_texture))
    {
        return 0;
    }

    const auto& display = egl->Display();
    const auto stream = egl->fCreateStreamKHR(display, nullptr);
    MOZ_ASSERT(stream);
    if (!stream)
        return 0;
    bool ok = true;
    MOZ_ALWAYS_TRUE( ok &= bool(egl->fStreamConsumerGLTextureExternalAttribsNV(display,
                                                                               stream,
                                                                               nullptr)) );
    MOZ_ALWAYS_TRUE( ok &= bool(egl->fCreateStreamProducerD3DTextureANGLE(display, stream,
                                                                          nullptr)) );
    MOZ_ALWAYS_TRUE( ok &= bool(egl->fStreamPostD3DTextureANGLE(display, stream, texD3D,
                                                                postAttribs)) );
    if (ok)
        return stream;

    (void)egl->fDestroyStreamKHR(display, stream);
    return 0;
}

static RefPtr<ID3D11Texture2D>
OpenSharedTexture(ID3D11Device* const d3d, const WindowsHandle handle)
{
    RefPtr<ID3D11Texture2D> tex;
    auto hr = d3d->OpenSharedResource((HANDLE)handle, __uuidof(ID3D11Texture2D),
                                      (void**)(ID3D11Texture2D**)getter_AddRefs(tex));
    if (FAILED(hr)) {
        gfxCriticalError() << "Error code from OpenSharedResource: " << gfx::hexa(hr);
        return nullptr;
    }
    return tex;
}

// -------------------------------------

class BindAnglePlanes final
{
    const GLBlitHelper& mParent;
    const uint8_t mNumPlanes;
    const ScopedSaveMultiTex mMultiTex;
    GLuint mTempTexs[3];
    EGLStreamKHR mStreams[3];
    RefPtr<IDXGIKeyedMutex> mMutexList[3];
    bool mSuccess;

public:
    BindAnglePlanes(const GLBlitHelper* const parent, const uint8_t numPlanes,
                    const RefPtr<ID3D11Texture2D>* const texD3DList,
                    const EGLAttrib* const* postAttribsList = nullptr)
        : mParent(*parent)
        , mNumPlanes(numPlanes)
        , mMultiTex(mParent.mGL, mNumPlanes, LOCAL_GL_TEXTURE_EXTERNAL)
        , mTempTexs{0}
        , mStreams{0}
        , mSuccess(true)
    {
        MOZ_RELEASE_ASSERT(numPlanes >= 1 && numPlanes <= 3);

        const auto& gl = mParent.mGL;
        auto* egl = gl::GLLibraryEGL::Get();
        const auto& display = egl->Display();

        gl->fGenTextures(numPlanes, mTempTexs);

        for (uint8_t i = 0; i < mNumPlanes; i++) {
            gl->fActiveTexture(LOCAL_GL_TEXTURE0 + i);
            gl->fBindTexture(LOCAL_GL_TEXTURE_EXTERNAL, mTempTexs[i]);
            const EGLAttrib* postAttribs = nullptr;
            if (postAttribsList) {
                postAttribs = postAttribsList[i];
            }
            mStreams[i] = StreamFromD3DTexture(texD3DList[i], postAttribs);
            mSuccess &= bool(mStreams[i]);
        }

        if (mSuccess) {
            for (uint8_t i = 0; i < mNumPlanes; i++) {
                MOZ_ALWAYS_TRUE( egl->fStreamConsumerAcquireKHR(display, mStreams[i]) );

                auto& mutex = mMutexList[i];
                texD3DList[i]->QueryInterface(IID_IDXGIKeyedMutex,
                                              (void**)getter_AddRefs(mutex));
                if (mutex) {
                    const auto hr = mutex->AcquireSync(0, 100);
                    if (FAILED(hr)) {
                        NS_WARNING("BindAnglePlanes failed to acquire KeyedMutex.");
                        mSuccess = false;
                    }
                }
            }
        }
    }

    ~BindAnglePlanes()
    {
        const auto& gl = mParent.mGL;
        auto* egl = gl::GLLibraryEGL::Get();
        const auto& display = egl->Display();

        if (mSuccess) {
            for (uint8_t i = 0; i < mNumPlanes; i++) {
                MOZ_ALWAYS_TRUE( egl->fStreamConsumerReleaseKHR(display, mStreams[i]) );
                if (mMutexList[i]) {
                    mMutexList[i]->ReleaseSync(0);
                }
            }
        }

        for (uint8_t i = 0; i < mNumPlanes; i++) {
            (void)egl->fDestroyStreamKHR(display, mStreams[i]);
        }

        gl->fDeleteTextures(mNumPlanes, mTempTexs);
    }

    const bool& Success() const { return mSuccess; }
};

// -------------------------------------

ID3D11Device*
GLBlitHelper::GetD3D11() const
{
    if (mD3D11)
        return mD3D11;

    if (!mGL->IsANGLE())
        return nullptr;

    auto* egl = gl::GLLibraryEGL::Get();
    EGLDeviceEXT deviceEGL = 0;
    MOZ_ALWAYS_TRUE( egl->fQueryDisplayAttribEXT(egl->Display(), LOCAL_EGL_DEVICE_EXT,
                                                 (EGLAttrib*)&deviceEGL) );
    if (!egl->fQueryDeviceAttribEXT(deviceEGL, LOCAL_EGL_D3D11_DEVICE_ANGLE,
                                    (EGLAttrib*)(ID3D11Device**)getter_AddRefs(mD3D11)))
    {
        MOZ_ASSERT(false, "d3d9?");
        return nullptr;
    }
    return mD3D11;
}

// -------------------------------------

bool
GLBlitHelper::BlitImage(layers::GPUVideoImage* const srcImage,
                        const gfx::IntSize& destSize, const OriginPos destOrigin) const
{
    const auto& data = srcImage->GetData();
    if (!data)
        return false;

    const auto& desc = data->SD();
    const auto& subdescUnion = desc.subdesc();
    switch (subdescUnion.type()) {
    case layers::GPUVideoSubDescriptor::TSurfaceDescriptorD3D10:
        {
            const auto& subdesc = subdescUnion.get_SurfaceDescriptorD3D10();
            return BlitDescriptor(subdesc, destSize, destOrigin);
        }
    case layers::GPUVideoSubDescriptor::TSurfaceDescriptorDXGIYCbCr:
        {
            const auto& subdesc = subdescUnion.get_SurfaceDescriptorDXGIYCbCr();

            const auto& clipSize = subdesc.size();
            const auto& ySize = subdesc.sizeY();
            const auto& uvSize = subdesc.sizeCbCr();
            const auto& colorSpace = subdesc.yUVColorSpace();

            const gfx::IntRect clipRect(0, 0, clipSize.width, clipSize.height);

            const WindowsHandle handles[3] = {
                subdesc.handleY(),
                subdesc.handleCb(),
                subdesc.handleCr()
            };
            return BlitAngleYCbCr(handles, clipRect, ySize, uvSize, colorSpace, destSize,
                                  destOrigin);
        }
    default:
        gfxCriticalError() << "Unhandled subdesc type: " << uint32_t(subdescUnion.type());
        return false;
    }
}

// -------------------------------------

bool
GLBlitHelper::BlitImage(layers::D3D11YCbCrImage* const srcImage,
                        const gfx::IntSize& destSize, const OriginPos destOrigin) const
{
    const auto& data = srcImage->GetData();
    if (!data)
        return false;

    const WindowsHandle handles[3] = {
        (WindowsHandle)data->mHandles[0],
        (WindowsHandle)data->mHandles[1],
        (WindowsHandle)data->mHandles[2]
    };
    return BlitAngleYCbCr(handles, srcImage->mPictureRect, srcImage->mYSize,
                          srcImage->mCbCrSize, srcImage->mColorSpace, destSize,
                          destOrigin);
}

// -------------------------------------

bool
GLBlitHelper::BlitDescriptor(const layers::SurfaceDescriptorD3D10& desc,
                             const gfx::IntSize& destSize,
                             const OriginPos destOrigin) const
{
    const auto& d3d = GetD3D11();
    if (!d3d)
        return false;

    const auto& handle = desc.handle();
    const auto& format = desc.format();
    const auto& clipSize = desc.size();

    const auto srcOrigin = OriginPos::BottomLeft;
    const gfx::IntRect clipRect(0, 0, clipSize.width, clipSize.height);
    const auto colorSpace = YUVColorSpace::BT601;

    if (format != gfx::SurfaceFormat::NV12) {
        gfxCriticalError() << "Non-NV12 format for SurfaceDescriptorD3D10: "
                           << uint32_t(format);
        return false;
    }

    const auto tex = OpenSharedTexture(d3d, handle);
    if (!tex) {
        MOZ_ASSERT(false, "Get a nullptr from OpenSharedResource.");
        return false;
    }
    const RefPtr<ID3D11Texture2D> texList[2] = { tex, tex };
    const EGLAttrib postAttribs0[] = {
        LOCAL_EGL_NATIVE_BUFFER_PLANE_OFFSET_IMG, 0,
        LOCAL_EGL_NONE
    };
    const EGLAttrib postAttribs1[] = {
        LOCAL_EGL_NATIVE_BUFFER_PLANE_OFFSET_IMG, 1,
        LOCAL_EGL_NONE
    };
    const EGLAttrib* const postAttribsList[2] = { postAttribs0, postAttribs1 };
    // /layers/d3d11/CompositorD3D11.cpp uses bt601 for EffectTypes::NV12.
    //return BlitAngleNv12(tex, YUVColorSpace::BT601, destSize, destOrigin);

    const BindAnglePlanes bindPlanes(this, 2, texList, postAttribsList);
    if (!bindPlanes.Success()) {
        MOZ_ASSERT(false, "BindAnglePlanes failed.");
        return false;
    }

    D3D11_TEXTURE2D_DESC texDesc = {0};
    tex->GetDesc(&texDesc);

    const gfx::IntSize ySize(texDesc.Width, texDesc.Height);
    const gfx::IntSize divisors(2, 2);
    MOZ_ASSERT(ySize.width % divisors.width == 0);
    MOZ_ASSERT(ySize.height % divisors.height == 0);
    const gfx::IntSize uvSize(ySize.width / divisors.width,
                              ySize.height / divisors.height);

    const bool yFlip = destOrigin != srcOrigin;
    const DrawBlitProg::BaseArgs baseArgs = {
        SubRectMat3(clipRect, ySize),
        yFlip, destSize, Nothing()
    };
    const DrawBlitProg::YUVArgs yuvArgs = {
        SubRectMat3(clipRect, uvSize, divisors),
        colorSpace
    };

    const auto& prog = GetDrawBlitProg({kFragHeader_TexExt, kFragBody_NV12});
    MOZ_RELEASE_ASSERT(prog);
    prog->Draw(baseArgs, &yuvArgs);
    return true;
}

// --

bool
GLBlitHelper::BlitAngleYCbCr(const WindowsHandle (&handleList)[3],
                             const gfx::IntRect& clipRect,
                             const gfx::IntSize& ySize, const gfx::IntSize& uvSize,
                             const YUVColorSpace colorSpace, const gfx::IntSize& destSize,
                             const OriginPos destOrigin) const
{
    const auto& d3d = GetD3D11();
    if (!d3d)
        return false;

    const auto srcOrigin = OriginPos::BottomLeft;

    gfx::IntSize divisors;
    if (!GuessDivisors(ySize, uvSize, &divisors))
        return false;

    const RefPtr<ID3D11Texture2D> texList[3] = {
        OpenSharedTexture(d3d, handleList[0]),
        OpenSharedTexture(d3d, handleList[1]),
        OpenSharedTexture(d3d, handleList[2])
    };
    const BindAnglePlanes bindPlanes(this, 3, texList);

    const bool yFlip = destOrigin != srcOrigin;
    const DrawBlitProg::BaseArgs baseArgs = {
        SubRectMat3(clipRect, ySize),
        yFlip, destSize, Nothing()
    };
    const DrawBlitProg::YUVArgs yuvArgs = {
        SubRectMat3(clipRect, uvSize, divisors),
        colorSpace
    };

    const auto& prog = GetDrawBlitProg({kFragHeader_TexExt, kFragBody_PlanarYUV});
    MOZ_RELEASE_ASSERT(prog);
    prog->Draw(baseArgs, &yuvArgs);
    return true;
}

} // namespace gl
} // namespace mozilla
