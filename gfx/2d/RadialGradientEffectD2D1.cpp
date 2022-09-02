/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RadialGradientEffectD2D1.h"

#include "Logging.h"

#include "ShadersD2D1.h"
#include "HelpersD2D.h"

#include <vector>

#define TEXTW(x) L##x
#define XML(X) \
  TEXTW(#X)  // This macro creates a single string from multiple lines of text.

static const PCWSTR kXmlDescription =
    XML(
        <?xml version='1.0'?>
        <Effect>
            <!-- System Properties -->
            <Property name='DisplayName' type='string' value='RadialGradientEffect'/>
            <Property name='Author' type='string' value='Mozilla'/>
            <Property name='Category' type='string' value='Pattern effects'/>
            <Property name='Description' type='string' value='This effect is used to render radial gradients in a manner compliant with the 2D Canvas specification.'/>
            <Inputs>
                <Input name='Geometry'/>
            </Inputs>
            <Property name='StopCollection' type='iunknown'>
              <Property name='DisplayName' type='string' value='Gradient stop collection'/>
            </Property>
            <Property name='Center1' type='vector2'>
              <Property name='DisplayName' type='string' value='Inner circle center'/>
            </Property>
            <Property name='Center2' type='vector2'>
              <Property name='DisplayName' type='string' value='Outer circle center'/>
            </Property>
            <Property name='Radius1' type='float'>
              <Property name='DisplayName' type='string' value='Inner circle radius'/>
            </Property>
            <Property name='Radius2' type='float'>
              <Property name='DisplayName' type='string' value='Outer circle radius'/>
            </Property>
            <Property name='Transform' type='matrix3x2'>
              <Property name='DisplayName' type='string' value='Transform applied to the pattern'/>
            </Property>

        </Effect>
        );

// {FB947CDA-718E-40CC-AE7B-D255830D7D14}
static const GUID GUID_SampleRadialGradientPS = {
    0xfb947cda,
    0x718e,
    0x40cc,
    {0xae, 0x7b, 0xd2, 0x55, 0x83, 0xd, 0x7d, 0x14}};
// {2C468128-6546-453C-8E25-F2DF0DE10A0F}
static const GUID GUID_SampleRadialGradientA0PS = {
    0x2c468128, 0x6546, 0x453c, {0x8e, 0x25, 0xf2, 0xdf, 0xd, 0xe1, 0xa, 0xf}};

namespace mozilla {
namespace gfx {

RadialGradientEffectD2D1::RadialGradientEffectD2D1()
    : mRefCount(0),
      mCenter1(D2D1::Vector2F(0, 0)),
      mCenter2(D2D1::Vector2F(0, 0)),
      mRadius1(0),
      mRadius2(0),
      mTransform(D2D1::IdentityMatrix())

{}

IFACEMETHODIMP
RadialGradientEffectD2D1::Initialize(ID2D1EffectContext* pContextInternal,
                                     ID2D1TransformGraph* pTransformGraph) {
  HRESULT hr;

  hr = pContextInternal->LoadPixelShader(GUID_SampleRadialGradientPS,
                                         SampleRadialGradientPS,
                                         sizeof(SampleRadialGradientPS));

  if (FAILED(hr)) {
    return hr;
  }

  hr = pContextInternal->LoadPixelShader(GUID_SampleRadialGradientA0PS,
                                         SampleRadialGradientA0PS,
                                         sizeof(SampleRadialGradientA0PS));

  if (FAILED(hr)) {
    return hr;
  }

  hr = pTransformGraph->SetSingleTransformNode(this);

  if (FAILED(hr)) {
    return hr;
  }

  mEffectContext = pContextInternal;

  return S_OK;
}

IFACEMETHODIMP
RadialGradientEffectD2D1::PrepareForRender(D2D1_CHANGE_TYPE changeType) {
  if (changeType == D2D1_CHANGE_TYPE_NONE) {
    return S_OK;
  }

  // We'll need to inverse transform our pixel, precompute inverse here.
  Matrix mat = ToMatrix(mTransform);
  if (!mat.Invert()) {
    // Singular
    return S_OK;
  }

  if (!mStopCollection) {
    return S_OK;
  }

  D2D1_POINT_2F dc =
      D2D1::Point2F(mCenter2.x - mCenter1.x, mCenter2.y - mCenter1.y);
  float dr = mRadius2 - mRadius1;
  float A = dc.x * dc.x + dc.y * dc.y - dr * dr;

  HRESULT hr;

  if (A == 0) {
    hr = mDrawInfo->SetPixelShader(GUID_SampleRadialGradientA0PS);
  } else {
    hr = mDrawInfo->SetPixelShader(GUID_SampleRadialGradientPS);
  }

  if (FAILED(hr)) {
    return hr;
  }

  RefPtr<ID2D1ResourceTexture> tex = CreateGradientTexture();
  hr = mDrawInfo->SetResourceTexture(1, tex);

  if (FAILED(hr)) {
    return hr;
  }

  struct PSConstantBuffer {
    float diff[3];
    float padding;
    float center1[2];
    float A;
    float radius1;
    float sq_radius1;
    float repeat_correct;
    float allow_odd;
    float padding2[1];
    float transform[8];
  };

  PSConstantBuffer buffer = {
      {dc.x, dc.y, dr},
      0.0f,
      {mCenter1.x, mCenter1.y},
      A,
      mRadius1,
      mRadius1 * mRadius1,
      mStopCollection->GetExtendMode() != D2D1_EXTEND_MODE_CLAMP ? 1.0f : 0.0f,
      mStopCollection->GetExtendMode() == D2D1_EXTEND_MODE_MIRROR ? 1.0f : 0.0f,
      {0.0f},
      {mat._11, mat._21, mat._31, 0.0f, mat._12, mat._22, mat._32, 0.0f}};

  hr = mDrawInfo->SetPixelShaderConstantBuffer((BYTE*)&buffer, sizeof(buffer));

  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

IFACEMETHODIMP
RadialGradientEffectD2D1::SetGraph(ID2D1TransformGraph* pGraph) {
  return pGraph->SetSingleTransformNode(this);
}

IFACEMETHODIMP_(ULONG)
RadialGradientEffectD2D1::AddRef() { return ++mRefCount; }

IFACEMETHODIMP_(ULONG)
RadialGradientEffectD2D1::Release() {
  if (!--mRefCount) {
    delete this;
    return 0;
  }
  return mRefCount;
}

IFACEMETHODIMP
RadialGradientEffectD2D1::QueryInterface(const IID& aIID, void** aPtr) {
  if (!aPtr) {
    return E_POINTER;
  }

  if (aIID == IID_IUnknown) {
    *aPtr = static_cast<IUnknown*>(static_cast<ID2D1EffectImpl*>(this));
  } else if (aIID == IID_ID2D1EffectImpl) {
    *aPtr = static_cast<ID2D1EffectImpl*>(this);
  } else if (aIID == IID_ID2D1DrawTransform) {
    *aPtr = static_cast<ID2D1DrawTransform*>(this);
  } else if (aIID == IID_ID2D1Transform) {
    *aPtr = static_cast<ID2D1Transform*>(this);
  } else if (aIID == IID_ID2D1TransformNode) {
    *aPtr = static_cast<ID2D1TransformNode*>(this);
  } else {
    return E_NOINTERFACE;
  }

  static_cast<IUnknown*>(*aPtr)->AddRef();
  return S_OK;
}

IFACEMETHODIMP
RadialGradientEffectD2D1::MapInputRectsToOutputRect(
    const D2D1_RECT_L* pInputRects, const D2D1_RECT_L* pInputOpaqueSubRects,
    UINT32 inputRectCount, D2D1_RECT_L* pOutputRect,
    D2D1_RECT_L* pOutputOpaqueSubRect) {
  if (inputRectCount != 1) {
    return E_INVALIDARG;
  }

  *pOutputRect = *pInputRects;
  *pOutputOpaqueSubRect = *pInputOpaqueSubRects;
  return S_OK;
}

IFACEMETHODIMP
RadialGradientEffectD2D1::MapOutputRectToInputRects(
    const D2D1_RECT_L* pOutputRect, D2D1_RECT_L* pInputRects,
    UINT32 inputRectCount) const {
  if (inputRectCount != 1) {
    return E_INVALIDARG;
  }

  *pInputRects = *pOutputRect;
  return S_OK;
}

IFACEMETHODIMP
RadialGradientEffectD2D1::MapInvalidRect(
    UINT32 inputIndex, D2D1_RECT_L invalidInputRect,
    D2D1_RECT_L* pInvalidOutputRect) const {
  MOZ_ASSERT(inputIndex == 0);

  *pInvalidOutputRect = invalidInputRect;
  return S_OK;
}

IFACEMETHODIMP
RadialGradientEffectD2D1::SetDrawInfo(ID2D1DrawInfo* pDrawInfo) {
  mDrawInfo = pDrawInfo;
  return S_OK;
}

HRESULT
RadialGradientEffectD2D1::Register(ID2D1Factory1* aFactory) {
  D2D1_PROPERTY_BINDING bindings[] = {
      D2D1_VALUE_TYPE_BINDING(L"StopCollection",
                              &RadialGradientEffectD2D1::SetStopCollection,
                              &RadialGradientEffectD2D1::GetStopCollection),
      D2D1_VALUE_TYPE_BINDING(L"Center1", &RadialGradientEffectD2D1::SetCenter1,
                              &RadialGradientEffectD2D1::GetCenter1),
      D2D1_VALUE_TYPE_BINDING(L"Center2", &RadialGradientEffectD2D1::SetCenter2,
                              &RadialGradientEffectD2D1::GetCenter2),
      D2D1_VALUE_TYPE_BINDING(L"Radius1", &RadialGradientEffectD2D1::SetRadius1,
                              &RadialGradientEffectD2D1::GetRadius1),
      D2D1_VALUE_TYPE_BINDING(L"Radius2", &RadialGradientEffectD2D1::SetRadius2,
                              &RadialGradientEffectD2D1::GetRadius2),
      D2D1_VALUE_TYPE_BINDING(L"Transform",
                              &RadialGradientEffectD2D1::SetTransform,
                              &RadialGradientEffectD2D1::GetTransform)};
  HRESULT hr = aFactory->RegisterEffectFromString(
      CLSID_RadialGradientEffect, kXmlDescription, bindings,
      ARRAYSIZE(bindings), CreateEffect);

  if (FAILED(hr)) {
    gfxWarning() << "Failed to register radial gradient effect.";
  }
  return hr;
}

void RadialGradientEffectD2D1::Unregister(ID2D1Factory1* aFactory) {
  aFactory->UnregisterEffect(CLSID_RadialGradientEffect);
}

HRESULT __stdcall RadialGradientEffectD2D1::CreateEffect(
    IUnknown** aEffectImpl) {
  *aEffectImpl = static_cast<ID2D1EffectImpl*>(new RadialGradientEffectD2D1());
  (*aEffectImpl)->AddRef();

  return S_OK;
}

HRESULT
RadialGradientEffectD2D1::SetStopCollection(IUnknown* aStopCollection) {
  if (SUCCEEDED(aStopCollection->QueryInterface(
          (ID2D1GradientStopCollection**)getter_AddRefs(mStopCollection)))) {
    return S_OK;
  }

  return E_INVALIDARG;
}

already_AddRefed<ID2D1ResourceTexture>
RadialGradientEffectD2D1::CreateGradientTexture() {
  std::vector<D2D1_GRADIENT_STOP> rawStops;
  rawStops.resize(mStopCollection->GetGradientStopCount());
  mStopCollection->GetGradientStops(&rawStops.front(), rawStops.size());

  std::vector<unsigned char> textureData;
  textureData.resize(4096 * 4);
  unsigned char* texData = &textureData.front();

  float prevColorPos = 0;
  float nextColorPos = 1.0f;
  D2D1_COLOR_F prevColor = rawStops[0].color;
  D2D1_COLOR_F nextColor = prevColor;

  if (rawStops.size() >= 2) {
    nextColor = rawStops[1].color;
    nextColorPos = rawStops[1].position;
  }

  uint32_t stopPosition = 2;

  // Not the most optimized way but this will do for now.
  for (int i = 0; i < 4096; i++) {
    // The 4095 seems a little counter intuitive, but we want the gradient
    // color at offset 0 at the first pixel, and at offset 1.0f at the last
    // pixel.
    float pos = float(i) / 4095;

    while (pos > nextColorPos) {
      prevColor = nextColor;
      prevColorPos = nextColorPos;
      if (rawStops.size() > stopPosition) {
        nextColor = rawStops[stopPosition].color;
        nextColorPos = rawStops[stopPosition++].position;
      } else {
        nextColorPos = 1.0f;
      }
    }

    float interp;

    if (nextColorPos != prevColorPos) {
      interp = (pos - prevColorPos) / (nextColorPos - prevColorPos);
    } else {
      interp = 0;
    }

    DeviceColor newColor(prevColor.r + (nextColor.r - prevColor.r) * interp,
                         prevColor.g + (nextColor.g - prevColor.g) * interp,
                         prevColor.b + (nextColor.b - prevColor.b) * interp,
                         prevColor.a + (nextColor.a - prevColor.a) * interp);

    // Note D2D expects RGBA here!!
    texData[i * 4] = (unsigned char)(255.0f * newColor.r);
    texData[i * 4 + 1] = (unsigned char)(255.0f * newColor.g);
    texData[i * 4 + 2] = (unsigned char)(255.0f * newColor.b);
    texData[i * 4 + 3] = (unsigned char)(255.0f * newColor.a);
  }

  RefPtr<ID2D1ResourceTexture> tex;

  UINT32 width = 4096;
  UINT32 stride = 4096 * 4;
  D2D1_RESOURCE_TEXTURE_PROPERTIES props;
  // Older shader models do not support 1D textures. So just use a width x 1
  // texture.
  props.dimensions = 2;
  UINT32 dims[] = {width, 1};
  props.extents = dims;
  props.channelDepth = D2D1_CHANNEL_DEPTH_4;
  props.bufferPrecision = D2D1_BUFFER_PRECISION_8BPC_UNORM;
  props.filter = D2D1_FILTER_MIN_MAG_MIP_LINEAR;
  D2D1_EXTEND_MODE extendMode[] = {mStopCollection->GetExtendMode(),
                                   mStopCollection->GetExtendMode()};
  props.extendModes = extendMode;

  HRESULT hr = mEffectContext->CreateResourceTexture(
      nullptr, &props, &textureData.front(), &stride, 4096 * 4,
      getter_AddRefs(tex));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create resource texture: " << hexa(hr);
  }

  return tex.forget();
}

}  // namespace gfx
}  // namespace mozilla
