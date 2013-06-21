/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
  * This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RADIALGRADIENTEFFECTD2D1_H_
#define MOZILLA_GFX_RADIALGRADIENTEFFECTD2D1_H_

#include <d2d1_1.h>
#include <d2d1effectauthor.h>
#include <d2d1effecthelpers.h>

#include "2D.h"

// {97143DC6-CBC4-4DD4-A8BA-13342B0BA46D}
DEFINE_GUID(CLSID_RadialGradientEffect, 
0x97143dc6, 0xcbc4, 0x4dd4, 0xa8, 0xba, 0x13, 0x34, 0x2b, 0xb, 0xa4, 0x6d);

// Macro to keep our class nice and clean.
#define SIMPLE_PROP(type, name) \
public: \
  HRESULT Set##name(type a##name) \
  { m##name = a##name; return S_OK; } \
  type Get##name() const { return m##name; } \
private: \
  type m##name;

namespace mozilla {
namespace gfx {

enum {
  RADIAL_PROP_STOP_COLLECTION = 0,
  RADIAL_PROP_CENTER_1,
  RADIAL_PROP_CENTER_2,
  RADIAL_PROP_RADIUS_1,
  RADIAL_PROP_RADIUS_2,
  RADIAL_PROP_TRANSFORM
};

class RadialGradientEffectD2D1 : public ID2D1EffectImpl
                               , public ID2D1DrawTransform
{
public:
  // ID2D1EffectImpl
  IFACEMETHODIMP Initialize(ID2D1EffectContext* pContextInternal, ID2D1TransformGraph* pTransformGraph);
  IFACEMETHODIMP PrepareForRender(D2D1_CHANGE_TYPE changeType);
  IFACEMETHODIMP SetGraph(ID2D1TransformGraph* pGraph);

  // IUnknown
  IFACEMETHODIMP_(ULONG) AddRef();
  IFACEMETHODIMP_(ULONG) Release();
  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppOutput);
  
  // ID2D1Transform
  IFACEMETHODIMP MapInputRectsToOutputRect(const D2D1_RECT_L* pInputRects,
                                           const D2D1_RECT_L* pInputOpaqueSubRects,
                                           UINT32 inputRectCount,
                                           D2D1_RECT_L* pOutputRect,
                                           D2D1_RECT_L* pOutputOpaqueSubRect);    
  IFACEMETHODIMP MapOutputRectToInputRects(const D2D1_RECT_L* pOutputRect,
                                           D2D1_RECT_L* pInputRects,
                                           UINT32 inputRectCount) const;
  IFACEMETHODIMP MapInvalidRect(UINT32 inputIndex,
                                D2D1_RECT_L invalidInputRect,
                                D2D1_RECT_L* pInvalidOutputRect) const;

  // ID2D1TransformNode
  IFACEMETHODIMP_(UINT32) GetInputCount() const { return 1; }

  // ID2D1DrawTransform
  IFACEMETHODIMP SetDrawInfo(ID2D1DrawInfo *pDrawInfo);

  static HRESULT Register(ID2D1Factory1* aFactory);
  static HRESULT __stdcall CreateEffect(IUnknown** aEffectImpl);

  HRESULT SetStopCollection(IUnknown *aStopCollection);
  IUnknown *GetStopCollection() const { return mStopCollection; }

private:
  TemporaryRef<ID2D1ResourceTexture> CreateGradientTexture();

  RadialGradientEffectD2D1();

  uint32_t mRefCount;
  RefPtr<ID2D1GradientStopCollection> mStopCollection;
  RefPtr<ID2D1EffectContext> mEffectContext;
  RefPtr<ID2D1DrawInfo> mDrawInfo;
  SIMPLE_PROP(D2D1_VECTOR_2F, Center1);
  SIMPLE_PROP(D2D1_VECTOR_2F, Center2);
  SIMPLE_PROP(FLOAT, Radius1);
  SIMPLE_PROP(FLOAT, Radius2);
  SIMPLE_PROP(D2D_MATRIX_3X2_F, Transform);
};

}
}
#undef SIMPLE_PROP

#endif
