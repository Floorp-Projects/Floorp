/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
  * This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_EXTENDINPUTEFFECTD2D1_H_
#define MOZILLA_GFX_EXTENDINPUTEFFECTD2D1_H_

#include <d2d1_1.h>
#include <d2d1effectauthor.h>
#include <d2d1effecthelpers.h>

#include "2D.h"
#include "mozilla/Attributes.h"

// {97143DC6-CBC4-4DD4-A8BA-13342B0BA46D}
DEFINE_GUID(CLSID_ExtendInputEffect,
0x5fb55c7c, 0xd795, 0x4ba3, 0xa9, 0x5c, 0x22, 0x82, 0x5d, 0x0c, 0x4d, 0xf7);

namespace mozilla {
namespace gfx {

enum {
  EXTENDINPUT_PROP_OUTPUT_RECT = 0
};

// An effect type that passes through its input unchanged but sets the effect's
// output rect to a specified rect. Unlike the built-in Crop effect, the
// ExtendInput effect can extend the input rect, and not just make it smaller.
// The added margins are filled with transparent black.
// Some effects have different output depending on their input effect's output
// rect, for example the Border effect (which repeats the edges of its input
// effect's output rect) or the component transfer and color matrix effects
// (which can transform transparent pixels into non-transparent ones, but only
// inside their input effect's output rect).
class ExtendInputEffectD2D1 MOZ_FINAL : public ID2D1EffectImpl
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
  IFACEMETHODIMP SetDrawInfo(ID2D1DrawInfo *pDrawInfo) { return S_OK; }

  static HRESULT Register(ID2D1Factory1* aFactory);
  static void Unregister(ID2D1Factory1* aFactory);
  static HRESULT __stdcall CreateEffect(IUnknown** aEffectImpl);

  HRESULT SetOutputRect(D2D1_VECTOR_4F aOutputRect)
  { mOutputRect = aOutputRect; return S_OK; }
  D2D1_VECTOR_4F GetOutputRect() const { return mOutputRect; }

private:
  ExtendInputEffectD2D1();

  uint32_t mRefCount;
  D2D1_VECTOR_4F mOutputRect;
};

}
}
#undef SIMPLE_PROP

#endif
