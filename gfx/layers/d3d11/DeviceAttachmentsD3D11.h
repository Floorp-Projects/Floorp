/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_d3d11_DeviceAttachmentsD3D11_h
#define mozilla_gfx_layers_d3d11_DeviceAttachmentsD3D11_h

#include "mozilla/EnumeratedArray.h"
#include "mozilla/RefPtr.h"
#include "mozilla/layers/CompositorTypes.h"
#include <d3d11.h>
#include <dxgi1_2.h>

struct ShaderBytes;

namespace mozilla {
namespace layers {

struct DeviceAttachmentsD3D11
{
  explicit DeviceAttachmentsD3D11(ID3D11Device* device);

  bool Initialize(nsCString* const aOutFailureReason);
  bool InitBlendShaders();

  bool EnsureTriangleBuffer(size_t aNumTriangles);

  typedef EnumeratedArray<MaskType, MaskType::NumMaskTypes, RefPtr<ID3D11VertexShader>>
          VertexShaderArray;
  typedef EnumeratedArray<MaskType, MaskType::NumMaskTypes, RefPtr<ID3D11PixelShader>>
          PixelShaderArray;

  RefPtr<ID3D11InputLayout> mInputLayout;
  RefPtr<ID3D11InputLayout> mDynamicInputLayout;

  RefPtr<ID3D11Buffer> mVertexBuffer;
  RefPtr<ID3D11Buffer> mDynamicVertexBuffer;

  VertexShaderArray mVSQuadShader;
  VertexShaderArray mVSQuadBlendShader;

  VertexShaderArray mVSDynamicShader;
  VertexShaderArray mVSDynamicBlendShader;

  PixelShaderArray mSolidColorShader;
  PixelShaderArray mRGBAShader;
  PixelShaderArray mRGBShader;
  PixelShaderArray mYCbCrShader;
  PixelShaderArray mNV12Shader;
  PixelShaderArray mComponentAlphaShader;
  PixelShaderArray mBlendShader;
  RefPtr<ID3D11Buffer> mPSConstantBuffer;
  RefPtr<ID3D11Buffer> mVSConstantBuffer;
  RefPtr<ID3D11RasterizerState> mRasterizerState;
  RefPtr<ID3D11SamplerState> mLinearSamplerState;
  RefPtr<ID3D11SamplerState> mPointSamplerState;

  RefPtr<ID3D11BlendState> mPremulBlendState;
  RefPtr<ID3D11BlendState> mNonPremulBlendState;
  RefPtr<ID3D11BlendState> mComponentBlendState;
  RefPtr<ID3D11BlendState> mDisabledBlendState;
  RefPtr<IDXGIResource> mSyncTexture;
  HANDLE mSyncHandle;

private:
  bool CreateShaders();
  bool InitSyncObject();

  void InitVertexShader(const ShaderBytes& aShader, VertexShaderArray& aArray, MaskType aMaskType) {
    InitVertexShader(aShader, getter_AddRefs(aArray[aMaskType]));
  }
  void InitPixelShader(const ShaderBytes& aShader, PixelShaderArray& aArray, MaskType aMaskType) {
    InitPixelShader(aShader, getter_AddRefs(aArray[aMaskType]));
  }

  void InitVertexShader(const ShaderBytes& aShader, ID3D11VertexShader** aOut);
  void InitPixelShader(const ShaderBytes& aShader, ID3D11PixelShader** aOut);

  bool Failed(HRESULT hr, const char* aContext);

private:
  size_t mMaximumTriangles;

  // Only used during initialization.
  RefPtr<ID3D11Device> mDevice;
  bool mInitOkay;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_d3d11_DeviceAttachmentsD3D11_h
