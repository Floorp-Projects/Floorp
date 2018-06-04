/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(XP_WIN)
#include "CompositorD3D11.h"
#include "TextureD3D11.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#elif defined(XP_MACOSX)
#include "mozilla/gfx/MacIOSurface.h"
#endif

#include "mozilla/Base64.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "gfxPrefs.h"
#include "gfxUtils.h"
#include "gfxVRPuppet.h"
#include "VRManager.h"
#include "VRThread.h"

#include "mozilla/dom/GamepadEventTypes.h"
#include "mozilla/dom/GamepadBinding.h"

// See CompositorD3D11Shaders.h
namespace mozilla {
namespace layers {
struct ShaderBytes { const void* mData; size_t mLength; };
extern ShaderBytes sRGBShader;
extern ShaderBytes sLayerQuadVS;
} // namespace layers
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::gfx::impl;
using namespace mozilla::layers;

// Reminder: changing the order of these buttons may break web content
static const uint64_t kPuppetButtonMask[] = {
  1,
  2,
  4,
  8
};
static const uint32_t kNumPuppetButtonMask = sizeof(kPuppetButtonMask) /
                                             sizeof(uint64_t);
static const uint32_t kNumPuppetAxis = 3;
static const uint32_t kNumPuppetHaptcs = 1;

VRDisplayPuppet::VRDisplayPuppet()
 : VRDisplayHost(VRDeviceType::Puppet)
 , mIsPresenting(false)
 , mSensorState{}
{
  MOZ_COUNT_CTOR_INHERITED(VRDisplayPuppet, VRDisplayHost);

  VRDisplayState& state = mDisplayInfo.mDisplayState;
  strncpy(state.mDisplayName, "Puppet HMD", kVRDisplayNameMaxLen);
  state.mIsConnected = true;
  state.mIsMounted = false;
  state.mCapabilityFlags = VRDisplayCapabilityFlags::Cap_None |
                           VRDisplayCapabilityFlags::Cap_Orientation |
                           VRDisplayCapabilityFlags::Cap_Position |
                           VRDisplayCapabilityFlags::Cap_External |
                           VRDisplayCapabilityFlags::Cap_Present |
                           VRDisplayCapabilityFlags::Cap_StageParameters;
  state.mEyeResolution.width = 1836; // 1080 * 1.7
  state.mEyeResolution.height = 2040; // 1200 * 1.7

  // SteamVR gives the application a single FOV to use; it's not configurable as with Oculus
  for (uint32_t eye = 0; eye < 2; ++eye) {
    state.mEyeTranslation[eye].x = 0.0f;
    state.mEyeTranslation[eye].y = 0.0f;
    state.mEyeTranslation[eye].z = 0.0f;
    state.mEyeFOV[eye] = VRFieldOfView(45.0, 45.0, 45.0, 45.0);
  }

  // default: 1m x 1m space, 0.75m high in seated position
  state.mStageSize.width = 1.0f;
  state.mStageSize.height = 1.0f;

  state.mSittingToStandingTransform[0] = 1.0f;
  state.mSittingToStandingTransform[1] = 0.0f;
  state.mSittingToStandingTransform[2] = 0.0f;
  state.mSittingToStandingTransform[3] = 0.0f;

  state.mSittingToStandingTransform[4] = 0.0f;
  state.mSittingToStandingTransform[5] = 1.0f;
  state.mSittingToStandingTransform[6] = 0.0f;
  state.mSittingToStandingTransform[7] = 0.0f;

  state.mSittingToStandingTransform[8] = 0.0f;
  state.mSittingToStandingTransform[9] = 0.0f;
  state.mSittingToStandingTransform[10] = 1.0f;
  state.mSittingToStandingTransform[11] = 0.0f;

  state.mSittingToStandingTransform[12] = 0.0f;
  state.mSittingToStandingTransform[13] = 0.75f;
  state.mSittingToStandingTransform[14] = 0.0f;
  state.mSittingToStandingTransform[15] = 1.0f;

  gfx::Quaternion rot;

  mSensorState.flags |= VRDisplayCapabilityFlags::Cap_Orientation;
  mSensorState.orientation[0] = rot.x;
  mSensorState.orientation[1] = rot.y;
  mSensorState.orientation[2] = rot.z;
  mSensorState.orientation[3] = rot.w;
  mSensorState.angularVelocity[0] = 0.0f;
  mSensorState.angularVelocity[1] = 0.0f;
  mSensorState.angularVelocity[2] = 0.0f;

  mSensorState.flags |= VRDisplayCapabilityFlags::Cap_Position;
  mSensorState.position[0] = 0.0f;
  mSensorState.position[1] = 0.0f;
  mSensorState.position[2] = 0.0f;
  mSensorState.linearVelocity[0] = 0.0f;
  mSensorState.linearVelocity[1] = 0.0f;
  mSensorState.linearVelocity[2] = 0.0f;
}

VRDisplayPuppet::~VRDisplayPuppet()
{
  MOZ_COUNT_DTOR_INHERITED(VRDisplayPuppet, VRDisplayHost);
}

void
VRDisplayPuppet::SetDisplayInfo(const VRDisplayInfo& aDisplayInfo)
{
  // We are only interested in the eye and mount info of the display info.
  VRDisplayState& state = mDisplayInfo.mDisplayState;
  state.mEyeResolution = aDisplayInfo.mDisplayState.mEyeResolution;
  state.mIsMounted = aDisplayInfo.mDisplayState.mIsMounted;
  memcpy(&state.mEyeFOV, &aDisplayInfo.mDisplayState.mEyeFOV,
         sizeof(state.mEyeFOV[0]) * VRDisplayState::NumEyes);
  memcpy(&state.mEyeTranslation, &aDisplayInfo.mDisplayState.mEyeTranslation,
         sizeof(state.mEyeTranslation[0]) * VRDisplayState::NumEyes);
}

void
VRDisplayPuppet::Destroy()
{
  StopPresentation();
}

void
VRDisplayPuppet::ZeroSensor()
{
}

VRHMDSensorState
VRDisplayPuppet::GetSensorState()
{
  mSensorState.inputFrameID = mDisplayInfo.mFrameId;

  Matrix4x4 matHeadToEye[2];
  for (uint32_t eye = 0; eye < 2; ++eye) {
    matHeadToEye[eye].PreTranslate(mDisplayInfo.GetEyeTranslation(eye));
  }
  mSensorState.CalcViewMatrices(matHeadToEye);

  return mSensorState;
}

void
VRDisplayPuppet::SetSensorState(const VRHMDSensorState& aSensorState)
{
  memcpy(&mSensorState, &aSensorState, sizeof(mSensorState));
}

void
VRDisplayPuppet::StartPresentation()
{
  if (mIsPresenting) {
    return;
  }
  mIsPresenting = true;

#if defined(XP_WIN)
  if (!CreateD3DObjects()) {
    return;
  }

  if (FAILED(mDevice->CreateVertexShader(sLayerQuadVS.mData,
                      sLayerQuadVS.mLength, nullptr, &mQuadVS))) {
    NS_WARNING("Failed to create vertex shader for Puppet");
    return;
  }

  if (FAILED(mDevice->CreatePixelShader(sRGBShader.mData,
                      sRGBShader.mLength, nullptr, &mQuadPS))) {
    NS_WARNING("Failed to create pixel shader for Puppet");
    return;
  }

  CD3D11_BUFFER_DESC cBufferDesc(sizeof(layers::VertexShaderConstants),
                                 D3D11_BIND_CONSTANT_BUFFER,
                                 D3D11_USAGE_DYNAMIC,
                                 D3D11_CPU_ACCESS_WRITE);

  if (FAILED(mDevice->CreateBuffer(&cBufferDesc, nullptr, getter_AddRefs(mVSConstantBuffer)))) {
    NS_WARNING("Failed to vertex shader constant buffer for Puppet");
    return;
  }

  cBufferDesc.ByteWidth = sizeof(layers::PixelShaderConstants);
  if (FAILED(mDevice->CreateBuffer(&cBufferDesc, nullptr, getter_AddRefs(mPSConstantBuffer)))) {
    NS_WARNING("Failed to pixel shader constant buffer for Puppet");
    return;
  }

  CD3D11_SAMPLER_DESC samplerDesc(D3D11_DEFAULT);
  if (FAILED(mDevice->CreateSamplerState(&samplerDesc, getter_AddRefs(mLinearSamplerState)))) {
    NS_WARNING("Failed to create sampler state for Puppet");
    return;
  }

  D3D11_INPUT_ELEMENT_DESC layout[] =
  {
    { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
  };

  if (FAILED(mDevice->CreateInputLayout(layout,
                                        sizeof(layout) / sizeof(D3D11_INPUT_ELEMENT_DESC),
                                        sLayerQuadVS.mData,
                                        sLayerQuadVS.mLength,
                                        getter_AddRefs(mInputLayout)))) {
    NS_WARNING("Failed to create input layout for Puppet");
    return;
  }

  Vertex vertices[] = { { { 0.0, 0.0 } },{ { 1.0, 0.0 } },{ { 0.0, 1.0 } },{ { 1.0, 1.0 } } };
  CD3D11_BUFFER_DESC bufferDesc(sizeof(vertices), D3D11_BIND_VERTEX_BUFFER);
  D3D11_SUBRESOURCE_DATA data;
  data.pSysMem = (void*)vertices;

  if (FAILED(mDevice->CreateBuffer(&bufferDesc, &data, getter_AddRefs(mVertexBuffer)))) {
    NS_WARNING("Failed to create vertex buffer for Puppet");
    return;
  }

  memset(&mVSConstants, 0, sizeof(mVSConstants));
  memset(&mPSConstants, 0, sizeof(mPSConstants));
#endif // XP_WIN
}

void
VRDisplayPuppet::StopPresentation()
{
  if (!mIsPresenting) {
    return;
  }

  mIsPresenting = false;
}

#if defined(XP_WIN)
bool
VRDisplayPuppet::UpdateConstantBuffers()
{
  HRESULT hr;
  D3D11_MAPPED_SUBRESOURCE resource;
  resource.pData = nullptr;

  hr = mContext->Map(mVSConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
  if (FAILED(hr) || !resource.pData) {
    return false;
  }
  *(VertexShaderConstants*)resource.pData = mVSConstants;
  mContext->Unmap(mVSConstantBuffer, 0);
  resource.pData = nullptr;

  hr = mContext->Map(mPSConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
  if (FAILED(hr) || !resource.pData) {
    return false;
  }
  *(PixelShaderConstants*)resource.pData = mPSConstants;
  mContext->Unmap(mPSConstantBuffer, 0);

  ID3D11Buffer *buffer = mVSConstantBuffer;
  mContext->VSSetConstantBuffers(0, 1, &buffer);
  buffer = mPSConstantBuffer;
  mContext->PSSetConstantBuffers(0, 1, &buffer);
  return true;
}

bool
VRDisplayPuppet::SubmitFrame(ID3D11Texture2D* aSource,
                             const IntSize& aSize,
                             const gfx::Rect& aLeftEyeRect,
                             const gfx::Rect& aRightEyeRect)
{
  MOZ_ASSERT(mSubmitThread->GetThread() == NS_GetCurrentThread());
  if (!mIsPresenting) {
    return false;
  }

  if (!CreateD3DObjects()) {
    return false;
  }
  AutoRestoreRenderState restoreState(this);
  if (!restoreState.IsSuccess()) {
    return false;
  }

  VRManager *vm = VRManager::Get();
  MOZ_ASSERT(vm);

  switch (gfxPrefs::VRPuppetSubmitFrame()) {
    case 0:
      // The VR frame is not displayed.
      break;
    case 1:
    {
      // The frames are submitted to VR compositor are decoded
      // into a base64Image and dispatched to the DOM side.
      D3D11_TEXTURE2D_DESC desc;
      aSource->GetDesc(&desc);
      MOZ_ASSERT(desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM,
                 "Only support B8G8R8A8_UNORM format.");
      // Map the staging resource
      ID3D11Texture2D* mappedTexture = nullptr;
      D3D11_MAPPED_SUBRESOURCE mapInfo;
      HRESULT hr = mContext->Map(aSource,
                                 0,  // Subsource
                                 D3D11_MAP_READ,
                                 0,  // MapFlags
                                 &mapInfo);

      if (FAILED(hr)) {
        // If we can't map this texture, copy it to a staging resource.
        if (hr == E_INVALIDARG) {
          D3D11_TEXTURE2D_DESC desc2;
          desc2.Width = desc.Width;
          desc2.Height = desc.Height;
          desc2.MipLevels = desc.MipLevels;
          desc2.ArraySize = desc.ArraySize;
          desc2.Format = desc.Format;
          desc2.SampleDesc = desc.SampleDesc;
          desc2.Usage = D3D11_USAGE_STAGING;
          desc2.BindFlags = 0;
          desc2.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
          desc2.MiscFlags = 0;

          ID3D11Texture2D* stagingTexture = nullptr;
          hr = mDevice->CreateTexture2D(&desc2, nullptr, &stagingTexture);
          if (FAILED(hr)) {
            MOZ_ASSERT(false, "Failed to create a staging texture");
            return false;
          }
          // Copy the texture to a staging resource
          mContext->CopyResource(stagingTexture, aSource);
          // Map the staging resource
          hr = mContext->Map(stagingTexture,
                             0,  // Subsource
                             D3D11_MAP_READ,
                             0,  // MapFlags
                             &mapInfo);
          if (FAILED(hr)) {
            MOZ_ASSERT(false, "Failed to map staging texture");
          }
          mappedTexture = stagingTexture;
        } else {
          MOZ_ASSERT(false, "Failed to map staging texture");
          return false;
        }
      } else {
        mappedTexture = aSource;
      }
      // Ideally, we should convert the srcData to a PNG image and decode it
      // to a Base64 string here, but the GPU process does not have the privilege to
      // access the image library. So, we have to convert the RAW image data
      // to a base64 string and forward it to let the content process to
      // do the image conversion.
      const char* srcData = static_cast<const char*>(mapInfo.pData);
      VRSubmitFrameResultInfo result;
      result.mFormat = SurfaceFormat::B8G8R8A8;
      result.mWidth = desc.Width;
      result.mHeight = desc.Height;
      result.mFrameNum = mDisplayInfo.mFrameId;
      // If the original texture size is not pow of 2, the data will not be tightly strided.
      // We have to copy the pixels by rows.
      nsCString rawString;
      for (uint32_t i = 0; i < desc.Height; i++) {
        rawString += Substring(srcData + i * mapInfo.RowPitch,
                               desc.Width * 4);
      }
      mContext->Unmap(mappedTexture, 0);

      if (Base64Encode(rawString, result.mBase64Image) != NS_OK) {
        MOZ_ASSERT(false, "Failed to encode base64 images.");
      }
      // Dispatch the base64 encoded string to the DOM side. Then, it will be decoded
      // and convert to a PNG image there.
      MessageLoop* loop = VRListenerThreadHolder::Loop();
      loop->PostTask(NewRunnableMethod<const uint32_t, VRSubmitFrameResultInfo>(
        "VRManager::DispatchSubmitFrameResult",
        vm, &VRManager::DispatchSubmitFrameResult, mDisplayInfo.mDisplayID, result
      ));
      break;
    }
    case 2:
    {
      // The VR compositor sumbmit frame to the screen window,
      // the current coordinate is at (0, 0, width, height).
      Matrix viewMatrix = Matrix::Translation(-1.0, 1.0);
      viewMatrix.PreScale(2.0f / float(aSize.width), 2.0f / float(aSize.height));
      viewMatrix.PreScale(1.0f, -1.0f);
      Matrix4x4 projection = Matrix4x4::From2D(viewMatrix);
      projection._33 = 0.0f;

      Matrix transform2d;
      gfx::Matrix4x4 transform = gfx::Matrix4x4::From2D(transform2d);

      const float posX = 0.0f, posY = 0.0f;
      D3D11_VIEWPORT viewport;
      viewport.MinDepth = 0.0f;
      viewport.MaxDepth = 1.0f;
      viewport.Width = aSize.width;
      viewport.Height = aSize.height;
      viewport.TopLeftX = posX;
      viewport.TopLeftY = posY;

      D3D11_RECT scissor;
      scissor.left = posX;
      scissor.right = aSize.width + posX;
      scissor.top = posY;
      scissor.bottom = aSize.height + posY;

      memcpy(&mVSConstants.layerTransform, &transform._11, sizeof(mVSConstants.layerTransform));
      memcpy(&mVSConstants.projection, &projection._11, sizeof(mVSConstants.projection));
      mVSConstants.renderTargetOffset[0] = 0.0f;
      mVSConstants.renderTargetOffset[1] = 0.0f;
      mVSConstants.layerQuad = Rect(0.0f, 0.0f, aSize.width, aSize.height);
      mVSConstants.textureCoords = Rect(0.0f, 1.0f, 1.0f, -1.0f);

      mPSConstants.layerOpacity[0] = 1.0f;

      ID3D11Buffer* vbuffer = mVertexBuffer;
      UINT vsize = sizeof(Vertex);
      UINT voffset = 0;
      mContext->IASetVertexBuffers(0, 1, &vbuffer, &vsize, &voffset);
      mContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
      mContext->IASetInputLayout(mInputLayout);
      mContext->RSSetViewports(1, &viewport);
      mContext->RSSetScissorRects(1, &scissor);
      mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      mContext->VSSetShader(mQuadVS, nullptr, 0);
      mContext->PSSetShader(mQuadPS, nullptr, 0);

      RefPtr<ID3D11ShaderResourceView> srView;
      HRESULT hr = mDevice->CreateShaderResourceView(aSource, nullptr, getter_AddRefs(srView));
      if (FAILED(hr) || !srView) {
        gfxWarning() << "Could not create shader resource view for Puppet: " << hexa(hr);
        return false;
      }
      ID3D11ShaderResourceView* viewPtr = srView.get();
      mContext->PSSetShaderResources(0 /* 0 == TexSlot::RGB */, 1, &viewPtr);
      // XXX Use Constant from TexSlot in CompositorD3D11.cpp?

      ID3D11SamplerState *sampler = mLinearSamplerState;
      mContext->PSSetSamplers(0, 1, &sampler);

      if (!UpdateConstantBuffers()) {
        NS_WARNING("Failed to update constant buffers for Puppet");
        return false;
      }
      mContext->Draw(4, 0);
      break;
    }
  }

  // We will always return false for gfxVRPuppet to ensure that the fallback "watchdog"
  // code in VRDisplayHost::NotifyVSync() throttles the render loop.  This "watchdog" will
  // result in a refresh rate that is quite low compared to real hardware, but should be
  // sufficient for non-performance oriented tests.  If we wish to simulate realistic frame
  // rates with VRDisplayPuppet, we should block here for the appropriate amount of time and
  // return true to indicate that we have blocked.
  return false;
}

#elif defined(XP_MACOSX)

bool
VRDisplayPuppet::SubmitFrame(MacIOSurface* aMacIOSurface,
                             const IntSize& aSize,
                             const gfx::Rect& aLeftEyeRect,
                             const gfx::Rect& aRightEyeRect)
{
  MOZ_ASSERT(mSubmitThread->GetThread() == NS_GetCurrentThread());
  if (!mIsPresenting || !aMacIOSurface) {
    return false;
  }

  VRManager* vm = VRManager::Get();
  MOZ_ASSERT(vm);

  switch (gfxPrefs::VRPuppetSubmitFrame()) {
    case 0:
      // The VR frame is not displayed.
      break;
    case 1:
    {
      // The frames are submitted to VR compositor are decoded
      // into a base64Image and dispatched to the DOM side.
      RefPtr<SourceSurface> surf = aMacIOSurface->GetAsSurface();
      RefPtr<DataSourceSurface> dataSurf = surf ? surf->GetDataSurface() :
                                           nullptr;
      if (dataSurf) {
        // Ideally, we should convert the srcData to a PNG image and decode it
        // to a Base64 string here, but the GPU process does not have the privilege to
        // access the image library. So, we have to convert the RAW image data
        // to a base64 string and forward it to let the content process to
        // do the image conversion.
        DataSourceSurface::MappedSurface map;
        if (!dataSurf->Map(gfx::DataSourceSurface::MapType::READ, &map)) {
          MOZ_ASSERT(false, "Read DataSourceSurface fail.");
          return false;
        }
        const uint8_t* srcData = map.mData;
        const auto& surfSize = dataSurf->GetSize();
        VRSubmitFrameResultInfo result;
        result.mFormat = SurfaceFormat::B8G8R8A8;
        result.mWidth = surfSize.width;
        result.mHeight = surfSize.height;
        result.mFrameNum = mDisplayInfo.mFrameId;
        // If the original texture size is not pow of 2, the data will not be tightly strided.
        // We have to copy the pixels by rows.
        nsCString rawString;
        for (int32_t i = 0; i < surfSize.height; i++) {
          rawString += Substring((const char*)(srcData) + i * map.mStride,
                                  surfSize.width * 4);
        }
        dataSurf->Unmap();

        if (Base64Encode(rawString, result.mBase64Image) != NS_OK) {
          MOZ_ASSERT(false, "Failed to encode base64 images.");
        }
        // Dispatch the base64 encoded string to the DOM side. Then, it will be decoded
        // and convert to a PNG image there.
        MessageLoop* loop = VRListenerThreadHolder::Loop();
        loop->PostTask(NewRunnableMethod<const uint32_t, VRSubmitFrameResultInfo>(
          "VRManager::DispatchSubmitFrameResult",
          vm, &VRManager::DispatchSubmitFrameResult, mDisplayInfo.mDisplayID, result
        ));
      }
      break;
    }
    case 2:
    {
      MOZ_ASSERT(false, "No support for showing VR frames on MacOSX yet.");
      break;
    }
  }

  return false;
}

#elif defined(MOZ_WIDGET_ANDROID)

bool
VRDisplayPuppet::SubmitFrame(const mozilla::layers::SurfaceTextureDescriptor& aDescriptor,
                             const gfx::Rect& aLeftEyeRect,
                             const gfx::Rect& aRightEyeRect)
{
  MOZ_ASSERT(mSubmitThread->GetThread() == NS_GetCurrentThread());
  return false;
}

#endif

void
VRDisplayPuppet::Refresh()
{
  // We update mIsConneced once per refresh.
  mDisplayInfo.mDisplayState.mIsConnected = true;
}

VRControllerPuppet::VRControllerPuppet(dom::GamepadHand aHand, uint32_t aDisplayID)
  : VRControllerHost(VRDeviceType::Puppet, aHand, aDisplayID)
  , mButtonPressState(0)
  , mButtonTouchState(0)
{
  MOZ_COUNT_CTOR_INHERITED(VRControllerPuppet, VRControllerHost);
  VRControllerState& state = mControllerInfo.mControllerState;
  strncpy(state.mControllerName, "Puppet Gamepad", kVRControllerNameMaxLen);
  state.mNumButtons = kNumPuppetButtonMask;
  state.mNumAxes = kNumPuppetAxis;
  state.mNumHaptics = kNumPuppetHaptcs;
}

VRControllerPuppet::~VRControllerPuppet()
{
  MOZ_COUNT_DTOR_INHERITED(VRControllerPuppet, VRControllerHost);
}

void
VRControllerPuppet::SetButtonPressState(uint32_t aButton, bool aPressed)
{
  const uint64_t buttonMask = kPuppetButtonMask[aButton];
  uint64_t pressedBit = GetButtonPressed();

  if (aPressed) {
    pressedBit |= kPuppetButtonMask[aButton];
  } else if (pressedBit & buttonMask) {
    // this button was pressed but is released now.
    uint64_t mask = 0xff ^ buttonMask;
    pressedBit &= mask;
  }

  mButtonPressState = pressedBit;
}

uint64_t
VRControllerPuppet::GetButtonPressState()
{
  return mButtonPressState;
}

void
VRControllerPuppet::SetButtonTouchState(uint32_t aButton, bool aTouched)
{
  const uint64_t buttonMask = kPuppetButtonMask[aButton];
  uint64_t touchedBit = GetButtonTouched();

  if (aTouched) {
    touchedBit |= kPuppetButtonMask[aButton];
  } else if (touchedBit & buttonMask) {
    // this button was touched but is released now.
    uint64_t mask = 0xff ^ buttonMask;
    touchedBit &= mask;
  }

  mButtonTouchState = touchedBit;
}

uint64_t
VRControllerPuppet::GetButtonTouchState()
{
  return mButtonTouchState;
}

void
VRControllerPuppet::SetAxisMoveState(uint32_t aAxis, double aValue)
{
  MOZ_ASSERT((sizeof(mAxisMoveState) / sizeof(float)) == kNumPuppetAxis);
  MOZ_ASSERT(aAxis <= kNumPuppetAxis);

  mAxisMoveState[aAxis] = aValue;
}

double
VRControllerPuppet::GetAxisMoveState(uint32_t aAxis)
{
  return mAxisMoveState[aAxis];
}

void
VRControllerPuppet::SetPoseMoveState(const dom::GamepadPoseState& aPose)
{
  mPoseState = aPose;
}

const dom::GamepadPoseState&
VRControllerPuppet::GetPoseMoveState()
{
  return mPoseState;
}

float
VRControllerPuppet::GetAxisMove(uint32_t aAxis)
{
  return mControllerInfo.mControllerState.mAxisValue[aAxis];
}

void
VRControllerPuppet::SetAxisMove(uint32_t aAxis, float aValue)
{
  mControllerInfo.mControllerState.mAxisValue[aAxis] = aValue;
}

VRSystemManagerPuppet::VRSystemManagerPuppet()
  : mPuppetDisplayCount(0)
  , mPuppetDisplayInfo{}
  , mPuppetDisplaySensorState{}
{
}

/*static*/ already_AddRefed<VRSystemManagerPuppet>
VRSystemManagerPuppet::Create()
{
  if (!gfxPrefs::VREnabled() || !gfxPrefs::VRPuppetEnabled()) {
    return nullptr;
  }

  RefPtr<VRSystemManagerPuppet> manager = new VRSystemManagerPuppet();
  return manager.forget();
}

void
VRSystemManagerPuppet::Destroy()
{
  Shutdown();
}

void
VRSystemManagerPuppet::Shutdown()
{
  mPuppetHMDs.Clear();
}

void
VRSystemManagerPuppet::NotifyVSync()
{
  VRSystemManager::NotifyVSync();

  for (const auto& display: mPuppetHMDs) {
    display->Refresh();
  }
}

uint32_t
VRSystemManagerPuppet::CreateTestDisplay()
{
  if (mPuppetDisplayCount >= kMaxPuppetDisplays) {
    MOZ_ASSERT(false);
    return mPuppetDisplayCount;
  }
  return mPuppetDisplayCount++;
}

void
VRSystemManagerPuppet::ClearTestDisplays()
{
  mPuppetDisplayCount = 0;
}

void
VRSystemManagerPuppet::Enumerate()
{
  while (mPuppetHMDs.Length() < mPuppetDisplayCount) {
    VRDisplayPuppet* puppetDisplay = new VRDisplayPuppet();
    uint32_t deviceID = mPuppetHMDs.Length();
    puppetDisplay->SetDisplayInfo(mPuppetDisplayInfo[deviceID]);
    puppetDisplay->SetSensorState(mPuppetDisplaySensorState[deviceID]);
    mPuppetHMDs.AppendElement(puppetDisplay);
  }
  while (mPuppetHMDs.Length() > mPuppetDisplayCount) {
    mPuppetHMDs.RemoveLastElement();
  }
}

void
VRSystemManagerPuppet::SetPuppetDisplayInfo(const uint32_t& aDeviceID,
                                            const VRDisplayInfo& aDisplayInfo)
{
  if (aDeviceID >= mPuppetDisplayCount) {
    MOZ_ASSERT(false);
    return;
  }
  /**
   * Even if mPuppetHMDs.Length() <= aDeviceID, we need to
   * update mPuppetDisplayInfo[aDeviceID].  In the case that
   * a puppet display is added and SetPuppetDisplayInfo is
   * immediately called, mPuppetHMDs may not be populated yet.
   * VRSystemManagerPuppet::Enumerate() will initialize
   * the VRDisplayPuppet later using mPuppetDisplayInfo.
   */
  mPuppetDisplayInfo[aDeviceID] = aDisplayInfo;
  if (mPuppetHMDs.Length() > aDeviceID) {
    /**
     * In the event that the VRDisplayPuppet has already been
     * created, we update it directly.
     */
    mPuppetHMDs[aDeviceID]->SetDisplayInfo(aDisplayInfo);
  }
}

void
VRSystemManagerPuppet::SetPuppetDisplaySensorState(const uint32_t& aDeviceID,
                                                   const VRHMDSensorState& aSensorState)
{
  if (aDeviceID >= mPuppetDisplayCount) {
    MOZ_ASSERT(false);
    return;
  }
  /**
   * Even if mPuppetHMDs.Length() <= aDeviceID, we need to
   * update mPuppetDisplaySensorState[aDeviceID].  In the case that
   * a puppet display is added and SetPuppetDisplaySensorState is
   * immediately called, mPuppetHMDs may not be populated yet.
   * VRSystemManagerPuppet::Enumerate() will initialize
   * the VRDisplayPuppet later using mPuppetDisplaySensorState.
   */
  mPuppetDisplaySensorState[aDeviceID] = aSensorState;
  if (mPuppetHMDs.Length() > aDeviceID) {
    /**
     * In the event that the VRDisplayPuppet has already been
     * created, we update it directly.
     */
    mPuppetHMDs[aDeviceID]->SetSensorState(aSensorState);
  }
}

void
VRSystemManagerPuppet::GetHMDs(nsTArray<RefPtr<VRDisplayHost>>& aHMDResult)
{
  for (auto display: mPuppetHMDs) {
    aHMDResult.AppendElement(display);
  }
}

bool
VRSystemManagerPuppet::GetIsPresenting()
{
  for (const auto& display: mPuppetHMDs) {
    const VRDisplayInfo& displayInfo(display->GetDisplayInfo());
    if (displayInfo.GetPresentingGroups() != kVRGroupNone) {
      return true;
    }
  }
  return false;
}

void
VRSystemManagerPuppet::HandleInput()
{
  RefPtr<impl::VRControllerPuppet> controller;
  for (uint32_t i = 0; i < mPuppetController.Length(); ++i) {
    controller = mPuppetController[i];
    for (uint32_t j = 0; j < kNumPuppetButtonMask; ++j) {
      HandleButtonPress(i, j, kPuppetButtonMask[j], controller->GetButtonPressState(),
                        controller->GetButtonTouchState());
    }
    controller->SetButtonPressed(controller->GetButtonPressState());
    controller->SetButtonTouched(controller->GetButtonTouchState());

    for (uint32_t j = 0; j < kNumPuppetAxis; ++j) {
      HandleAxisMove(i, j, controller->GetAxisMoveState(j));
    }
    HandlePoseTracking(i, controller->GetPoseMoveState(), controller);
  }
}

void
VRSystemManagerPuppet::HandleButtonPress(uint32_t aControllerIdx,
                                         uint32_t aButton,
                                         uint64_t aButtonMask,
                                         uint64_t aButtonPressed,
                                         uint64_t aButtonTouched)
{
  RefPtr<impl::VRControllerPuppet> controller(mPuppetController[aControllerIdx]);
  MOZ_ASSERT(controller);
  const uint64_t pressedDiff = (controller->GetButtonPressed() ^ aButtonPressed);
  const uint64_t touchedDiff = (controller->GetButtonTouched() ^ aButtonTouched);

  if (!pressedDiff && !touchedDiff) {
    return;
  }

   if (pressedDiff & aButtonMask
      || touchedDiff & aButtonMask) {
    // diff & (aButtonPressed, aButtonTouched) would be true while a new button pressed or
    // touched event, otherwise it is an old event and needs to notify
    // the button has been released.
    NewButtonEvent(aControllerIdx, aButton, aButtonMask & aButtonPressed,
                   aButtonMask & aButtonPressed,
                   (aButtonMask & aButtonPressed) ? 1.0L : 0.0L);
  }
}

void
VRSystemManagerPuppet::HandleAxisMove(uint32_t aControllerIdx, uint32_t aAxis,
                                      float aValue)
{
  RefPtr<impl::VRControllerPuppet> controller(mPuppetController[aControllerIdx]);
  MOZ_ASSERT(controller);

  if (controller->GetAxisMove(aAxis) != aValue) {
    NewAxisMove(aControllerIdx, aAxis, aValue);
    controller->SetAxisMove(aAxis, aValue);
  }
}

void
VRSystemManagerPuppet::HandlePoseTracking(uint32_t aControllerIdx,
                                          const dom::GamepadPoseState& aPose,
                                          VRControllerHost* aController)
{
  MOZ_ASSERT(aController);
  if (aPose != aController->GetPose()) {
    aController->SetPose(aPose);
    NewPoseState(aControllerIdx, aPose);
  }
}

void
VRSystemManagerPuppet::VibrateHaptic(uint32_t aControllerIdx,
                                     uint32_t aHapticIndex,
                                     double aIntensity,
                                     double aDuration,
                                     const VRManagerPromise& aPromise)
{
}

void
VRSystemManagerPuppet::StopVibrateHaptic(uint32_t aControllerIdx)
{
}

void
VRSystemManagerPuppet::GetControllers(nsTArray<RefPtr<VRControllerHost>>& aControllerResult)
{
  aControllerResult.Clear();
  for (uint32_t i = 0; i < mPuppetController.Length(); ++i) {
    aControllerResult.AppendElement(mPuppetController[i]);
  }
}

void
VRSystemManagerPuppet::ScanForControllers()
{
  // We make sure VRSystemManagerPuppet has two controllers
  // for each display
  const uint32_t newControllerCount = mPuppetHMDs.Length() * 2;

  if (newControllerCount != mControllerCount) {
    RemoveControllers();

    // Re-adding controllers to VRControllerManager.
    for (const auto& display: mPuppetHMDs) {
      uint32_t displayID = display->GetDisplayInfo().GetDisplayID();
      for (uint32_t i = 0; i < 2; i++) {
        dom::GamepadHand hand = (i % 2) ? dom::GamepadHand::Right :
                                          dom::GamepadHand::Left;
        RefPtr<VRControllerPuppet> puppetController;
        puppetController = new VRControllerPuppet(hand, displayID);
        mPuppetController.AppendElement(puppetController);

        // Not already present, add it.
        AddGamepad(puppetController->GetControllerInfo());
        ++mControllerCount;
      }
    }
  }
}

void
VRSystemManagerPuppet::RemoveControllers()
{
  // controller count is changed, removing the existing gamepads first.
  for (uint32_t i = 0; i < mPuppetController.Length(); ++i) {
    RemoveGamepad(i);
  }
  mPuppetController.Clear();
  mControllerCount = 0;
}
