/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(XP_WIN)
#include "CompositorD3D11.h"
#include "TextureD3D11.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#endif // XP_WIN

#include "mozilla/Base64.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "gfxUtils.h"
#include "gfxVRPuppet.h"

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

static const uint32_t kPuppetAxes[] = {
  0,
  1,
  2
};

static const uint32_t kNumPuppetAxis = sizeof(kPuppetAxes) /
                                       sizeof(uint32_t);

static const uint32_t kNumPuppetHaptcs = 1;

VRDisplayPuppet::VRDisplayPuppet()
 : VRDisplayHost(VRDeviceType::Puppet)
 , mIsPresenting(false)
{
  MOZ_COUNT_CTOR_INHERITED(VRDisplayPuppet, VRDisplayHost);

  mDisplayInfo.mDisplayName.AssignLiteral("Puppet HMD");
  mDisplayInfo.mIsConnected = true;
  mDisplayInfo.mIsMounted = false;
  mDisplayInfo.mCapabilityFlags = VRDisplayCapabilityFlags::Cap_None |
                                  VRDisplayCapabilityFlags::Cap_Orientation |
                                  VRDisplayCapabilityFlags::Cap_Position |
                                  VRDisplayCapabilityFlags::Cap_External |
                                  VRDisplayCapabilityFlags::Cap_Present |
                                  VRDisplayCapabilityFlags::Cap_StageParameters;
  mDisplayInfo.mEyeResolution.width = 1836; // 1080 * 1.7
  mDisplayInfo.mEyeResolution.height = 2040; // 1200 * 1.7

  // SteamVR gives the application a single FOV to use; it's not configurable as with Oculus
  for (uint32_t eye = 0; eye < 2; ++eye) {
    mDisplayInfo.mEyeTranslation[eye].x = 0.0f;
    mDisplayInfo.mEyeTranslation[eye].y = 0.0f;
    mDisplayInfo.mEyeTranslation[eye].z = 0.0f;
    mDisplayInfo.mEyeFOV[eye] = VRFieldOfView(45.0, 45.0, 45.0, 45.0);
  }

  // default: 1m x 1m space, 0.75m high in seated position
  mDisplayInfo.mStageSize.width = 1.0f;
  mDisplayInfo.mStageSize.height = 1.0f;

  mDisplayInfo.mSittingToStandingTransform._11 = 1.0f;
  mDisplayInfo.mSittingToStandingTransform._12 = 0.0f;
  mDisplayInfo.mSittingToStandingTransform._13 = 0.0f;
  mDisplayInfo.mSittingToStandingTransform._14 = 0.0f;

  mDisplayInfo.mSittingToStandingTransform._21 = 0.0f;
  mDisplayInfo.mSittingToStandingTransform._22 = 1.0f;
  mDisplayInfo.mSittingToStandingTransform._23 = 0.0f;
  mDisplayInfo.mSittingToStandingTransform._24 = 0.0f;

  mDisplayInfo.mSittingToStandingTransform._31 = 0.0f;
  mDisplayInfo.mSittingToStandingTransform._32 = 0.0f;
  mDisplayInfo.mSittingToStandingTransform._33 = 1.0f;
  mDisplayInfo.mSittingToStandingTransform._34 = 0.0f;

  mDisplayInfo.mSittingToStandingTransform._41 = 0.0f;
  mDisplayInfo.mSittingToStandingTransform._42 = 0.75f;
  mDisplayInfo.mSittingToStandingTransform._43 = 0.0f;

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
  mDisplayInfo.mEyeResolution = aDisplayInfo.mEyeResolution;
  mDisplayInfo.mIsMounted = aDisplayInfo.mIsMounted;
  memcpy(&mDisplayInfo.mEyeFOV, &aDisplayInfo.mEyeFOV,
         sizeof(mDisplayInfo.mEyeFOV[0]) * VRDisplayInfo::NumEyes);
  memcpy(&mDisplayInfo.mEyeTranslation, &aDisplayInfo.mEyeTranslation,
         sizeof(mDisplayInfo.mEyeTranslation[0]) * VRDisplayInfo::NumEyes);
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
  if (!mDevice) {
    mDevice = gfx::DeviceManagerDx::Get()->GetCompositorDevice();
    if (!mDevice) {
      NS_WARNING("Failed to get a D3D11Device for Puppet");
      return;
    }
  }

  mDevice->GetImmediateContext(getter_AddRefs(mContext));
  if (!mContext) {
    NS_WARNING("Failed to get immediate context for Puppet");
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
VRDisplayPuppet::SubmitFrame(TextureSourceD3D11* aSource,
                             const IntSize& aSize,
                             const gfx::Rect& aLeftEyeRect,
                             const gfx::Rect& aRightEyeRect)
{
  if (!mIsPresenting) {
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
      ID3D11Texture2D* texture = aSource->GetD3D11Texture();
      texture->GetDesc(&desc);
      MOZ_ASSERT(desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM,
                 "Only support B8G8R8A8_UNORM format.");
      // Map the staging resource
      ID3D11Texture2D* mappedTexture = nullptr;
      D3D11_MAPPED_SUBRESOURCE mapInfo;
      HRESULT hr = mContext->Map(texture,
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
          mContext->CopyResource(stagingTexture, texture);
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
        mappedTexture = texture;
      }
      // Ideally, we should convert the srcData to a PNG image and decode it
      // to a Base64 string here, but the GPU process does not have the privilege to
      // access the image library. So, we have to convert the RAW image data
      // to a base64 string and forward it to let the content process to
      // do the image conversion.
      char* srcData = static_cast<char*>(mapInfo.pData);
      VRSubmitFrameResultInfo result;
      result.mFormat = SurfaceFormat::B8G8R8A8;
      // If the original texture size is not pow of 2, CopyResource() will add padding,
      // so the size is adjusted. We have to get the correct size by (mapInfo.RowPitch /
      // the format size).
      result.mWidth = mapInfo.RowPitch / 4;
      result.mHeight = desc.Height;
      result.mFrameNum = mDisplayInfo.mFrameId;
      nsCString rawString(Substring((char*)srcData, mapInfo.RowPitch * desc.Height));

      if (Base64Encode(rawString, result.mBase64Image) != NS_OK) {
        MOZ_ASSERT(false, "Failed to encode base64 images.");
      }
      mContext->Unmap(mappedTexture, 0);
      // Dispatch the base64 encoded string to the DOM side. Then, it will be decoded
      // and convert to a PNG image there.
      vm->DispatchSubmitFrameResult(mDisplayInfo.mDisplayID, result);
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
      ID3D11ShaderResourceView* srView = aSource->GetShaderResourceView();
      if (!srView) {
        NS_WARNING("Failed to get SRV for Puppet");
        return false;
      }
      mContext->PSSetShaderResources(0 /* 0 == TexSlot::RGB */, 1, &srView);
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
#else
bool
VRDisplayPuppet::SubmitFrame(TextureSourceOGL* aSource,
                             const IntSize& aSize,
                             const gfx::Rect& aLeftEyeRect,
                             const gfx::Rect& aRightEyeRect)
{
  if (!mIsPresenting) {
    return false;
  }

  // TODO: Bug 1343730, Need to block until the next simulated
  // vblank interval and capture frames for use in reftests.

  return false;
}
#endif

void
VRDisplayPuppet::NotifyVSync()
{
  // We update mIsConneced once per frame.
  mDisplayInfo.mIsConnected = true;

  VRDisplayHost::NotifyVSync();
}

VRControllerPuppet::VRControllerPuppet(dom::GamepadHand aHand)
  : VRControllerHost(VRDeviceType::Puppet)
  , mButtonPressState(0)
{
  MOZ_COUNT_CTOR_INHERITED(VRControllerPuppet, VRControllerHost);
  mControllerInfo.mControllerName.AssignLiteral("Puppet Gamepad");
  mControllerInfo.mMappingType = GamepadMappingType::_empty;
  mControllerInfo.mHand = aHand;
  mControllerInfo.mNumButtons = kNumPuppetButtonMask;
  mControllerInfo.mNumAxes = kNumPuppetAxis;
  mControllerInfo.mNumHaptics = kNumPuppetHaptcs;
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
  return mAxisMove[aAxis];
}

void
VRControllerPuppet::SetAxisMove(uint32_t aAxis, float aValue)
{
  mAxisMove[aAxis] = aValue;
}

VRSystemManagerPuppet::VRSystemManagerPuppet()
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
  mPuppetHMD = nullptr;
}

bool
VRSystemManagerPuppet::GetHMDs(nsTArray<RefPtr<VRDisplayHost>>& aHMDResult)
{
  if (mPuppetHMD == nullptr) {
    mPuppetHMD = new VRDisplayPuppet();
  }
  aHMDResult.AppendElement(mPuppetHMD);
  return true;
}

bool
VRSystemManagerPuppet::GetIsPresenting()
{
  if (mPuppetHMD) {
    VRDisplayInfo displayInfo(mPuppetHMD->GetDisplayInfo());
    return displayInfo.GetPresentingGroups() != kVRGroupNone;
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
      HandleButtonPress(i, j, kPuppetButtonMask[i], controller->GetButtonPressState(),
                        controller->GetButtonTouchState());
    }
    controller->SetButtonPressed(controller->GetButtonPressState());

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
                                          const GamepadPoseState& aPose,
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
                                     uint32_t aPromiseID)
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
  // We make VRSystemManagerPuppet has two controllers always.
  const uint32_t newControllerCount = 2;

  if (newControllerCount != mControllerCount) {
    RemoveControllers();

    // Re-adding controllers to VRControllerManager.
    for (uint32_t i = 0; i < newControllerCount; ++i) {
      dom::GamepadHand hand = (i % 2) ? dom::GamepadHand::Right :
                                        dom::GamepadHand::Left;
      RefPtr<VRControllerPuppet> puppetController = new VRControllerPuppet(hand);
      mPuppetController.AppendElement(puppetController);

      // Not already present, add it.
      AddGamepad(puppetController->GetControllerInfo());
      ++mControllerCount;
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
