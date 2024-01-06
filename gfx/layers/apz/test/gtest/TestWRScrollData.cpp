/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestWRScrollData.h"
#include "APZTestAccess.h"
#include "gtest/gtest.h"
#include "FrameMetrics.h"
#include "gfxPlatform.h"
#include "mozilla/layers/APZUpdater.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/ScrollableLayerGuid.h"
#include "mozilla/layers/WebRenderScrollDataWrapper.h"
#include "mozilla/UniquePtr.h"
#include "apz/src/APZCTreeManager.h"

using mozilla::layers::APZCTreeManager;
using mozilla::layers::APZUpdater;
using mozilla::layers::LayersId;
using mozilla::layers::ScrollableLayerGuid;
using mozilla::layers::ScrollMetadata;
using mozilla::layers::TestWRScrollData;
using mozilla::layers::WebRenderLayerScrollData;
using mozilla::layers::WebRenderScrollDataWrapper;

/* static */
TestWRScrollData TestWRScrollData::Create(const char* aTreeShape,
                                          const APZUpdater& aUpdater,
                                          const LayerIntRect* aVisibleRects,
                                          const gfx::Matrix4x4* aTransforms) {
  // The WebRenderLayerScrollData tree needs to be created in a fairly
  // particular way (for example, each node needs to know the number of
  // descendants it has), so this function takes care to create the nodes
  // in the same order as WebRenderCommandBuilder would.
  TestWRScrollData result;
  const size_t len = strlen(aTreeShape);
  // "Layer index" in this function refers to the index by which a layer will
  // be accessible via TestWRScrollData::GetLayer(), and matches the order
  // in which the layer appears in |aTreeShape|.
  size_t currentLayerIndex = 0;
  struct LayerEntry {
    size_t mLayerIndex;
    int32_t mDescendantCount = 0;
  };
  // Layers we have encountered in |aTreeShape|, but have not built a
  // WebRenderLayerScrollData for. (It can only be built after its
  // descendants have been encountered and counted.)
  std::stack<LayerEntry> pendingLayers;
  std::vector<WebRenderLayerScrollData> finishedLayers;
  // Tracks the level of nesting of '(' characters. Starts at 1 to account
  // for the root layer.
  size_t depth = 1;
  // Helper function for finishing a layer once all its descendants have been
  // encountered.
  auto finishLayer = [&] {
    MOZ_ASSERT(!pendingLayers.empty());
    LayerEntry entry = pendingLayers.top();

    WebRenderLayerScrollData layer;
    APZTestAccess::InitializeForTest(layer, entry.mDescendantCount);
    if (aVisibleRects) {
      layer.SetVisibleRect(aVisibleRects[entry.mLayerIndex]);
    }
    if (aTransforms) {
      layer.SetTransform(aTransforms[entry.mLayerIndex]);
    }
    finishedLayers.push_back(std::move(layer));

    // |finishedLayers| stores the layers in a different order than they
    // appeared in |aTreeShape|. To be able to access layers by their layer
    // index, keep a mapping from layer index to index in |finishedLayers|.
    result.mIndexMap.emplace(entry.mLayerIndex, finishedLayers.size() - 1);

    pendingLayers.pop();

    // Keep track of descendant counts. The +1 is for the layer just finished.
    if (!pendingLayers.empty()) {
      pendingLayers.top().mDescendantCount += (entry.mDescendantCount + 1);
    }
  };
  for (size_t i = 0; i < len; ++i) {
    if (aTreeShape[i] == '(') {
      ++depth;
    } else if (aTreeShape[i] == ')') {
      if (pendingLayers.size() <= 1) {
        printf("Invalid tree shape: too many ')'\n");
        MOZ_CRASH();
      }
      finishLayer();  // finish last layer at current depth
      --depth;
    } else {
      if (aTreeShape[i] != 'x') {
        printf("The only allowed character to represent a layer is 'x'\n");
        MOZ_CRASH();
      }
      if (depth == pendingLayers.size()) {
        // We have a previous layer at this same depth to finish.
        if (depth <= 1) {
          printf("The tree is only allowed to have one root\n");
          MOZ_CRASH();
        }
        finishLayer();
      }
      MOZ_ASSERT(depth == pendingLayers.size() + 1);
      pendingLayers.push({currentLayerIndex});
      ++currentLayerIndex;
    }
  }
  if (pendingLayers.size() != 1) {
    printf("Invalid tree shape: '(' and ')' not balanced\n");
    MOZ_CRASH();
  }
  finishLayer();  // finish root layer

  // As in WebRenderCommandBuilder, the layers need to be added to the
  // WebRenderScrollData in reverse of the order in which they were built.
  for (auto it = finishedLayers.rbegin(); it != finishedLayers.rend(); ++it) {
    result.AddLayerData(std::move(*it));
  }
  // mIndexMap also needs to be adjusted to accout for the reversal above.
  for (auto& [layerIndex, storedIndex] : result.mIndexMap) {
    (void)layerIndex;  // suppress -Werror=unused-variable
    storedIndex = result.GetLayerCount() - storedIndex - 1;
  }

  return result;
}

const WebRenderLayerScrollData* TestWRScrollData::operator[](
    size_t aLayerIndex) const {
  auto it = mIndexMap.find(aLayerIndex);
  if (it == mIndexMap.end()) {
    return nullptr;
  }
  return GetLayerData(it->second);
}

WebRenderLayerScrollData* TestWRScrollData::operator[](size_t aLayerIndex) {
  auto it = mIndexMap.find(aLayerIndex);
  if (it == mIndexMap.end()) {
    return nullptr;
  }
  return GetLayerData(it->second);
}

void TestWRScrollData::SetScrollMetadata(
    size_t aLayerIndex, const nsTArray<ScrollMetadata>& aMetadata) {
  WebRenderLayerScrollData* layer = operator[](aLayerIndex);
  MOZ_ASSERT(layer);
  for (const ScrollMetadata& metadata : aMetadata) {
    layer->AppendScrollMetadata(*this, metadata);
  }
}

class WebRenderScrollDataWrapperTester : public ::testing::Test {
 protected:
  virtual void SetUp() {
    // This ensures ScrollMetadata::sNullMetadata is initialized.
    gfxPlatform::GetPlatform();

    mManager = APZCTreeManager::Create(LayersId{0});
    mUpdater = new APZUpdater(mManager, false);
  }

  RefPtr<APZCTreeManager> mManager;
  RefPtr<APZUpdater> mUpdater;
};

TEST_F(WebRenderScrollDataWrapperTester, SimpleTree) {
  auto layers = TestWRScrollData::Create("x(x(x(xx)x(x)))", *mUpdater);
  WebRenderScrollDataWrapper w0(*mUpdater, &layers);

  ASSERT_EQ(layers[0], w0.GetLayer());
  WebRenderScrollDataWrapper w1 = w0.GetLastChild();
  ASSERT_EQ(layers[1], w1.GetLayer());
  ASSERT_FALSE(w1.GetPrevSibling().IsValid());
  WebRenderScrollDataWrapper w5 = w1.GetLastChild();
  ASSERT_EQ(layers[5], w5.GetLayer());
  WebRenderScrollDataWrapper w6 = w5.GetLastChild();
  ASSERT_EQ(layers[6], w6.GetLayer());
  ASSERT_FALSE(w6.GetLastChild().IsValid());
  WebRenderScrollDataWrapper w2 = w5.GetPrevSibling();
  ASSERT_EQ(layers[2], w2.GetLayer());
  ASSERT_FALSE(w2.GetPrevSibling().IsValid());
  WebRenderScrollDataWrapper w4 = w2.GetLastChild();
  ASSERT_EQ(layers[4], w4.GetLayer());
  ASSERT_FALSE(w4.GetLastChild().IsValid());
  WebRenderScrollDataWrapper w3 = w4.GetPrevSibling();
  ASSERT_EQ(layers[3], w3.GetLayer());
  ASSERT_FALSE(w3.GetLastChild().IsValid());
  ASSERT_FALSE(w3.GetPrevSibling().IsValid());
}

static ScrollMetadata MakeMetadata(ScrollableLayerGuid::ViewID aId) {
  ScrollMetadata metadata;
  metadata.GetMetrics().SetScrollId(aId);
  return metadata;
}

TEST_F(WebRenderScrollDataWrapperTester, MultiFramemetricsTree) {
  auto layers = TestWRScrollData::Create("x(x(x(xx)x(x)))", *mUpdater);

  nsTArray<ScrollMetadata> metadata;
  metadata.InsertElementAt(0,
                           MakeMetadata(ScrollableLayerGuid::START_SCROLL_ID +
                                        0));  // topmost of root layer
  metadata.InsertElementAt(0,
                           MakeMetadata(ScrollableLayerGuid::NULL_SCROLL_ID));
  metadata.InsertElementAt(
      0, MakeMetadata(ScrollableLayerGuid::START_SCROLL_ID + 1));
  metadata.InsertElementAt(
      0, MakeMetadata(ScrollableLayerGuid::START_SCROLL_ID + 2));
  metadata.InsertElementAt(0,
                           MakeMetadata(ScrollableLayerGuid::NULL_SCROLL_ID));
  metadata.InsertElementAt(
      0, MakeMetadata(
             ScrollableLayerGuid::NULL_SCROLL_ID));  // bottom of root layer
  layers.SetScrollMetadata(0, metadata);

  metadata.Clear();
  metadata.InsertElementAt(
      0, MakeMetadata(ScrollableLayerGuid::START_SCROLL_ID + 3));
  layers.SetScrollMetadata(1, metadata);

  metadata.Clear();
  metadata.InsertElementAt(
      0, MakeMetadata(ScrollableLayerGuid::START_SCROLL_ID + 4));
  layers.SetScrollMetadata(2, metadata);

  metadata.Clear();
  metadata.InsertElementAt(
      0, MakeMetadata(ScrollableLayerGuid::START_SCROLL_ID + 5));
  layers.SetScrollMetadata(4, metadata);

  metadata.Clear();
  metadata.InsertElementAt(0,
                           MakeMetadata(ScrollableLayerGuid::NULL_SCROLL_ID));
  metadata.InsertElementAt(
      0, MakeMetadata(ScrollableLayerGuid::START_SCROLL_ID + 6));
  layers.SetScrollMetadata(5, metadata);

  WebRenderScrollDataWrapper wrapper(*mUpdater, &layers);
  nsTArray<WebRenderLayerScrollData*> expectedLayers;
  expectedLayers.AppendElement(layers[0]);
  expectedLayers.AppendElement(layers[0]);
  expectedLayers.AppendElement(layers[0]);
  expectedLayers.AppendElement(layers[0]);
  expectedLayers.AppendElement(layers[0]);
  expectedLayers.AppendElement(layers[0]);
  expectedLayers.AppendElement(layers[1]);
  expectedLayers.AppendElement(layers[5]);
  expectedLayers.AppendElement(layers[5]);
  expectedLayers.AppendElement(layers[6]);
  nsTArray<ScrollableLayerGuid::ViewID> expectedIds;
  expectedIds.AppendElement(ScrollableLayerGuid::START_SCROLL_ID + 0);
  expectedIds.AppendElement(ScrollableLayerGuid::NULL_SCROLL_ID);
  expectedIds.AppendElement(ScrollableLayerGuid::START_SCROLL_ID + 1);
  expectedIds.AppendElement(ScrollableLayerGuid::START_SCROLL_ID + 2);
  expectedIds.AppendElement(ScrollableLayerGuid::NULL_SCROLL_ID);
  expectedIds.AppendElement(ScrollableLayerGuid::NULL_SCROLL_ID);
  expectedIds.AppendElement(ScrollableLayerGuid::START_SCROLL_ID + 3);
  expectedIds.AppendElement(ScrollableLayerGuid::NULL_SCROLL_ID);
  expectedIds.AppendElement(ScrollableLayerGuid::START_SCROLL_ID + 6);
  expectedIds.AppendElement(ScrollableLayerGuid::NULL_SCROLL_ID);
  for (int i = 0; i < 10; i++) {
    ASSERT_EQ(expectedLayers[i], wrapper.GetLayer());
    ASSERT_EQ(expectedIds[i], wrapper.Metrics().GetScrollId());
    wrapper = wrapper.GetLastChild();
  }
  ASSERT_FALSE(wrapper.IsValid());
}
