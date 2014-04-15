/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <initguid.h>
#include "DrawTargetD2D.h"
#include "SourceSurfaceD2D.h"
#ifdef USE_D2D1_1
#include "SourceSurfaceD2D1.h"
#endif
#include "SourceSurfaceD2DTarget.h"
#include "ShadersD2D.h"
#include "PathD2D.h"
#include "GradientStopsD2D.h"
#include "ScaledFontDWrite.h"
#include "ImageScaling.h"
#include "Logging.h"
#include "Tools.h"
#include <algorithm>
#include "mozilla/Constants.h"
#include "FilterNodeSoftware.h"

#ifdef USE_D2D1_1
#include "FilterNodeD2D1.h"
#endif

#include <dwrite.h>

// decltype is not usable for overloaded functions.
typedef HRESULT (WINAPI*D2D1CreateFactoryFunc)(
    D2D1_FACTORY_TYPE factoryType,
    REFIID iid,
    CONST D2D1_FACTORY_OPTIONS *pFactoryOptions,
    void **factory
);

using namespace std;

namespace mozilla {
namespace gfx {

struct Vertex {
  float x;
  float y;
};

ID2D1Factory *DrawTargetD2D::mFactory;
IDWriteFactory *DrawTargetD2D::mDWriteFactory;
uint64_t DrawTargetD2D::mVRAMUsageDT;
uint64_t DrawTargetD2D::mVRAMUsageSS;

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

    HRESULT hr = mDT->mDevice->CreateTexture2D(&desc, nullptr, byRef(tmpTexture));
    if (FAILED(hr)) {
      gfxWarning() << "Failed to create temporary texture to hold surface data.";
    }
    mDT->mDevice->CopyResource(tmpTexture, mDT->mTexture);

    D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(D2DPixelFormat(format));

    RefPtr<IDXGISurface> surf;

    tmpTexture->QueryInterface((IDXGISurface**)byRef(surf));

    hr = mDT->mRT->CreateSharedBitmap(IID_IDXGISurface, surf,
                                      &props, byRef(mOldSurfBitmap));

    if (FAILED(hr)) {
      gfxWarning() << "Failed to create shared bitmap for old surface.";
    }

    IntRect clipBounds;
    mClippedArea = mDT->GetClippedGeometry(&clipBounds);

    if (!clipBounds.IsEqualEdges(IntRect(IntPoint(0, 0), mDT->mSize))) {
      // We still need to take into account clipBounds if it contains additional
      // clipping information.
      RefPtr<ID2D1RectangleGeometry> rectGeom;
      factory()->CreateRectangleGeometry(D2D1::Rect(Float(clipBounds.x),
                                                    Float(clipBounds.y),
                                                    Float(clipBounds.XMost()),
                                                    Float(clipBounds.YMost())),
                                         byRef(rectGeom));

      mClippedArea = IntersectGeometry(mClippedArea, rectGeom);
    }
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

    rectGeom->CombineWithGeometry(mClippedArea, D2D1_COMBINE_MODE_EXCLUDE, nullptr, sink);
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

ID2D1Factory *D2DFactory()
{
  return DrawTargetD2D::factory();
}

DrawTargetD2D::DrawTargetD2D()
  : mCurrentCachedLayer(0)
  , mClipsArePushed(false)
  , mPrivateData(nullptr)
{
}

DrawTargetD2D::~DrawTargetD2D()
{
  if (mRT) {  
    PopAllClips();

    mRT->EndDraw();

    mVRAMUsageDT -= GetByteSize();
  }
  if (mTempRT) {
    mTempRT->EndDraw();

    mVRAMUsageDT -= GetByteSize();
  }

  if (mSnapshot) {
    // We may hold the only reference. MarkIndependent will clear mSnapshot;
    // keep the snapshot object alive so it doesn't get destroyed while
    // MarkIndependent is running.
    RefPtr<SourceSurfaceD2DTarget> deathGrip = mSnapshot;
    // mSnapshot can be treated as independent of this DrawTarget since we know
    // this DrawTarget won't change again.
    deathGrip->MarkIndependent();
    // mSnapshot will be cleared now.
  }

  for (int i = 0; i < kLayerCacheSize; i++) {
    if (mCachedLayers[i]) {
      mCachedLayers[i] = nullptr;
      mVRAMUsageDT -= GetByteSize();
    }
  }

  // Targets depending on us can break that dependency, since we're obviously not going to
  // be modified in the future.
  for (TargetSet::iterator iter = mDependentTargets.begin();
       iter != mDependentTargets.end(); iter++) {
    (*iter)->mDependingOnTargets.erase(this);
  }
  // Our dependencies on other targets no longer matter.
  for (TargetSet::iterator iter = mDependingOnTargets.begin();
       iter != mDependingOnTargets.end(); iter++) {
    (*iter)->mDependentTargets.erase(this);
  }
}

/*
 * DrawTarget Implementation
 */
TemporaryRef<SourceSurface>
DrawTargetD2D::Snapshot()
{
  if (!mSnapshot) {
    mSnapshot = new SourceSurfaceD2DTarget(this, mTexture, mFormat);
    Flush();
  }

  return mSnapshot;
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
  for (TargetSet::iterator iter = mDependingOnTargets.begin();
       iter != mDependingOnTargets.end(); iter++) {
    (*iter)->mDependentTargets.erase(this);
  }
  mDependingOnTargets.clear();
}

void
DrawTargetD2D::AddDependencyOnSource(SourceSurfaceD2DTarget* aSource)
{
  if (aSource->mDrawTarget && !mDependingOnTargets.count(aSource->mDrawTarget)) {
    aSource->mDrawTarget->mDependentTargets.insert(this);
    mDependingOnTargets.insert(aSource->mDrawTarget);
  }
}

TemporaryRef<ID2D1Bitmap>
DrawTargetD2D::GetBitmapForSurface(SourceSurface *aSurface,
                                   Rect &aSource)
{
  RefPtr<ID2D1Bitmap> bitmap;

  switch (aSurface->GetType()) {

  case SurfaceType::D2D1_BITMAP:
    {
      SourceSurfaceD2D *srcSurf = static_cast<SourceSurfaceD2D*>(aSurface);
      bitmap = srcSurf->GetBitmap();
    }
    break;
  case SurfaceType::D2D1_DRAWTARGET:
    {
      SourceSurfaceD2DTarget *srcSurf = static_cast<SourceSurfaceD2DTarget*>(aSurface);
      bitmap = srcSurf->GetBitmap(mRT);
      AddDependencyOnSource(srcSurf);
    }
    break;
  default:
    {
      RefPtr<DataSourceSurface> srcSurf = aSurface->GetDataSurface();

      if (!srcSurf) {
        gfxDebug() << "Not able to deal with non-data source surface.";
        return nullptr;
      }

      // We need to include any pixels that are overlapped by aSource
      Rect sourceRect(aSource);
      sourceRect.RoundOut();

      if (sourceRect.IsEmpty()) {
        gfxDebug() << "Bitmap source is empty. DrawBitmap will silently fail.";
        return nullptr;
      }

      if (sourceRect.width > mRT->GetMaximumBitmapSize() ||
          sourceRect.height > mRT->GetMaximumBitmapSize()) {
        gfxDebug() << "Bitmap source larger than texture size specified. DrawBitmap will silently fail.";
        // Don't know how to deal with this yet.
        return nullptr;
      }

      int stride = srcSurf->Stride();

      unsigned char *data = srcSurf->GetData() +
                            (uint32_t)sourceRect.y * stride +
                            (uint32_t)sourceRect.x * BytesPerPixel(srcSurf->GetFormat());

      D2D1_BITMAP_PROPERTIES props =
        D2D1::BitmapProperties(D2DPixelFormat(srcSurf->GetFormat()));
      mRT->CreateBitmap(D2D1::SizeU(UINT32(sourceRect.width), UINT32(sourceRect.height)), data, stride, props, byRef(bitmap));

      // subtract the integer part leaving the fractional part
      aSource.x -= (uint32_t)aSource.x;
      aSource.y -= (uint32_t)aSource.y;
    }
    break;
  }

  return bitmap;
}

#ifdef USE_D2D1_1
TemporaryRef<ID2D1Image>
DrawTargetD2D::GetImageForSurface(SourceSurface *aSurface)
{
  RefPtr<ID2D1Image> image;

  if (aSurface->GetType() == SurfaceType::D2D1_1_IMAGE) {
    image = static_cast<SourceSurfaceD2D1*>(aSurface)->GetImage();
    static_cast<SourceSurfaceD2D1*>(aSurface)->EnsureIndependent();
  } else {
    Rect r(Point(), Size(aSurface->GetSize()));
    image = GetBitmapForSurface(aSurface, r);
  }

  return image;
}
#endif

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

  rt->SetAntialiasMode(D2DAAMode(aOptions.mAntialiasMode));

  Rect srcRect = aSource;

  bitmap = GetBitmapForSurface(aSurface, srcRect);
  if (!bitmap) {
      return;
  }
 
  rt->DrawBitmap(bitmap, D2DRect(aDest), aOptions.mAlpha, D2DFilter(aSurfOptions.mFilter), D2DRect(srcRect));

  FinalizeRTForOperation(aOptions.mCompositionOp, ColorPattern(Color()), aDest);
}

void
DrawTargetD2D::DrawFilter(FilterNode *aNode,
                          const Rect &aSourceRect,
                          const Point &aDestPoint,
                          const DrawOptions &aOptions)
{
#ifdef USE_D2D1_1
  RefPtr<ID2D1DeviceContext> dc;
  HRESULT hr;
  
  hr = mRT->QueryInterface((ID2D1DeviceContext**)byRef(dc));

  if (SUCCEEDED(hr) && aNode->GetBackendType() == FILTER_BACKEND_DIRECT2D1_1) {
    ID2D1RenderTarget *rt = GetRTForOperation(aOptions.mCompositionOp, ColorPattern(Color()));
  
    PrepareForDrawing(rt);

    rt->SetAntialiasMode(D2DAAMode(aOptions.mAntialiasMode));
    hr = rt->QueryInterface((ID2D1DeviceContext**)byRef(dc));

    if (SUCCEEDED(hr)) {
      dc->DrawImage(static_cast<FilterNodeD2D1*>(aNode)->OutputEffect(), D2DPoint(aDestPoint), D2DRect(aSourceRect));

      Rect destRect = aSourceRect;
      destRect.MoveBy(aDestPoint);
      FinalizeRTForOperation(aOptions.mCompositionOp, ColorPattern(Color()), destRect);
      return;
    }
  }
#endif

  if (aNode->GetBackendType() != FILTER_BACKEND_SOFTWARE) {
    gfxWarning() << "Invalid filter backend passed to DrawTargetD2D!";
    return;
  }

  FilterNodeSoftware* filter = static_cast<FilterNodeSoftware*>(aNode);
  filter->Draw(this, aSourceRect, aDestPoint, aOptions);
}

void
DrawTargetD2D::MaskSurface(const Pattern &aSource,
                           SourceSurface *aMask,
                           Point aOffset,
                           const DrawOptions &aOptions)
{
  RefPtr<ID2D1Bitmap> bitmap;

  ID2D1RenderTarget *rt = GetRTForOperation(aOptions.mCompositionOp, ColorPattern(Color()));

  PrepareForDrawing(rt);

  // FillOpacityMask only works if the antialias mode is MODE_ALIASED
  rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

  IntSize size = aMask->GetSize();
  Rect maskRect = Rect(0.f, 0.f, size.width, size.height);
  bitmap = GetBitmapForSurface(aMask, maskRect);
  if (!bitmap) {
       return;
  }

  Rect dest = Rect(aOffset.x, aOffset.y, size.width, size.height);
  RefPtr<ID2D1Brush> brush = CreateBrushForPattern(aSource, aOptions.mAlpha);
  rt->FillOpacityMask(bitmap, brush, D2D1_OPACITY_MASK_CONTENT_GRAPHICS, D2DRect(dest), D2DRect(maskRect));

  FinalizeRTForOperation(aOptions.mCompositionOp, ColorPattern(Color()), dest);
}

void
DrawTargetD2D::DrawSurfaceWithShadow(SourceSurface *aSurface,
                                     const Point &aDest,
                                     const Color &aColor,
                                     const Point &aOffset,
                                     Float aSigma,
                                     CompositionOp aOperator)
{
  RefPtr<ID3D10ShaderResourceView> srView = nullptr;
  if (aSurface->GetType() != SurfaceType::D2D1_DRAWTARGET) {
    return;
  }

  SetScissorToRect(nullptr);

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
    HRESULT hr = mDevice->CreateRenderTargetView(mTempTexture, nullptr, byRef(mTempRTView));

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
  IntRect clipBounds;
  if (mPushedClips.size()) {
    EnsureClipMaskTexture(&clipBounds);

    mDevice->CreateShaderResourceView(mCurrentClipMaskTexture, nullptr, byRef(maskSRView));
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
    hr = mDevice->CreateTexture2D(&desc, nullptr, byRef(mipTexture));

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
    hr = mDevice->CreateTexture2D(&desc, nullptr, byRef(tmpDSTexture));

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

    mDevice->CreateShaderResourceView(mipTexture, nullptr,  byRef(srView));
    mDevice->GenerateMips(srView);

    RefPtr<ID3D10RenderTargetView> dsRTView;
    RefPtr<ID3D10ShaderResourceView> dsSRView;
    mDevice->CreateRenderTargetView(tmpDSTexture, nullptr,  byRef(dsRTView));
    mDevice->CreateShaderResourceView(tmpDSTexture, nullptr,  byRef(dsSRView));

    // We're not guaranteed the texture we created will be empty, we've
    // seen old content at least on NVidia drivers.
    float color[4] = { 0, 0, 0, 0 };
    mDevice->ClearRenderTargetView(dsRTView, color);

    rtViews = dsRTView;
    mDevice->OMSetRenderTargets(1, &rtViews, nullptr);

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

    mDevice->OMSetBlendState(GetBlendStateForOperator(CompositionOp::OP_OVER), nullptr, 0xffffffff);

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

    mDevice->CreateTexture2D(&desc, nullptr,  byRef(tmpTexture));
    mDevice->CreateRenderTargetView(tmpTexture, nullptr,  byRef(tmpRTView));
    mDevice->CreateShaderResourceView(tmpTexture, nullptr,  byRef(tmpSRView));

    tmpSurfSize = srcSurfSize;
  }

  rtViews = tmpRTView;
  mDevice->OMSetRenderTargets(1, &rtViews, nullptr);

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
  mDevice->OMSetRenderTargets(1, &rtViews, nullptr);

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
      SetFloatVector(ShaderConstantRectD3D10(shadowDest.x / mSize.width, shadowDest.y / mSize.height,
                                             Float(aSurface->GetSize().width) / mSize.width,
                                             Float(aSurface->GetSize().height) / mSize.height));
    mPrivateData->mEffect->GetTechniqueByName("SampleTextureWithShadow")->
      GetPassByIndex(2)->Apply(0);
    SetScissorToRect(&clipBounds);
  } else {
    mPrivateData->mEffect->GetTechniqueByName("SampleTextureWithShadow")->
      GetPassByIndex(1)->Apply(0);
  }

  mDevice->OMSetBlendState(GetBlendStateForOperator(aOperator), nullptr, 0xffffffff);

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
      SetFloatVector(ShaderConstantRectD3D10(aDest.x / mSize.width, aDest.y / mSize.height,
                                             Float(aSurface->GetSize().width) / mSize.width,
                                             Float(aSurface->GetSize().height) / mSize.height));
    mPrivateData->mEffect->GetTechniqueByName("SampleMaskedTexture")->
      GetPassByIndex(0)->Apply(0);
    // We've set the scissor rect here for the previous draw call.
  } else {
    mPrivateData->mEffect->GetTechniqueByName("SampleTexture")->
      GetPassByIndex(0)->Apply(0);
  }

  mDevice->OMSetBlendState(GetBlendStateForOperator(aOperator), nullptr, 0xffffffff);

  mDevice->Draw(4, 0);
}

void
DrawTargetD2D::ClearRect(const Rect &aRect)
{
  MarkChanged();
  PushClipRect(aRect);

  PopAllClips();

  AutoSaveRestoreClippedOut restoreClippedOut(this);

  D2D1_RECT_F clipRect;
  bool isPixelAligned;
  bool pushedClip = false;
  if (mTransform.IsRectilinear() &&
      GetDeviceSpaceClipRect(clipRect, isPixelAligned)) {
    if (mTransformDirty ||
        !mTransform.IsIdentity()) {
      mRT->SetTransform(D2D1::IdentityMatrix());
      mTransformDirty = true;
    }

    mRT->PushAxisAlignedClip(clipRect, isPixelAligned ? D2D1_ANTIALIAS_MODE_ALIASED : D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    pushedClip = true;
  } else {
    FlushTransformToRT();
    restoreClippedOut.Save();
  }

  mRT->Clear(D2D1::ColorF(0, 0.0f));

  if (pushedClip) {
    mRT->PopAxisAlignedClip();
  }

  PopClip();
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
  mTransformDirty = true;
  mRT->PushAxisAlignedClip(D2DRect(dstRect), D2D1_ANTIALIAS_MODE_ALIASED);
  mRT->Clear(D2D1::ColorF(0, 0.0f));
  mRT->PopAxisAlignedClip();

  RefPtr<ID2D1Bitmap> bitmap = GetBitmapForSurface(aSurface, srcRect);
  if (!bitmap) {
    return;
  }

  if (aSurface->GetFormat() == SurfaceFormat::A8) {
    RefPtr<ID2D1SolidColorBrush> brush;
    mRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),
                               D2D1::BrushProperties(), byRef(brush));
    mRT->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    mRT->FillOpacityMask(bitmap, brush, D2D1_OPACITY_MASK_CONTENT_GRAPHICS);
  } else {
    mRT->DrawBitmap(bitmap, D2DRect(dstRect), 1.0f,
            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
            D2DRect(srcRect));
  }
}

void
DrawTargetD2D::FillRect(const Rect &aRect,
                        const Pattern &aPattern,
                        const DrawOptions &aOptions)
{
  ID2D1RenderTarget *rt = GetRTForOperation(aOptions.mCompositionOp, aPattern);

  PrepareForDrawing(rt);

  rt->SetAntialiasMode(D2DAAMode(aOptions.mAntialiasMode));

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

  rt->SetAntialiasMode(D2DAAMode(aOptions.mAntialiasMode));

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

  rt->SetAntialiasMode(D2DAAMode(aOptions.mAntialiasMode));

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
  if (aPath->GetBackendType() != BackendType::DIRECT2D) {
    gfxDebug() << *this << ": Ignoring drawing call for incompatible path.";
    return;
  }

  const PathD2D *d2dPath = static_cast<const PathD2D*>(aPath);

  ID2D1RenderTarget *rt = GetRTForOperation(aOptions.mCompositionOp, aPattern);

  PrepareForDrawing(rt);

  rt->SetAntialiasMode(D2DAAMode(aOptions.mAntialiasMode));

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
  if (aPath->GetBackendType() != BackendType::DIRECT2D) {
    gfxDebug() << *this << ": Ignoring drawing call for incompatible path.";
    return;
  }

  const PathD2D *d2dPath = static_cast<const PathD2D*>(aPath);

  ID2D1RenderTarget *rt = GetRTForOperation(aOptions.mCompositionOp, aPattern);

  PrepareForDrawing(rt);

  rt->SetAntialiasMode(D2DAAMode(aOptions.mAntialiasMode));

  RefPtr<ID2D1Brush> brush = CreateBrushForPattern(aPattern, aOptions.mAlpha);

  if (brush) {
    rt->FillGeometry(d2dPath->mGeometry, brush);
  }

  Rect bounds;
  if (aOptions.mCompositionOp != CompositionOp::OP_OVER) {
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
                          const DrawOptions &aOptions,
                          const GlyphRenderingOptions* aRenderOptions)
{
  if (aFont->GetType() != FontType::DWRITE) {
    gfxDebug() << *this << ": Ignoring drawing call for incompatible font.";
    return;
  }

  ScaledFontDWrite *font = static_cast<ScaledFontDWrite*>(aFont);

  IDWriteRenderingParams *params = nullptr;
  if (aRenderOptions) {
    if (aRenderOptions->GetType() != FontType::DWRITE) {
      gfxDebug() << *this << ": Ignoring incompatible GlyphRenderingOptions.";
      // This should never happen.
      MOZ_ASSERT(false);
    } else {
      params = static_cast<const GlyphRenderingOptionsDWrite*>(aRenderOptions)->mParams;
    }
  }

  AntialiasMode aaMode = font->GetDefaultAAMode();

  if (aOptions.mAntialiasMode != AntialiasMode::DEFAULT) {
    aaMode = aOptions.mAntialiasMode;
  }

  if (mFormat == SurfaceFormat::B8G8R8A8 && mPermitSubpixelAA &&
      aOptions.mCompositionOp == CompositionOp::OP_OVER && aPattern.GetType() == PatternType::COLOR &&
      aaMode == AntialiasMode::SUBPIXEL) {
    if (FillGlyphsManual(font, aBuffer,
                         static_cast<const ColorPattern*>(&aPattern)->mColor,
                         params, aOptions)) {
      return;
    }
  }

  ID2D1RenderTarget *rt = GetRTForOperation(aOptions.mCompositionOp, aPattern);

  PrepareForDrawing(rt);

  D2D1_TEXT_ANTIALIAS_MODE d2dAAMode = D2D1_TEXT_ANTIALIAS_MODE_DEFAULT;

  switch (aaMode) {
  case AntialiasMode::NONE:
    d2dAAMode = D2D1_TEXT_ANTIALIAS_MODE_ALIASED;
    break;
  case AntialiasMode::GRAY:
    d2dAAMode = D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE;
    break;
  case AntialiasMode::SUBPIXEL:
    d2dAAMode = D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE;
    break;
  default:
    d2dAAMode = D2D1_TEXT_ANTIALIAS_MODE_DEFAULT;
  }

  if (d2dAAMode == D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE &&
      mFormat != SurfaceFormat::B8G8R8X8) {
    d2dAAMode = D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE;
  }

  rt->SetTextAntialiasMode(d2dAAMode);

  if (rt != mRT || params != mTextRenderingParams) {
    rt->SetTextRenderingParams(params);
    if (rt == mRT) {
      mTextRenderingParams = params;
    }
  }

  RefPtr<ID2D1Brush> brush = CreateBrushForPattern(aPattern, aOptions.mAlpha);

  AutoDWriteGlyphRun autoRun;
  DWriteGlyphRunFromGlyphs(aBuffer, font, &autoRun);

  if (brush) {
    rt->DrawGlyphRun(D2D1::Point2F(), &autoRun, brush);
  }

  FinalizeRTForOperation(aOptions.mCompositionOp, aPattern, Rect(0, 0, (Float)mSize.width, (Float)mSize.height));
}

void
DrawTargetD2D::Mask(const Pattern &aSource,
                    const Pattern &aMask,
                    const DrawOptions &aOptions)
{
  ID2D1RenderTarget *rt = GetRTForOperation(aOptions.mCompositionOp, aSource);
  
  PrepareForDrawing(rt);

  RefPtr<ID2D1Brush> brush = CreateBrushForPattern(aSource, aOptions.mAlpha);
  RefPtr<ID2D1Brush> maskBrush = CreateBrushForPattern(aMask, 1.0f);

  RefPtr<ID2D1Layer> layer;

  layer = GetCachedLayer();

  rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), nullptr,
                                      D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                      D2D1::IdentityMatrix(),
                                      1.0f, maskBrush),
                layer);

  Rect rect(0, 0, (Float)mSize.width, (Float)mSize.height);
  Matrix mat = mTransform;
  mat.Invert();
  
  rt->FillRectangle(D2DRect(mat.TransformBounds(rect)), brush);
  PopCachedLayer(rt);

  FinalizeRTForOperation(aOptions.mCompositionOp, aSource, Rect(0, 0, (Float)mSize.width, (Float)mSize.height));
}

void
DrawTargetD2D::PushClip(const Path *aPath)
{
  if (aPath->GetBackendType() != BackendType::DIRECT2D) {
    gfxDebug() << *this << ": Ignoring clipping call for incompatible path.";
    return;
  }

  mCurrentClipMaskTexture = nullptr;
  mCurrentClippedGeometry = nullptr;

  RefPtr<PathD2D> pathD2D = static_cast<PathD2D*>(const_cast<Path*>(aPath));

  PushedClip clip;
  clip.mTransform = D2DMatrix(mTransform);
  clip.mPath = pathD2D;
  
  pathD2D->mGeometry->GetBounds(clip.mTransform, &clip.mBounds);
  
  clip.mLayer = GetCachedLayer();

  mPushedClips.push_back(clip);

  // The transform of clips is relative to the world matrix, since we use the total
  // transform for the clips, make the world matrix identity.
  mRT->SetTransform(D2D1::IdentityMatrix());
  mTransformDirty = true;

  if (mClipsArePushed) {
    PushD2DLayer(mRT, pathD2D->mGeometry, clip.mLayer, clip.mTransform);
  }
}

void
DrawTargetD2D::PushClipRect(const Rect &aRect)
{
  mCurrentClipMaskTexture = nullptr;
  mCurrentClippedGeometry = nullptr;
  if (!mTransform.IsRectilinear()) {
    // Whoops, this isn't a rectangle in device space, Direct2D will not deal
    // with this transform the way we want it to.
    // See remarks: http://msdn.microsoft.com/en-us/library/dd316860%28VS.85%29.aspx

    RefPtr<PathBuilder> pathBuilder = CreatePathBuilder();
    pathBuilder->MoveTo(aRect.TopLeft());
    pathBuilder->LineTo(aRect.TopRight());
    pathBuilder->LineTo(aRect.BottomRight());
    pathBuilder->LineTo(aRect.BottomLeft());
    pathBuilder->Close();
    RefPtr<Path> path = pathBuilder->Finish();
    return PushClip(path);
  }

  PushedClip clip;
  Rect rect = mTransform.TransformBounds(aRect);
  IntRect intRect;
  clip.mIsPixelAligned = rect.ToIntRect(&intRect);

  // Do not store the transform, just store the device space rectangle directly.
  clip.mBounds = D2DRect(rect);

  mPushedClips.push_back(clip);

  mRT->SetTransform(D2D1::IdentityMatrix());
  mTransformDirty = true;

  if (mClipsArePushed) {
    mRT->PushAxisAlignedClip(clip.mBounds, clip.mIsPixelAligned ? D2D1_ANTIALIAS_MODE_ALIASED : D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
  }
}

void
DrawTargetD2D::PopClip()
{
  mCurrentClipMaskTexture = nullptr;
  mCurrentClippedGeometry = nullptr;
  if (mClipsArePushed) {
    if (mPushedClips.back().mLayer) {
      PopCachedLayer(mRT);
    } else {
      mRT->PopAxisAlignedClip();
    }
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
    return nullptr;
  }

  return newSurf;
}

TemporaryRef<SourceSurface> 
DrawTargetD2D::OptimizeSourceSurface(SourceSurface *aSurface) const
{
  // Unsupported!
  return nullptr;
}

TemporaryRef<SourceSurface>
DrawTargetD2D::CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const
{
  if (aSurface.mType != NativeSurfaceType::D3D10_TEXTURE) {
    gfxDebug() << *this << ": Failure to create source surface from non-D3D10 texture native surface.";
    return nullptr;
  }
  RefPtr<SourceSurfaceD2D> newSurf = new SourceSurfaceD2D();

  if (!newSurf->InitFromTexture(static_cast<ID3D10Texture2D*>(aSurface.mSurface),
                                aSurface.mFormat,
                                mRT))
  {
    gfxWarning() << *this << ": Failed to create SourceSurface from texture.";
    return nullptr;
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
    return nullptr;
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
    return nullptr;
  }

  RefPtr<ID2D1GeometrySink> sink;
  hr = path->Open(byRef(sink));
  if (FAILED(hr)) {
    gfxWarning() << "Failed to access Direct2D Path Geometry. Code: " << hr;
    return nullptr;
  }

  if (aFillRule == FillRule::FILL_WINDING) {
    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
  }

  return new PathBuilderD2D(sink, path, aFillRule);
}

TemporaryRef<GradientStops>
DrawTargetD2D::CreateGradientStops(GradientStop *rawStops, uint32_t aNumStops, ExtendMode aExtendMode) const
{
  D2D1_GRADIENT_STOP *stops = new D2D1_GRADIENT_STOP[aNumStops];

  for (uint32_t i = 0; i < aNumStops; i++) {
    stops[i].position = rawStops[i].offset;
    stops[i].color = D2DColor(rawStops[i].color);
  }

  RefPtr<ID2D1GradientStopCollection> stopCollection;

  HRESULT hr =
    mRT->CreateGradientStopCollection(stops, aNumStops,
                                      D2D1_GAMMA_2_2, D2DExtend(aExtendMode),
                                      byRef(stopCollection));
  delete [] stops;

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create GradientStopCollection. Code: " << hr;
    return nullptr;
  }

  return new GradientStopsD2D(stopCollection);
}

TemporaryRef<FilterNode>
DrawTargetD2D::CreateFilter(FilterType aType)
{
#ifdef USE_D2D1_1
  RefPtr<ID2D1DeviceContext> dc;
  HRESULT hr = mRT->QueryInterface((ID2D1DeviceContext**)byRef(dc));

  if (SUCCEEDED(hr)) {
    return FilterNodeD2D1::Create(this, dc, aType);
  }
#endif
  return FilterNodeSoftware::Create(aType);
}

void*
DrawTargetD2D::GetNativeSurface(NativeSurfaceType aType)
{
  if (aType != NativeSurfaceType::D3D10_TEXTURE) {
    return nullptr;
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

  CD3D10_TEXTURE2D_DESC desc(DXGIFormat(aFormat),
                             mSize.width,
                             mSize.height,
                             1, 1);
  desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;

  hr = mDevice->CreateTexture2D(&desc, nullptr, byRef(mTexture));

  if (FAILED(hr)) {
    gfxDebug() << "Failed to init Direct2D DrawTarget. Size: " << mSize << " Code: " << hr;
    return false;
  }

  if (!InitD2DRenderTarget()) {
    return false;
  }

  mRT->Clear(D2D1::ColorF(0, 0));
  return true;
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

  decltype(D3D10CreateEffectFromMemory)* createD3DEffect;
  HMODULE d3dModule = LoadLibraryW(L"d3d10_1.dll");
  createD3DEffect = (decltype(D3D10CreateEffectFromMemory)*)
      GetProcAddress(d3dModule, "D3D10CreateEffectFromMemory");

  hr = createD3DEffect((void*)d2deffect, sizeof(d2deffect), 0, mDevice, nullptr, byRef(mPrivateData->mEffect));

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
uint32_t
DrawTargetD2D::GetByteSize() const
{
  return mSize.width * mSize.height * BytesPerPixel(mFormat);
}

TemporaryRef<ID2D1Layer>
DrawTargetD2D::GetCachedLayer()
{
  RefPtr<ID2D1Layer> layer;

  if (mCurrentCachedLayer < 5) {
    if (!mCachedLayers[mCurrentCachedLayer]) {
      mRT->CreateLayer(byRef(mCachedLayers[mCurrentCachedLayer]));
      mVRAMUsageDT += GetByteSize();
    }
    layer = mCachedLayers[mCurrentCachedLayer];
  } else {
    mRT->CreateLayer(byRef(layer));
  }

  mCurrentCachedLayer++;
  return layer;
}

void
DrawTargetD2D::PopCachedLayer(ID2D1RenderTarget *aRT)
{
  aRT->PopLayer();
  mCurrentCachedLayer--;
}

bool
DrawTargetD2D::InitD2DRenderTarget()
{
  if (!factory()) {
    return false;
  }

  mRT = CreateRTForTexture(mTexture, mFormat);

  if (!mRT) {
    return false;
  }

  mRT->BeginDraw();

  if (mFormat == SurfaceFormat::B8G8R8X8) {
    mRT->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
  }

  mVRAMUsageDT += GetByteSize();

  return InitD3D10Data();
}

void
DrawTargetD2D::PrepareForDrawing(ID2D1RenderTarget *aRT)
{
  if (!mClipsArePushed || aRT == mTempRT) {
    if (mPushedClips.size()) {
      // The transform of clips is relative to the world matrix, since we use the total
      // transform for the clips, make the world matrix identity.
      aRT->SetTransform(D2D1::IdentityMatrix());
      if (aRT == mRT) {
        mTransformDirty = true;
        mClipsArePushed = true;
      }
      PushClipsToRT(aRT);
    }
  }
  FlushTransformToRT();
  MarkChanged();

  if (aRT == mTempRT) {
    mTempRT->SetTransform(D2DMatrix(mTransform));
  }
}

void
DrawTargetD2D::MarkChanged()
{
  if (mSnapshot) {
    if (mSnapshot->hasOneRef()) {
      // Just destroy it, since no-one else knows about it.
      mSnapshot = nullptr;
    } else {
      mSnapshot->DrawTargetWillChange();
      // The snapshot will no longer depend on this target.
      MOZ_ASSERT(!mSnapshot);
    }
  }
  if (mDependentTargets.size()) {
    // Copy mDependentTargets since the Flush()es below will modify it.
    TargetSet tmpTargets = mDependentTargets;
    for (TargetSet::iterator iter = tmpTargets.begin();
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
  size_t operatorIndex = static_cast<size_t>(aOperator);
  if (mPrivateData->mBlendStates[operatorIndex]) {
    return mPrivateData->mBlendStates[operatorIndex];
  }

  D3D10_BLEND_DESC desc;

  memset(&desc, 0, sizeof(D3D10_BLEND_DESC));

  desc.AlphaToCoverageEnable = FALSE;
  desc.BlendEnable[0] = TRUE;
  desc.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_ALL;
  desc.BlendOp = desc.BlendOpAlpha = D3D10_BLEND_OP_ADD;

  switch (aOperator) {
  case CompositionOp::OP_ADD:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_ONE;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_ONE;
    break;
  case CompositionOp::OP_IN:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_DEST_ALPHA;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_ZERO;
    break;
  case CompositionOp::OP_OUT:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_INV_DEST_ALPHA;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_ZERO;
    break;
  case CompositionOp::OP_ATOP:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_DEST_ALPHA;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
    break;
  case CompositionOp::OP_DEST_IN:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_ZERO;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_SRC_ALPHA;
    break;
  case CompositionOp::OP_DEST_OUT:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_ZERO;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
    break;
  case CompositionOp::OP_DEST_ATOP:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_INV_DEST_ALPHA;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_SRC_ALPHA;
    break;
  case CompositionOp::OP_DEST_OVER:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_INV_DEST_ALPHA;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_ONE;
    break;
  case CompositionOp::OP_XOR:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_INV_DEST_ALPHA;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
    break;
  case CompositionOp::OP_SOURCE:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_ONE;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_ZERO;
    break;
  default:
    desc.SrcBlend = desc.SrcBlendAlpha = D3D10_BLEND_ONE;
    desc.DestBlend = desc.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
  }
  
  mDevice->CreateBlendState(&desc, byRef(mPrivateData->mBlendStates[operatorIndex]));

  return mPrivateData->mBlendStates[operatorIndex];
}

/* This function prepares the temporary RT for drawing and returns it when a
 * drawing operation other than OVER is required.
 */
ID2D1RenderTarget*
DrawTargetD2D::GetRTForOperation(CompositionOp aOperator, const Pattern &aPattern)
{
  if (aOperator == CompositionOp::OP_OVER && IsPatternSupportedByD2D(aPattern)) {
    return mRT;
  }

  PopAllClips();

  if (aOperator > CompositionOp::OP_XOR) {
    mRT->Flush();
  }

  if (mTempRT) {
    mTempRT->Clear(D2D1::ColorF(0, 0));
    return mTempRT;
  }

  EnsureViews();

  if (!mRTView || !mSRView) {
    gfxDebug() << *this << ": Failed to get required views. Defaulting to CompositionOp::OP_OVER.";
    return mRT;
  }

  mTempRT = CreateRTForTexture(mTempTexture, SurfaceFormat::B8G8R8A8);

  if (!mTempRT) {
    return mRT;
  }

  mVRAMUsageDT += GetByteSize();

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
  if (aOperator == CompositionOp::OP_OVER && IsPatternSupportedByD2D(aPattern)) {
    return;
  }

  if (!mTempRT) {
    return;
  }

  PopClipsFromRT(mTempRT);

  mRT->Flush();
  mTempRT->Flush();

  AutoSaveRestoreClippedOut restoreClippedOut(this);

  bool needsWriteBack =
    !IsOperatorBoundByMask(aOperator) && mPushedClips.size();

  if (needsWriteBack) {
    restoreClippedOut.Save();
  }

  ID3D10RenderTargetView *rtViews = mRTView;
  mDevice->OMSetRenderTargets(1, &rtViews, nullptr);

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

  RefPtr<ID3D10Texture2D> tmpTexture;
  RefPtr<ID3D10ShaderResourceView> mBckSRView;

  mDevice->RSSetViewports(1, &viewport);
  mPrivateData->mEffect->GetVariableByName("QuadDesc")->AsVector()->
    SetFloatVector(ShaderConstantRectD3D10(-1.0f, 1.0f, 2.0f, -2.0f));

  if (IsPatternSupportedByD2D(aPattern)) {
    mPrivateData->mEffect->GetVariableByName("TexCoords")->AsVector()->
      SetFloatVector(ShaderConstantRectD3D10(0, 0, 1.0f, 1.0f));
    mPrivateData->mEffect->GetVariableByName("tex")->AsShaderResource()->SetResource(mSRView);

    // Handle the case where we blend with the backdrop
    if (aOperator > CompositionOp::OP_XOR) {
      IntSize size = mSize;
      SurfaceFormat format = mFormat;

      CD3D10_TEXTURE2D_DESC desc(DXGIFormat(format), size.width, size.height, 1, 1);
      desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;

      HRESULT hr = mDevice->CreateTexture2D(&desc, nullptr, byRef(tmpTexture));
      if (FAILED(hr)) {
        gfxWarning() << "Failed to create temporary texture to hold surface data.";
        return;
      }

      mDevice->CopyResource(tmpTexture, mTexture);
      if (FAILED(hr)) {
        gfxWarning() << *this << "Failed to create shader resource view for temp texture. Code: " << hr;
        return;
      }

      DrawTargetD2D::Flush();

      hr = mDevice->CreateShaderResourceView(tmpTexture, nullptr, byRef(mBckSRView));

      if (FAILED(hr)) {
        gfxWarning() << *this << "Failed to create shader resource view for temp texture. Code: " << hr;
        return;
      }

      unsigned int compop = (unsigned int)aOperator - (unsigned int)CompositionOp::OP_XOR;
      mPrivateData->mEffect->GetVariableByName("bcktex")->AsShaderResource()->SetResource(mBckSRView);
      mPrivateData->mEffect->GetVariableByName("blendop")->AsScalar()->SetInt(compop);

      if (aOperator > CompositionOp::OP_EXCLUSION)
        mPrivateData->mEffect->GetTechniqueByName("SampleTextureForNonSeparableBlending")->
          GetPassByIndex(0)->Apply(0);
      else if (aOperator > CompositionOp::OP_COLOR_DODGE)
        mPrivateData->mEffect->GetTechniqueByName("SampleTextureForSeparableBlending_2")->
          GetPassByIndex(0)->Apply(0);
      else
        mPrivateData->mEffect->GetTechniqueByName("SampleTextureForSeparableBlending_1")->
          GetPassByIndex(0)->Apply(0);
    }
    else {
      mPrivateData->mEffect->GetTechniqueByName("SampleTexture")->GetPassByIndex(0)->Apply(0);
    }

  } else if (aPattern.GetType() == PatternType::RADIAL_GRADIENT) {
    const RadialGradientPattern *pat = static_cast<const RadialGradientPattern*>(&aPattern);

    if (pat->mCenter1 == pat->mCenter2 && pat->mRadius1 == pat->mRadius2) {
      // Draw nothing!
      return;
    }

    mPrivateData->mEffect->GetVariableByName("mask")->AsShaderResource()->SetResource(mSRView);

    SetupEffectForRadialGradient(pat);
  }

  mDevice->OMSetBlendState(GetBlendStateForOperator(aOperator), nullptr, 0xffffffff);
  
  SetScissorToRect(nullptr);
  mDevice->Draw(4, 0);
}

static D2D1_RECT_F
IntersectRect(const D2D1_RECT_F& aRect1, const D2D1_RECT_F& aRect2)
{
  D2D1_RECT_F result;
  result.left = max(aRect1.left, aRect2.left);
  result.top = max(aRect1.top, aRect2.top);
  result.right = min(aRect1.right, aRect2.right);
  result.bottom = min(aRect1.bottom, aRect2.bottom);

  result.right = max(result.right, result.left);
  result.bottom = max(result.bottom, result.top);

  return result;
}

bool
DrawTargetD2D::GetDeviceSpaceClipRect(D2D1_RECT_F& aClipRect, bool& aIsPixelAligned)
{
  if (!mPushedClips.size()) {
    return false;
  }

  std::vector<DrawTargetD2D::PushedClip>::iterator iter = mPushedClips.begin();
  if (iter->mPath) {
    return false;
  }
  aClipRect = iter->mBounds;
  aIsPixelAligned = iter->mIsPixelAligned;

  iter++;
  for (;iter != mPushedClips.end(); iter++) {
    if (iter->mPath) {
      return false;
    }
    aClipRect = IntersectRect(aClipRect, iter->mBounds);
    if (!iter->mIsPixelAligned) {
      aIsPixelAligned = false;
    }
  }
  return true;
}

TemporaryRef<ID2D1Geometry>
DrawTargetD2D::GetClippedGeometry(IntRect *aClipBounds)
{
  if (mCurrentClippedGeometry) {
    *aClipBounds = mCurrentClipBounds;
    return mCurrentClippedGeometry;
  }

  mCurrentClipBounds = IntRect(IntPoint(0, 0), mSize);

  // if pathGeom is null then pathRect represents the path.
  RefPtr<ID2D1Geometry> pathGeom;
  D2D1_RECT_F pathRect;
  bool pathRectIsAxisAligned = false;
  std::vector<DrawTargetD2D::PushedClip>::iterator iter = mPushedClips.begin();
  
  if (iter->mPath) {
    pathGeom = GetTransformedGeometry(iter->mPath->GetGeometry(), iter->mTransform);
  } else {
    pathRect = iter->mBounds;
    pathRectIsAxisAligned = iter->mIsPixelAligned;
  }

  iter++;
  for (;iter != mPushedClips.end(); iter++) {
    // Do nothing but add it to the current clip bounds.
    if (!iter->mPath && iter->mIsPixelAligned) {
      mCurrentClipBounds.IntersectRect(mCurrentClipBounds,
        IntRect(int32_t(iter->mBounds.left), int32_t(iter->mBounds.top),
                int32_t(iter->mBounds.right - iter->mBounds.left),
                int32_t(iter->mBounds.bottom - iter->mBounds.top)));
      continue;
    }

    if (!pathGeom) {
      if (pathRectIsAxisAligned) {
        mCurrentClipBounds.IntersectRect(mCurrentClipBounds,
          IntRect(int32_t(pathRect.left), int32_t(pathRect.top),
                  int32_t(pathRect.right - pathRect.left),
                  int32_t(pathRect.bottom - pathRect.top)));
      }
      if (iter->mPath) {
        // See if pathRect needs to go into the path geometry.
        if (!pathRectIsAxisAligned) {
          pathGeom = ConvertRectToGeometry(pathRect);
        } else {
          pathGeom = GetTransformedGeometry(iter->mPath->GetGeometry(), iter->mTransform);
        }
      } else {
        pathRect = IntersectRect(pathRect, iter->mBounds);
        pathRectIsAxisAligned = false;
        continue;
      }
    }

    RefPtr<ID2D1PathGeometry> newGeom;
    factory()->CreatePathGeometry(byRef(newGeom));

    RefPtr<ID2D1GeometrySink> currentSink;
    newGeom->Open(byRef(currentSink));

    if (iter->mPath) {
      pathGeom->CombineWithGeometry(iter->mPath->GetGeometry(), D2D1_COMBINE_MODE_INTERSECT,
                                    iter->mTransform, currentSink);
    } else {
      RefPtr<ID2D1Geometry> rectGeom = ConvertRectToGeometry(iter->mBounds);
      pathGeom->CombineWithGeometry(rectGeom, D2D1_COMBINE_MODE_INTERSECT,
                                    D2D1::IdentityMatrix(), currentSink);
    }

    currentSink->Close();

    pathGeom = newGeom.forget();
  }

  // For now we need mCurrentClippedGeometry to always be non-nullptr. This
  // method might seem a little strange but it is just fine, if pathGeom is
  // nullptr pathRect will always still contain 1 clip unaccounted for
  // regardless of mCurrentClipBounds.
  if (!pathGeom) {
    pathGeom = ConvertRectToGeometry(pathRect);
  }
  mCurrentClippedGeometry = pathGeom.forget();
  *aClipBounds = mCurrentClipBounds;
  return mCurrentClippedGeometry;
}

TemporaryRef<ID2D1RenderTarget>
DrawTargetD2D::CreateRTForTexture(ID3D10Texture2D *aTexture, SurfaceFormat aFormat)
{
  HRESULT hr;

  RefPtr<IDXGISurface> surface;
  RefPtr<ID2D1RenderTarget> rt;

  hr = aTexture->QueryInterface((IDXGISurface**)byRef(surface));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to QI texture to surface.";
    return nullptr;
  }

  D3D10_TEXTURE2D_DESC desc;
  aTexture->GetDesc(&desc);

  D2D1_ALPHA_MODE alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;

  if (aFormat == SurfaceFormat::B8G8R8X8 && aTexture == mTexture) {
    alphaMode = D2D1_ALPHA_MODE_IGNORE;
  }

  D2D1_RENDER_TARGET_PROPERTIES props =
    D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(desc.Format, alphaMode));
  hr = factory()->CreateDxgiSurfaceRenderTarget(surface, props, byRef(rt));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create D2D render target for texture.";
    return nullptr;
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

  hr = mDevice->CreateTexture2D(&desc, nullptr, byRef(mTempTexture));

  if (FAILED(hr)) {
    gfxWarning() << *this << "Failed to create temporary texture for rendertarget. Size: "
      << mSize << " Code: " << hr;
    return;
  }

  hr = mDevice->CreateShaderResourceView(mTempTexture, nullptr, byRef(mSRView));

  if (FAILED(hr)) {
    gfxWarning() << *this << "Failed to create shader resource view for temp texture. Code: " << hr;
    return;
  }

  hr = mDevice->CreateRenderTargetView(mTexture, nullptr, byRef(mRTView));

  if (FAILED(hr)) {
    gfxWarning() << *this << "Failed to create rendertarget view for temp texture. Code: " << hr;
  }
}

void
DrawTargetD2D::PopAllClips()
{
  if (mClipsArePushed) {
    PopClipsFromRT(mRT);
  
    mClipsArePushed = false;
  }
}

void
DrawTargetD2D::PushClipsToRT(ID2D1RenderTarget *aRT)
{
  for (std::vector<PushedClip>::iterator iter = mPushedClips.begin();
        iter != mPushedClips.end(); iter++) {
    if (iter->mLayer) {
      PushD2DLayer(aRT, iter->mPath->mGeometry, iter->mLayer, iter->mTransform);
    } else {
      aRT->PushAxisAlignedClip(iter->mBounds, iter->mIsPixelAligned ? D2D1_ANTIALIAS_MODE_ALIASED : D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }
  }
}

void
DrawTargetD2D::PopClipsFromRT(ID2D1RenderTarget *aRT)
{
  for (int i = mPushedClips.size() - 1; i >= 0; i--) {
    if (mPushedClips[i].mLayer) {
      aRT->PopLayer();
    } else {
      aRT->PopAxisAlignedClip();
    }
  }
}

void
DrawTargetD2D::EnsureClipMaskTexture(IntRect *aBounds)
{
  if (mCurrentClipMaskTexture || mPushedClips.empty()) {
    *aBounds = mCurrentClipBounds;
    return;
  }
  
  RefPtr<ID2D1Geometry> geometry = GetClippedGeometry(aBounds);

  CD3D10_TEXTURE2D_DESC desc(DXGI_FORMAT_A8_UNORM,
                             mSize.width,
                             mSize.height,
                             1, 1);
  desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;

  HRESULT hr = mDevice->CreateTexture2D(&desc, nullptr, byRef(mCurrentClipMaskTexture));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create texture for ClipMask!";
    return;
  }

  RefPtr<ID2D1RenderTarget> rt = CreateRTForTexture(mCurrentClipMaskTexture, SurfaceFormat::A8);

  if (!rt) {
    gfxWarning() << "Failed to create RT for ClipMask!";
    return;
  }
  
  RefPtr<ID2D1SolidColorBrush> brush;
  rt->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), byRef(brush));
    
  rt->BeginDraw();
  rt->Clear(D2D1::ColorF(0, 0));
  rt->FillGeometry(geometry, brush);
  rt->EndDraw();
}

bool
DrawTargetD2D::FillGlyphsManual(ScaledFontDWrite *aFont,
                                const GlyphBuffer &aBuffer,
                                const Color &aColor,
                                IDWriteRenderingParams *aParams,
                                const DrawOptions &aOptions)
{
  HRESULT hr;

  RefPtr<IDWriteRenderingParams> params;

  if (aParams) {
    params = aParams;
  } else {
    mRT->GetTextRenderingParams(byRef(params));
  }

  DWRITE_RENDERING_MODE renderMode = DWRITE_RENDERING_MODE_DEFAULT;
  if (params) {
    hr = aFont->mFontFace->GetRecommendedRenderingMode(
      (FLOAT)aFont->GetSize(),
      1.0f,
      DWRITE_MEASURING_MODE_NATURAL,
      params,
      &renderMode);
    if (FAILED(hr)) {
      // this probably never happens, but let's play it safe
      renderMode = DWRITE_RENDERING_MODE_DEFAULT;
    }
  }

  // Deal with rendering modes CreateGlyphRunAnalysis doesn't accept.
  switch (renderMode) {
  case DWRITE_RENDERING_MODE_ALIASED:
    // ClearType texture creation will fail in this mode, so bail out
    return false;
  case DWRITE_RENDERING_MODE_DEFAULT:
    // As per DWRITE_RENDERING_MODE documentation, pick Natural for font
    // sizes under 16 ppem
    if (aFont->GetSize() < 16.0f) {
      renderMode = DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL;
    } else {
      renderMode = DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL_SYMMETRIC;
    }
    break;
  case DWRITE_RENDERING_MODE_OUTLINE:
    renderMode = DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL_SYMMETRIC;
    break;
  default:
    break;
  }

  DWRITE_MEASURING_MODE measureMode =
    renderMode <= DWRITE_RENDERING_MODE_CLEARTYPE_GDI_CLASSIC ? DWRITE_MEASURING_MODE_GDI_CLASSIC :
    renderMode == DWRITE_RENDERING_MODE_CLEARTYPE_GDI_NATURAL ? DWRITE_MEASURING_MODE_GDI_NATURAL :
    DWRITE_MEASURING_MODE_NATURAL;

  DWRITE_MATRIX mat = DWriteMatrixFromMatrix(mTransform);

  AutoDWriteGlyphRun autoRun;
  DWriteGlyphRunFromGlyphs(aBuffer, aFont, &autoRun);

  RefPtr<IDWriteGlyphRunAnalysis> analysis;
  hr = GetDWriteFactory()->CreateGlyphRunAnalysis(&autoRun, 1.0f, &mat,
                                                  renderMode, measureMode, 0, 0, byRef(analysis));

  if (FAILED(hr)) {
    return false;
  }

  RECT bounds;
  hr = analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_CLEARTYPE_3x1, &bounds);

  if (bounds.bottom <= bounds.top || bounds.right <= bounds.left) {
    // DWrite seems to do this sometimes. I'm not 100% sure why. See bug 758980.
    gfxDebug() << "Empty alpha texture bounds! Falling back to regular drawing.";
    return false;
  }
  IntRect rectBounds(bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top);
  IntRect surfBounds(IntPoint(0, 0), mSize);

  rectBounds.IntersectRect(rectBounds, surfBounds);

  if (rectBounds.IsEmpty()) {
    // Nothing to do.
    return true;
  }

  RefPtr<ID3D10Texture2D> tex = CreateTextureForAnalysis(analysis, rectBounds);

  if (!tex) {
    return false;
  }

  RefPtr<ID3D10ShaderResourceView> srView;
  hr = mDevice->CreateShaderResourceView(tex, nullptr, byRef(srView));

  if (FAILED(hr)) {
    return false;
  }

  MarkChanged();

  // Prepare our background texture for drawing.
  PopAllClips();
  mRT->Flush();

  SetupStateForRendering();

  ID3D10EffectTechnique *technique = mPrivateData->mEffect->GetTechniqueByName("SampleTextTexture");

  mPrivateData->mEffect->GetVariableByName("QuadDesc")->AsVector()->
    SetFloatVector(ShaderConstantRectD3D10(-1.0f + ((Float(rectBounds.x) / mSize.width) * 2.0f),
                                           1.0f - (Float(rectBounds.y) / mSize.height * 2.0f),
                                           (Float(rectBounds.width) / mSize.width) * 2.0f,
                                           (-Float(rectBounds.height) / mSize.height) * 2.0f));
  mPrivateData->mEffect->GetVariableByName("TexCoords")->AsVector()->
    SetFloatVector(ShaderConstantRectD3D10(0, 0, 1.0f, 1.0f));
  FLOAT color[4] = { aColor.r, aColor.g, aColor.b, aColor.a };
  mPrivateData->mEffect->GetVariableByName("TextColor")->AsVector()->
    SetFloatVector(color);
  
  mPrivateData->mEffect->GetVariableByName("tex")->AsShaderResource()->SetResource(srView);

  bool isMasking = false;

  IntRect clipBoundsStorage;
  IntRect *clipBounds = nullptr;

  if (!mPushedClips.empty()) {
    clipBounds = &clipBoundsStorage;
    RefPtr<ID2D1Geometry> geom = GetClippedGeometry(clipBounds);

    RefPtr<ID2D1RectangleGeometry> rectGeom;
    factory()->CreateRectangleGeometry(D2D1::RectF(Float(rectBounds.x),
                                                   Float(rectBounds.y),
                                                   Float(rectBounds.width + rectBounds.x),
                                                   Float(rectBounds.height + rectBounds.y)),
                                       byRef(rectGeom));

    D2D1_GEOMETRY_RELATION relation;
    if (FAILED(geom->CompareWithGeometry(rectGeom, D2D1::IdentityMatrix(), &relation)) ||
        relation != D2D1_GEOMETRY_RELATION_CONTAINS ) {
      isMasking = true;
    }        
  }
  
  if (isMasking) {
    clipBounds = &clipBoundsStorage;
    EnsureClipMaskTexture(clipBounds);

    RefPtr<ID3D10ShaderResourceView> srViewMask;
    hr = mDevice->CreateShaderResourceView(mCurrentClipMaskTexture, nullptr, byRef(srViewMask));

    if (FAILED(hr)) {
      return false;
    }

    mPrivateData->mEffect->GetVariableByName("mask")->AsShaderResource()->SetResource(srViewMask);

    mPrivateData->mEffect->GetVariableByName("MaskTexCoords")->AsVector()->
      SetFloatVector(ShaderConstantRectD3D10(Float(rectBounds.x) / mSize.width, Float(rectBounds.y) / mSize.height,
                                             Float(rectBounds.width) / mSize.width, Float(rectBounds.height) / mSize.height));

    technique->GetPassByIndex(1)->Apply(0);
  } else {
    technique->GetPassByIndex(0)->Apply(0);
  }  

  RefPtr<ID3D10RenderTargetView> rtView;
  ID3D10RenderTargetView *rtViews;
  mDevice->CreateRenderTargetView(mTexture, nullptr, byRef(rtView));

  rtViews = rtView;
  mDevice->OMSetRenderTargets(1, &rtViews, nullptr);
  SetScissorToRect(clipBounds);
  mDevice->Draw(4, 0);
  return true;
}

TemporaryRef<ID2D1Brush>
DrawTargetD2D::CreateBrushForPattern(const Pattern &aPattern, Float aAlpha)
{
  if (!IsPatternSupportedByD2D(aPattern)) {
    RefPtr<ID2D1SolidColorBrush> colBrush;
    mRT->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), byRef(colBrush));
    return colBrush;
  }

  if (aPattern.GetType() == PatternType::COLOR) {
    RefPtr<ID2D1SolidColorBrush> colBrush;
    Color color = static_cast<const ColorPattern*>(&aPattern)->mColor;
    mRT->CreateSolidColorBrush(D2D1::ColorF(color.r, color.g,
                                            color.b, color.a),
                               D2D1::BrushProperties(aAlpha),
                               byRef(colBrush));
    return colBrush;
  } else if (aPattern.GetType() == PatternType::LINEAR_GRADIENT) {
    RefPtr<ID2D1LinearGradientBrush> gradBrush;
    const LinearGradientPattern *pat =
      static_cast<const LinearGradientPattern*>(&aPattern);

    GradientStopsD2D *stops = static_cast<GradientStopsD2D*>(pat->mStops.get());

    if (!stops) {
      gfxDebug() << "No stops specified for gradient pattern.";
      return nullptr;
    }

    if (pat->mBegin == pat->mEnd) {
      RefPtr<ID2D1SolidColorBrush> colBrush;
      uint32_t stopCount = stops->mStopCollection->GetGradientStopCount();
      vector<D2D1_GRADIENT_STOP> d2dStops(stopCount);
      stops->mStopCollection->GetGradientStops(&d2dStops.front(), stopCount);
      mRT->CreateSolidColorBrush(d2dStops.back().color,
                                 D2D1::BrushProperties(aAlpha),
                                 byRef(colBrush));
      return colBrush;
    }

    mRT->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2DPoint(pat->mBegin),
                                                                       D2DPoint(pat->mEnd)),
                                   D2D1::BrushProperties(aAlpha, D2DMatrix(pat->mMatrix)),
                                   stops->mStopCollection,
                                   byRef(gradBrush));
    return gradBrush;
  } else if (aPattern.GetType() == PatternType::RADIAL_GRADIENT) {
    RefPtr<ID2D1RadialGradientBrush> gradBrush;
    const RadialGradientPattern *pat =
      static_cast<const RadialGradientPattern*>(&aPattern);

    GradientStopsD2D *stops = static_cast<GradientStopsD2D*>(pat->mStops.get());

    if (!stops) {
      gfxDebug() << "No stops specified for gradient pattern.";
      return nullptr;
    }

    // This will not be a complex radial gradient brush.
    mRT->CreateRadialGradientBrush(
      D2D1::RadialGradientBrushProperties(D2DPoint(pat->mCenter2),
                                          D2DPoint(pat->mCenter1 - pat->mCenter2),
                                          pat->mRadius2, pat->mRadius2),
      D2D1::BrushProperties(aAlpha, D2DMatrix(pat->mMatrix)),
      stops->mStopCollection,
      byRef(gradBrush));

    return gradBrush;
  } else if (aPattern.GetType() == PatternType::SURFACE) {
    RefPtr<ID2D1BitmapBrush> bmBrush;
    const SurfacePattern *pat =
      static_cast<const SurfacePattern*>(&aPattern);

    if (!pat->mSurface) {
      gfxDebug() << "No source surface specified for surface pattern";
      return nullptr;
    }

    RefPtr<ID2D1Bitmap> bitmap;

    Matrix mat = pat->mMatrix;
    
    switch (pat->mSurface->GetType()) {
    case SurfaceType::D2D1_BITMAP:
      {
        SourceSurfaceD2D *surf = static_cast<SourceSurfaceD2D*>(pat->mSurface.get());

        bitmap = surf->mBitmap;

        if (!bitmap) {
          return nullptr;
        }
      }
      break;
    case SurfaceType::D2D1_DRAWTARGET:
      {
        SourceSurfaceD2DTarget *surf =
          static_cast<SourceSurfaceD2DTarget*>(pat->mSurface.get());
        bitmap = surf->GetBitmap(mRT);
        AddDependencyOnSource(surf);
      }
      break;
    default:
      {
        RefPtr<DataSourceSurface> dataSurf = pat->mSurface->GetDataSurface();
        if (!dataSurf) {
          gfxWarning() << "Invalid surface type.";
          return nullptr;
        }

        bitmap = CreatePartialBitmapForSurface(dataSurf, mTransform, mSize, pat->mExtendMode, mat, mRT); 
        if (!bitmap) {
          return nullptr;
        }
      }
      break;
    }
    
    mRT->CreateBitmapBrush(bitmap,
                           D2D1::BitmapBrushProperties(D2DExtend(pat->mExtendMode),
                                                       D2DExtend(pat->mExtendMode),
                                                       D2DFilter(pat->mFilter)),
                           D2D1::BrushProperties(aAlpha, D2DMatrix(mat)),
                           byRef(bmBrush));

    return bmBrush;
  }

  gfxWarning() << "Invalid pattern type detected.";
  return nullptr;
}

TemporaryRef<ID3D10Texture2D>
DrawTargetD2D::CreateGradientTexture(const GradientStopsD2D *aStops)
{
  CD3D10_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, 4096, 1, 1, 1);

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
  data.SysMemPitch = 4096 * 4;

  RefPtr<ID3D10Texture2D> tex;
  mDevice->CreateTexture2D(&desc, &data, byRef(tex));

  return tex;
}

TemporaryRef<ID3D10Texture2D>
DrawTargetD2D::CreateTextureForAnalysis(IDWriteGlyphRunAnalysis *aAnalysis, const IntRect &aBounds)
{
  HRESULT hr;

  uint32_t bufferSize = aBounds.width * aBounds.height * 3;

  RECT bounds;
  bounds.left = aBounds.x;
  bounds.top = aBounds.y;
  bounds.right = aBounds.x + aBounds.width;
  bounds.bottom = aBounds.y + aBounds.height;

  // Add one byte so we can safely read a 32-bit int when copying the last
  // 3 bytes.
  BYTE *texture = new BYTE[bufferSize + 1];
  hr = aAnalysis->CreateAlphaTexture(DWRITE_TEXTURE_CLEARTYPE_3x1, &bounds, texture, bufferSize);

  if (FAILED(hr)) {
    delete [] texture;
    return nullptr;
  }

  int alignedBufferSize = aBounds.width * aBounds.height * 4;

  // Create a one-off immutable texture from system memory.
  BYTE *alignedTextureData = new BYTE[alignedBufferSize];
  for (int y = 0; y < aBounds.height; y++) {
    for (int x = 0; x < aBounds.width; x++) {
      // Copy 3 Bpp source to 4 Bpp destination memory used for
      // texture creation. D3D10 has no 3 Bpp texture format we can
      // use.
      //
      // Since we don't care what ends up in the alpha pixel of the
      // destination, therefor we can simply copy a normal 32 bit
      // integer each time, filling the alpha pixel of the destination
      // with the first subpixel of the next pixel from the source.
      *((int*)(alignedTextureData + (y * aBounds.width + x) * 4)) =
        *((int*)(texture + (y * aBounds.width + x) * 3));
    }
  }

  D3D10_SUBRESOURCE_DATA data;
  
  CD3D10_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM,
                             aBounds.width, aBounds.height,
                             1, 1);
  desc.Usage = D3D10_USAGE_IMMUTABLE;

  data.SysMemPitch = aBounds.width * 4;
  data.pSysMem = alignedTextureData;

  RefPtr<ID3D10Texture2D> tex;
  hr = mDevice->CreateTexture2D(&desc, &data, byRef(tex));
	
  delete [] alignedTextureData;
  delete [] texture;

  if (FAILED(hr)) {
    return nullptr;
  }

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

  RefPtr<ID3D10Texture2D> tex = CreateGradientTexture(stops);

  RefPtr<ID3D10ShaderResourceView> srView;
  mDevice->CreateShaderResourceView(tex, nullptr, byRef(srView));

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

  uint32_t offset = 0;
  switch (stops->mStopCollection->GetExtendMode()) {
  case D2D1_EXTEND_MODE_WRAP:
    offset = 1;
    break;
  case D2D1_EXTEND_MODE_MIRROR:
    offset = 2;
    break;
  default:
    gfxWarning() << "This shouldn't happen! Invalid extend mode for gradient stops.";
  }

  if (A == 0) {
    mPrivateData->mEffect->GetTechniqueByName("SampleRadialGradient")->
      GetPassByIndex(offset * 2 + 1)->Apply(0);
  } else {
    mPrivateData->mEffect->GetVariableByName("A")->AsScalar()->SetFloat(A);
    mPrivateData->mEffect->GetTechniqueByName("SampleRadialGradient")->
      GetPassByIndex(offset * 2)->Apply(0);
  }
}

void
DrawTargetD2D::SetupStateForRendering()
{
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
    return nullptr;
  }

  D2D1_FACTORY_OPTIONS options;
#ifdef _DEBUG
  options.debugLevel = D2D1_DEBUG_LEVEL_WARNING;
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

void
DrawTargetD2D::CleanupD2D()
{
  if (mFactory) {
    mFactory->Release();
    mFactory = nullptr;
  }
}

IDWriteFactory*
DrawTargetD2D::GetDWriteFactory()
{
  if (mDWriteFactory) {
    return mDWriteFactory;
  }

  decltype(DWriteCreateFactory)* createDWriteFactory;
  HMODULE dwriteModule = LoadLibraryW(L"dwrite.dll");
  createDWriteFactory = (decltype(DWriteCreateFactory)*)
    GetProcAddress(dwriteModule, "DWriteCreateFactory");

  if (!createDWriteFactory) {
    gfxWarning() << "Failed to locate DWriteCreateFactory function.";
    return nullptr;
  }

  HRESULT hr = createDWriteFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(&mDWriteFactory));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create DWrite Factory.";
  }

  return mDWriteFactory;
}

void
DrawTargetD2D::SetScissorToRect(IntRect *aRect)
{
  D3D10_RECT rect;
  if (aRect) {
    rect.left = aRect->x;
    rect.right = aRect->XMost();
    rect.top = aRect->y;
    rect.bottom = aRect->YMost();
  } else {
    rect.left = rect.top = INT32_MIN;
    rect.right = rect.bottom = INT32_MAX;
  }

  mDevice->RSSetScissorRects(1, &rect);
}

void
DrawTargetD2D::PushD2DLayer(ID2D1RenderTarget *aRT, ID2D1Geometry *aGeometry, ID2D1Layer *aLayer, const D2D1_MATRIX_3X2_F &aTransform)
{
  D2D1_LAYER_OPTIONS options = D2D1_LAYER_OPTIONS_NONE;
  D2D1_LAYER_OPTIONS1 options1 =  D2D1_LAYER_OPTIONS1_NONE;

  if (aRT->GetPixelFormat().alphaMode == D2D1_ALPHA_MODE_IGNORE) {
    options = D2D1_LAYER_OPTIONS_INITIALIZE_FOR_CLEARTYPE;
    options1 = D2D1_LAYER_OPTIONS1_IGNORE_ALPHA | D2D1_LAYER_OPTIONS1_INITIALIZE_FROM_BACKGROUND;
  }

	RefPtr<ID2D1DeviceContext> dc;
	HRESULT hr = aRT->QueryInterface(IID_ID2D1DeviceContext, (void**)((ID2D1DeviceContext**)byRef(dc)));

	if (FAILED(hr)) {
	    aRT->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), aGeometry,
				                                   D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, aTransform,
				                                   1.0, nullptr, options),
				             aLayer);
	} else {
	    dc->PushLayer(D2D1::LayerParameters1(D2D1::InfiniteRect(), aGeometry,
				                                   D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, aTransform,
				                                   1.0, nullptr, options1),
				            aLayer);
	}
}

}
}
