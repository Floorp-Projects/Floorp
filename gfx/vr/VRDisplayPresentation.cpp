/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRDisplayPresentation.h"

#include "mozilla/unused.h"
#include "VRDisplayClient.h"
#include "VRLayerChild.h"

using namespace mozilla;
using namespace mozilla::gfx;

VRDisplayPresentation::VRDisplayPresentation(VRDisplayClient *aDisplayClient,
                                             const nsTArray<mozilla::dom::VRLayer>& aLayers)
  : mDisplayClient(aDisplayClient)
  , mDOMLayers(aLayers)
{
  CreateLayers();
}

void
VRDisplayPresentation::CreateLayers()
{
  if (mLayers.Length()) {
    return;
  }

  for (dom::VRLayer& layer : mDOMLayers) {
    dom::HTMLCanvasElement* canvasElement = layer.mSource;
    if (!canvasElement) {
      /// XXX In the future we will support WebVR in WebWorkers here
      continue;
    }

    Rect leftBounds(0.0, 0.0, 0.5, 1.0);
    if (layer.mLeftBounds.Length() == 4) {
      leftBounds.x = layer.mLeftBounds[0];
      leftBounds.y = layer.mLeftBounds[1];
      leftBounds.width = layer.mLeftBounds[2];
      leftBounds.height = layer.mLeftBounds[3];
    } else if (layer.mLeftBounds.Length() != 0) {
      /**
       * We ignore layers with an incorrect number of values.
       * In the future, VRDisplay.requestPresent may throw in
       * this case.  See https://github.com/w3c/webvr/issues/71
       */
      continue;
    }

    Rect rightBounds(0.5, 0.0, 0.5, 1.0);
    if (layer.mRightBounds.Length() == 4) {
      rightBounds.x = layer.mRightBounds[0];
      rightBounds.y = layer.mRightBounds[1];
      rightBounds.width = layer.mRightBounds[2];
      rightBounds.height = layer.mRightBounds[3];
    } else if (layer.mRightBounds.Length() != 0) {
      /**
       * We ignore layers with an incorrect number of values.
       * In the future, VRDisplay.requestPresent may throw in
       * this case.  See https://github.com/w3c/webvr/issues/71
       */
      continue;
    }

    VRManagerChild *manager = VRManagerChild::Get();
    if (!manager) {
      NS_WARNING("VRManagerChild::Get returned null!");
      continue;
    }

    RefPtr<VRLayerChild> vrLayer = static_cast<VRLayerChild*>(manager->CreateVRLayer(mDisplayClient->GetDisplayInfo().GetDisplayID(), leftBounds, rightBounds));
    if (!vrLayer) {
      NS_WARNING("CreateVRLayer returned null!");
      continue;
    }

    vrLayer->Initialize(canvasElement);

    mLayers.AppendElement(vrLayer);
  }
}

void
VRDisplayPresentation::DestroyLayers()
{
  for (VRLayerChild* layer : mLayers) {
    Unused << layer->SendDestroy();
  }
  mLayers.Clear();
}

void
VRDisplayPresentation::GetDOMLayers(nsTArray<dom::VRLayer>& result)
{
  result = mDOMLayers;
}

VRDisplayPresentation::~VRDisplayPresentation()
{
  DestroyLayers();
  mDisplayClient->PresentationDestroyed();
}

void VRDisplayPresentation::SubmitFrame(int32_t aInputFrameID)
{
  for (VRLayerChild *layer : mLayers) {
    layer->SubmitFrame(aInputFrameID);
    break; // Currently only one layer supported, submit only the first
  }
}
