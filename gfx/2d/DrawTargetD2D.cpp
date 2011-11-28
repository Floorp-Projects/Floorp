/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "DrawTargetD2D.h"
#include "SourceSurfaceD2D.h"
#include "SourceSurfaceD2DTarget.h"
#include "ShadersD2D.h"
#include "PathD2D.h"
#include "GradientStopsD2D.h"
#include "ScaledFontDWrite.h"
#include "Logging.h"
#include "Tools.h"
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef HRESULT (WINAPI*D2D1CreateFactoryFunc)(
    D2D1_FACTORY_TYPE factoryType,
    REFIID iid,
    CONST D2D1_FACTORY_OPTIONS *pFactoryOptions,
    void **factory
);

typedef HRESULT (WINAPI*D3D10CreateEffectFromMemoryFunc)(
    void *pData,
    SIZE_T DataLength,
    UINT FXFlags,
    ID3D10Device *pDevice,
    ID3D10EffectPool *pEffectPool,
    ID3D10Effect **ppEffect
);

using namespace std;

namespace mozilla {
namespace gfx {

struct Vertex {
  float x;
  float y;
};

ID2D1Factory *DrawTargetD2D::mFactory;

// Helper class to restore surface contents that was clipped out but may have
// been altered by a drawing call.
class AutoSaveRestoreClippedOut
{
public:
  AutoSaveRestoreClippedOut(DrawTargetD2D *aDT)
    : mDT(aDT)
  {}

  void Save() {
    if (!mDT->mPushedClips.size()) {
      return;
    }

    mDT->Flush();

    RefPtr<ID3D10Texture2D> tmpTexture;
    IntSize size = mDT->mSize;
    SurfaceFormat format = mDT->mFormat;

    CD3D10_TEXTURE2D_DESC desc(DXGIFormat(format), size.width, size.height,
                               1, 1);
    desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;

    HRESULT hr = mDT->mDevice->CreateTexture2D(&desc, NULL, byRef(tmpTexture));
    if (FAILED(hr)) {
      gfxWarning() << "Failed to create temporary texture to hold surface data.";
    }
    mDT->mDevice->CopyResource(tmpTexture, mDT->mTexture);

    D2D1_BITMAP_PROPERTIES props =
      D2D1::BitmapProperties(D2D1::PixelFormat(DXGIFormat(format),
                             AlphaMode(format)));

    RefPtr<IDXGISurface> surf;

    tmpTexture->QueryInterface((IDXGISurface**)byRef(surf));

    hr = mDT->mRT->CreateSharedBitmap(IID_IDXGISurface, surf,
                                      &props, byRef(mOldSurfBitmap));

    if (FAILED(hr)) {
      gfxWarning() << "Failed to create shared bitmap for old surface.";
    }

    mClippedArea = mDT->GetClippedGeometry();
  }

  ID2D1Factory *factory() { return mDT->factory(); }

  ~AutoSaveRestoreClippedOut()
  {
    if (!mOldSurfBitmap) {
      return;
    }

    ID2D1RenderTarget *rt = mDT->mRT;

    // Write the area that was clipped out back to the surface. This all
    // happens in device space.
    rt->SetTransform(D2D1::IdentityMatrix());
    mDT->mTransformDirty = true;

    RefPtr<ID2D1RectangleGeometry> rectGeom;
    factory()->CreateRectangleGeometry(
      D2D1::RectF(0, 0, float(mDT->mSize.width), float(mDT->mSize.height)),
      byRef(rectGeom));

    RefPtr<ID2D1PathGeometry> invClippedArea;
    factory()->CreatePathGeometry(byRef(invClippedArea));
    RefPtr<ID2D1GeometrySink> sink;
    invClippedArea->Open(byRef(sink));

    HRESULT hr = rectGeom->CombineWithGeometry(mClippedArea, D2D1_COMBINE_MODE_EXCLUDE,
                                               NULL, sink);
    sink->Close();

    RefPtr<ID2D1BitmapBrush> brush;
    rt->CreateBitmapBrush(mOldSurfBitmap, D2D1::BitmapBrushProperties(), D2D1::BrushProperties(), byRef(brush));                   

    rt->FillGeometry(invClippedArea, brush);
  }

private:

  DrawTargetD2D *mDT;  

  // If we have an operator unbound by the source, this will contain a bitmap
  // with the old dest surface data.
  RefPtr<ID2D1Bitmap> mOldSurfBitmap;
  // This contains the area drawing is clipped to.
  RefPtr<ID2D1Geometry> mClippedArea;
};

DrawTargetD2D::DrawTargetD2D()
  : mClipsArePushed(false)
  , mPrivateData(NULL)
{
}

DrawTargetD2D::~DrawTargetD2D()
{
  if (mRT) {  
    PopAllClips();

    mRT->EndDraw();
  }
  if (mTempRT) {
    mTempRT->EndDraw();
  }

  // Targets depending on us can break that dependency, since we're obviously not going to
  // be modified in the future.
  for (std::vector<DrawTargetD2D*>::iterator iter = mDependentTargets.begin();
       iter != mDependentTargets.end(); iter++) {
    (*iter)->mDependingOnTargets.erase(
      std::find((*iter)->mDependingOnTargets.begin(), (*iter)->mDependingOnTargets.end(), this));
  }
  // Our dependencies on other targets no longer matter.
  for (std::vector<DrawTargetD2D*>::iterator iter = mDependingOnTargets.begin();
       iter != mDependingOnTargets.end(); iter++) {
    (*iter)->mDependentTargets.erase(
      std::find((*iter)->mDependentTargets.begin(), (*iter)->mDependentTargets.end(), this));
  }
}

/*
 * DrawTarget Implementation
 */
TemporaryRef<SourceSurface>
DrawTargetD2D::Snapshot()
{
  RefPtr<SourceSurfaceD2DTarget> newSurf = new SourceSurfaceD2DTarget();

  newSurf->mFormat = mFormat;
  newSurf->mTexture = mTexture;
  newSurf->mDrawTarget = this;

  mSnapshots.push_back(newSurf);

  Flush();

  return newSurf;
}

void
DrawTargetD2D::Flush()
{
  PopAllClips();

  HRESULT hr = mRT->Flush();

  if (FAILED(hr)) {
    gfxWarning() << "Error reported when trying to flush D2D rendertarget. Code: " << hr;
  }

  // We no longer depend on any target.
  for (std::vector<DrawTargetD2D*>::iterator iter = mDependingOnTargets.begin();
       iter != mDependingOnTargets.end(); iter++) {
    (*iter)->mDependentTargets.erase(
      std::find((*iter)->mDependentTargets.begin(), (*iter)->mDependentTargets.end(), this));
  }
  mDependingOnTargets.clear();
}

void
DrawTargetD2D::DrawSurface(SourceSurface *aSurface,
                           const Rect &aDest,
                           const Rect &aSource,
                           const DrawSurfaceOptions &aSurfOptions,
                           const DrawOptions &aOptions)
{
  RefPtr<ID2D1Bitmap> bitmap;

  ID2D1RenderTarget *rt = GetRTForOperation(aOptions.mCompositionOp, ColorPattern(Color()));
  
  PrepareForDrawing(rt);

  Rect srcRect = aSource;

  switch (aSurface->GetType()) {

  case SURFACE_D2D1_BITMAP:
    {
      SourceSurfaceD2D *srcSurf = static_cast<SourceSurfaceD2D*>(aSurface);
      bitmap = srcSurf->GetBitmap();

      if (!bitmap) {
        if (aSource.width > rt->GetMaximumBitmapSize() ||
            aSource.height > rt->GetMaximumBitmapSize()) {
          gfxDebug() << "Bitmap source larger than texture size specified. DrawBitmap will silently fail.";
          // Don't know how to deal with this yet.
          return;
        }

        int stride = srcSurf->GetSize().width * BytesPerPixel(srcSurf->GetFormat());

        unsigned char *data = &srcSurf->mRawData.front() +
                              (uint32_t)aSource.y * stride +
                              (uint32_t)aSource.x * BytesPerPixel(srcSurf->GetFormat());

        D2D1_BITMAP_PROPERTIES props =
          D2D1::BitmapProperties(D2D1::PixelFormat(DXGIFormat(srcSurf->GetFormat()), AlphaMode(srcSurf->GetFormat())));
        mRT->CreateBitmap(D2D1::SizeU(UINT32(aSource.width), UINT32(aSource.height)), data, stride, props, byRef(bitmap));

        srcRect.x -= (uint32_t)aSource.x;
        srcRect.y -= (uint32_t)aSource.y;
      }
    }
    break;
  case SURFACE_D2D1_DRAWTARGET:
    {
      SourceSurfaceD2DTarget *srcSurf = static_cast<SourceSurfaceD2DTarget*>(aSurface);
      bitmap = srcSurf->GetBitmap(mRT);

      if (!srcSurf->IsCopy()) {
        srcSurf->mDrawTarget->mDependentTargets.push_back(this);
        mDependingOnTargets.push_back(srcSurf->mDrawTarget);
      }
    }
    break;
  }

  rt->DrawBitmap(bitmap, D2DRect(aDest), aOptions.mAlpha, D2DFilter(aSurfOptions.mFilter), D2DRect(srcRect));

  FinalizeRTForOperation(aOptions.mCompositionOp, ColorPattern(Color()), aDest);
}

void
DrawTargetD2D::DrawSurfaceWithShadow(SourceSurface *aSurface,
                                     const Point &aDest,
                                     const Color &aColor,
                                     const Point &aOffset,
                                     Float aSigma,
                                     CompositionOp aOperator)
{
  RefPtr<ID3D10ShaderResourceView> srView = NULL;
  if (aSurface->GetType() != SURFACE_D2D1_DRAWTARGET) {
    return;
  }

  // XXX - This function is way too long, it should be split up soon to make
  // it more graspable!

  Flush();

  AutoSaveRestoreClippedOut restoreClippedOut(this);

  if (!IsOperatorBoundByMask(aOperator)) {
    restoreClippedOut.Save();
  }

  srView = static_cast<SourceSurfaceD2DTarget*>(aSurface)->GetSRView();

  EnsureViews();

  if (!mTempRTView) {
    // This view is only needed in this path.
    HRESULT hr = mDevice->CreateRenderTargetView(mTempTexture, NULL, byRef(mTempRTView));

    if (FAILED(hr)) {
      gfxWarning() << "Failure to create RenderTargetView. Code: " << hr;
      return;
    }
  }


  RefPtr<ID3D10RenderTargetView> destRTView = mRTView;
  RefPtr<ID3D10Texture2D> destTexture;
  HRESULT hr;

  RefPtr<ID3D10Texture2D> maskTexture;
  RefPtr<ID3D10ShaderResourceView> maskSRView;
  if (mPushedClips.size()) {
    // Here we render a mask of the clipped out area for use as an input to the
    // shadow drawing.

    CD3D10_TEXTURE2D_DESC desc(DXGI_FORMAT_A8_UNORM,
                               mSize.width, mSize.height,
                               1, 1);
    desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;

    hr = mDevice->CreateTexture2D(&desc, NULL, byRef(maskTexture));

    RefPtr<ID2D1RenderTarget> rt = CreateRTForTexture(maskTexture);

    RefPtr<ID2D1SolidColorBrush> brush;
    rt->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), byRef(brush));
    
    RefPtr<ID2D1Geometry> geometry = GetClippedGeometry();

    rt->BeginDraw();
    rt->Clear(D2D1::ColorF(0, 0));
    rt->FillGeometry(geometry, brush);
    rt->EndDraw();

    mDevice->CreateShaderResourceView(maskTexture, NULL, byRef(maskSRView));
  }

  IntSize srcSurfSize;
  ID3D10RenderTargetView *rtViews;
  D3D10_VIEWPORT viewport;

  UINT stride = sizeof(Vertex);
  UINT offset = 0;
  ID3D10Buffer *buff = mPrivateData->mVB;

  mDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  mDevice->IASetVertexBuffers(0, 1, &buff, &stride, &offset);
  mDevice->IASetInputLayout(mPrivateData->mInputLayout);

  mPrivateData->mEffect->GetVariableByName("QuadDesc")->AsVector()->
    SetFloatVector(ShaderConstantRectD3D10(-1.0f, 1.0f, 2.0f, -2.0f));
  mPrivateData->mEffect->GetVariableByName("TexCoords")->AsVector()->
    SetFloatVector(ShaderConstantRectD3D10(0, 0, 1.0f, 1.0f));

  // If we create a downsampled source surface we need to correct aOffset for that.
  Point correctedOffset = aOffset + aDest;

  // The 'practical' scaling factors.
  Float dsFactorX = 1.0f;
  Float dsFactorY = 1.0f;

  if (aSigma > 1.7f) {
    // In this case 9 samples of our original will not cover it. Generate the
    // mip levels for the original and create a downsampled version from
    // them. We generate a version downsampled so that a kernel for a sigma
    // of 1.7 will produce the right results.
    float blurWeights[9] = { 0.234671f, 0.197389f, 0.197389f, 0.117465f, 0.117465f, 0.049456f, 0.049456f, 0.014732f, 0.014732f };
    mPrivateData->mEffect->GetVariableByName("BlurWeights")->SetRawValue(blurWeights, 0, sizeof(blurWeights));
    
    CD3D10_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM,
                               aSurface->GetSize().width,
                               aSurface->GetSize().height);
    desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D10_RESOURCE_MISC_GENERATE_MIPS;

    RefPtr<ID3D10Texture2D> mipTexture;
    hr = mDevice->CreateTexture2D(&desc, NULL, byRef(mipTexture));

    if (FAILED(hr)) {
      gfxWarning() << "Failure to create temporary texture. Size: " <<
        aSurface->GetSize() << " Code: " << hr;
      return;
    }

    IntSize dsSize = IntSize(int32_t(aSurface->GetSize().width * (1.7f / aSigma)),
                             int32_t(aSurface->GetSize().height * (1.7f / aSigma)));

    if (dsSize.width < 1) {
      dsSize.width = 1;
    }
    if (dsSize.height < 1) {
      dsSize.height = 1;
    }

    dsFactorX = dsSize.width / Float(aSurface->GetSize().width);
    dsFactorY = dsSize.height / Float(aSurface->GetSize().height);
    correctedOffset.x *= dsFactorX;
    correctedOffset.y *= dsFactorY;

    desc = CD3D10_TEXTURE2D_DESC(DXGI_FORMAT_B8G8R8A8_UNORM,
                                 dsSize.width,
                                 dsSize.height, 1, 1);
    desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    RefPtr<ID3D10Texture2D> tmpDSTexture;
    hr = mDevice->CreateTexture2D(&desc, NULL, byRef(tmpDSTexture));

    if (FAILED(hr)) {
      gfxWarning() << "Failure to create temporary texture. Size: " << dsSize << " Code: " << hr;
      return;
    }

    D3D10_BOX box;
    box.left = box.top = box.front = 0;
    box.back = 1;
    box.right = aSurface->GetSize().width;
    box.bottom = aSurface->GetSize().height;
    mDevice->CopySubresourceRegion(mipTexture, 0, 0, 0, 0, static_cast<SourceSurfaceD2DTarget*>(aSurface)->mTexture, 0, &box);

    mDevice->CreateShaderResourceView(mipTexture, NULL,  byRef(srView));
    mDevice->GenerateMips(srView);

    RefPtr<ID3D10RenderTargetView> dsRTView;
    RefPtr<ID3D10ShaderResourceView> dsSRView;
    mDevice->CreateRenderTargetView(tmpDSTexture, NULL,  byRef(dsRTView));
    mDevice->CreateShaderResourceView(tmpDSTexture, NULL,  byRef(dsSRView));

    // We're not guaranteed the texture we created will be empty, we've
    // seen old content at least on NVidia drivers.
    float color[4] = { 0, 0, 0, 0 };
    mDevice->ClearRenderTargetView(dsRTView, color);

    rtViews = dsRTView;
    mDevice->OMSetRenderTargets(1, &rtViews, NULL);

    viewport.MaxDepth = 1;
    viewport.MinDepth = 0;
    viewport.Height = dsSize.height;
    viewport.Width = dsSize.width;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;

    mDevice->RSSetViewports(1, &viewport);
    mPrivateData->mEffect->GetVariableByName("tex")->AsShaderResource()->SetResource(srView);
    mPrivateData->mEffect->GetTechniqueByName("SampleTexture")->
      GetPassByIndex(0)->Apply(0);

    mDevice->OMSetBlendState(GetBlendStateForOperator(OP_OVER), NULL, 0xffffffff);

    mDevice->Draw(4, 0);
    
    srcSurfSize = dsSize;

    srView = dsSRView;
  } else {
    // In this case generate a kernel to draw the blur directly to the temp
    // surf in one direction and to final in the other.
    float blurWeights[9];

    float normalizeFactor = 1.0f;
    if (aSigma != 0) {
      normalizeFactor = 1.0f / Float(sqrt(2 * M_PI * pow(aSigma, 2)));
    }

    blurWeights[0] = normalizeFactor;

    // XXX - We should actually optimize for Sigma = 0 here. We could use a
    // much simpler shader and save a lot of texture lookups.
    for (int i = 1; i < 9; i += 2) {
      if (aSigma != 0) {
        blurWeights[i] = blurWeights[i + 1] = normalizeFactor *
          exp(-pow(float((i + 1) / 2), 2) / (2 * pow(aSigma, 2)));
      } else {
        blurWeights[i] = blurWeights[i + 1] = 0;
      }
    }
    
    mPrivateData->mEffect->GetVariableByName("BlurWeights")->SetRawValue(blurWeights, 0, sizeof(blurWeights));

    viewport.MaxDepth = 1;
    viewport.MinDepth = 0;
    viewport.Height = aSurface->GetSize().height;
    viewport.Width = aSurface->GetSize().width;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;

    mDevice->RSSetViewports(1, &viewport);

    srcSurfSize = aSurface->GetSize();
  }

  // We may need to draw to a different intermediate surface if our temp
  // texture isn't big enough.
  bool needBiggerTemp = srcSurfSize.width > mSize.width ||
                        srcSurfSize.height > mSize.height;

  RefPtr<ID3D10RenderTargetView> tmpRTView;
  RefPtr<ID3D10ShaderResourceView> tmpSRView;
  RefPtr<ID3D10Texture2D> tmpTexture;
  
  IntSize tmpSurfSize = mSize;

  if (!needBiggerTemp) {
    tmpRTView = mTempRTView;
    tmpSRView = mSRView;

    // There could still be content here!
    float color[4] = { 0, 0, 0, 0 };
    mDevice->ClearRenderTargetView(tmpRTView, color);
  } else {
    CD3D10_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM,
                               srcSurfSize.width,
                               srcSurfSize.height,
                               1, 1);
    desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;

    mDevice->CreateTexture2D(&desc, NULL,  byRef(tmpTexture));
    mDevice->CreateRenderTargetView(tmpTexture, NULL,  byRef(tmpRTView));
    mDevice->CreateShaderResourceView(tmpTexture, NULL,  byRef(tmpSRView));

    tmpSurfSize = srcSurfSize;
  }

  rtViews = tmpRTView;
  mDevice->OMSetRenderTargets(1, &rtViews, NULL);

  mPrivateData->mEffect->GetVariableByName("tex")->AsShaderResource()->SetResource(srView);

  // Premultiplied!
  float shadowColor[4] = { aColor.r * aColor.a, aColor.g * aColor.a,
                           aColor.b * aColor.a, aColor.a };
  mPrivateData->mEffect->GetVariableByName("ShadowColor")->AsVector()->
    SetFloatVector(shadowColor);

  float pixelOffset = 1.0f / float(srcSurfSize.width);
  float blurOffsetsH[9] = { 0, pixelOffset, -pixelOffset,
                            2.0f * pixelOffset, -2.0f * pixelOffset,
                            3.0f * pixelOffset, -3.0f * pixelOffset,
                            4.0f * pixelOffset, - 4.0f * pixelOffset };

  pixelOffset = 1.0f / float(tmpSurfSize.height);
  float blurOffsetsV[9] = { 0, pixelOffset, -pixelOffset,
                            2.0f * pixelOffset, -2.0f * pixelOffset,
                            3.0f * pixelOffset, -3.0f * pixelOffset,
                            4.0f * pixelOffset, - 4.0f * pixelOffset };

  mPrivateData->mEffect->GetVariableByName("BlurOffsetsH")->
    SetRawValue(blurOffsetsH, 0, sizeof(blurOffsetsH));
  mPrivateData->mEffect->GetVariableByName("BlurOffsetsV")->
    SetRawValue(blurOffsetsV, 0, sizeof(blurOffsetsV));

  mPrivateData->mEffect->GetTechniqueByName("SampleTextureWithShadow")->
    GetPassByIndex(0)->Apply(0);

  mDevice->Draw(4, 0);

  viewport.MaxDepth = 1;
  viewport.MinDepth = 0;
  viewport.Height = mSize.height;
  viewport.Width = mSize.width;
  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;

  mDevice->RSSetViewports(1, &viewport);

  mPrivateData->mEffect->GetVariableByName("tex")->AsShaderResource()->SetResource(tmpSRView);

  rtViews = destRTView;
  mDevice->OMSetRenderTargets(1, &rtViews, NULL);

  Point shadowDest = aDest + aOffset;

  mPrivateData->mEffect->GetVariableByName("QuadDesc")->AsVector()->
    SetFloatVector(ShaderConstantRectD3D10(-1.0f + ((shadowDest.x / mSize.width) * 2.0f),
                                           1.0f - (shadowDest.y / mSize.height * 2.0f),
                                           (Float(aSurface->GetSize().width) / mSize.width) * 2.0f,
                                           (-Float(aSurface->GetSize().height) / mSize.height) * 2.0f));
  mPrivateData->mEffect->GetVariableByName("TexCoords")->AsVector()->
    SetFloatVector(ShaderConstantRectD3D10(0, 0, Float(srcSurfSize.width) / tmpSurfSize.width,
                                                 Float(srcSurfSize.height) / tmpSurfSize.height));

  if (mPushedClips.size()) {
    mPrivateData->mEffect->GetVariableByName("mask")->AsShaderResource()->SetResource(maskSRView);
    mPrivateData->mEffect->GetVariableByName("MaskTexCoords")->AsVector()->
      SetFloatVector(ShaderConstantRectD3D10(shadowDest.x / mSize.width, shadowDest.y / mSize.width,
                                             Float(aSurface->GetSize().width) / mSize.width,
                                             Float(aSurface->GetSize().height) / mSize.height));
    mPrivateData->mEffect->GetTechniqueByName("SampleTextureWithShadow")->
      GetPassByIndex(2)->Apply(0);
  } else {
    mPrivateData->mEffect->GetTechniqueByName("SampleTextureWithShadow")->
      GetPassByIndex(1)->Apply(0);
  }

  mDevice->OMSetBlendState(GetBlendStateForOperator(aOperator), NULL, 0xffffffff);

  mDevice->Draw(4, 0);

  mPrivateData->mEffect->GetVariableByName("QuadDesc")->AsVector()->
    SetFloatVector(ShaderConstantRectD3D10(-1.0f + ((aDest.x / mSize.width) * 2.0f),
                                           1.0f - (aDest.y / mSize.height * 2.0f),
                                           (Float(aSurface->GetSize().width) / mSize.width) * 2.0f,
                                           (-Float(aSurface->GetSize().height) / mSize.height) * 2.0f));
  mPrivateData->mEffect->GetVariableByName("tex")->AsShaderResource()->SetResource(static_cast<SourceSurfaceD2DTarget*>(aSurface)->GetSRView());
  mPrivateData->mEffect->GetVariableByName("TexCoords")->AsVector()->
    SetFloatVector(ShaderConstantRectD3D10(0, 0, 1.0f, 1.0f));

  if (mPushedClips.size()) {
    mPrivateData->mEffect->GetVariableByName("MaskTexCoords")->AsVector()->
      SetFloatVector(ShaderConstantRectD3D10(aDest.x / mSize.width, aDest.y / mSize.width,
                                             Float(aSurface->GetSize().width) / mSize.width,
                                             Float(aSurface->GetSize().height) / mSize.height));
    mPrivateData->mEffect->GetTechniqueByName("SampleMaskedTexture")->
      GetPassByIndex(0)->Apply(0);
  } else {
    mPrivateData->mEffect->GetTechniqueByName("SampleTexture")->
      GetPassByIndex(0)->Apply(0);
  }

  mDevice->OMSetBlendState(GetBlendStateForOperator(aOperator), NULL, 0xffffffff);

  mDevice->Draw(4, 0);
}

void
DrawTargetD2D::ClearRect(const Rect &aRect)
{
  MarkChanged();

  mRT->SetTransform(D2DMatrix(mTransform));
  PopAllClips();

  AutoSaveRestoreClippedOut restoreClippedOut(this);

  restoreClippedOut.Save();

  bool needsClip = false;

  needsClip = aRect.x > 0 || aRect.y > 0 ||
              aRect.XMost() < mSize.width ||
              aRect.YMost() < mSize.height;

  if (needsClip) {
    mRT->PushAxisAlignedClip(D2DRect(aRect), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
  }
  mRT->Clear(D2D1::ColorF(0, 0.0f));
  if (needsClip) {
    mRT->PopAxisAlignedClip();
  }
  return;
}

void
DrawTargetD2D::CopySurface(SourceSurface *aSurface,
                           const IntRect &aSourceRect,
                           const IntPoint &aDestination)
{
  MarkChanged();

  Rect srcRect(Float(aSourceRect.x), Float(aSourceRect.y),
               Float(aSourceRect.width), Float(aSourceRect.height));
  Rect dstRect(Float(aDestination.x), Float(aDestination.y),
               Float(aSourceRect.width), Float(aSourceRect.height));

  mRT->SetTransform(D2D1::IdentityMatrix());
  mRT->PushAxisAlignedClip(D2DRect(dstRect), D2D1_ANTIALIAS_MODE_ALIASED);
  mRT->Clear(D2D1::ColorF(0, 0.0f));
  mRT->PopAxisAlignedClip();

  RefPtr<ID2D1Bitmap> bitmap;

  switch (aSurface->GetType()) {
  case SURFACE_D2D1_BITMAP:
    {
      SourceSurfaceD2D *srcSurf = static_cast<SourceSurfaceD2D*>(aSurface);
      bitmap = srcSurf->GetBitmap();
    }
    break;
  case SURFACE_D2D1_DRAWTARGET:
    {
      SourceSurfaceD2DTarget *srcSurf = static_cast<SourceSurfaceD2DTarget*>(aSurface);
      bitmap = srcSurf->GetBitmap(mRT);

      if (!srcSurf->IsCopy()) {
        srcSurf->mDrawTarget->mDependentTargets.push_back(this);
        mDependingOnTargets.push_back(srcSurf->mDrawTarget);
      }
    }
    break;
  }

  if (!bitmap) {
    return;
  }

  mRT->DrawBitmap(bitmap, D2DRect(dstRect), 1.0f,
                  D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                  D2DRect(srcRect));
}

void
DrawTargetD2D::FillRect(const Rect &aRect,
                        const Pattern &aPattern,
                        const DrawOptions &aOptions)
{
  ID2D1RenderTarget *rt = GetRTForOperation(aOptions.mCompositionOp, aPattern);

  PrepareForDrawing(rt);

  RefPtr<ID2D1Brush> brush = CreateBrushForPattern(aPattern, aOptions.mAlpha);

  if (brush) {
    rt->FillRectangle(D2DRect(aRect), brush);
  }

  FinalizeRTForOperation(aOptions.mCompositionOp, aPattern, aRect);
}

void
DrawTargetD2D::StrokeRect(const Rect &aRect,
                          const Pattern &aPattern,
                          const StrokeOptions &aStrokeOptions,
                          const DrawOptions &aOptions)
{
  ID2D1RenderTarget *rt = GetRTForOperation(aOptions.mCompositionOp, aPattern);

  PrepareForDrawing(rt);

  RefPtr<ID2D1Brush> brush = CreateBrushForPattern(aPattern, aOptions.mAlpha);

  RefPtr<ID2D1StrokeStyle> strokeStyle = CreateStrokeStyleForOptions(aStrokeOptions);

  if (brush && strokeStyle) {
    rt->DrawRectangle(D2DRect(aRect), brush, aStrokeOptions.mLineWidth, strokeStyle);
  }

  FinalizeRTForOperation(aOptions.mCompositionOp, aPattern, aRect);
}

void
DrawTargetD2D::StrokeLine(const Point &aStart,
                          const Point &aEnd,
                          const Pattern &aPattern,
                          const StrokeOptions &aStrokeOptions,
                          const DrawOptions &aOptions)
{
  ID2D1RenderTarget *rt = GetRTForOperation(aOptions.mCompositionOp, aPattern);

  PrepareForDrawing(rt);

  RefPtr<ID2D1Brush> brush = CreateBrushForPattern(aPattern, aOptions.mAlpha);

  RefPtr<ID2D1StrokeStyle> strokeStyle = CreateStrokeStyleForOptions(aStrokeOptions);

  if (brush && strokeStyle) {
    rt->DrawLine(D2DPoint(aStart), D2DPoint(aEnd), brush, aStrokeOptions.mLineWidth, strokeStyle);
  }

  FinalizeRTForOperation(aOptions.mCompositionOp, aPattern, Rect(0, 0, Float(mSize.width), Float(mSize.height)));
}

void
DrawTargetD2D::Stroke(const Path *aPath,
                      const Pattern &aPattern,
                      const StrokeOptions &aStrokeOptions,
                      const DrawOptions &aOptions)
{
  if (aPath->GetBackendType() != BACKEND_DIRECT2D) {
    gfxDebug() << *this << ": Ignoring drawing call for incompatible path.";
    return;
  }

  const PathD2D *d2dPath = static_cast<const PathD2D*>(aPath);

  ID2D1RenderTarget *rt = GetRTForOperation(aOptions.mCompositionOp, aPattern);

  PrepareForDrawing(rt);

  RefPtr<ID2D1Brush> brush = CreateBrushForPattern(aPattern, aOptions.mAlpha);

  RefPtr<ID2D1StrokeStyle> strokeStyle = CreateStrokeStyleForOptions(aStrokeOptions);

  if (brush && strokeStyle) {
    rt->DrawGeometry(d2dPath->mGeometry, brush, aStrokeOptions.mLineWidth, strokeStyle);
  }

  FinalizeRTForOperation(aOptions.mCompositionOp, aPattern, Rect(0, 0, Float(mSize.width), Float(mSize.height)));
}

void
DrawTargetD2D::Fill(const Path *aPath,
                    const Pattern &aPattern,
                    const DrawOptions &aOptions)
{
  if (aPath->GetBackendType() != BACKEND_DIRECT2D) {
    gfxDebug() << *this << ": Ignoring drawing call for incompatible path.";
    return;
  }

  const PathD2D *d2dPath = static_cast<const PathD2D*>(aPath);

  ID2D1RenderTarget *rt = GetRTForOperation(aOptions.mCompositionOp, aPattern);

  PrepareForDrawing(rt);

  RefPtr<ID2D1Brush> brush = CreateBrushForPattern(aPattern, aOptions.mAlpha);

  if (brush) {
    rt->FillGeometry(d2dPath->mGeometry, brush);
  }

  Rect bounds;
  if (aOptions.mCompositionOp != OP_OVER) {
    D2D1_RECT_F d2dbounds;
    d2dPath->mGeometry->GetBounds(D2D1::IdentityMatrix(), &d2dbounds);
    bounds = ToRect(d2dbounds);
  }
  FinalizeRTForOperation(aOptions.mCompositionOp, aPattern, bounds);
}

void
DrawTargetD2D::FillGlyphs(ScaledFont *aFont,
                          const GlyphBuffer &aBuffer,
                          const Pattern &aPattern,
                          const DrawOptions &aOptions)
{
  if (aFont->GetType() != FONT_DWRITE) {
    gfxDebug() << *this << ": Ignoring drawing call for incompatible font.";
    return;
  }

  ScaledFontDWrite *font = static_cast<ScaledFontDWrite*>(aFont);

  ID2D1RenderTarget *rt = GetRTForOperation(aOptions.mCompositionOp, aPattern);

  PrepareForDrawing(rt);

  RefPtr<ID2D1Brush> brush = CreateBrushForPattern(aPattern, aOptions.mAlpha);

  DWRITE_GLYPH_RUN glyphRun;

  glyphRun.bidiLevel = 0;
  glyphRun.fontEmSize = font->mSize;
  glyphRun.isSideways = FALSE;
  glyphRun.fontFace = font->mFontFace;
  glyphRun.glyphCount = aBuffer.mNumGlyphs;
  
  std::vector<UINT16> indices;
  std::vector<FLOAT> advances;
  std::vector<DWRITE_GLYPH_OFFSET> offsets;
  indices.resize(aBuffer.mNumGlyphs);
  advances.resize(aBuffer.mNumGlyphs);
  offsets.resize(aBuffer.mNumGlyphs);

  memset(&advances.front(), 0, sizeof(FLOAT) * aBuffer.mNumGlyphs);
  for (unsigned int i = 0; i < aBuffer.mNumGlyphs; i++) {
    indices[i] = aBuffer.mGlyphs[i].mIndex;
    offsets[i].advanceOffset = aBuffer.mGlyphs[i].mPosition.x;
    offsets[i].ascenderOffset = -aBuffer.mGlyphs[i].mPosition.y;
  }

  glyphRun.glyphAdvances = &advances.front();
  glyphRun.glyphIndices = &indices.front();
  glyphRun.glyphOffsets = &offsets.front();

  if (brush) {
    rt->DrawGlyphRun(D2D1::Point2F(), &glyphRun, brush);
  }

  FinalizeRTForOperation(aOptions.mCompositionOp, aPattern, Rect(0, 0, (Float)mSize.width, (Float)mSize.height));
}

void
DrawTargetD2D::PushClip(const Path *aPath)
{
  if (aPath->GetBackendType() != BACKEND_DIRECT2D) {
    gfxDebug() << *this << ": Ignoring clipping call for incompatible path.";
    return;
  }

  RefPtr<PathD2D> pathD2D = static_cast<PathD2D*>(const_cast<Path*>(aPath));

  PushedClip clip;
  clip.mTransform = D2DMatrix(mTransform);
  clip.mPath = pathD2D;
  
  RefPtr<ID2D1Layer> layer;
  pathD2D->mGeometry->GetBounds(clip.mTransform, &clip.mBounds);

  mRT->CreateLayer( byRef(layer));

  clip.mLayer = layer;
  mPushedClips.push_back(clip);

  // The transform of clips is relative to the world matrix, since we use the total
  // transform for the clips, make the world matrix identity.
  mRT->SetTransform(D2D1::IdentityMatrix());
  mTransformDirty = true;

  if (mClipsArePushed) {
    D2D1_LAYER_OPTIONS options = D2D1_LAYER_OPTIONS_NONE;

    if (mFormat == FORMAT_B8G8R8X8) {
      options = D2D1_LAYER_OPTIONS_INITIALIZE_FOR_CLEARTYPE;
    }

    mRT->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), pathD2D->mGeometry,
                                         D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                         clip.mTransform, 1.0f, NULL,
                                         options), layer);
  }
}

void
DrawTargetD2D::PopClip()
{
  if (mClipsArePushed) {
    mRT->PopLayer();
  }
  mPushedClips.pop_back();
}

TemporaryRef<SourceSurface> 
DrawTargetD2D::CreateSourceSurfaceFromData(unsigned char *aData,
                                           const IntSize &aSize,
                                           int32_t aStride,
                                           SurfaceFormat aFormat) const
{
  RefPtr<SourceSurfaceD2D> newSurf = new SourceSurfaceD2D();

  if (!newSurf->InitFromData(aData, aSize, aStride, aFormat, mRT)) {
    gfxDebug() << *this << ": Failure to create source surface from data. Size: " << aSize;
    return NULL;
  }

  return newSurf;
}

TemporaryRef<SourceSurface> 
DrawTargetD2D::OptimizeSourceSurface(SourceSurface *aSurface) const
{
  // Unsupported!
  return NULL;
}

TemporaryRef<SourceSurface>
DrawTargetD2D::CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const
{
  if (aSurface.mType != NATIVE_SURFACE_D3D10_TEXTURE) {
    gfxDebug() << *this << ": Failure to create source surface from non-D3D10 texture native surface.";
    return NULL;
  }
  RefPtr<SourceSurfaceD2D> newSurf = new SourceSurfaceD2D();

  if (!newSurf->InitFromTexture(static_cast<ID3D10Texture2D*>(aSurface.mSurface),
                                aSurface.mFormat,
                                mRT))
  {
    gfxWarning() << *this << ": Failed to create SourceSurface from texture.";
    return NULL;
  }

  return newSurf;
}

TemporaryRef<DrawTarget>
DrawTargetD2D::CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const
{
  RefPtr<DrawTargetD2D> newTarget =
    new DrawTargetD2D();

  if (!newTarget->Init(aSize, aFormat)) {
    gfxDebug() << *this << ": Failed to create optimal draw target. Size: " << aSize;
    return NULL;
  }

  return newTarget;
}

TemporaryRef<PathBuilder>
DrawTargetD2D::CreatePathBuilder(FillRule aFillRule) const
{
  RefPtr<ID2D1PathGeometry> path;
  HRESULT hr = factory()->CreatePathGeometry(byRef(path));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create Direct2D Path Geometry. Code: " << hr;
    return NULL;
  }

  RefPtr<ID2D1GeometrySink> sink;
  hr = path->Open(byRef(sink));
  if (FAILED(hr)) {
    gfxWarning() << "Failed to access Direct2D Path Geometry. Code: " << hr;
    return NULL;
  }

  if (aFillRule == FILL_WINDING) {
    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
  }

  return new PathBuilderD2D(sink, path, aFillRule);
}

TemporaryRef<GradientStops>
DrawTargetD2D::CreateGradientStops(GradientStop *rawStops, uint32_t aNumStops) const
{
  D2D1_GRADIENT_STOP *stops = new D2D1_GRADIENT_STOP[aNumStops];

  for (uint32_t i = 0; i < aNumStops; i++) {
    stops[i].position = rawStops[i].offset;
    stops[i].color = D2DColor(rawStops[i].color);
  }

  RefPtr<ID2D1GradientStopCollection> stopCollection;

  HRESULT hr = mRT->CreateGradientStopCollection(stops, aNumStops, byRef(stopCollection));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create GradientStopCollection. Code: " << hr;
    return NULL;
  }

  return new GradientStopsD2D(stopCollection);
}

void*
DrawTargetD2D::GetNativeSurface(NativeSurfaceType aType)
{
  if (aType != NATIVE_SURFACE_D3D10_TEXTURE) {
    return NULL;
  }

  return mTexture;
}

/*
 * Public functions
 */
bool
DrawTargetD2D::Init(const IntSize &aSize, SurfaceFormat aFormat)
{
  HRESULT hr;

  mSize = aSize;
  mFormat = aFormat;

  if (!Factory::GetDirect3D10Device()) {
    gfxDebug() << "Failed to Init Direct2D DrawTarget (No D3D10 Device set.)";
    return false;
  }
  mDevice = Factory::GetDirect3D10Device();

  CD3D10_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM,
                             mSize.width,
                             mSize.height,
                             1, 1);
  desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;

  hr = mDevice->CreateTexture2D(&desc, NULL, byRef(mTexture));

  if (FAILED(hr)) {
    gfxDebug() << "Failed to init Direct2D DrawTarget. Size: " << mSize << " Code: " << hr;
    return false;
  }

  return InitD2DRenderTarget();
}

bool
DrawTargetD2D::Init(ID3D10Texture2D *aTexture, SurfaceFormat aFormat)
{
  HRESULT hr;

  mTexture = aTexture;
  mFormat = aFormat;

  if (!mTexture) {
    gfxDebug() << "No valid texture for Direct2D draw target initialization.";
    return false;
  }

  RefPtr<ID3D10Device> device;
  mTexture->GetDevice(byRef(device));

  hr = device->QueryInterface((ID3D10Device1**)byRef(mDevice));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to get D3D10 device from texture.";
    return false;
  }

  D3D10_TEXTURE2D_DESC desc;
  mTexture->GetDesc(&desc);
  mSize.width = desc.Width;
  mSize.height = desc.Height;

  return InitD2DRenderTarget();
}

// {0D398B49-AE7B-416F-B26D-EA3C137D1CF7}
static const GUID sPrivateDataD2D = 
{ 0xd398b49, 0xae7b, 0x416f, { 0xb2, 0x6d, 0xea, 0x3c, 0x13, 0x7d, 0x1c, 0xf7 } };

bool
DrawTargetD2D::InitD3D10Data()
{
  HRESULT hr;
  
  UINT privateDataSize;
  privateDataSize = sizeof(mPrivateData);
  hr = mDevice->GetPrivateData(sPrivateDataD2D, &privateDataSize, &mPrivateData);

  if (SUCCEEDED(hr)) {
      return true;
  }

  mPrivateData = new PrivateD3D10DataD2D;

  D3D10CreateEffectFromMemoryFunc createD3DEffect;
  HMODULE d3dModule = LoadLibraryW(L"d3d10_1.dll");
  createD3DEffect = (D3D10CreateEffectFromMemoryFunc)
      GetProcAddress(d3dModule, "D3D10CreateEffectFromMemory");

  hr = createD3DEffect((void*)d2deffect, sizeof(d2deffect), 0, mDevice, NULL, byRef(mPrivateData->mEffect));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to initialize Direct2D required effects. Code: " << hr;
    return false;
  }

  privateDataSize = sizeof(mPrivateData);
  mDevice->SetPrivateData(sPrivateDataD2D, privateDataSize, &mPrivateData);

  D3D10_INPUT_ELEMENT_DESC layout[] =
  {
    { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
  };
  D3D10_PASS_DESC passDesc;
  
  mPrivateData->mEffect->GetTechniqueByName("SampleTexture")->GetPassByIndex(0)->GetDesc(&passDesc);

  hr = mDevice->CreateInputLayout(layout,
                                  sizeof(layout) / sizeof(D3D10_INPUT_ELEMENT_DESC),
                                  passDesc.pIAInputSignature,
                                  passDesc.IAInputSignatureSize,
                                  byRef(mPrivateData->mInputLayout));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to initialize Direct2D required InputLayout. Code: " << hr;
    return false;
  }

  D3D10_SUBRESOURCE_DATA data;
  Vertex vertices[] = { {0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}, {1.0, 1.0} };
  data.pSysMem = vertices;
  CD3D10_BUFFER_DESC bufferDesc(sizeof(vertices), D3D10_BIND_VERTEX_BUFFER);

  hr = mDevice->CreateBuffer(&bufferDesc, &data, byRef(mPrivateData->mVB));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to initialize Direct2D required VertexBuffer. Code: " << hr;
    return false;
  }

  return true;
}

/*
 * Private helpers
 */
bool
DrawTargetD2D::InitD2DRenderTarget()
{
  if (!factory()) {
    return false;
  }

  mRT = CreateRTForTexture(mTexture);

  if (!mRT) {
    return false;
  }

  mRT->BeginDraw();

  mRT->Clear(D2D1::ColorF(0, 0));

  if (mFormat == FORMAT_B8G8R8X8) {
    mRT->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
  }

  return InitD3D10Data();
}

void
DrawTargetD2D::PrepareForDrawing(ID2D1RenderTarget *aRT)
{
  if (!mClipsArePushed || aRT == mTempRT) {
    if (mPushedClips.size()) {
      // The transform of clips is relative to the world matrix, since we use the total
      // transform for the clips, make the world matrix identity.
      mRT->SetTransform(D2D1::IdentityMatrix());
      for (std::vector<PushedClip>::iterator iter = mPushedClips.begin();
           iter != mPushedClips.end(); iter++) {
        D2D1_LAYER_OPTIONS options = D2D1_LAYER_OPTIONS_NONE;

        if (mFormat == FORMAT_B8G8R8X8) {
          options = D2D1_LAYER_OPTIONS_INITIALIZE_FOR_CLEARTYPE;
        }

        aRT->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), iter->mPath->mGeometry,
                                             D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                             iter->mTransform, 1.0f, NULL,
                                             options), iter->mLayer);
      }
      if (aRT == mRT) {
        mClipsArePushed = true;
      }
    }
  }
  mRT->SetTransform(D2DMatrix(mTransform));
  MarkChanged();

  if (aRT == mTempRT) {
    mTempRT->SetTransform(D2DMatrix(mTransform));
  }
}

void
DrawTargetD2D::MarkChanged()
{
  if (mSnapshots.size()) {
    for (std::vector<SourceSurfaceD2DTarget*>::iterator iter = mSnapshots.begin();
         iter != mSnapshots.end(); iter++) {
      (*iter)->DrawTargetWillChange();
    }
    // All snapshots will now have copied data.
    mSnapshots.clear();
  }
  if (mDependentTargets.size()) {
    // Copy mDependentTargets since the Flush()es below will modify it.
    std::vector<DrawTargetD2D*> tmpTargets = mDependentTargets;
    for (std::vector<DrawTargetD2D*>::iterator iter = tmpTargets.begin();
         iter != tmpTargets.end(); iter++) {
      (*iter)->Flush();
    }
    // The Flush() should have broken all dependencies on this target.
    MOZ_ASSERT(!mDependentTargets.size());
  }
}

ID3D10BlendState*
DrawTargetD2D::GetBlendStateForOperator(CompositionOp aOperator)
{
  if (mPrivateData->mBlendStates[aOperator]) {
    return mPrivateData->mBlendStates[aOperator];
  }

  D3D10_BLEND_DESC desc;

  memset(&desc, 0, sizeof(D3D10_BLEND_DESC));

  desc.AlphaToCoverageEnable = FALSE;
  desc.BlendEnable[0] = TRUE;
  desc.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_ALL;
  desc.BlendOp = desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;

  switch (aOperator) {
  case OP_ADD:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_ONE;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_ONE;
    break;
  case OP_IN:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_DEST_ALPHA;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_ZERO;
    break;
  case OP_OUT:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_INV_DEST_ALPHA;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_ZERO;
    break;
  case OP_ATOP:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_DEST_ALPHA;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
    break;
  case OP_DEST_IN:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_ZERO;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_SRC_ALPHA;
    break;
  case OP_DEST_OUT:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_ZERO;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
    break;
  case OP_DEST_ATOP:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_INV_DEST_ALPHA;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_SRC_ALPHA;
    break;
  case OP_DEST_OVER:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_INV_DEST_ALPHA;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_ONE;
    break;
  case OP_XOR:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_INV_DEST_ALPHA;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
    break;
  case OP_SOURCE:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_ONE;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_ZERO;
    break;
  default:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_ONE;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
  }
  
  mDevice->CreateBlendState(&desc, byRef(mPrivateData->mBlendStates[aOperator]));

  return mPrivateData->mBlendStates[aOperator];
}

/* This function prepares the temporary RT for drawing and returns it when a
 * drawing operation other than OVER is required.
 */
ID2D1RenderTarget*
DrawTargetD2D::GetRTForOperation(CompositionOp aOperator, const Pattern &aPattern)
{
  if (aOperator == OP_OVER && !IsPatternSupportedByD2D(aPattern)) {
    return mRT;
  }

  PopAllClips();

  if (mTempRT) {
    mTempRT->Clear(D2D1::ColorF(0, 0));
    return mTempRT;
  }

  EnsureViews();

  if (!mRTView || !mSRView) {
    gfxDebug() << *this << ": Failed to get required views. Defaulting to OP_OVER.";
    return mRT;
  }

  mTempRT = CreateRTForTexture(mTempTexture);

  if (!mTempRT) {
    return mRT;
  }

  mTempRT->BeginDraw();

  mTempRT->Clear(D2D1::ColorF(0, 0));

  return mTempRT;
}

/* This function blends back the content of a drawing operation (drawn to an
 * empty surface with OVER, so the surface now contains the source operation
 * contents) to the rendertarget using the requested composition operation.
 * In order to respect clip for operations which are unbound by their mask,
 * the old content of the surface outside the clipped area may be blended back
 * to the surface.
 */
void
DrawTargetD2D::FinalizeRTForOperation(CompositionOp aOperator, const Pattern &aPattern, const Rect &aBounds)
{
  if (aOperator == OP_OVER && !IsPatternSupportedByD2D(aPattern)) {
    return;
  }

  if (!mTempRT) {
    return;
  }

  for (unsigned int i = 0; i < mPushedClips.size(); i++) {
    mTempRT->PopLayer();
  }

  mRT->Flush();
  mTempRT->Flush();

  AutoSaveRestoreClippedOut restoreClippedOut(this);

  bool needsWriteBack =
    !IsOperatorBoundByMask(aOperator) && mPushedClips.size();

  if (needsWriteBack) {
    restoreClippedOut.Save();
  }

  ID3D10RenderTargetView *rtViews = mRTView;
  mDevice->OMSetRenderTargets(1, &rtViews, NULL);

  UINT stride = sizeof(Vertex);
  UINT offset = 0;
  ID3D10Buffer *buff = mPrivateData->mVB;

  mDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  mDevice->IASetVertexBuffers(0, 1, &buff, &stride, &offset);
  mDevice->IASetInputLayout(mPrivateData->mInputLayout);

  D3D10_VIEWPORT viewport;
  viewport.MaxDepth = 1;
  viewport.MinDepth = 0;
  viewport.Height = mSize.height;
  viewport.Width = mSize.width;
  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;

  mDevice->RSSetViewports(1, &viewport);
  mPrivateData->mEffect->GetVariableByName("QuadDesc")->AsVector()->
    SetFloatVector(ShaderConstantRectD3D10(-1.0f, 1.0f, 2.0f, -2.0f));

  if (!IsPatternSupportedByD2D(aPattern)) {
    mPrivateData->mEffect->GetVariableByName("TexCoords")->AsVector()->
      SetFloatVector(ShaderConstantRectD3D10(0, 0, 1.0f, 1.0f));
    mPrivateData->mEffect->GetVariableByName("tex")->AsShaderResource()->SetResource(mSRView);
    mPrivateData->mEffect->GetTechniqueByName("SampleTexture")->GetPassByIndex(0)->Apply(0);
  } else if (aPattern.GetType() == PATTERN_RADIAL_GRADIENT) {
    const RadialGradientPattern *pat = static_cast<const RadialGradientPattern*>(&aPattern);

    if (pat->mCenter1 == pat->mCenter2 && pat->mRadius1 == pat->mRadius2) {
      // Draw nothing!
      return;
    }

    mPrivateData->mEffect->GetVariableByName("mask")->AsShaderResource()->SetResource(mSRView);

    SetupEffectForRadialGradient(pat);
  }

  mDevice->OMSetBlendState(GetBlendStateForOperator(aOperator), NULL, 0xffffffff);
  
  mDevice->Draw(4, 0);
}

TemporaryRef<ID2D1Geometry>
DrawTargetD2D::GetClippedGeometry()
{
  RefPtr<ID2D1GeometrySink> currentSink;
  RefPtr<ID2D1PathGeometry> clippedGeometry;

  factory()->CreatePathGeometry(byRef(clippedGeometry));
  clippedGeometry->Open(byRef(currentSink));
      
  std::vector<DrawTargetD2D::PushedClip>::iterator iter = mPushedClips.begin();
  iter->mPath->GetGeometry()->Simplify(D2D1_GEOMETRY_SIMPLIFICATION_OPTION_CUBICS_AND_LINES,
                                    iter->mTransform, currentSink);

  currentSink->Close();

  iter++;
  for (;iter != mPushedClips.end(); iter++) {
    RefPtr<ID2D1PathGeometry> newGeom;
    factory()->CreatePathGeometry(byRef(newGeom));

    newGeom->Open(byRef(currentSink));
    clippedGeometry->CombineWithGeometry(iter->mPath->GetGeometry(), D2D1_COMBINE_MODE_INTERSECT,
                                      iter->mTransform, currentSink);

    currentSink->Close();

    clippedGeometry = newGeom;
  }

  return clippedGeometry;
}

TemporaryRef<ID2D1RenderTarget>
DrawTargetD2D::CreateRTForTexture(ID3D10Texture2D *aTexture)
{
  HRESULT hr;

  RefPtr<IDXGISurface> surface;
  RefPtr<ID2D1RenderTarget> rt;

  hr = aTexture->QueryInterface((IDXGISurface**)byRef(surface));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to QI texture to surface.";
    return NULL;
  }

  D3D10_TEXTURE2D_DESC desc;
  aTexture->GetDesc(&desc);

  D2D1_ALPHA_MODE alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;

  if (mFormat == FORMAT_B8G8R8X8) {
    alphaMode = D2D1_ALPHA_MODE_IGNORE;
  }

  D2D1_RENDER_TARGET_PROPERTIES props =
    D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(desc.Format, alphaMode));
  hr = factory()->CreateDxgiSurfaceRenderTarget(surface, props, byRef(rt));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create D2D render target for texture.";
    return NULL;
  }

  return rt;
}

void
DrawTargetD2D::EnsureViews()
{
  if (mTempTexture && mSRView && mRTView) {
    return;
  }

  HRESULT hr;

  CD3D10_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM,
                             mSize.width,
                             mSize.height,
                             1, 1);
  desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;

  hr = mDevice->CreateTexture2D(&desc, NULL, byRef(mTempTexture));

  if (FAILED(hr)) {
    gfxWarning() << *this << "Failed to create temporary texture for rendertarget. Size: "
      << mSize << " Code: " << hr;
    return;
  }

  hr = mDevice->CreateShaderResourceView(mTempTexture, NULL, byRef(mSRView));

  if (FAILED(hr)) {
    gfxWarning() << *this << "Failed to create shader resource view for temp texture. Code: " << hr;
    return;
  }

  hr = mDevice->CreateRenderTargetView(mTexture, NULL, byRef(mRTView));

  if (FAILED(hr)) {
    gfxWarning() << *this << "Failed to create rendertarget view for temp texture. Code: " << hr;
  }
}

void
DrawTargetD2D::PopAllClips()
{
  if (mClipsArePushed) {
    for (unsigned int i = 0; i < mPushedClips.size(); i++) {
      mRT->PopLayer();
    }
  
    mClipsArePushed = false;
  }
}

TemporaryRef<ID2D1Brush>
DrawTargetD2D::CreateBrushForPattern(const Pattern &aPattern, Float aAlpha)
{
  if (IsPatternSupportedByD2D(aPattern)) {
    RefPtr<ID2D1SolidColorBrush> colBrush;
    mRT->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), byRef(colBrush));
    return colBrush;
  }

  if (aPattern.GetType() == PATTERN_COLOR) {
    RefPtr<ID2D1SolidColorBrush> colBrush;
    Color color = static_cast<const ColorPattern*>(&aPattern)->mColor;
    mRT->CreateSolidColorBrush(D2D1::ColorF(color.r, color.g,
                                            color.b, color.a),
                               D2D1::BrushProperties(aAlpha),
                               byRef(colBrush));
    return colBrush;
  } else if (aPattern.GetType() == PATTERN_LINEAR_GRADIENT) {
    RefPtr<ID2D1LinearGradientBrush> gradBrush;
    const LinearGradientPattern *pat =
      static_cast<const LinearGradientPattern*>(&aPattern);

    GradientStopsD2D *stops = static_cast<GradientStopsD2D*>(pat->mStops.get());

    if (!stops) {
      gfxDebug() << "No stops specified for gradient pattern.";
      return NULL;
    }

    mRT->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2DPoint(pat->mBegin),
                                                                       D2DPoint(pat->mEnd)),
                                   D2D1::BrushProperties(aAlpha),
                                   stops->mStopCollection,
                                   byRef(gradBrush));
    return gradBrush;
  } else if (aPattern.GetType() == PATTERN_RADIAL_GRADIENT) {
    RefPtr<ID2D1RadialGradientBrush> gradBrush;
    const RadialGradientPattern *pat =
      static_cast<const RadialGradientPattern*>(&aPattern);

    GradientStopsD2D *stops = static_cast<GradientStopsD2D*>(pat->mStops.get());

    if (!stops) {
      gfxDebug() << "No stops specified for gradient pattern.";
      return NULL;
    }

    // This will not be a complex radial gradient brush.
    mRT->CreateRadialGradientBrush(
      D2D1::RadialGradientBrushProperties(D2DPoint(pat->mCenter1),
                                          D2D1::Point2F(),
                                          pat->mRadius2, pat->mRadius2),
      D2D1::BrushProperties(aAlpha),
      stops->mStopCollection,
      byRef(gradBrush));

    return gradBrush;
  } else if (aPattern.GetType() == PATTERN_SURFACE) {
    RefPtr<ID2D1BitmapBrush> bmBrush;
    const SurfacePattern *pat =
      static_cast<const SurfacePattern*>(&aPattern);

    if (!pat->mSurface) {
      gfxDebug() << "No source surface specified for surface pattern";
      return NULL;
    }

    RefPtr<ID2D1Bitmap> bitmap;
    
    switch (pat->mSurface->GetType()) {
    case SURFACE_D2D1_BITMAP:
      {
        SourceSurfaceD2D *surf = static_cast<SourceSurfaceD2D*>(pat->mSurface.get());

        bitmap = surf->mBitmap;

        if (!bitmap) {
          gfxDebug() << "Source surface used for pattern too large!";
          return NULL;
        }
      }
      break;
    case SURFACE_D2D1_DRAWTARGET:
      {
        SourceSurfaceD2DTarget *surf =
          static_cast<SourceSurfaceD2DTarget*>(pat->mSurface.get());

        bitmap = surf->GetBitmap(mRT);

        if (!surf->IsCopy()) {
          surf->mDrawTarget->mDependentTargets.push_back(this);
          mDependingOnTargets.push_back(surf->mDrawTarget);
        }
      }
      break;
    }

    D2D1_EXTEND_MODE extend = D2D1_EXTEND_MODE_CLAMP;
    switch (pat->mExtendMode) {
    case EXTEND_WRAP:
      extend = D2D1_EXTEND_MODE_WRAP;
      break;
    case EXTEND_MIRROR:
      extend = D2D1_EXTEND_MODE_MIRROR;
      break;
    }

    mRT->CreateBitmapBrush(bitmap,
                           D2D1::BitmapBrushProperties(extend,
                                                       extend,
                                                       D2DFilter(pat->mFilter)),
                           D2D1::BrushProperties(aAlpha),
                           byRef(bmBrush));

    return bmBrush;
  }

  gfxWarning() << "Invalid pattern type detected.";
  return NULL;
}

TemporaryRef<ID2D1StrokeStyle>
DrawTargetD2D::CreateStrokeStyleForOptions(const StrokeOptions &aStrokeOptions)
{
  RefPtr<ID2D1StrokeStyle> style;

  D2D1_CAP_STYLE capStyle;
  D2D1_LINE_JOIN joinStyle;

  switch (aStrokeOptions.mLineCap) {
  case CAP_BUTT:
    capStyle = D2D1_CAP_STYLE_FLAT;
    break;
  case CAP_ROUND:
    capStyle = D2D1_CAP_STYLE_ROUND;
    break;
  case CAP_SQUARE:
    capStyle = D2D1_CAP_STYLE_SQUARE;
    break;
  }

  switch (aStrokeOptions.mLineJoin) {
  case JOIN_MITER:
    joinStyle = D2D1_LINE_JOIN_MITER;
    break;
  case JOIN_MITER_OR_BEVEL:
    joinStyle = D2D1_LINE_JOIN_MITER_OR_BEVEL;
    break;
  case JOIN_ROUND:
    joinStyle = D2D1_LINE_JOIN_ROUND;
    break;
  case JOIN_BEVEL:
    joinStyle = D2D1_LINE_JOIN_BEVEL;
    break;
  }


  HRESULT hr;
  if (aStrokeOptions.mDashPattern) {
    typedef vector<Float> FloatVector;
    // D2D "helpfully" multiplies the dash pattern by the line width.
    // That's not what cairo does, or is what <canvas>'s dash wants.
    // So fix the multiplication in advance.
    Float lineWidth = aStrokeOptions.mLineWidth;
    FloatVector dash(aStrokeOptions.mDashPattern,
                     aStrokeOptions.mDashPattern + aStrokeOptions.mDashLength);
    for (FloatVector::iterator it = dash.begin(); it != dash.end(); ++it) {
      *it /= lineWidth;
    }

    hr = factory()->CreateStrokeStyle(
      D2D1::StrokeStyleProperties(capStyle, capStyle,
                                  capStyle, joinStyle,
                                  aStrokeOptions.mMiterLimit,
                                  D2D1_DASH_STYLE_CUSTOM,
                                  aStrokeOptions.mDashOffset),
      &dash[0], // data() is not C++98, although it's in recent gcc
                // and VC10's STL
      dash.size(),
      byRef(style));
  } else {
    hr = factory()->CreateStrokeStyle(
      D2D1::StrokeStyleProperties(capStyle, capStyle,
                                  capStyle, joinStyle,
                                  aStrokeOptions.mMiterLimit),
      NULL, 0, byRef(style));
  }

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create Direct2D stroke style.";
  }

  return style;
}

TemporaryRef<ID3D10Texture1D>
DrawTargetD2D::CreateGradientTexture(const GradientStopsD2D *aStops)
{
  CD3D10_TEXTURE1D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, 4096, 1, 1);

  std::vector<D2D1_GRADIENT_STOP> rawStops;
  rawStops.resize(aStops->mStopCollection->GetGradientStopCount());
  aStops->mStopCollection->GetGradientStops(&rawStops.front(), rawStops.size());

  std::vector<unsigned char> textureData;
  textureData.resize(4096 * 4);
  unsigned char *texData = &textureData.front();

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

    if (pos > nextColorPos) {
      prevColor = nextColor;
      prevColorPos = nextColorPos;
      if (rawStops.size() > stopPosition) {
        nextColor = rawStops[stopPosition].color;
        nextColorPos = rawStops[stopPosition++].position;
      } else {
        nextColorPos = 1.0f;
      }
    }

    float interp = (pos - prevColorPos) / (nextColorPos - prevColorPos);

    Color newColor(prevColor.r + (nextColor.r - prevColor.r) * interp,
                    prevColor.g + (nextColor.g - prevColor.g) * interp,
                    prevColor.b + (nextColor.b - prevColor.b) * interp,
                    prevColor.a + (nextColor.a - prevColor.a) * interp);

    texData[i * 4] = (char)(255.0f * newColor.b);
    texData[i * 4 + 1] = (char)(255.0f * newColor.g);
    texData[i * 4 + 2] = (char)(255.0f * newColor.r);
    texData[i * 4 + 3] = (char)(255.0f * newColor.a);
  }

  D3D10_SUBRESOURCE_DATA data;
  data.pSysMem = &textureData.front();

  RefPtr<ID3D10Texture1D> tex;
  mDevice->CreateTexture1D(&desc, &data, byRef(tex));

  return tex;
}

void
DrawTargetD2D::SetupEffectForRadialGradient(const RadialGradientPattern *aPattern)
{
  mPrivateData->mEffect->GetTechniqueByName("SampleRadialGradient")->GetPassByIndex(0)->Apply(0);
  mPrivateData->mEffect->GetVariableByName("MaskTexCoords")->AsVector()->
    SetFloatVector(ShaderConstantRectD3D10(0, 0, 1.0f, 1.0f));

  float dimensions[] = { float(mSize.width), float(mSize.height), 0, 0 };
  mPrivateData->mEffect->GetVariableByName("dimensions")->AsVector()->
    SetFloatVector(dimensions);

  const GradientStopsD2D *stops =
    static_cast<const GradientStopsD2D*>(aPattern->mStops.get());

  RefPtr<ID3D10Texture1D> tex = CreateGradientTexture(stops);

  RefPtr<ID3D10ShaderResourceView> srView;
  mDevice->CreateShaderResourceView(tex, NULL, byRef(srView));

  mPrivateData->mEffect->GetVariableByName("tex")->AsShaderResource()->SetResource(srView);

  Point dc = aPattern->mCenter2 - aPattern->mCenter1;
  float dr = aPattern->mRadius2 - aPattern->mRadius1;

  float diffv[] = { dc.x, dc.y, dr, 0 };
  mPrivateData->mEffect->GetVariableByName("diff")->AsVector()->
    SetFloatVector(diffv);

  float center1[] = { aPattern->mCenter1.x, aPattern->mCenter1.y, dr, 0 };
  mPrivateData->mEffect->GetVariableByName("center1")->AsVector()->
    SetFloatVector(center1);

  mPrivateData->mEffect->GetVariableByName("radius1")->AsScalar()->
    SetFloat(aPattern->mRadius1);
  mPrivateData->mEffect->GetVariableByName("sq_radius1")->AsScalar()->
    SetFloat(pow(aPattern->mRadius1, 2));

  Matrix invTransform = mTransform;

  if (!invTransform.Invert()) {
    // Bail if the matrix is singular.
    return;
  }
  float matrix[] = { invTransform._11, invTransform._12, 0, 0,
                      invTransform._21, invTransform._22, 0, 0,
                      invTransform._31, invTransform._32, 1.0f, 0,
                      0, 0, 0, 1.0f };

  mPrivateData->mEffect->GetVariableByName("DeviceSpaceToUserSpace")->
    AsMatrix()->SetMatrix(matrix);

  float A = dc.x * dc.x + dc.y * dc.y - dr * dr;
  if (A == 0) {
    mPrivateData->mEffect->GetTechniqueByName("SampleRadialGradient")->
      GetPassByIndex(1)->Apply(0);
  } else {
    mPrivateData->mEffect->GetVariableByName("A")->AsScalar()->SetFloat(A);
    mPrivateData->mEffect->GetTechniqueByName("SampleRadialGradient")->
      GetPassByIndex(0)->Apply(0);
  }
}

ID2D1Factory*
DrawTargetD2D::factory()
{
  if (mFactory) {
    return mFactory;
  }

  D2D1CreateFactoryFunc createD2DFactory;
  HMODULE d2dModule = LoadLibraryW(L"d2d1.dll");
  createD2DFactory = (D2D1CreateFactoryFunc)
      GetProcAddress(d2dModule, "D2D1CreateFactory");

  if (!createD2DFactory) {
    gfxWarning() << "Failed to locate D2D1CreateFactory function.";
    return NULL;
  }

  D2D1_FACTORY_OPTIONS options;
#ifdef _DEBUG
  options.debugLevel = D2D1_DEBUG_LEVEL_NONE;
#else
  options.debugLevel = D2D1_DEBUG_LEVEL_NONE;
#endif

  HRESULT hr = createD2DFactory(D2D1_FACTORY_TYPE_MULTI_THREADED,
                                __uuidof(ID2D1Factory),
                                &options,
                                (void**)&mFactory);

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create Direct2D factory.";
  }

  return mFactory;
}

}
}
