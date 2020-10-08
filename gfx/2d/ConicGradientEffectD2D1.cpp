/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ConicGradientEffectD2D1.h"

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
            <Property name='DisplayName' type='string' value='ConicGradientEffect'/>
            <Property name='Author' type='string' value='Mozilla'/>
            <Property name='Category' type='string' value='Pattern effects'/>
            <Property name='Description' type='string' value='This effect is used to render CSS conic gradients.'/>
            <Inputs>
                <Input name='Geometry'/>
            </Inputs>
            <Property name='StopCollection' type='iunknown'>
              <Property name='DisplayName' type='string' value='Gradient stop collection'/>
            </Property>
            <Property name='Center' type='vector2'>
              <Property name='DisplayName' type='string' value='Gradient center'/>
            </Property>
            <Property name='Angle' type='vector2'>
              <Property name='DisplayName' type='string' value='Gradient angle'/>
            </Property>
            <Property name='StartOffset' type='float'>
              <Property name='DisplayName' type='string' value='Start stop offset'/>
            </Property>
            <Property name='EndOffset' type='float'>
              <Property name='DisplayName' type='string' value='End stop offset'/>
            </Property>
            <Property name='Transform' type='matrix3x2'>
              <Property name='DisplayName' type='string' value='Transform applied to the pattern'/>
            </Property>

        </Effect>
        );

// {091fda1d-857e-4b1e-828f-1c839d9b7897}
static const GUID GUID_SampleConicGradientPS = {
    0x091fda1d,
    0x857e,
    0x4b1e,
    {0x82, 0x8f, 0x1c, 0x83, 0x9d, 0x9b, 0x78, 0x97}};

namespace mozilla {
namespace gfx {

ConicGradientEffectD2D1::ConicGradientEffectD2D1()
    : mRefCount(0),
      mCenter(D2D1::Vector2F(0, 0)),
      mAngle(0),
      mStartOffset(0),
      mEndOffset(0),
      mTransform(D2D1::IdentityMatrix())

{}

IFACEMETHODIMP
ConicGradientEffectD2D1::Initialize(ID2D1EffectContext* pContextInternal,
                                    ID2D1TransformGraph* pTransformGraph) {
  HRESULT hr;

  hr = pContextInternal->LoadPixelShader(GUID_SampleConicGradientPS,
                                         SampleConicGradientPS,
                                         sizeof(SampleConicGradientPS));

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
ConicGradientEffectD2D1::PrepareForRender(D2D1_CHANGE_TYPE changeType) {
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

  HRESULT hr = mDrawInfo->SetPixelShader(GUID_SampleConicGradientPS);

  if (FAILED(hr)) {
    return hr;
  }

  RefPtr<ID2D1ResourceTexture> tex = CreateGradientTexture();
  hr = mDrawInfo->SetResourceTexture(1, tex);

  if (FAILED(hr)) {
    return hr;
  }

  struct PSConstantBuffer {
    float center[2];
    float angle;
    float start_offset;
    float end_offset;
    float repeat_correct;
    float allow_odd;
    float padding[1];
    float transform[8];
  };

  PSConstantBuffer buffer = {
      {mCenter.x, mCenter.y},
      mAngle,
      mStartOffset,
      mEndOffset,
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
ConicGradientEffectD2D1::SetGraph(ID2D1TransformGraph* pGraph) {
  return pGraph->SetSingleTransformNode(this);
}

IFACEMETHODIMP_(ULONG)
ConicGradientEffectD2D1::AddRef() { return ++mRefCount; }

IFACEMETHODIMP_(ULONG)
ConicGradientEffectD2D1::Release() {
  if (!--mRefCount) {
    delete this;
    return 0;
  }
  return mRefCount;
}

IFACEMETHODIMP
ConicGradientEffectD2D1::QueryInterface(const IID& aIID, void** aPtr) {
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
ConicGradientEffectD2D1::MapInputRectsToOutputRect(
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
ConicGradientEffectD2D1::MapOutputRectToInputRects(
    const D2D1_RECT_L* pOutputRect, D2D1_RECT_L* pInputRects,
    UINT32 inputRectCount) const {
  if (inputRectCount != 1) {
    return E_INVALIDARG;
  }

  *pInputRects = *pOutputRect;
  return S_OK;
}

IFACEMETHODIMP
ConicGradientEffectD2D1::MapInvalidRect(UINT32 inputIndex,
                                        D2D1_RECT_L invalidInputRect,
                                        D2D1_RECT_L* pInvalidOutputRect) const {
  MOZ_ASSERT(inputIndex == 0);

  *pInvalidOutputRect = invalidInputRect;
  return S_OK;
}

IFACEMETHODIMP
ConicGradientEffectD2D1::SetDrawInfo(ID2D1DrawInfo* pDrawInfo) {
  mDrawInfo = pDrawInfo;
  return S_OK;
}

HRESULT
ConicGradientEffectD2D1::Register(ID2D1Factory1* aFactory) {
  D2D1_PROPERTY_BINDING bindings[] = {
      D2D1_VALUE_TYPE_BINDING(L"StopCollection",
                              &ConicGradientEffectD2D1::SetStopCollection,
                              &ConicGradientEffectD2D1::GetStopCollection),
      D2D1_VALUE_TYPE_BINDING(L"Center", &ConicGradientEffectD2D1::SetCenter,
                              &ConicGradientEffectD2D1::GetCenter),
      D2D1_VALUE_TYPE_BINDING(L"Angle", &ConicGradientEffectD2D1::SetAngle,
                              &ConicGradientEffectD2D1::GetAngle),
      D2D1_VALUE_TYPE_BINDING(L"StartOffset",
                              &ConicGradientEffectD2D1::SetStartOffset,
                              &ConicGradientEffectD2D1::GetStartOffset),
      D2D1_VALUE_TYPE_BINDING(L"EndOffset",
                              &ConicGradientEffectD2D1::SetEndOffset,
                              &ConicGradientEffectD2D1::GetEndOffset),
      D2D1_VALUE_TYPE_BINDING(L"Transform",
                              &ConicGradientEffectD2D1::SetTransform,
                              &ConicGradientEffectD2D1::GetTransform)};
  HRESULT hr = aFactory->RegisterEffectFromString(
      CLSID_ConicGradientEffect, kXmlDescription, bindings, ARRAYSIZE(bindings),
      CreateEffect);

  if (FAILED(hr)) {
    gfxWarning() << "Failed to register radial gradient effect.";
  }
  return hr;
}

void ConicGradientEffectD2D1::Unregister(ID2D1Factory1* aFactory) {
  aFactory->UnregisterEffect(CLSID_ConicGradientEffect);
}

HRESULT __stdcall ConicGradientEffectD2D1::CreateEffect(
    IUnknown** aEffectImpl) {
  *aEffectImpl = static_cast<ID2D1EffectImpl*>(new ConicGradientEffectD2D1());
  (*aEffectImpl)->AddRef();

  return S_OK;
}

HRESULT
ConicGradientEffectD2D1::SetStopCollection(IUnknown* aStopCollection) {
  if (SUCCEEDED(aStopCollection->QueryInterface(
          (ID2D1GradientStopCollection**)getter_AddRefs(mStopCollection)))) {
    return S_OK;
  }

  return E_INVALIDARG;
}

already_AddRefed<ID2D1ResourceTexture>
ConicGradientEffectD2D1::CreateGradientTexture() {
  std::vector<D2D1_GRADIENT_STOP> rawStops;
  rawStops.resize(mStopCollection->GetGradientStopCount());
  mStopCollection->GetGradientStops(&rawStops.front(), rawStops.size());

  std::vector<unsigned char> textureData;
  textureData.resize(4096 * 4);
  unsigned char* texData = &textureData.front();

  float prevColorPos = 0;
  float nextColorPos = rawStops[0].position;
  D2D1_COLOR_F prevColor = rawStops[0].color;
  D2D1_COLOR_F nextColor = prevColor;
  uint32_t stopPosition = 1;

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
    texData[i * 4] = (char)(255.0f * newColor.r);
    texData[i * 4 + 1] = (char)(255.0f * newColor.g);
    texData[i * 4 + 2] = (char)(255.0f * newColor.b);
    texData[i * 4 + 3] = (char)(255.0f * newColor.a);
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
