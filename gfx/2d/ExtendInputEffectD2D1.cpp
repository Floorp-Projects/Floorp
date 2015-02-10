/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
  * This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtendInputEffectD2D1.h"

#include "Logging.h"

#include "ShadersD2D1.h"
#include "HelpersD2D.h"

#include <vector>

#define TEXTW(x) L##x
#define XML(X) TEXTW(#X) // This macro creates a single string from multiple lines of text.

static const PCWSTR kXmlDescription =
    XML(
        <?xml version='1.0'?>
        <Effect>
            <!-- System Properties -->
            <Property name='DisplayName' type='string' value='ExtendInputEffect'/>
            <Property name='Author' type='string' value='Mozilla'/>
            <Property name='Category' type='string' value='Utility Effects'/>
            <Property name='Description' type='string' value='This effect is used to extend the output rect of any input effect to a specified rect.'/>
            <Inputs>
                <Input name='InputEffect'/>
            </Inputs>
            <Property name='OutputRect' type='vector4'>
              <Property name='DisplayName' type='string' value='Output Rect'/>
            </Property>
        </Effect>
        );

// {FB947CDA-718E-40CC-AE7B-D255830D7D14}
static const GUID GUID_SampleExtendInputPS =
  {0xfb947cda, 0x718e, 0x40cc, {0xae, 0x7b, 0xd2, 0x55, 0x83, 0xd, 0x7d, 0x14}};
// {2C468128-6546-453C-8E25-F2DF0DE10A0F}
static const GUID GUID_SampleExtendInputA0PS =
  {0x2c468128, 0x6546, 0x453c, {0x8e, 0x25, 0xf2, 0xdf, 0xd, 0xe1, 0xa, 0xf}};

namespace mozilla {
namespace gfx {

ExtendInputEffectD2D1::ExtendInputEffectD2D1()
  : mRefCount(0)
  , mOutputRect(D2D1::Vector4F(-FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX))
{
}

IFACEMETHODIMP
ExtendInputEffectD2D1::Initialize(ID2D1EffectContext* pContextInternal, ID2D1TransformGraph* pTransformGraph)
{
  HRESULT hr;
  hr = pTransformGraph->SetSingleTransformNode(this);

  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

IFACEMETHODIMP
ExtendInputEffectD2D1::PrepareForRender(D2D1_CHANGE_TYPE changeType)
{
  return S_OK;
}

IFACEMETHODIMP
ExtendInputEffectD2D1::SetGraph(ID2D1TransformGraph* pGraph)
{
  return E_NOTIMPL;
}

IFACEMETHODIMP_(ULONG)
ExtendInputEffectD2D1::AddRef()
{
  return ++mRefCount;
}

IFACEMETHODIMP_(ULONG)
ExtendInputEffectD2D1::Release()
{
  if (!--mRefCount) {
    delete this;
    return 0;
  }
  return mRefCount;
}

IFACEMETHODIMP
ExtendInputEffectD2D1::QueryInterface(const IID &aIID, void **aPtr)
{
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

static D2D1_RECT_L
ConvertFloatToLongRect(const D2D1_VECTOR_4F& aRect)
{
  // Clamp values to LONG range. We can't use std::min/max here because we want
  // the comparison to operate on a type that's different from the type of the
  // result.
  return D2D1::RectL(aRect.x <= LONG_MIN ? LONG_MIN : LONG(aRect.x),
                     aRect.y <= LONG_MIN ? LONG_MIN : LONG(aRect.y),
                     aRect.z >= LONG_MAX ? LONG_MAX : LONG(aRect.z),
                     aRect.w >= LONG_MAX ? LONG_MAX : LONG(aRect.w));
}

static D2D1_RECT_L
IntersectRect(const D2D1_RECT_L& aRect1, const D2D1_RECT_L& aRect2)
{
  return D2D1::RectL(std::max(aRect1.left, aRect2.left),
                     std::max(aRect1.top, aRect2.top),
                     std::min(aRect1.right, aRect2.right),
                     std::min(aRect1.bottom, aRect2.bottom));
}

IFACEMETHODIMP
ExtendInputEffectD2D1::MapInputRectsToOutputRect(const D2D1_RECT_L* pInputRects,
                                                 const D2D1_RECT_L* pInputOpaqueSubRects,
                                                 UINT32 inputRectCount,
                                                 D2D1_RECT_L* pOutputRect,
                                                 D2D1_RECT_L* pOutputOpaqueSubRect)
{
  // This transform only accepts one input, so there will only be one input rect.
  if (inputRectCount != 1) {
    return E_INVALIDARG;
  }

  // Set the output rect to the specified rect. This is the whole purpose of this effect.
  *pOutputRect = ConvertFloatToLongRect(mOutputRect);
  *pOutputOpaqueSubRect = IntersectRect(*pOutputRect, pInputOpaqueSubRects[0]);
  return S_OK;
}

IFACEMETHODIMP
ExtendInputEffectD2D1::MapOutputRectToInputRects(const D2D1_RECT_L* pOutputRect,
                                                 D2D1_RECT_L* pInputRects,
                                                 UINT32 inputRectCount) const
{
  if (inputRectCount != 1) {
      return E_INVALIDARG;
  }

  *pInputRects = *pOutputRect;
  return S_OK;
}

IFACEMETHODIMP
ExtendInputEffectD2D1::MapInvalidRect(UINT32 inputIndex,
                                      D2D1_RECT_L invalidInputRect,
                                      D2D1_RECT_L* pInvalidOutputRect) const
{
  MOZ_ASSERT(inputIndex = 0);

  *pInvalidOutputRect = invalidInputRect;
  return S_OK;
}

HRESULT
ExtendInputEffectD2D1::Register(ID2D1Factory1 *aFactory)
{
  D2D1_PROPERTY_BINDING bindings[] = {
    D2D1_VALUE_TYPE_BINDING(L"OutputRect", &ExtendInputEffectD2D1::SetOutputRect, &ExtendInputEffectD2D1::GetOutputRect),
  };
  HRESULT hr = aFactory->RegisterEffectFromString(CLSID_ExtendInputEffect, kXmlDescription, bindings, 1, CreateEffect);

  if (FAILED(hr)) {
    gfxWarning() << "Failed to register extend input effect.";
  }
  return hr;
}

void
ExtendInputEffectD2D1::Unregister(ID2D1Factory1 *aFactory)
{
  aFactory->UnregisterEffect(CLSID_ExtendInputEffect);
}

HRESULT __stdcall
ExtendInputEffectD2D1::CreateEffect(IUnknown **aEffectImpl)
{
  *aEffectImpl = static_cast<ID2D1EffectImpl*>(new ExtendInputEffectD2D1());
  (*aEffectImpl)->AddRef();

  return S_OK;
}

}
}
