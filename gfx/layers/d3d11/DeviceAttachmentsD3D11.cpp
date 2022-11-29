/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeviceAttachmentsD3D11.h"
#include "mozilla/Telemetry.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/Compositor.h"
#include "CompositorD3D11Shaders.h"
#include "ShaderDefinitionsD3D11.h"

namespace mozilla {
namespace layers {

using namespace gfx;

DeviceAttachmentsD3D11::DeviceAttachmentsD3D11(ID3D11Device* device)
    : mDevice(device),
      mContinueInit(true),
      mInitialized(false),
      mDeviceReset(false) {}

DeviceAttachmentsD3D11::~DeviceAttachmentsD3D11() {}

/* static */
RefPtr<DeviceAttachmentsD3D11> DeviceAttachmentsD3D11::Create(
    ID3D11Device* aDevice) {
  // We don't return null even if the attachments object even if it fails to
  // initialize, so the compositor can grab the failure ID.
  RefPtr<DeviceAttachmentsD3D11> attachments =
      new DeviceAttachmentsD3D11(aDevice);
  attachments->Initialize();
  return attachments.forget();
}

bool DeviceAttachmentsD3D11::Initialize() {
  D3D11_INPUT_ELEMENT_DESC layout[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
       D3D11_INPUT_PER_VERTEX_DATA, 0},
  };

  HRESULT hr;
  hr = mDevice->CreateInputLayout(
      layout, sizeof(layout) / sizeof(D3D11_INPUT_ELEMENT_DESC), LayerQuadVS,
      sizeof(LayerQuadVS), getter_AddRefs(mInputLayout));

  if (Failed(hr, "CreateInputLayout")) {
    mInitFailureId = "FEATURE_FAILURE_D3D11_INPUT_LAYOUT";
    return false;
  }

  Vertex vertices[] = {{{0.0, 0.0}}, {{1.0, 0.0}}, {{0.0, 1.0}}, {{1.0, 1.0}}};
  CD3D11_BUFFER_DESC bufferDesc(sizeof(vertices), D3D11_BIND_VERTEX_BUFFER);
  D3D11_SUBRESOURCE_DATA data;
  data.pSysMem = (void*)vertices;

  hr = mDevice->CreateBuffer(&bufferDesc, &data, getter_AddRefs(mVertexBuffer));
  if (Failed(hr, "create vertex buffer")) {
    mInitFailureId = "FEATURE_FAILURE_D3D11_VERTEX_BUFFER";
    return false;
  }
  if (!CreateShaders()) {
    mInitFailureId = "FEATURE_FAILURE_D3D11_CREATE_SHADERS";
    return false;
  }

  CD3D11_BUFFER_DESC cBufferDesc(sizeof(VertexShaderConstants),
                                 D3D11_BIND_CONSTANT_BUFFER,
                                 D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

  hr = mDevice->CreateBuffer(&cBufferDesc, nullptr,
                             getter_AddRefs(mVSConstantBuffer));
  if (Failed(hr, "create vs buffer")) {
    mInitFailureId = "FEATURE_FAILURE_D3D11_VS_BUFFER";
    return false;
  }

  cBufferDesc.ByteWidth = sizeof(PixelShaderConstants);
  hr = mDevice->CreateBuffer(&cBufferDesc, nullptr,
                             getter_AddRefs(mPSConstantBuffer));
  if (Failed(hr, "create ps buffer")) {
    mInitFailureId = "FEATURE_FAILURE_D3D11_PS_BUFFER";
    return false;
  }

  CD3D11_RASTERIZER_DESC rastDesc(D3D11_DEFAULT);
  rastDesc.CullMode = D3D11_CULL_NONE;
  rastDesc.ScissorEnable = TRUE;

  hr = mDevice->CreateRasterizerState(&rastDesc,
                                      getter_AddRefs(mRasterizerState));
  if (Failed(hr, "create rasterizer")) {
    mInitFailureId = "FEATURE_FAILURE_D3D11_RASTERIZER";
    return false;
  }

  CD3D11_SAMPLER_DESC samplerDesc(D3D11_DEFAULT);
  hr = mDevice->CreateSamplerState(&samplerDesc,
                                   getter_AddRefs(mLinearSamplerState));
  if (Failed(hr, "create linear sampler")) {
    mInitFailureId = "FEATURE_FAILURE_D3D11_LINEAR_SAMPLER";
    return false;
  }

  samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
  hr = mDevice->CreateSamplerState(&samplerDesc,
                                   getter_AddRefs(mPointSamplerState));
  if (Failed(hr, "create point sampler")) {
    mInitFailureId = "FEATURE_FAILURE_D3D11_POINT_SAMPLER";
    return false;
  }

  CD3D11_BLEND_DESC blendDesc(D3D11_DEFAULT);
  D3D11_RENDER_TARGET_BLEND_DESC rtBlendPremul = {TRUE,
                                                  D3D11_BLEND_ONE,
                                                  D3D11_BLEND_INV_SRC_ALPHA,
                                                  D3D11_BLEND_OP_ADD,
                                                  D3D11_BLEND_ONE,
                                                  D3D11_BLEND_INV_SRC_ALPHA,
                                                  D3D11_BLEND_OP_ADD,
                                                  D3D11_COLOR_WRITE_ENABLE_ALL};
  blendDesc.RenderTarget[0] = rtBlendPremul;
  hr = mDevice->CreateBlendState(&blendDesc, getter_AddRefs(mPremulBlendState));
  if (Failed(hr, "create pm blender")) {
    mInitFailureId = "FEATURE_FAILURE_D3D11_PM_BLENDER";
    return false;
  }

  D3D11_RENDER_TARGET_BLEND_DESC rtCopyPremul = {TRUE,
                                                 D3D11_BLEND_ONE,
                                                 D3D11_BLEND_ZERO,
                                                 D3D11_BLEND_OP_ADD,
                                                 D3D11_BLEND_ONE,
                                                 D3D11_BLEND_ZERO,
                                                 D3D11_BLEND_OP_ADD,
                                                 D3D11_COLOR_WRITE_ENABLE_ALL};
  blendDesc.RenderTarget[0] = rtCopyPremul;
  hr = mDevice->CreateBlendState(&blendDesc, getter_AddRefs(mPremulCopyState));
  if (Failed(hr, "create pm copy blender")) {
    mInitFailureId = "FEATURE_FAILURE_D3D11_PM_COPY_BLENDER";
    return false;
  }

  D3D11_RENDER_TARGET_BLEND_DESC rtBlendNonPremul = {
      TRUE,
      D3D11_BLEND_SRC_ALPHA,
      D3D11_BLEND_INV_SRC_ALPHA,
      D3D11_BLEND_OP_ADD,
      D3D11_BLEND_ONE,
      D3D11_BLEND_INV_SRC_ALPHA,
      D3D11_BLEND_OP_ADD,
      D3D11_COLOR_WRITE_ENABLE_ALL};
  blendDesc.RenderTarget[0] = rtBlendNonPremul;
  hr = mDevice->CreateBlendState(&blendDesc,
                                 getter_AddRefs(mNonPremulBlendState));
  if (Failed(hr, "create npm blender")) {
    mInitFailureId = "FEATURE_FAILURE_D3D11_NPM_BLENDER";
    return false;
  }

  D3D11_RENDER_TARGET_BLEND_DESC rtBlendDisabled = {
      FALSE,
      D3D11_BLEND_SRC_ALPHA,
      D3D11_BLEND_INV_SRC_ALPHA,
      D3D11_BLEND_OP_ADD,
      D3D11_BLEND_ONE,
      D3D11_BLEND_INV_SRC_ALPHA,
      D3D11_BLEND_OP_ADD,
      D3D11_COLOR_WRITE_ENABLE_ALL};
  blendDesc.RenderTarget[0] = rtBlendDisabled;
  hr = mDevice->CreateBlendState(&blendDesc,
                                 getter_AddRefs(mDisabledBlendState));
  if (Failed(hr, "create null blender")) {
    mInitFailureId = "FEATURE_FAILURE_D3D11_NULL_BLENDER";
    return false;
  }

  if (!InitSyncObject()) {
    mInitFailureId = "FEATURE_FAILURE_D3D11_OBJ_SYNC";
    return false;
  }

  mInitialized = true;
  return true;
}

bool DeviceAttachmentsD3D11::InitSyncObject() {
  // Sync object is not supported on WARP.
  if (DeviceManagerDx::Get()->IsWARP()) {
    return true;
  }

  MOZ_ASSERT(!mSyncObject);
  MOZ_ASSERT(mDevice);

  mSyncObject = SyncObjectHost::CreateSyncObjectHost(mDevice);
  MOZ_ASSERT(mSyncObject);

  return mSyncObject->Init();
}

bool DeviceAttachmentsD3D11::CreateShaders() {
  InitVertexShader(sLayerQuadVS, mVSQuadShader);

  InitPixelShader(sSolidColorShader, mSolidColorShader);
  InitPixelShader(sRGBShader, mRGBShader);
  InitPixelShader(sRGBAShader, mRGBAShader);
  InitPixelShader(sYCbCrShader, mYCbCrShader);
  InitPixelShader(sNV12Shader, mNV12Shader);
  return mContinueInit;
}

void DeviceAttachmentsD3D11::InitVertexShader(const ShaderBytes& aShader,
                                              ID3D11VertexShader** aOut) {
  if (!mContinueInit) {
    return;
  }
  if (Failed(mDevice->CreateVertexShader(aShader.mData, aShader.mLength,
                                         nullptr, aOut),
             "create vs")) {
    mContinueInit = false;
  }
}

void DeviceAttachmentsD3D11::InitPixelShader(const ShaderBytes& aShader,
                                             ID3D11PixelShader** aOut) {
  if (!mContinueInit) {
    return;
  }
  if (Failed(mDevice->CreatePixelShader(aShader.mData, aShader.mLength, nullptr,
                                        aOut),
             "create ps")) {
    mContinueInit = false;
  }
}

bool DeviceAttachmentsD3D11::Failed(HRESULT hr, const char* aContext) {
  if (SUCCEEDED(hr)) {
    return false;
  }

  gfxCriticalNote << "[D3D11] " << aContext << " failed: " << hexa(hr);
  return true;
}

}  // namespace layers
}  // namespace mozilla
