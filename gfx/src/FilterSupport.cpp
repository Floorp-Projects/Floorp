/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FilterSupport.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Filters.h"
#include "mozilla/PodOperations.h"

#include "gfxContext.h"
#include "gfxPattern.h"
#include "gfxPlatform.h"
#include "gfx2DGlue.h"

#include "nsMargin.h"

// c = n / 255
// c <= 0.0031308f ? c * 12.92f : 1.055f * powf(c, 1 / 2.4f) - 0.055f
static const float glinearRGBTosRGBMap[256] = {
  0.000f, 0.050f, 0.085f, 0.111f, 0.132f, 0.150f, 0.166f, 0.181f,
  0.194f, 0.207f, 0.219f, 0.230f, 0.240f, 0.250f, 0.260f, 0.269f,
  0.278f, 0.286f, 0.295f, 0.303f, 0.310f, 0.318f, 0.325f, 0.332f,
  0.339f, 0.346f, 0.352f, 0.359f, 0.365f, 0.371f, 0.378f, 0.383f,
  0.389f, 0.395f, 0.401f, 0.406f, 0.412f, 0.417f, 0.422f, 0.427f,
  0.433f, 0.438f, 0.443f, 0.448f, 0.452f, 0.457f, 0.462f, 0.466f,
  0.471f, 0.476f, 0.480f, 0.485f, 0.489f, 0.493f, 0.498f, 0.502f,
  0.506f, 0.510f, 0.514f, 0.518f, 0.522f, 0.526f, 0.530f, 0.534f,
  0.538f, 0.542f, 0.546f, 0.549f, 0.553f, 0.557f, 0.561f, 0.564f,
  0.568f, 0.571f, 0.575f, 0.579f, 0.582f, 0.586f, 0.589f, 0.592f,
  0.596f, 0.599f, 0.603f, 0.606f, 0.609f, 0.613f, 0.616f, 0.619f,
  0.622f, 0.625f, 0.629f, 0.632f, 0.635f, 0.638f, 0.641f, 0.644f,
  0.647f, 0.650f, 0.653f, 0.656f, 0.659f, 0.662f, 0.665f, 0.668f,
  0.671f, 0.674f, 0.677f, 0.680f, 0.683f, 0.685f, 0.688f, 0.691f,
  0.694f, 0.697f, 0.699f, 0.702f, 0.705f, 0.708f, 0.710f, 0.713f,
  0.716f, 0.718f, 0.721f, 0.724f, 0.726f, 0.729f, 0.731f, 0.734f,
  0.737f, 0.739f, 0.742f, 0.744f, 0.747f, 0.749f, 0.752f, 0.754f,
  0.757f, 0.759f, 0.762f, 0.764f, 0.767f, 0.769f, 0.772f, 0.774f,
  0.776f, 0.779f, 0.781f, 0.784f, 0.786f, 0.788f, 0.791f, 0.793f,
  0.795f, 0.798f, 0.800f, 0.802f, 0.805f, 0.807f, 0.809f, 0.812f,
  0.814f, 0.816f, 0.818f, 0.821f, 0.823f, 0.825f, 0.827f, 0.829f,
  0.832f, 0.834f, 0.836f, 0.838f, 0.840f, 0.843f, 0.845f, 0.847f,
  0.849f, 0.851f, 0.853f, 0.855f, 0.857f, 0.860f, 0.862f, 0.864f,
  0.866f, 0.868f, 0.870f, 0.872f, 0.874f, 0.876f, 0.878f, 0.880f,
  0.882f, 0.884f, 0.886f, 0.888f, 0.890f, 0.892f, 0.894f, 0.896f,
  0.898f, 0.900f, 0.902f, 0.904f, 0.906f, 0.908f, 0.910f, 0.912f,
  0.914f, 0.916f, 0.918f, 0.920f, 0.922f, 0.924f, 0.926f, 0.928f,
  0.930f, 0.931f, 0.933f, 0.935f, 0.937f, 0.939f, 0.941f, 0.943f,
  0.945f, 0.946f, 0.948f, 0.950f, 0.952f, 0.954f, 0.956f, 0.957f,
  0.959f, 0.961f, 0.963f, 0.965f, 0.967f, 0.968f, 0.970f, 0.972f,
  0.974f, 0.975f, 0.977f, 0.979f, 0.981f, 0.983f, 0.984f, 0.986f,
  0.988f, 0.990f, 0.991f, 0.993f, 0.995f, 0.997f, 0.998f, 1.000f
};

// c = n / 255
// c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f)
static const float gsRGBToLinearRGBMap[256] = {
  0.000f, 0.000f, 0.001f, 0.001f, 0.001f, 0.002f, 0.002f, 0.002f,
  0.002f, 0.003f, 0.003f, 0.003f, 0.004f, 0.004f, 0.004f, 0.005f,
  0.005f, 0.006f, 0.006f, 0.007f, 0.007f, 0.007f, 0.008f, 0.009f,
  0.009f, 0.010f, 0.010f, 0.011f, 0.012f, 0.012f, 0.013f, 0.014f,
  0.014f, 0.015f, 0.016f, 0.017f, 0.018f, 0.019f, 0.019f, 0.020f,
  0.021f, 0.022f, 0.023f, 0.024f, 0.025f, 0.026f, 0.027f, 0.028f,
  0.030f, 0.031f, 0.032f, 0.033f, 0.034f, 0.036f, 0.037f, 0.038f,
  0.040f, 0.041f, 0.042f, 0.044f, 0.045f, 0.047f, 0.048f, 0.050f,
  0.051f, 0.053f, 0.054f, 0.056f, 0.058f, 0.060f, 0.061f, 0.063f,
  0.065f, 0.067f, 0.068f, 0.070f, 0.072f, 0.074f, 0.076f, 0.078f,
  0.080f, 0.082f, 0.084f, 0.087f, 0.089f, 0.091f, 0.093f, 0.095f,
  0.098f, 0.100f, 0.102f, 0.105f, 0.107f, 0.109f, 0.112f, 0.114f,
  0.117f, 0.120f, 0.122f, 0.125f, 0.127f, 0.130f, 0.133f, 0.136f,
  0.138f, 0.141f, 0.144f, 0.147f, 0.150f, 0.153f, 0.156f, 0.159f,
  0.162f, 0.165f, 0.168f, 0.171f, 0.175f, 0.178f, 0.181f, 0.184f,
  0.188f, 0.191f, 0.195f, 0.198f, 0.202f, 0.205f, 0.209f, 0.212f,
  0.216f, 0.220f, 0.223f, 0.227f, 0.231f, 0.235f, 0.238f, 0.242f,
  0.246f, 0.250f, 0.254f, 0.258f, 0.262f, 0.266f, 0.270f, 0.275f,
  0.279f, 0.283f, 0.287f, 0.292f, 0.296f, 0.301f, 0.305f, 0.309f,
  0.314f, 0.319f, 0.323f, 0.328f, 0.332f, 0.337f, 0.342f, 0.347f,
  0.352f, 0.356f, 0.361f, 0.366f, 0.371f, 0.376f, 0.381f, 0.386f,
  0.392f, 0.397f, 0.402f, 0.407f, 0.413f, 0.418f, 0.423f, 0.429f,
  0.434f, 0.440f, 0.445f, 0.451f, 0.456f, 0.462f, 0.468f, 0.474f,
  0.479f, 0.485f, 0.491f, 0.497f, 0.503f, 0.509f, 0.515f, 0.521f,
  0.527f, 0.533f, 0.539f, 0.546f, 0.552f, 0.558f, 0.565f, 0.571f,
  0.578f, 0.584f, 0.591f, 0.597f, 0.604f, 0.610f, 0.617f, 0.624f,
  0.631f, 0.638f, 0.644f, 0.651f, 0.658f, 0.665f, 0.672f, 0.680f,
  0.687f, 0.694f, 0.701f, 0.708f, 0.716f, 0.723f, 0.730f, 0.738f,
  0.745f, 0.753f, 0.761f, 0.768f, 0.776f, 0.784f, 0.791f, 0.799f,
  0.807f, 0.815f, 0.823f, 0.831f, 0.839f, 0.847f, 0.855f, 0.863f,
  0.871f, 0.880f, 0.888f, 0.896f, 0.905f, 0.913f, 0.922f, 0.930f,
  0.939f, 0.947f, 0.956f, 0.965f, 0.973f, 0.982f, 0.991f, 1.000f
};

namespace mozilla {
namespace gfx {

// Some convenience FilterNode creation functions.

namespace FilterWrappers {

  static TemporaryRef<FilterNode>
  Unpremultiply(DrawTarget* aDT, FilterNode* aInput)
  {
    RefPtr<FilterNode> filter = aDT->CreateFilter(FILTER_UNPREMULTIPLY);
    filter->SetInput(IN_UNPREMULTIPLY_IN, aInput);
    return filter;
  }

  static TemporaryRef<FilterNode>
  Premultiply(DrawTarget* aDT, FilterNode* aInput)
  {
    RefPtr<FilterNode> filter = aDT->CreateFilter(FILTER_PREMULTIPLY);
    filter->SetInput(IN_PREMULTIPLY_IN, aInput);
    return filter;
  }

  static TemporaryRef<FilterNode>
  LinearRGBToSRGB(DrawTarget* aDT, FilterNode* aInput)
  {
    RefPtr<FilterNode> transfer = aDT->CreateFilter(FILTER_DISCRETE_TRANSFER);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_DISABLE_R, false);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_TABLE_R, glinearRGBTosRGBMap, 256);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_DISABLE_G, false);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_TABLE_G, glinearRGBTosRGBMap, 256);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_DISABLE_B, false);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_TABLE_B, glinearRGBTosRGBMap, 256);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_DISABLE_A, true);
    transfer->SetInput(IN_DISCRETE_TRANSFER_IN, aInput);
    return transfer;
  }

  static TemporaryRef<FilterNode>
  SRGBToLinearRGB(DrawTarget* aDT, FilterNode* aInput)
  {
    RefPtr<FilterNode> transfer = aDT->CreateFilter(FILTER_DISCRETE_TRANSFER);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_DISABLE_R, false);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_TABLE_R, gsRGBToLinearRGBMap, 256);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_DISABLE_G, false);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_TABLE_G, gsRGBToLinearRGBMap, 256);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_DISABLE_B, false);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_TABLE_B, gsRGBToLinearRGBMap, 256);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_DISABLE_A, true);
    transfer->SetInput(IN_DISCRETE_TRANSFER_IN, aInput);
    return transfer;
  }

  static TemporaryRef<FilterNode>
  Crop(DrawTarget* aDT, FilterNode* aInputFilter, const IntRect& aRect)
  {
    RefPtr<FilterNode> filter = aDT->CreateFilter(FILTER_CROP);
    filter->SetAttribute(ATT_CROP_RECT, Rect(aRect));
    filter->SetInput(IN_CROP_IN, aInputFilter);
    return filter;
  }

  static TemporaryRef<FilterNode>
  Offset(DrawTarget* aDT, FilterNode* aInputFilter, const IntPoint& aOffset)
  {
    RefPtr<FilterNode> filter = aDT->CreateFilter(FILTER_TRANSFORM);
    filter->SetAttribute(ATT_TRANSFORM_MATRIX, Matrix().Translate(aOffset.x, aOffset.y));
    filter->SetInput(IN_TRANSFORM_IN, aInputFilter);
    return filter;
  }

  static TemporaryRef<FilterNode>
  Clear(DrawTarget* aDT)
  {
    RefPtr<FilterNode> filter = aDT->CreateFilter(FILTER_FLOOD);
    filter->SetAttribute(ATT_FLOOD_COLOR, Color(0,0,0,0));
    return filter;
  }

  static TemporaryRef<FilterNode>
  ForSurface(DrawTarget* aDT, SourceSurface* aSurface,
             const IntPoint& aSurfacePosition)
  {
    RefPtr<FilterNode> filter = aDT->CreateFilter(FILTER_TRANSFORM);
    filter->SetAttribute(ATT_TRANSFORM_MATRIX,
      Matrix().Translate(aSurfacePosition.x, aSurfacePosition.y));
    filter->SetInput(IN_TRANSFORM_IN, aSurface);
    return filter;
  }

  static TemporaryRef<FilterNode>
  ToAlpha(DrawTarget* aDT, FilterNode* aInput)
  {
    float zero = 0.0f;
    RefPtr<FilterNode> transfer = aDT->CreateFilter(FILTER_DISCRETE_TRANSFER);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_DISABLE_R, false);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_TABLE_R, &zero, 1);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_DISABLE_G, false);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_TABLE_G, &zero, 1);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_DISABLE_B, false);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_TABLE_B, &zero, 1);
    transfer->SetAttribute(ATT_DISCRETE_TRANSFER_DISABLE_A, true);
    transfer->SetInput(IN_DISCRETE_TRANSFER_IN, aInput);
    return transfer;
  }

}

// A class that wraps a FilterNode and handles conversion between different
// color models. Create FilterCachedColorModels with your original filter and
// the color model that this filter outputs in natively, and then call
// ->ForColorModel(colorModel) in order to get a FilterNode which outputs to
// the specified colorModel.
// Internally, this is achieved by wrapping the original FilterNode with
// conversion FilterNodes. These filter nodes are cached in such a way that no
// repeated or back-and-forth conversions happen.
class FilterCachedColorModels : public RefCounted<FilterCachedColorModels>
{
public:
  // aFilter can be null. In that case, ForColorModel will return a non-null
  // completely transparent filter for all color models.
  FilterCachedColorModels(DrawTarget* aDT,
                          FilterNode* aFilter,
                          ColorModel aOriginalColorModel);

  // Get a FilterNode for the specified color model, guaranteed to be non-null.
  TemporaryRef<FilterNode> ForColorModel(ColorModel aColorModel);

  AlphaModel OriginalAlphaModel() const { return mOriginalColorModel.mAlphaModel; }

private:
  // Create the required FilterNode that will be cached by ForColorModel.
  TemporaryRef<FilterNode> WrapForColorModel(ColorModel aColorModel);

  RefPtr<DrawTarget> mDT;
  ColorModel mOriginalColorModel;

  // This array is indexed by ColorModel::ToIndex.
  RefPtr<FilterNode> mFilterForColorModel[4];
};

FilterCachedColorModels::FilterCachedColorModels(DrawTarget* aDT,
                                                 FilterNode* aFilter,
                                                 ColorModel aOriginalColorModel)
 : mDT(aDT)
 , mOriginalColorModel(aOriginalColorModel)
{
  if (aFilter) {
    mFilterForColorModel[aOriginalColorModel.ToIndex()] = aFilter;
  } else {
    RefPtr<FilterNode> clear = FilterWrappers::Clear(aDT);
    mFilterForColorModel[0] = clear;
    mFilterForColorModel[1] = clear;
    mFilterForColorModel[2] = clear;
    mFilterForColorModel[3] = clear;
  }
}

TemporaryRef<FilterNode>
FilterCachedColorModels::ForColorModel(ColorModel aColorModel)
{
  if (!mFilterForColorModel[aColorModel.ToIndex()]) {
    mFilterForColorModel[aColorModel.ToIndex()] = WrapForColorModel(aColorModel);
  }
  return mFilterForColorModel[aColorModel.ToIndex()];
}

TemporaryRef<FilterNode>
FilterCachedColorModels::WrapForColorModel(ColorModel aColorModel)
{
  // Convert one aspect at a time and recurse.
  // Conversions between premultiplied / unpremultiplied color channels for the
  // same color space can happen directly.
  // Conversions between different color spaces can only happen on
  // unpremultiplied color channels.

  if (aColorModel.mAlphaModel == PREMULTIPLIED) {
    RefPtr<FilterNode> unpre =
      ForColorModel(ColorModel(aColorModel.mColorSpace, UNPREMULTIPLIED));
    return FilterWrappers::Premultiply(mDT, unpre);
  }

  MOZ_ASSERT(aColorModel.mAlphaModel == UNPREMULTIPLIED);
  if (aColorModel.mColorSpace == mOriginalColorModel.mColorSpace) {
    RefPtr<FilterNode> premultiplied =
      ForColorModel(ColorModel(aColorModel.mColorSpace, PREMULTIPLIED));
    return FilterWrappers::Unpremultiply(mDT, premultiplied);
  }

  RefPtr<FilterNode> unpremultipliedOriginal =
    ForColorModel(ColorModel(mOriginalColorModel.mColorSpace, UNPREMULTIPLIED));
  if (aColorModel.mColorSpace == LINEAR_RGB) {
    return FilterWrappers::SRGBToLinearRGB(mDT, unpremultipliedOriginal);
  }
  return FilterWrappers::LinearRGBToSRGB(mDT, unpremultipliedOriginal);
}

// Create a 4x5 color matrix for the different ways to specify color matrices
// in SVG.
static nsresult
ComputeColorMatrix(uint32_t aColorMatrixType, const nsTArray<float>& aValues,
                   float aOutMatrix[20])
{
  static const float identityMatrix[] =
    { 1, 0, 0, 0, 0,
      0, 1, 0, 0, 0,
      0, 0, 1, 0, 0,
      0, 0, 0, 1, 0 };

  static const float luminanceToAlphaMatrix[] =
    { 0,       0,       0,       0, 0,
      0,       0,       0,       0, 0,
      0,       0,       0,       0, 0,
      0.2125f, 0.7154f, 0.0721f, 0, 0 };

  switch (aColorMatrixType) {

    case SVG_FECOLORMATRIX_TYPE_MATRIX:
    {
      if (aValues.Length() != 20)
        return NS_ERROR_FAILURE;

      PodCopy(aOutMatrix, aValues.Elements(), 20);
      break;
    }

    case SVG_FECOLORMATRIX_TYPE_SATURATE:
    {
      if (aValues.Length() != 1)
        return NS_ERROR_FAILURE;

      float s = aValues[0];

      if (s > 1 || s < 0)
        return NS_ERROR_FAILURE;

      PodCopy(aOutMatrix, identityMatrix, 20);

      aOutMatrix[0] = 0.213f + 0.787f * s;
      aOutMatrix[1] = 0.715f - 0.715f * s;
      aOutMatrix[2] = 0.072f - 0.072f * s;

      aOutMatrix[5] = 0.213f - 0.213f * s;
      aOutMatrix[6] = 0.715f + 0.285f * s;
      aOutMatrix[7] = 0.072f - 0.072f * s;

      aOutMatrix[10] = 0.213f - 0.213f * s;
      aOutMatrix[11] = 0.715f - 0.715f * s;
      aOutMatrix[12] = 0.072f + 0.928f * s;

      break;
    }

    case SVG_FECOLORMATRIX_TYPE_HUE_ROTATE:
    {
      if (aValues.Length() != 1)
        return NS_ERROR_FAILURE;

      PodCopy(aOutMatrix, identityMatrix, 20);

      float hueRotateValue = aValues[0];

      float c = static_cast<float>(cos(hueRotateValue * M_PI / 180));
      float s = static_cast<float>(sin(hueRotateValue * M_PI / 180));

      aOutMatrix[0] = 0.213f + 0.787f * c - 0.213f * s;
      aOutMatrix[1] = 0.715f - 0.715f * c - 0.715f * s;
      aOutMatrix[2] = 0.072f - 0.072f * c + 0.928f * s;

      aOutMatrix[5] = 0.213f - 0.213f * c + 0.143f * s;
      aOutMatrix[6] = 0.715f + 0.285f * c + 0.140f * s;
      aOutMatrix[7] = 0.072f - 0.072f * c - 0.283f * s;

      aOutMatrix[10] = 0.213f - 0.213f * c - 0.787f * s;
      aOutMatrix[11] = 0.715f - 0.715f * c + 0.715f * s;
      aOutMatrix[12] = 0.072f + 0.928f * c + 0.072f * s;

      break;
    }

    case SVG_FECOLORMATRIX_TYPE_LUMINANCE_TO_ALPHA:
    {
      PodCopy(aOutMatrix, luminanceToAlphaMatrix, 20);
      break;
    }

    default:
      return NS_ERROR_FAILURE;

  }

  return NS_OK;
}

static void
DisableAllTransfers(FilterNode* aTransferFilterNode)
{
  aTransferFilterNode->SetAttribute(ATT_TRANSFER_DISABLE_R, true);
  aTransferFilterNode->SetAttribute(ATT_TRANSFER_DISABLE_G, true);
  aTransferFilterNode->SetAttribute(ATT_TRANSFER_DISABLE_B, true);
  aTransferFilterNode->SetAttribute(ATT_TRANSFER_DISABLE_A, true);
}

// Called for one channel at a time.
// This function creates the required FilterNodes on demand and tries to
// merge conversions of different channels into the same FilterNode if
// possible.
// There's a mismatch between the way SVG and the Moz2D API handle transfer
// functions: In SVG, it's possible to specify a different transfer function
// type for each color channel, but in Moz2D, a given transfer function type
// applies to all color channels.
//
//  @param aFunctionAttributes The attributes of the transfer function for this
//                             channel.
//  @param aChannel The color channel that this function applies to, where
//                  0 = red, 1 = green, 2 = blue, 3 = alpha
//  @param aDT The DrawTarget that the FilterNodes should be created for.
//  @param aTableTransfer Existing FilterNode holders (which may still be
//                        null) that the resulting FilterNodes from this
//                        function will be stored in.
//           
static void
ConvertComponentTransferFunctionToFilter(const AttributeMap& aFunctionAttributes,
                                         int32_t aChannel,
                                         DrawTarget* aDT,
                                         RefPtr<FilterNode>& aTableTransfer,
                                         RefPtr<FilterNode>& aDiscreteTransfer,
                                         RefPtr<FilterNode>& aLinearTransfer,
                                         RefPtr<FilterNode>& aGammaTransfer)
{
  static const TransferAtts disableAtt[4] = {
    ATT_TRANSFER_DISABLE_R,
    ATT_TRANSFER_DISABLE_G,
    ATT_TRANSFER_DISABLE_B,
    ATT_TRANSFER_DISABLE_A
  };

  RefPtr<FilterNode> filter;

  uint32_t type = aFunctionAttributes.GetUint(eComponentTransferFunctionType);

  switch (type) {
  case SVG_FECOMPONENTTRANSFER_TYPE_TABLE:
  {
    const nsTArray<float>& tableValues =
      aFunctionAttributes.GetFloats(eComponentTransferFunctionTableValues);
    if (tableValues.Length() < 2)
      return;

    if (!aTableTransfer) {
      aTableTransfer = aDT->CreateFilter(FILTER_TABLE_TRANSFER);
      DisableAllTransfers(aTableTransfer);
    }
    filter = aTableTransfer;
    static const TableTransferAtts tableAtt[4] = {
      ATT_TABLE_TRANSFER_TABLE_R,
      ATT_TABLE_TRANSFER_TABLE_G,
      ATT_TABLE_TRANSFER_TABLE_B,
      ATT_TABLE_TRANSFER_TABLE_A
    };
    filter->SetAttribute(disableAtt[aChannel], false);
    filter->SetAttribute(tableAtt[aChannel], &tableValues[0], tableValues.Length());
    break;
  }

  case SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE:
  {
    const nsTArray<float>& tableValues =
      aFunctionAttributes.GetFloats(eComponentTransferFunctionTableValues);
    if (tableValues.Length() < 1)
      return;

    if (!aDiscreteTransfer) {
      aDiscreteTransfer = aDT->CreateFilter(FILTER_DISCRETE_TRANSFER);
      DisableAllTransfers(aDiscreteTransfer);
    }
    filter = aDiscreteTransfer;
    static const DiscreteTransferAtts tableAtt[4] = {
      ATT_DISCRETE_TRANSFER_TABLE_R,
      ATT_DISCRETE_TRANSFER_TABLE_G,
      ATT_DISCRETE_TRANSFER_TABLE_B,
      ATT_DISCRETE_TRANSFER_TABLE_A
    };
    filter->SetAttribute(disableAtt[aChannel], false);
    filter->SetAttribute(tableAtt[aChannel], &tableValues[0], tableValues.Length());

    break;
  }

  case SVG_FECOMPONENTTRANSFER_TYPE_LINEAR:
  {
    static const LinearTransferAtts slopeAtt[4] = {
      ATT_LINEAR_TRANSFER_SLOPE_R,
      ATT_LINEAR_TRANSFER_SLOPE_G,
      ATT_LINEAR_TRANSFER_SLOPE_B,
      ATT_LINEAR_TRANSFER_SLOPE_A
    };
    static const LinearTransferAtts interceptAtt[4] = {
      ATT_LINEAR_TRANSFER_INTERCEPT_R,
      ATT_LINEAR_TRANSFER_INTERCEPT_G,
      ATT_LINEAR_TRANSFER_INTERCEPT_B,
      ATT_LINEAR_TRANSFER_INTERCEPT_A
    };
    if (!aLinearTransfer) {
      aLinearTransfer = aDT->CreateFilter(FILTER_LINEAR_TRANSFER);
      DisableAllTransfers(aLinearTransfer);
    }
    filter = aLinearTransfer;
    filter->SetAttribute(disableAtt[aChannel], false);
    float slope = aFunctionAttributes.GetFloat(eComponentTransferFunctionSlope);
    float intercept = aFunctionAttributes.GetFloat(eComponentTransferFunctionIntercept);
    filter->SetAttribute(slopeAtt[aChannel], slope);
    filter->SetAttribute(interceptAtt[aChannel], intercept);
    break;
  }

  case SVG_FECOMPONENTTRANSFER_TYPE_GAMMA:
  {
    static const GammaTransferAtts amplitudeAtt[4] = {
      ATT_GAMMA_TRANSFER_AMPLITUDE_R,
      ATT_GAMMA_TRANSFER_AMPLITUDE_G,
      ATT_GAMMA_TRANSFER_AMPLITUDE_B,
      ATT_GAMMA_TRANSFER_AMPLITUDE_A
    };
    static const GammaTransferAtts exponentAtt[4] = {
      ATT_GAMMA_TRANSFER_EXPONENT_R,
      ATT_GAMMA_TRANSFER_EXPONENT_G,
      ATT_GAMMA_TRANSFER_EXPONENT_B,
      ATT_GAMMA_TRANSFER_EXPONENT_A
    };
    static const GammaTransferAtts offsetAtt[4] = {
      ATT_GAMMA_TRANSFER_OFFSET_R,
      ATT_GAMMA_TRANSFER_OFFSET_G,
      ATT_GAMMA_TRANSFER_OFFSET_B,
      ATT_GAMMA_TRANSFER_OFFSET_A
    };
    if (!aGammaTransfer) {
      aGammaTransfer = aDT->CreateFilter(FILTER_GAMMA_TRANSFER);
      DisableAllTransfers(aGammaTransfer);
    }
    filter = aGammaTransfer;
    filter->SetAttribute(disableAtt[aChannel], false);
    float amplitude = aFunctionAttributes.GetFloat(eComponentTransferFunctionAmplitude);
    float exponent = aFunctionAttributes.GetFloat(eComponentTransferFunctionExponent);
    float offset = aFunctionAttributes.GetFloat(eComponentTransferFunctionOffset);
    filter->SetAttribute(amplitudeAtt[aChannel], amplitude);
    filter->SetAttribute(exponentAtt[aChannel], exponent);
    filter->SetAttribute(offsetAtt[aChannel], offset);
    break;
  }

  case SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY:
  default:
    break;
  }
}

const int32_t kMorphologyMaxRadius = 100000;

// Handle the different primitive description types and create the necessary
// FilterNode(s) for each.
// Returns nullptr for invalid filter primitives. This should be interpreted as
// transparent black by the caller.
// aSourceRegions contains the filter primitive subregions of the source
// primitives; only needed for eTile primitives.
// aInputImages carries additional surfaces that are used by eImage primitives.
static TemporaryRef<FilterNode>
FilterNodeFromPrimitiveDescription(const FilterPrimitiveDescription& aDescription,
                                   DrawTarget* aDT,
                                   nsTArray<RefPtr<FilterNode> >& aSources,
                                   nsTArray<IntRect>& aSourceRegions,
                                   nsTArray<nsRefPtr<gfxASurface> >& aInputImages)
{
  const AttributeMap& atts = aDescription.Attributes();
  switch (aDescription.Type()) {

    case FilterPrimitiveDescription::eNone:
      return nullptr;

    case FilterPrimitiveDescription::eBlend:
    {
      uint32_t mode = atts.GetUint(eBlendBlendmode);
      RefPtr<FilterNode> filter;
      if (mode == SVG_FEBLEND_MODE_UNKNOWN) {
        return nullptr;
      }
      if (mode == SVG_FEBLEND_MODE_NORMAL) {
        filter = aDT->CreateFilter(FILTER_COMPOSITE);
        filter->SetInput(IN_COMPOSITE_IN_START, aSources[1]);
        filter->SetInput(IN_COMPOSITE_IN_START + 1, aSources[0]);
      } else {
        filter = aDT->CreateFilter(FILTER_BLEND);
        static const uint8_t blendModes[SVG_FEBLEND_MODE_LIGHTEN + 1] = {
          0,
          0,
          BLEND_MODE_MULTIPLY,
          BLEND_MODE_SCREEN,
          BLEND_MODE_DARKEN,
          BLEND_MODE_LIGHTEN
        };
        filter->SetAttribute(ATT_BLEND_BLENDMODE, (uint32_t)blendModes[mode]);
        filter->SetInput(IN_BLEND_IN, aSources[0]);
        filter->SetInput(IN_BLEND_IN2, aSources[1]);
      }
      return filter;
    }

    case FilterPrimitiveDescription::eColorMatrix:
    {
      float colorMatrix[20];
      uint32_t type = atts.GetUint(eColorMatrixType);
      const nsTArray<float>& values = atts.GetFloats(eColorMatrixValues);
      if (NS_FAILED(ComputeColorMatrix(type, values, colorMatrix))) {
        return aSources[0];
      }
      Matrix5x4 matrix(colorMatrix[0], colorMatrix[5], colorMatrix[10],  colorMatrix[15],
                       colorMatrix[1], colorMatrix[6], colorMatrix[11],  colorMatrix[16],
                       colorMatrix[2], colorMatrix[7], colorMatrix[12],  colorMatrix[17],
                       colorMatrix[3], colorMatrix[8], colorMatrix[13],  colorMatrix[18],
                       colorMatrix[4], colorMatrix[9], colorMatrix[14],  colorMatrix[19]);
      RefPtr<FilterNode> filter = aDT->CreateFilter(FILTER_COLOR_MATRIX);
      filter->SetAttribute(ATT_COLOR_MATRIX_MATRIX, matrix);
      filter->SetAttribute(ATT_COLOR_MATRIX_ALPHA_MODE, (uint32_t)ALPHA_MODE_STRAIGHT);
      filter->SetInput(IN_COLOR_MATRIX_IN, aSources[0]);
      return filter;
    }

    case FilterPrimitiveDescription::eMorphology:
    {
      Size radii = atts.GetSize(eMorphologyRadii);
      int32_t rx = radii.width;
      int32_t ry = radii.height;
      if (rx < 0 || ry < 0) {
        // XXX SVGContentUtils::ReportToConsole()
        return nullptr;
      }
      if (rx == 0 && ry == 0) {
        return nullptr;
      }

      // Clamp radii to prevent completely insane values:
      rx = std::min(rx, kMorphologyMaxRadius);
      ry = std::min(ry, kMorphologyMaxRadius);

      MorphologyOperator op = atts.GetUint(eMorphologyOperator) == SVG_OPERATOR_ERODE ?
        MORPHOLOGY_OPERATOR_ERODE : MORPHOLOGY_OPERATOR_DILATE;

      RefPtr<FilterNode> filter = aDT->CreateFilter(FILTER_MORPHOLOGY);
      filter->SetAttribute(ATT_MORPHOLOGY_RADII, IntSize(rx, ry));
      filter->SetAttribute(ATT_MORPHOLOGY_OPERATOR, (uint32_t)op);
      filter->SetInput(IN_MORPHOLOGY_IN, aSources[0]);
      return filter;
    }

    case FilterPrimitiveDescription::eFlood:
    {
      Color color = atts.GetColor(eFloodColor);
      RefPtr<FilterNode> filter = aDT->CreateFilter(FILTER_FLOOD);
      filter->SetAttribute(ATT_FLOOD_COLOR, color);
      return filter;
    }

    case FilterPrimitiveDescription::eTile:
    {
      RefPtr<FilterNode> filter = aDT->CreateFilter(FILTER_TILE);
      filter->SetAttribute(ATT_TILE_SOURCE_RECT, aSourceRegions[0]);
      filter->SetInput(IN_TILE_IN, aSources[0]);
      return filter;
    }

    case FilterPrimitiveDescription::eComponentTransfer:
    {
      RefPtr<FilterNode> filters[4]; // one for each FILTER_*_TRANSFER type
      static const AttributeName componentFunctionNames[4] = {
        eComponentTransferFunctionR,
        eComponentTransferFunctionG,
        eComponentTransferFunctionB,
        eComponentTransferFunctionA
      };
      for (int32_t i = 0; i < 4; i++) {
        AttributeMap functionAttributes =
          atts.GetAttributeMap(componentFunctionNames[i]);
        ConvertComponentTransferFunctionToFilter(functionAttributes, i, aDT,
          filters[0], filters[1], filters[2], filters[3]);
      }

      // Connect all used filters nodes.
      RefPtr<FilterNode> lastFilter = aSources[0];
      for (int32_t i = 0; i < 4; i++) {
        if (filters[i]) {
          filters[i]->SetInput(0, lastFilter);
          lastFilter = filters[i];
        }
      }

      return lastFilter;
    }

    case FilterPrimitiveDescription::eConvolveMatrix:
    {
      RefPtr<FilterNode> filter = aDT->CreateFilter(FILTER_CONVOLVE_MATRIX);
      filter->SetAttribute(ATT_CONVOLVE_MATRIX_KERNEL_SIZE, atts.GetIntSize(eConvolveMatrixKernelSize));
      const nsTArray<float>& matrix = atts.GetFloats(eConvolveMatrixKernelMatrix);
      filter->SetAttribute(ATT_CONVOLVE_MATRIX_KERNEL_MATRIX,
                           matrix.Elements(), matrix.Length());
      filter->SetAttribute(ATT_CONVOLVE_MATRIX_DIVISOR,
                           atts.GetFloat(eConvolveMatrixDivisor));
      filter->SetAttribute(ATT_CONVOLVE_MATRIX_BIAS,
                           atts.GetFloat(eConvolveMatrixBias));
      filter->SetAttribute(ATT_CONVOLVE_MATRIX_TARGET,
                           atts.GetIntPoint(eConvolveMatrixTarget));
      filter->SetAttribute(ATT_CONVOLVE_MATRIX_SOURCE_RECT,
                           aSourceRegions[0]);
      uint32_t edgeMode = atts.GetUint(eConvolveMatrixEdgeMode);
      static const uint8_t edgeModes[SVG_EDGEMODE_NONE+1] = {
        EDGE_MODE_NONE,      // SVG_EDGEMODE_UNKNOWN
        EDGE_MODE_DUPLICATE, // SVG_EDGEMODE_DUPLICATE
        EDGE_MODE_WRAP,      // SVG_EDGEMODE_WRAP
        EDGE_MODE_NONE       // SVG_EDGEMODE_NONE
      };
      filter->SetAttribute(ATT_CONVOLVE_MATRIX_EDGE_MODE, (uint32_t)edgeModes[edgeMode]);
      filter->SetAttribute(ATT_CONVOLVE_MATRIX_KERNEL_UNIT_LENGTH,
                           atts.GetSize(eConvolveMatrixKernelUnitLength));
      filter->SetAttribute(ATT_CONVOLVE_MATRIX_PRESERVE_ALPHA,
                           atts.GetBool(eConvolveMatrixPreserveAlpha));
      filter->SetInput(IN_CONVOLVE_MATRIX_IN, aSources[0]);
      return filter;
    }

    case FilterPrimitiveDescription::eOffset:
    {
      return FilterWrappers::Offset(aDT, aSources[0],
                                    atts.GetIntPoint(eOffsetOffset));
    }

    case FilterPrimitiveDescription::eDisplacementMap:
    {
      RefPtr<FilterNode> filter = aDT->CreateFilter(FILTER_DISPLACEMENT_MAP);
      filter->SetAttribute(ATT_DISPLACEMENT_MAP_SCALE,
                           atts.GetFloat(eDisplacementMapScale));
      static const uint8_t channel[SVG_CHANNEL_A+1] = {
        COLOR_CHANNEL_R, // SVG_CHANNEL_UNKNOWN
        COLOR_CHANNEL_R, // SVG_CHANNEL_R
        COLOR_CHANNEL_G, // SVG_CHANNEL_G
        COLOR_CHANNEL_B, // SVG_CHANNEL_B
        COLOR_CHANNEL_A  // SVG_CHANNEL_A
      };
      filter->SetAttribute(ATT_DISPLACEMENT_MAP_X_CHANNEL,
                           (uint32_t)channel[atts.GetUint(eDisplacementMapXChannel)]);
      filter->SetAttribute(ATT_DISPLACEMENT_MAP_Y_CHANNEL,
                           (uint32_t)channel[atts.GetUint(eDisplacementMapYChannel)]);
      filter->SetInput(IN_DISPLACEMENT_MAP_IN, aSources[0]);
      filter->SetInput(IN_DISPLACEMENT_MAP_IN2, aSources[1]);
      return filter;
    }

    case FilterPrimitiveDescription::eTurbulence:
    {
      RefPtr<FilterNode> filter = aDT->CreateFilter(FILTER_TURBULENCE);
      filter->SetAttribute(ATT_TURBULENCE_BASE_FREQUENCY,
                           atts.GetSize(eTurbulenceBaseFrequency));
      filter->SetAttribute(ATT_TURBULENCE_NUM_OCTAVES,
                           atts.GetUint(eTurbulenceNumOctaves));
      filter->SetAttribute(ATT_TURBULENCE_STITCHABLE,
                           atts.GetBool(eTurbulenceStitchable));
      filter->SetAttribute(ATT_TURBULENCE_SEED,
                           (uint32_t)atts.GetFloat(eTurbulenceSeed));
      static const uint8_t type[SVG_TURBULENCE_TYPE_TURBULENCE+1] = {
        TURBULENCE_TYPE_FRACTAL_NOISE, // SVG_TURBULENCE_TYPE_UNKNOWN
        TURBULENCE_TYPE_FRACTAL_NOISE, // SVG_TURBULENCE_TYPE_FRACTALNOISE
        TURBULENCE_TYPE_TURBULENCE     // SVG_TURBULENCE_TYPE_TURBULENCE
      };
      filter->SetAttribute(ATT_TURBULENCE_TYPE,
                           (uint32_t)type[atts.GetUint(eTurbulenceType)]);
      filter->SetAttribute(ATT_TURBULENCE_RECT,
                           aDescription.PrimitiveSubregion() - atts.GetIntPoint(eTurbulenceOffset));
      return FilterWrappers::Offset(aDT, filter, atts.GetIntPoint(eTurbulenceOffset));
    }

    case FilterPrimitiveDescription::eComposite:
    {
      RefPtr<FilterNode> filter;
      uint32_t op = atts.GetUint(eCompositeOperator);
      if (op == SVG_FECOMPOSITE_OPERATOR_ARITHMETIC) {
        filter = aDT->CreateFilter(FILTER_ARITHMETIC_COMBINE);
        const nsTArray<float>& coefficients = atts.GetFloats(eCompositeCoefficients);
        filter->SetAttribute(ATT_ARITHMETIC_COMBINE_COEFFICIENTS,
                             coefficients.Elements(), coefficients.Length());
        filter->SetInput(IN_ARITHMETIC_COMBINE_IN, aSources[0]);
        filter->SetInput(IN_ARITHMETIC_COMBINE_IN2, aSources[1]);
      } else {
        filter = aDT->CreateFilter(FILTER_COMPOSITE);
        static const uint8_t operators[SVG_FECOMPOSITE_OPERATOR_ARITHMETIC] = {
          COMPOSITE_OPERATOR_OVER, // SVG_FECOMPOSITE_OPERATOR_UNKNOWN
          COMPOSITE_OPERATOR_OVER, // SVG_FECOMPOSITE_OPERATOR_OVER
          COMPOSITE_OPERATOR_IN,   // SVG_FECOMPOSITE_OPERATOR_IN
          COMPOSITE_OPERATOR_OUT,  // SVG_FECOMPOSITE_OPERATOR_OUT
          COMPOSITE_OPERATOR_ATOP, // SVG_FECOMPOSITE_OPERATOR_ATOP
          COMPOSITE_OPERATOR_XOR   // SVG_FECOMPOSITE_OPERATOR_XOR
        };
        filter->SetAttribute(ATT_COMPOSITE_OPERATOR, (uint32_t)operators[op]);
        filter->SetInput(IN_COMPOSITE_IN_START, aSources[1]);
        filter->SetInput(IN_COMPOSITE_IN_START + 1, aSources[0]);
      }
      return filter;
    }

    case FilterPrimitiveDescription::eMerge:
    {
      if (aSources.Length() == 0) {
        return nullptr;
      }
      if (aSources.Length() == 1) {
        return aSources[0];
      }
      RefPtr<FilterNode> filter = aDT->CreateFilter(FILTER_COMPOSITE);
      filter->SetAttribute(ATT_COMPOSITE_OPERATOR, (uint32_t)COMPOSITE_OPERATOR_OVER);
      for (size_t i = 0; i < aSources.Length(); i++) {
        filter->SetInput(IN_COMPOSITE_IN_START + i, aSources[i]);
      }
      return filter;
    }

    case FilterPrimitiveDescription::eGaussianBlur:
    {
      Size stdDeviation = atts.GetSize(eGaussianBlurStdDeviation);
      if (stdDeviation.width == stdDeviation.height) {
        RefPtr<FilterNode> filter = aDT->CreateFilter(FILTER_GAUSSIAN_BLUR);
        filter->SetAttribute(ATT_GAUSSIAN_BLUR_STD_DEVIATION, float(stdDeviation.width));
        filter->SetInput(IN_GAUSSIAN_BLUR_IN, aSources[0]);
        return filter;
      }
      RefPtr<FilterNode> filterH = aDT->CreateFilter(FILTER_DIRECTIONAL_BLUR);
      RefPtr<FilterNode> filterV = aDT->CreateFilter(FILTER_DIRECTIONAL_BLUR);
      filterH->SetAttribute(ATT_DIRECTIONAL_BLUR_DIRECTION, (uint32_t)BLUR_DIRECTION_X);
      filterH->SetAttribute(ATT_DIRECTIONAL_BLUR_STD_DEVIATION, float(stdDeviation.width));
      filterV->SetAttribute(ATT_DIRECTIONAL_BLUR_DIRECTION, (uint32_t)BLUR_DIRECTION_Y);
      filterV->SetAttribute(ATT_DIRECTIONAL_BLUR_STD_DEVIATION, float(stdDeviation.height));
      filterH->SetInput(IN_DIRECTIONAL_BLUR_IN, aSources[0]);
      filterV->SetInput(IN_DIRECTIONAL_BLUR_IN, filterH);
      return filterV;
    }

    case FilterPrimitiveDescription::eDiffuseLighting:
    case FilterPrimitiveDescription::eSpecularLighting:
    {
      bool isSpecular =
        aDescription.Type() == FilterPrimitiveDescription::eSpecularLighting;

      AttributeMap lightAttributes = atts.GetAttributeMap(eLightingLight);

      if (lightAttributes.GetUint(eLightType) == eLightTypeNone) {
        return nullptr;
      }

      enum { POINT = 0, SPOT, DISTANT } lightType = POINT;

      switch (lightAttributes.GetUint(eLightType)) {
        case eLightTypePoint:   lightType = POINT;   break;
        case eLightTypeSpot:    lightType = SPOT;    break;
        case eLightTypeDistant: lightType = DISTANT; break;
      }

      static const FilterType filterType[2][DISTANT+1] = {
        { FILTER_POINT_DIFFUSE, FILTER_SPOT_DIFFUSE, FILTER_DISTANT_DIFFUSE },
        { FILTER_POINT_SPECULAR, FILTER_SPOT_SPECULAR, FILTER_DISTANT_SPECULAR }
      };
      RefPtr<FilterNode> filter =
        aDT->CreateFilter(filterType[isSpecular][lightType]);

      filter->SetAttribute(ATT_LIGHTING_COLOR,
                           atts.GetColor(eLightingColor));
      filter->SetAttribute(ATT_LIGHTING_SURFACE_SCALE,
                           atts.GetFloat(eLightingSurfaceScale));
      filter->SetAttribute(ATT_LIGHTING_KERNEL_UNIT_LENGTH,
                           atts.GetSize(eLightingKernelUnitLength));

      if (isSpecular) {
        filter->SetAttribute(ATT_SPECULAR_LIGHTING_SPECULAR_CONSTANT,
                             atts.GetFloat(eSpecularLightingSpecularConstant));
        filter->SetAttribute(ATT_SPECULAR_LIGHTING_SPECULAR_EXPONENT,
                             atts.GetFloat(eSpecularLightingSpecularExponent));
      } else {
        filter->SetAttribute(ATT_DIFFUSE_LIGHTING_DIFFUSE_CONSTANT,
                             atts.GetFloat(eDiffuseLightingDiffuseConstant));
      }

      switch (lightType) {
        case POINT:
          filter->SetAttribute(ATT_POINT_LIGHT_POSITION,
                               lightAttributes.GetPoint3D(ePointLightPosition));
          break;
        case SPOT:
          filter->SetAttribute(ATT_SPOT_LIGHT_POSITION,
                               lightAttributes.GetPoint3D(eSpotLightPosition));
          filter->SetAttribute(ATT_SPOT_LIGHT_POINTS_AT,
                               lightAttributes.GetPoint3D(eSpotLightPointsAt));
          filter->SetAttribute(ATT_SPOT_LIGHT_FOCUS,
                               lightAttributes.GetFloat(eSpotLightFocus));
          filter->SetAttribute(ATT_SPOT_LIGHT_LIMITING_CONE_ANGLE,
                               lightAttributes.GetFloat(eSpotLightLimitingConeAngle));
          break;
        case DISTANT:
          filter->SetAttribute(ATT_DISTANT_LIGHT_AZIMUTH,
                               lightAttributes.GetFloat(eDistantLightAzimuth));
          filter->SetAttribute(ATT_DISTANT_LIGHT_ELEVATION,
                               lightAttributes.GetFloat(eDistantLightElevation));
          break;
      }

      filter->SetInput(IN_LIGHTING_IN, aSources[0]);

      return filter;
    }

    case FilterPrimitiveDescription::eImage:
    {
      Matrix TM = atts.GetMatrix(eImageTransform);
      if (!TM.Determinant()) {
        return nullptr;
      }

      // Pull the image from the additional image list using the index that's
      // stored in the primitive description.
      nsRefPtr<gfxASurface> inputImage =
        aInputImages[atts.GetUint(eImageInputIndex)];
      RefPtr<SourceSurface> inputSurface =
        gfxPlatform::GetPlatform()->GetSourceSurfaceForSurface(aDT, inputImage);

      RefPtr<FilterNode> transform = aDT->CreateFilter(FILTER_TRANSFORM);
      transform->SetInput(IN_TRANSFORM_IN, inputSurface);
      transform->SetAttribute(ATT_TRANSFORM_MATRIX, TM);
      transform->SetAttribute(ATT_TRANSFORM_FILTER, atts.GetUint(eImageFilter));
      return transform;
    }

    default:
      return nullptr;
  }
}

template<typename T>
static const T&
ElementForIndex(int32_t aIndex,
                const nsTArray<T>& aPrimitiveElements,
                const T& aSourceGraphicElement,
                const T& aFillPaintElement,
                const T& aStrokePaintElement)
{
  switch (aIndex) {
    case FilterPrimitiveDescription::kPrimitiveIndexSourceGraphic:
    case FilterPrimitiveDescription::kPrimitiveIndexSourceAlpha:
      return aSourceGraphicElement;
    case FilterPrimitiveDescription::kPrimitiveIndexFillPaint:
      return aFillPaintElement;
    case FilterPrimitiveDescription::kPrimitiveIndexStrokePaint:
      return aStrokePaintElement;
    default:
      MOZ_ASSERT(aIndex >= 0, "bad index");
      return aPrimitiveElements[aIndex];
  }
}

static AlphaModel
InputAlphaModelForPrimitive(const FilterPrimitiveDescription& aDescr,
                            int32_t aInputIndex,
                            AlphaModel aOriginalAlphaModel)
{
  switch (aDescr.Type()) {
    case FilterPrimitiveDescription::eTile:
    case FilterPrimitiveDescription::eOffset:
      return aOriginalAlphaModel;

    case FilterPrimitiveDescription::eColorMatrix:
    case FilterPrimitiveDescription::eComponentTransfer:
      return UNPREMULTIPLIED;

    case FilterPrimitiveDescription::eDisplacementMap:
      return aInputIndex == 0 ? PREMULTIPLIED : UNPREMULTIPLIED;

    case FilterPrimitiveDescription::eConvolveMatrix:
      return aDescr.Attributes().GetBool(eConvolveMatrixPreserveAlpha) ?
        UNPREMULTIPLIED : PREMULTIPLIED;

    default:
      return PREMULTIPLIED;
  }
}

static AlphaModel
OutputAlphaModelForPrimitive(const FilterPrimitiveDescription& aDescr,
                             const nsTArray<AlphaModel>& aInputAlphaModels)
{
  if (aInputAlphaModels.Length()) {
    // For filters with inputs, the output is premultiplied if and only if the
    // first input is premultiplied.
    return InputAlphaModelForPrimitive(aDescr, 0, aInputAlphaModels[0]);
  }

  // All filters without inputs produce premultiplied alpha.
  return PREMULTIPLIED;
}

// Returns the output FilterNode, in premultiplied sRGB space.
static TemporaryRef<FilterNode>
FilterNodeGraphFromDescription(DrawTarget* aDT,
                               const FilterDescription& aFilter,
                               const Rect& aResultNeededRect,
                               SourceSurface* aSourceGraphic,
                               const IntRect& aSourceGraphicRect,
                               SourceSurface* aFillPaint,
                               const IntRect& aFillPaintRect,
                               SourceSurface* aStrokePaint,
                               const IntRect& aStrokePaintRect,
                               nsTArray<nsRefPtr<gfxASurface> >& aAdditionalImages)
{
  const nsTArray<FilterPrimitiveDescription>& primitives = aFilter.mPrimitives;
  const IntRect& filterSpaceBounds = aFilter.mFilterSpaceBounds;

  Rect resultNeededRect(aResultNeededRect);
  resultNeededRect.RoundOut();

  RefPtr<FilterCachedColorModels> sourceFilters[4];
  nsTArray<RefPtr<FilterCachedColorModels> > primitiveFilters;

  for (size_t i = 0; i < primitives.Length(); ++i) {
    const FilterPrimitiveDescription& descr = primitives[i];

    nsTArray<RefPtr<FilterNode> > inputFilterNodes;
    nsTArray<IntRect> inputSourceRects;
    nsTArray<AlphaModel> inputAlphaModels;

    for (size_t j = 0; j < descr.NumberOfInputs(); j++) {

      int32_t inputIndex = descr.InputPrimitiveIndex(j);
      if (inputIndex < 0) {
        inputSourceRects.AppendElement(filterSpaceBounds);
      } else {
        inputSourceRects.AppendElement(filterSpaceBounds.Intersect(
          primitives[inputIndex].PrimitiveSubregion()));
      }

      RefPtr<FilterCachedColorModels> inputFilter;
      if (inputIndex >= 0) {
        MOZ_ASSERT(inputIndex < (int64_t)primitiveFilters.Length(), "out-of-bounds input index!");
        inputFilter = primitiveFilters[inputIndex];
        MOZ_ASSERT(inputFilter, "Referred to input filter that comes after the current one?");
      } else {
        int32_t sourceIndex = -inputIndex - 1;
        MOZ_ASSERT(sourceIndex >= 0, "invalid source index");
        MOZ_ASSERT(sourceIndex < 4, "invalid source index");
        inputFilter = sourceFilters[sourceIndex];
        if (!inputFilter) {
          RefPtr<FilterNode> sourceFilterNode;

          nsTArray<SourceSurface*> primitiveSurfaces;
          nsTArray<IntRect> primitiveSurfaceRects;
          RefPtr<SourceSurface> surf =
            ElementForIndex(inputIndex, primitiveSurfaces,
                            aSourceGraphic, aFillPaint, aStrokePaint);
          IntRect surfaceRect =
            ElementForIndex(inputIndex, primitiveSurfaceRects,
                            aSourceGraphicRect, aFillPaintRect, aStrokePaintRect);
          if (surf) {
            IntPoint offset = surfaceRect.TopLeft();
            sourceFilterNode = FilterWrappers::ForSurface(aDT, surf, offset);

            if (inputIndex == FilterPrimitiveDescription::kPrimitiveIndexSourceAlpha) {
              sourceFilterNode = FilterWrappers::ToAlpha(aDT, sourceFilterNode);
            }
          }

          inputFilter = new FilterCachedColorModels(aDT, sourceFilterNode,
                                                    ColorModel::PremulSRGB());
          sourceFilters[sourceIndex] = inputFilter;
        }
      }
      MOZ_ASSERT(inputFilter);

      AlphaModel inputAlphaModel =
        InputAlphaModelForPrimitive(descr, j, inputFilter->OriginalAlphaModel());
      inputAlphaModels.AppendElement(inputAlphaModel);
      ColorModel inputColorModel(descr.InputColorSpace(j), inputAlphaModel);
      inputFilterNodes.AppendElement(inputFilter->ForColorModel(inputColorModel));
    }

    RefPtr<FilterNode> primitiveFilterNode =
      FilterNodeFromPrimitiveDescription(descr, aDT, inputFilterNodes,
                                         inputSourceRects, aAdditionalImages);

    if (primitiveFilterNode) {
      IntRect cropRect = filterSpaceBounds.Intersect(descr.PrimitiveSubregion());
      primitiveFilterNode = FilterWrappers::Crop(aDT, primitiveFilterNode, cropRect);
    }

    ColorModel outputColorModel(descr.OutputColorSpace(),
      OutputAlphaModelForPrimitive(descr, inputAlphaModels));
    RefPtr<FilterCachedColorModels> primitiveFilter =
      new FilterCachedColorModels(aDT, primitiveFilterNode, outputColorModel);

    primitiveFilters.AppendElement(primitiveFilter);
  }

  return primitiveFilters.LastElement()->ForColorModel(ColorModel::PremulSRGB());
}

// FilterSupport

void
FilterSupport::RenderFilterDescription(DrawTarget* aDT,
                                       const FilterDescription& aFilter,
                                       const Rect& aRenderRect,
                                       SourceSurface* aSourceGraphic,
                                       const IntRect& aSourceGraphicRect,
                                       SourceSurface* aFillPaint,
                                       const IntRect& aFillPaintRect,
                                       SourceSurface* aStrokePaint,
                                       const IntRect& aStrokePaintRect,
                                       nsTArray<nsRefPtr<gfxASurface> >& aAdditionalImages)
{
  RefPtr<FilterNode> resultFilter =
    FilterNodeGraphFromDescription(aDT, aFilter, aRenderRect,
                                   aSourceGraphic, aSourceGraphicRect, aFillPaint, aFillPaintRect,
                                   aStrokePaint, aStrokePaintRect, aAdditionalImages);

  aDT->DrawFilter(resultFilter, aRenderRect, Point(0, 0));
}

static nsIntRegion
UnionOfRegions(const nsTArray<nsIntRegion>& aRegions)
{
  nsIntRegion result;
  for (size_t i = 0; i < aRegions.Length(); i++) {
    result.Or(result, aRegions[i]);
  }
  return result;
}

static int32_t
InflateSizeForBlurStdDev(double aStdDev)
{
  double size = aStdDev * (3 * sqrt(2 * M_PI) / 4) * 1.5;
  static const double max = 1024;
  size = std::min(size, max);
  return uint32_t(floor(size + 0.5));
}

static nsIntRegion
ResultChangeRegionForPrimitive(const FilterPrimitiveDescription& aDescription,
                               const nsTArray<nsIntRegion>& aInputChangeRegions)
{
  const AttributeMap& atts = aDescription.Attributes();
  switch (aDescription.Type()) {

    case FilterPrimitiveDescription::eNone:
    case FilterPrimitiveDescription::eFlood:
    case FilterPrimitiveDescription::eTurbulence:
    case FilterPrimitiveDescription::eImage:
      return nsIntRegion();

    case FilterPrimitiveDescription::eBlend:
    case FilterPrimitiveDescription::eComposite:
    case FilterPrimitiveDescription::eMerge:
      return UnionOfRegions(aInputChangeRegions);

    case FilterPrimitiveDescription::eColorMatrix:
    case FilterPrimitiveDescription::eComponentTransfer:
      return aInputChangeRegions[0];

    case FilterPrimitiveDescription::eMorphology:
    {
      Size radii = atts.GetSize(eMorphologyRadii);
      int32_t rx = clamped(int32_t(ceil(radii.width)), 0, kMorphologyMaxRadius);
      int32_t ry = clamped(int32_t(ceil(radii.height)), 0, kMorphologyMaxRadius);
      return aInputChangeRegions[0].Inflated(nsIntMargin(ry, rx, ry, rx));
    }

    case FilterPrimitiveDescription::eTile:
      return ThebesIntRect(aDescription.PrimitiveSubregion());

    case FilterPrimitiveDescription::eConvolveMatrix:
    {
      Size kernelUnitLength = atts.GetSize(eConvolveMatrixKernelUnitLength);
      IntSize kernelSize = atts.GetIntSize(eConvolveMatrixKernelSize);
      IntPoint target = atts.GetIntPoint(eConvolveMatrixTarget);
      nsIntMargin m(ceil(kernelUnitLength.width * (target.x)),
                    ceil(kernelUnitLength.height * (target.y)),
                    ceil(kernelUnitLength.width * (kernelSize.width - target.x - 1)),
                    ceil(kernelUnitLength.height * (kernelSize.height - target.y - 1)));
      return aInputChangeRegions[0].Inflated(m);
    }

    case FilterPrimitiveDescription::eOffset:
    {
      IntPoint offset = atts.GetIntPoint(eOffsetOffset);
      return aInputChangeRegions[0].MovedBy(offset.x, offset.y);
    }

    case FilterPrimitiveDescription::eDisplacementMap:
    {
      int32_t scale = ceil(abs(atts.GetFloat(eDisplacementMapScale)));
      return aInputChangeRegions[0].Inflated(nsIntMargin(scale, scale, scale, scale));
    }

    case FilterPrimitiveDescription::eGaussianBlur:
    {
      Size stdDeviation = atts.GetSize(eGaussianBlurStdDeviation);
      int32_t dx = InflateSizeForBlurStdDev(stdDeviation.width);
      int32_t dy = InflateSizeForBlurStdDev(stdDeviation.height);
      return aInputChangeRegions[0].Inflated(nsIntMargin(dy, dx, dy, dx));
    }

    case FilterPrimitiveDescription::eDiffuseLighting:
    case FilterPrimitiveDescription::eSpecularLighting:
    {
      Size kernelUnitLength = atts.GetSize(eLightingKernelUnitLength);
      int32_t dx = ceil(kernelUnitLength.width);
      int32_t dy = ceil(kernelUnitLength.height);
      return aInputChangeRegions[0].Inflated(nsIntMargin(dy, dx, dy, dx));
    }

    default:
      return nsIntRegion();
  }
}

/* static */ nsIntRegion
FilterSupport::ComputeResultChangeRegion(const FilterDescription& aFilter,
                                         const nsIntRegion& aSourceGraphicChange,
                                         const nsIntRegion& aFillPaintChange,
                                         const nsIntRegion& aStrokePaintChange)
{
  const nsTArray<FilterPrimitiveDescription>& primitives = aFilter.mPrimitives;
  nsTArray<nsIntRegion> resultChangeRegions;

  for (int32_t i = 0; i < int32_t(primitives.Length()); ++i) {
    const FilterPrimitiveDescription& descr = primitives[i];

    nsTArray<nsIntRegion> inputChangeRegions;
    for (size_t j = 0; j < descr.NumberOfInputs(); j++) {
      int32_t inputIndex = descr.InputPrimitiveIndex(j);
      MOZ_ASSERT(inputIndex < i, "bad input index");
      nsIntRegion inputChangeRegion =
        ElementForIndex(inputIndex, resultChangeRegions,
                        aSourceGraphicChange, aFillPaintChange,
                        aStrokePaintChange);
      inputChangeRegions.AppendElement(inputChangeRegion);
    }
    nsIntRegion changeRegion =
      ResultChangeRegionForPrimitive(descr, inputChangeRegions);
    IntRect cropRect =
      descr.PrimitiveSubregion().Intersect(aFilter.mFilterSpaceBounds);
    changeRegion.And(changeRegion, ThebesIntRect(cropRect));
    resultChangeRegions.AppendElement(changeRegion);
  }

  return resultChangeRegions[resultChangeRegions.Length() - 1];
}

static nsIntRegion
PostFilterExtentsForPrimitive(const FilterPrimitiveDescription& aDescription,
                              const nsTArray<nsIntRegion>& aInputExtents)
{
  const AttributeMap& atts = aDescription.Attributes();
  switch (aDescription.Type()) {

    case FilterPrimitiveDescription::eNone:
      return nsIntRect();

    case FilterPrimitiveDescription::eComposite:
    {
      uint32_t op = atts.GetUint(eCompositeOperator);
      if (op == SVG_FECOMPOSITE_OPERATOR_ARITHMETIC) {
        const nsTArray<float>& coefficients = atts.GetFloats(eCompositeCoefficients);
        MOZ_ASSERT(coefficients.Length() == 4);
        if (coefficients[3] > 0.0f) {
          // The arithmetic composite primitive can draw outside the bounding
          // box of its source images.
          return ThebesIntRect(aDescription.PrimitiveSubregion());
        }
      }
      return ResultChangeRegionForPrimitive(aDescription, aInputExtents);
    }

    case FilterPrimitiveDescription::eFlood:
    case FilterPrimitiveDescription::eTurbulence:
    case FilterPrimitiveDescription::eImage:
    {
      return ThebesIntRect(aDescription.PrimitiveSubregion());
    }

    case FilterPrimitiveDescription::eMorphology:
    {
      uint32_t op = atts.GetUint(eMorphologyOperator);
      if (op == SVG_OPERATOR_ERODE) {
        return aInputExtents[0];
      }
      Size radii = atts.GetSize(eMorphologyRadii);
      int32_t rx = clamped(int32_t(ceil(radii.width)), 0, kMorphologyMaxRadius);
      int32_t ry = clamped(int32_t(ceil(radii.height)), 0, kMorphologyMaxRadius);
      return aInputExtents[0].Inflated(nsIntMargin(ry, rx, ry, rx));
    }

    default:
      return ResultChangeRegionForPrimitive(aDescription, aInputExtents);
  }
}

/* static */ nsIntRegion
FilterSupport::ComputePostFilterExtents(const FilterDescription& aFilter,
                                        const nsIntRegion& aSourceGraphicExtents)
{
  const nsTArray<FilterPrimitiveDescription>& primitives = aFilter.mPrimitives;
  nsIntRegion filterSpace = ThebesIntRect(aFilter.mFilterSpaceBounds);
  nsTArray<nsIntRegion> postFilterExtents;

  for (int32_t i = 0; i < int32_t(primitives.Length()); ++i) {
    const FilterPrimitiveDescription& descr = primitives[i];

    nsTArray<nsIntRegion> inputExtents;
    for (size_t j = 0; j < descr.NumberOfInputs(); j++) {
      int32_t inputIndex = descr.InputPrimitiveIndex(j);
      MOZ_ASSERT(inputIndex < i, "bad input index");
      nsIntRegion inputExtent =
        ElementForIndex(inputIndex, postFilterExtents,
                        aSourceGraphicExtents, filterSpace, filterSpace);
      inputExtents.AppendElement(inputExtent);
    }
    nsIntRegion extent = PostFilterExtentsForPrimitive(descr, inputExtents);
    IntRect cropRect =
      descr.PrimitiveSubregion().Intersect(aFilter.mFilterSpaceBounds);
    extent.And(extent, ThebesIntRect(cropRect));
    postFilterExtents.AppendElement(extent);
  }

  return postFilterExtents[postFilterExtents.Length() - 1];
}

static nsIntRegion
SourceNeededRegionForPrimitive(const FilterPrimitiveDescription& aDescription,
                               const nsIntRegion& aResultNeededRegion,
                               int32_t aInputIndex)
{
  const AttributeMap& atts = aDescription.Attributes();
  switch (aDescription.Type()) {

    case FilterPrimitiveDescription::eNone:
    case FilterPrimitiveDescription::eFlood:
    case FilterPrimitiveDescription::eTurbulence:
    case FilterPrimitiveDescription::eImage:
      MOZ_CRASH("this shouldn't be called for filters without inputs");
      return nsIntRegion();

    case FilterPrimitiveDescription::eBlend:
    case FilterPrimitiveDescription::eComposite:
    case FilterPrimitiveDescription::eMerge:
    case FilterPrimitiveDescription::eColorMatrix:
    case FilterPrimitiveDescription::eComponentTransfer:
      return aResultNeededRegion;

    case FilterPrimitiveDescription::eMorphology:
    {
      Size radii = atts.GetSize(eMorphologyRadii);
      int32_t rx = clamped(int32_t(ceil(radii.width)), 0, kMorphologyMaxRadius);
      int32_t ry = clamped(int32_t(ceil(radii.height)), 0, kMorphologyMaxRadius);
      return aResultNeededRegion.Inflated(nsIntMargin(ry, rx, ry, rx));
    }

    case FilterPrimitiveDescription::eTile:
      return nsIntRect(INT32_MIN/2, INT32_MIN/2, INT32_MAX, INT32_MAX);

    case FilterPrimitiveDescription::eConvolveMatrix:
    {
      Size kernelUnitLength = atts.GetSize(eConvolveMatrixKernelUnitLength);
      IntSize kernelSize = atts.GetIntSize(eConvolveMatrixKernelSize);
      IntPoint target = atts.GetIntPoint(eConvolveMatrixTarget);
      nsIntMargin m(ceil(kernelUnitLength.width * (kernelSize.width - target.x - 1)),
                    ceil(kernelUnitLength.height * (kernelSize.height - target.y - 1)),
                    ceil(kernelUnitLength.width * (target.x)),
                    ceil(kernelUnitLength.height * (target.y)));
      return aResultNeededRegion.Inflated(m);
    }

    case FilterPrimitiveDescription::eOffset:
    {
      IntPoint offset = atts.GetIntPoint(eOffsetOffset);
      return aResultNeededRegion.MovedBy(-nsIntPoint(offset.x, offset.y));
    }

    case FilterPrimitiveDescription::eDisplacementMap:
    {
      if (aInputIndex == 1) {
        return aResultNeededRegion;
      }
      int32_t scale = ceil(abs(atts.GetFloat(eDisplacementMapScale)));
      return aResultNeededRegion.Inflated(nsIntMargin(scale, scale, scale, scale));
    }

    case FilterPrimitiveDescription::eGaussianBlur:
    {
      Size stdDeviation = atts.GetSize(eGaussianBlurStdDeviation);
      int32_t dx = InflateSizeForBlurStdDev(stdDeviation.width);
      int32_t dy = InflateSizeForBlurStdDev(stdDeviation.height);
      return aResultNeededRegion.Inflated(nsIntMargin(dy, dx, dy, dx));
    }

    case FilterPrimitiveDescription::eDiffuseLighting:
    case FilterPrimitiveDescription::eSpecularLighting:
    {
      Size kernelUnitLength = atts.GetSize(eLightingKernelUnitLength);
      int32_t dx = ceil(kernelUnitLength.width);
      int32_t dy = ceil(kernelUnitLength.height);
      return aResultNeededRegion.Inflated(nsIntMargin(dy, dx, dy, dx));
    }

    default:
      return nsIntRegion();
  }

}

/* static */ void
FilterSupport::ComputeSourceNeededRegions(const FilterDescription& aFilter,
                                          const nsIntRegion& aResultNeededRegion,
                                          nsIntRegion& aSourceGraphicNeededRegion,
                                          nsIntRegion& aFillPaintNeededRegion,
                                          nsIntRegion& aStrokePaintNeededRegion)
{
  const nsTArray<FilterPrimitiveDescription>& primitives = aFilter.mPrimitives;
  nsTArray<nsIntRegion> primitiveNeededRegions;
  primitiveNeededRegions.AppendElements(primitives.Length());

  primitiveNeededRegions[primitives.Length() - 1] = aResultNeededRegion;

  for (int32_t i = primitives.Length() - 1; i >= 0; --i) {
    const FilterPrimitiveDescription& descr = primitives[i];
    nsIntRegion neededRegion = primitiveNeededRegions[i];
    neededRegion.And(neededRegion, ThebesIntRect(descr.PrimitiveSubregion()));

    for (size_t j = 0; j < descr.NumberOfInputs(); j++) {
      int32_t inputIndex = descr.InputPrimitiveIndex(j);
      MOZ_ASSERT(inputIndex < i, "bad input index");
      nsIntRegion* inputNeededRegion = const_cast<nsIntRegion*>(
        &ElementForIndex(inputIndex, primitiveNeededRegions,
                         aSourceGraphicNeededRegion,
                         aFillPaintNeededRegion, aStrokePaintNeededRegion));
      inputNeededRegion->Or(*inputNeededRegion,
        SourceNeededRegionForPrimitive(descr, neededRegion, j));
    }
  }

  aSourceGraphicNeededRegion.And(aSourceGraphicNeededRegion,
                                 ThebesIntRect(aFilter.mFilterSpaceBounds));
}

// FilterPrimitiveDescription

FilterPrimitiveDescription::FilterPrimitiveDescription(PrimitiveType aType)
 : mType(aType)
{
}

FilterPrimitiveDescription::FilterPrimitiveDescription(const FilterPrimitiveDescription& aOther)
 : mType(aOther.mType)
 , mAttributes(aOther.mAttributes)
 , mInputPrimitives(aOther.mInputPrimitives)
 , mFilterPrimitiveSubregion(aOther.mFilterPrimitiveSubregion)
 , mInputColorSpaces(aOther.mInputColorSpaces)
 , mOutputColorSpace(aOther.mOutputColorSpace)
{
}

FilterPrimitiveDescription&
FilterPrimitiveDescription::operator=(const FilterPrimitiveDescription& aOther)
{
  if (this != &aOther) {
    mType = aOther.mType;
    mAttributes = aOther.mAttributes;
    mInputPrimitives = aOther.mInputPrimitives;
    mFilterPrimitiveSubregion = aOther.mFilterPrimitiveSubregion;
    mInputColorSpaces = aOther.mInputColorSpaces;
    mOutputColorSpace = aOther.mOutputColorSpace;
  }
  return *this;
}

// AttributeMap

// A class that wraps different types for easy storage in a hashtable. Only
// used by AttributeMap.
struct FilterAttribute {
  FilterAttribute() : mType(eNone) {}
  FilterAttribute(const FilterAttribute& aOther);
  ~FilterAttribute();

#define MAKE_CONSTRUCTOR_AND_ACCESSOR_BASIC(type, typeLabel)   \
  FilterAttribute(type aValue)                                 \
   : mType(e##typeLabel), m##typeLabel(aValue)                 \
  {}                                                           \
  type As##typeLabel() {                                       \
    MOZ_ASSERT(mType == e##typeLabel);                         \
    return m##typeLabel;                                       \
  }

#define MAKE_CONSTRUCTOR_AND_ACCESSOR_CLASS(className)         \
  FilterAttribute(const className& aValue)                     \
   : mType(e##className), m##className(new className(aValue))  \
  {}                                                           \
  className As##className() {                                  \
    MOZ_ASSERT(mType == e##className);                         \
    return *m##className;                                      \
  }

  MAKE_CONSTRUCTOR_AND_ACCESSOR_BASIC(bool, Bool)
  MAKE_CONSTRUCTOR_AND_ACCESSOR_BASIC(uint32_t, Uint)
  MAKE_CONSTRUCTOR_AND_ACCESSOR_BASIC(float, Float)
  MAKE_CONSTRUCTOR_AND_ACCESSOR_CLASS(Size)
  MAKE_CONSTRUCTOR_AND_ACCESSOR_CLASS(IntSize)
  MAKE_CONSTRUCTOR_AND_ACCESSOR_CLASS(IntPoint)
  MAKE_CONSTRUCTOR_AND_ACCESSOR_CLASS(Matrix)
  MAKE_CONSTRUCTOR_AND_ACCESSOR_CLASS(Matrix5x4)
  MAKE_CONSTRUCTOR_AND_ACCESSOR_CLASS(Point3D)
  MAKE_CONSTRUCTOR_AND_ACCESSOR_CLASS(Color)
  MAKE_CONSTRUCTOR_AND_ACCESSOR_CLASS(AttributeMap)

#undef MAKE_CONSTRUCTOR_AND_ACCESSOR_BASIC
#undef MAKE_CONSTRUCTOR_AND_ACCESSOR_CLASS

  FilterAttribute(const float* aValue, uint32_t aLength)
   : mType(eFloats)
  {
    mFloats = new nsTArray<float>();
    mFloats->AppendElements(aValue, aLength);
  }

  const nsTArray<float>& AsFloats() const {
    MOZ_ASSERT(mType == eFloats);
    return *mFloats;
  }

  enum AttributeType {
    eNone = 0,
    eBool,
    eUint,
    eFloat,
    eSize,
    eIntSize,
    eIntPoint,
    eMatrix,
    eMatrix5x4,
    ePoint3D,
    eColor,
    eAttributeMap,
    eFloats
  };

private:
  const AttributeType mType;

  union {
    bool mBool;
    uint32_t mUint;
    float mFloat;
    Size* mSize;
    IntSize* mIntSize;
    IntPoint* mIntPoint;
    Matrix* mMatrix;
    Matrix5x4* mMatrix5x4;
    Point3D* mPoint3D;
    Color* mColor;
    AttributeMap* mAttributeMap;
    nsTArray<float>* mFloats;
  };
};

FilterAttribute::FilterAttribute(const FilterAttribute& aOther)
 : mType(aOther.mType)
{
  switch (mType) {
    case eBool:
      mBool = aOther.mBool;
      break;
    case eUint:
      mUint = aOther.mUint;
      break;
    case eFloat:
      mFloat = aOther.mFloat;
      break;

#define HANDLE_CLASS(className)                            \
    case e##className:                                     \
      m##className = new className(*aOther.m##className);  \
      break;

    HANDLE_CLASS(Size)
    HANDLE_CLASS(IntSize)
    HANDLE_CLASS(IntPoint)
    HANDLE_CLASS(Matrix)
    HANDLE_CLASS(Matrix5x4)
    HANDLE_CLASS(Point3D)
    HANDLE_CLASS(Color)
    HANDLE_CLASS(AttributeMap)

#undef HANDLE_CLASS

    case eFloats:
      mFloats = new nsTArray<float>(*aOther.mFloats);
      break;
    case eNone:
      break;
  }
}

FilterAttribute::~FilterAttribute() {
  switch (mType) {
    case eNone:
    case eBool:
    case eUint:
    case eFloat:
      break;

#define HANDLE_CLASS(className)                            \
    case e##className:                                     \
      delete m##className;                                 \
      break;

    HANDLE_CLASS(Size)
    HANDLE_CLASS(IntSize)
    HANDLE_CLASS(IntPoint)
    HANDLE_CLASS(Matrix)
    HANDLE_CLASS(Matrix5x4)
    HANDLE_CLASS(Point3D)
    HANDLE_CLASS(Color)
    HANDLE_CLASS(AttributeMap)

#undef HANDLE_CLASS

    case eFloats:
      delete mFloats;
      break;
  }
}

typedef FilterAttribute Attribute;

AttributeMap::AttributeMap()
{
}

AttributeMap::~AttributeMap()
{
}

static PLDHashOperator
CopyAttribute(const uint32_t& aAttributeName,
              Attribute* aAttribute,
              void* aAttributes)
{
  typedef nsClassHashtable<nsUint32HashKey, Attribute> Map;
  Map* map = static_cast<Map*>(aAttributes);
  map->Put(aAttributeName, new Attribute(*aAttribute));
  return PL_DHASH_NEXT;
}

AttributeMap::AttributeMap(const AttributeMap& aOther)
{
  aOther.mMap.EnumerateRead(CopyAttribute, &mMap);
}

AttributeMap&
AttributeMap::operator=(const AttributeMap& aOther)
{
  if (this != &aOther) {
    mMap.Clear();
    aOther.mMap.EnumerateRead(CopyAttribute, &mMap);
  }
  return *this;
}

#define MAKE_ATTRIBUTE_HANDLERS_BASIC(type, typeLabel, defaultValue) \
  type                                                               \
  AttributeMap::Get##typeLabel(AttributeName aName) const {          \
    Attribute* value = mMap.Get(aName);                              \
    return value ? value->As##typeLabel() : defaultValue;            \
  }                                                                  \
  void                                                               \
  AttributeMap::Set(AttributeName aName, type aValue) {              \
    mMap.Remove(aName);                                              \
    mMap.Put(aName, new Attribute(aValue));                          \
  }

#define MAKE_ATTRIBUTE_HANDLERS_CLASS(className)                     \
  className                                                          \
  AttributeMap::Get##className(AttributeName aName) const {          \
    Attribute* value = mMap.Get(aName);                              \
    return value ? value->As##className() : className();             \
  }                                                                  \
  void                                                               \
  AttributeMap::Set(AttributeName aName, const className& aValue) {  \
    mMap.Remove(aName);                                              \
    mMap.Put(aName, new Attribute(aValue));                          \
  }

MAKE_ATTRIBUTE_HANDLERS_BASIC(bool, Bool, false)
MAKE_ATTRIBUTE_HANDLERS_BASIC(uint32_t, Uint, 0)
MAKE_ATTRIBUTE_HANDLERS_BASIC(float, Float, 0)
MAKE_ATTRIBUTE_HANDLERS_CLASS(Size)
MAKE_ATTRIBUTE_HANDLERS_CLASS(IntSize)
MAKE_ATTRIBUTE_HANDLERS_CLASS(IntPoint)
MAKE_ATTRIBUTE_HANDLERS_CLASS(Matrix)
MAKE_ATTRIBUTE_HANDLERS_CLASS(Matrix5x4)
MAKE_ATTRIBUTE_HANDLERS_CLASS(Point3D)
MAKE_ATTRIBUTE_HANDLERS_CLASS(Color)
MAKE_ATTRIBUTE_HANDLERS_CLASS(AttributeMap)

#undef MAKE_ATTRIBUTE_HANDLERS_BASIC
#undef MAKE_ATTRIBUTE_HANDLERS_CLASS

const nsTArray<float>&
AttributeMap::GetFloats(AttributeName aName) const
{
  Attribute* value = mMap.Get(aName);
  if (!value) {
    value = new Attribute(nullptr, 0);
    mMap.Put(aName, value);
  }
  return value->AsFloats();
}

void
AttributeMap::Set(AttributeName aName, const float* aValues, int32_t aLength)
{
  mMap.Remove(aName);
  mMap.Put(aName, new Attribute(aValues, aLength));
}

}
}
