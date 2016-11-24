/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "TestLayers.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "LayerUserData.h"
#include "mozilla/layers/LayerMetricsWrapper.h"
#include "mozilla/layers/CompositorBridgeParent.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;

class TestContainerLayer: public ContainerLayer {
public:
  explicit TestContainerLayer(LayerManager* aManager)
    : ContainerLayer(aManager, nullptr)
  {}

  virtual const char* Name() const {
    return "TestContainerLayer";
  }

  virtual LayerType GetType() const {
    return TYPE_CONTAINER;
  }

  virtual void ComputeEffectiveTransforms(const Matrix4x4& aTransformToSurface) {
    DefaultComputeEffectiveTransforms(aTransformToSurface);
  }
};

class TestPaintedLayer: public PaintedLayer {
public:
  explicit TestPaintedLayer(LayerManager* aManager)
    : PaintedLayer(aManager, nullptr)
  {}

  virtual const char* Name() const {
    return "TestPaintedLayer";
  }

  virtual LayerType GetType() const {
    return TYPE_PAINTED;
  }

  virtual void InvalidateRegion(const nsIntRegion& aRegion) {
    MOZ_CRASH();
  }
};

class TestLayerManager: public LayerManager {
public:
  TestLayerManager()
    : LayerManager()
  {}

  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) { return false; }
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer() {
    RefPtr<ContainerLayer> layer = new TestContainerLayer(this);
    return layer.forget();
  }
  virtual void GetBackendName(nsAString& aName) {}
  virtual LayersBackend GetBackendType() { return LayersBackend::LAYERS_BASIC; }
  virtual bool BeginTransaction() { return true; }
  virtual already_AddRefed<ImageLayer> CreateImageLayer() {
    NS_RUNTIMEABORT("Not implemented.");
    return nullptr;
  }
  virtual already_AddRefed<PaintedLayer> CreatePaintedLayer() {
    RefPtr<PaintedLayer> layer = new TestPaintedLayer(this);
    return layer.forget();
  }
  virtual already_AddRefed<ColorLayer> CreateColorLayer() {
    NS_RUNTIMEABORT("Not implemented.");
    return nullptr;
  }
  virtual void SetRoot(Layer* aLayer) {}
  virtual bool BeginTransactionWithTarget(gfxContext* aTarget) { return true; }
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer() {
    NS_RUNTIMEABORT("Not implemented.");
    return nullptr;
  }
  virtual void EndTransaction(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT) {}
  virtual int32_t GetMaxTextureSize() const { return 0; }
};

class TestUserData: public LayerUserData {
public:
  MOCK_METHOD0(Die, void());
  virtual ~TestUserData() { Die(); }
};


TEST(Layers, LayerConstructor) {
  TestContainerLayer layer(nullptr);
}

TEST(Layers, Defaults) {
  TestContainerLayer layer(nullptr);
  ASSERT_EQ(1.0, layer.GetOpacity());
  ASSERT_EQ(1.0f, layer.GetPostXScale());
  ASSERT_EQ(1.0f, layer.GetPostYScale());

  ASSERT_EQ(nullptr, layer.GetNextSibling());
  ASSERT_EQ(nullptr, layer.GetPrevSibling());
  ASSERT_EQ(nullptr, layer.GetFirstChild());
  ASSERT_EQ(nullptr, layer.GetLastChild());
}

TEST(Layers, Transform) {
  TestContainerLayer layer(nullptr);

  Matrix4x4 identity;
  ASSERT_EQ(true, identity.IsIdentity());

  ASSERT_EQ(identity, layer.GetTransform());
}

TEST(Layers, Type) {
  TestContainerLayer layer(nullptr);
  ASSERT_EQ(nullptr, layer.AsPaintedLayer());
  ASSERT_EQ(nullptr, layer.AsRefLayer());
  ASSERT_EQ(nullptr, layer.AsColorLayer());
}

TEST(Layers, UserData) {
  UniquePtr<TestContainerLayer> layerPtr(new TestContainerLayer(nullptr));
  TestContainerLayer& layer = *layerPtr;

  void* key1 = (void*)1;
  void* key2 = (void*)2;
  void* key3 = (void*)3;

  ASSERT_EQ(nullptr, layer.GetUserData(key1));
  ASSERT_EQ(nullptr, layer.GetUserData(key2));
  ASSERT_EQ(nullptr, layer.GetUserData(key3));

  TestUserData* data1 = new TestUserData;
  TestUserData* data2 = new TestUserData;
  TestUserData* data3 = new TestUserData;

  layer.SetUserData(key1, data1);
  layer.SetUserData(key2, data2);
  layer.SetUserData(key3, data3);

  // Also checking that the user data is returned but not free'd
  UniquePtr<LayerUserData> d1(layer.RemoveUserData(key1));
  UniquePtr<LayerUserData> d2(layer.RemoveUserData(key2));
  UniquePtr<LayerUserData> d3(layer.RemoveUserData(key3));
  ASSERT_EQ(data1, d1.get());
  ASSERT_EQ(data2, d2.get());
  ASSERT_EQ(data3, d3.get());

  layer.SetUserData(key1, d1.release());
  layer.SetUserData(key2, d2.release());
  layer.SetUserData(key3, d3.release());

  // Layer has ownership of data1-3, check that they are destroyed
  EXPECT_CALL(*data1, Die());
  EXPECT_CALL(*data2, Die());
  EXPECT_CALL(*data3, Die());
}

static
already_AddRefed<Layer> CreateLayer(char aLayerType, LayerManager* aManager) {
  RefPtr<Layer> layer = nullptr;
  if (aLayerType == 'c') {
    layer = aManager->CreateContainerLayer();
  } else if (aLayerType == 't') {
    layer = aManager->CreatePaintedLayer();
  } else if (aLayerType == 'o') {
    layer = aManager->CreateColorLayer();
  }
  return layer.forget();
}

already_AddRefed<Layer> CreateLayerTree(
    const char* aLayerTreeDescription,
    nsIntRegion* aVisibleRegions,
    const Matrix4x4* aTransforms,
    RefPtr<LayerManager>& manager,
    nsTArray<RefPtr<Layer> >& aLayersOut) {

  aLayersOut.Clear();

  if (!manager) {
    manager = new TestLayerManager();
  }

  RefPtr<Layer> rootLayer = nullptr;
  RefPtr<ContainerLayer> parentContainerLayer = nullptr;
  RefPtr<Layer> lastLayer = nullptr;
  int layerNumber = 0;
  for (size_t i = 0; i < strlen(aLayerTreeDescription); i++) {
    if (aLayerTreeDescription[i] == '(') {
      if (!lastLayer) {
        printf("Syntax error, likely '(' character isn't preceded by a container.\n");
        MOZ_CRASH();
      }
      parentContainerLayer = lastLayer->AsContainerLayer();
      if (!parentContainerLayer) {
        printf("Layer before '(' must be a container.\n");
        MOZ_CRASH();
      }
    } else if (aLayerTreeDescription[i] == ')') {
      parentContainerLayer = parentContainerLayer->GetParent();
      lastLayer = nullptr;
    } else {
      RefPtr<Layer> layer = CreateLayer(aLayerTreeDescription[i], manager.get());
      if (aVisibleRegions) {
        layer->SetVisibleRegion(LayerIntRegion::FromUnknownRegion(aVisibleRegions[layerNumber]));
        layer->SetEventRegions(EventRegions(aVisibleRegions[layerNumber]));
      }
      if (aTransforms) {
        layer->SetBaseTransform(aTransforms[layerNumber]);
      }
      aLayersOut.AppendElement(layer);
      layerNumber++;
      if (rootLayer && !parentContainerLayer) {
        MOZ_CRASH();
      }
      if (!rootLayer) {
        rootLayer = layer;
      }
      if (parentContainerLayer) {
        parentContainerLayer->InsertAfter(layer, parentContainerLayer->GetLastChild());
        layer->SetParent(parentContainerLayer);
      }
      lastLayer = layer;
    }
  }
  if (rootLayer) {
    rootLayer->ComputeEffectiveTransforms(Matrix4x4());
    manager->SetRoot(rootLayer);
    if (rootLayer->AsHostLayer()) {
      // Only perform this for LayerManagerComposite
      CompositorBridgeParent::SetShadowProperties(rootLayer);
    }
  }
  return rootLayer.forget();
}

TEST(Layers, LayerTree) {
  const char* layerTreeSyntax = "c(c(tt))";
  nsIntRegion layerVisibleRegion[] = {
    nsIntRegion(IntRect(0,0,100,100)),
    nsIntRegion(IntRect(0,0,100,100)),
    nsIntRegion(IntRect(0,0,100,100)),
    nsIntRegion(IntRect(10,10,20,20)),
  };
  Matrix4x4 transforms[] = {
    Matrix4x4(),
    Matrix4x4(),
    Matrix4x4(),
    Matrix4x4(),
  };
  nsTArray<RefPtr<Layer> > layers;

  RefPtr<LayerManager> lm;
  RefPtr<Layer> root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, transforms, lm, layers);

  // B2G g++ doesn't like ASSERT_NE with nullptr directly. It thinks it's
  // an int.
  Layer* nullLayer = nullptr;
  ASSERT_NE(nullLayer, layers[0]->AsContainerLayer());
  ASSERT_NE(nullLayer, layers[1]->AsContainerLayer());
  ASSERT_NE(nullLayer, layers[2]->AsPaintedLayer());
  ASSERT_NE(nullLayer, layers[3]->AsPaintedLayer());
}

static void ValidateTreePointers(Layer* aLayer) {
  if (aLayer->GetNextSibling()) {
    ASSERT_EQ(aLayer, aLayer->GetNextSibling()->GetPrevSibling());
  } else if (aLayer->GetParent()) {
    ASSERT_EQ(aLayer, aLayer->GetParent()->GetLastChild());
  }
  if (aLayer->GetPrevSibling()) {
    ASSERT_EQ(aLayer, aLayer->GetPrevSibling()->GetNextSibling());
  } else if (aLayer->GetParent()) {
    ASSERT_EQ(aLayer, aLayer->GetParent()->GetFirstChild());
  }
  if (aLayer->GetFirstChild()) {
    ASSERT_EQ(aLayer, aLayer->GetFirstChild()->GetParent());
  }
  if (aLayer->GetLastChild()) {
    ASSERT_EQ(aLayer, aLayer->GetLastChild()->GetParent());
  }
}

static void ValidateTreePointers(nsTArray<RefPtr<Layer> >& aLayers) {
  for (uint32_t i = 0; i < aLayers.Length(); i++) {
    ValidateTreePointers(aLayers[i]);
  }
}

TEST(Layers, RepositionChild) {
  const char* layerTreeSyntax = "c(ttt)";

  nsTArray<RefPtr<Layer> > layers;
  RefPtr<LayerManager> lm;
  RefPtr<Layer> root = CreateLayerTree(layerTreeSyntax, nullptr, nullptr, lm, layers);
  ContainerLayer* parent = root->AsContainerLayer();
  ValidateTreePointers(layers);

  // tree is currently like this (using indexes into layers):
  //   0
  // 1 2 3
  ASSERT_EQ(layers[2], layers[1]->GetNextSibling());
  ASSERT_EQ(layers[3], layers[2]->GetNextSibling());
  ASSERT_EQ(nullptr, layers[3]->GetNextSibling());

  parent->RepositionChild(layers[1], layers[3]);
  ValidateTreePointers(layers);

  // now the tree is like this:
  //   0
  // 2 3 1
  ASSERT_EQ(layers[3], layers[2]->GetNextSibling());
  ASSERT_EQ(layers[1], layers[3]->GetNextSibling());
  ASSERT_EQ(nullptr, layers[1]->GetNextSibling());

  parent->RepositionChild(layers[3], layers[2]);
  ValidateTreePointers(layers);

  // no change
  ASSERT_EQ(layers[3], layers[2]->GetNextSibling());
  ASSERT_EQ(layers[1], layers[3]->GetNextSibling());
  ASSERT_EQ(nullptr, layers[1]->GetNextSibling());

  parent->RepositionChild(layers[3], layers[1]);
  ValidateTreePointers(layers);

  //   0
  // 2 1 3
  ASSERT_EQ(layers[1], layers[2]->GetNextSibling());
  ASSERT_EQ(layers[3], layers[1]->GetNextSibling());
  ASSERT_EQ(nullptr, layers[3]->GetNextSibling());

  parent->RepositionChild(layers[3], nullptr);
  ValidateTreePointers(layers);

  //   0
  // 3 2 1
  ASSERT_EQ(layers[2], layers[3]->GetNextSibling());
  ASSERT_EQ(layers[1], layers[2]->GetNextSibling());
  ASSERT_EQ(nullptr, layers[1]->GetNextSibling());
}

class LayerMetricsWrapperTester : public ::testing::Test {
protected:
  virtual void SetUp() {
    // This ensures ScrollMetadata::sNullMetadata is initialized.
    gfxPlatform::GetPlatform();
  }
};

TEST_F(LayerMetricsWrapperTester, SimpleTree) {
  nsTArray<RefPtr<Layer> > layers;
  RefPtr<LayerManager> lm;
  RefPtr<Layer> root = CreateLayerTree("c(c(c(tt)c(t)))", nullptr, nullptr, lm, layers);
  LayerMetricsWrapper wrapper(root);

  ASSERT_EQ(root.get(), wrapper.GetLayer());
  wrapper = wrapper.GetFirstChild();
  ASSERT_EQ(layers[1].get(), wrapper.GetLayer());
  ASSERT_FALSE(wrapper.GetNextSibling().IsValid());
  wrapper = wrapper.GetFirstChild();
  ASSERT_EQ(layers[2].get(), wrapper.GetLayer());
  wrapper = wrapper.GetFirstChild();
  ASSERT_EQ(layers[3].get(), wrapper.GetLayer());
  ASSERT_FALSE(wrapper.GetFirstChild().IsValid());
  wrapper = wrapper.GetNextSibling();
  ASSERT_EQ(layers[4].get(), wrapper.GetLayer());
  ASSERT_FALSE(wrapper.GetNextSibling().IsValid());
  wrapper = wrapper.GetParent();
  ASSERT_EQ(layers[2].get(), wrapper.GetLayer());
  wrapper = wrapper.GetNextSibling();
  ASSERT_EQ(layers[5].get(), wrapper.GetLayer());
  ASSERT_FALSE(wrapper.GetNextSibling().IsValid());
  wrapper = wrapper.GetLastChild();
  ASSERT_EQ(layers[6].get(), wrapper.GetLayer());
  wrapper = wrapper.GetParent();
  ASSERT_EQ(layers[5].get(), wrapper.GetLayer());
  LayerMetricsWrapper layer5 = wrapper;
  wrapper = wrapper.GetPrevSibling();
  ASSERT_EQ(layers[2].get(), wrapper.GetLayer());
  wrapper = wrapper.GetParent();
  ASSERT_EQ(layers[1].get(), wrapper.GetLayer());
  ASSERT_TRUE(layer5 == wrapper.GetLastChild());
  LayerMetricsWrapper rootWrapper(root);
  ASSERT_TRUE(rootWrapper == wrapper.GetParent());
}

static ScrollMetadata
MakeMetadata(FrameMetrics::ViewID aId) {
  ScrollMetadata metadata;
  metadata.GetMetrics().SetScrollId(aId);
  return metadata;
}

TEST_F(LayerMetricsWrapperTester, MultiFramemetricsTree) {
  nsTArray<RefPtr<Layer> > layers;
  RefPtr<LayerManager> lm;
  RefPtr<Layer> root = CreateLayerTree("c(c(c(tt)c(t)))", nullptr, nullptr, lm, layers);

  nsTArray<ScrollMetadata> metadata;
  metadata.InsertElementAt(0, MakeMetadata(FrameMetrics::START_SCROLL_ID + 0)); // topmost of root layer
  metadata.InsertElementAt(0, MakeMetadata(FrameMetrics::NULL_SCROLL_ID));
  metadata.InsertElementAt(0, MakeMetadata(FrameMetrics::START_SCROLL_ID + 1));
  metadata.InsertElementAt(0, MakeMetadata(FrameMetrics::START_SCROLL_ID + 2));
  metadata.InsertElementAt(0, MakeMetadata(FrameMetrics::NULL_SCROLL_ID));
  metadata.InsertElementAt(0, MakeMetadata(FrameMetrics::NULL_SCROLL_ID));      // bottom of root layer
  root->SetScrollMetadata(metadata);

  metadata.Clear();
  metadata.InsertElementAt(0, MakeMetadata(FrameMetrics::START_SCROLL_ID + 3));
  layers[1]->SetScrollMetadata(metadata);

  metadata.Clear();
  metadata.InsertElementAt(0, MakeMetadata(FrameMetrics::NULL_SCROLL_ID));
  metadata.InsertElementAt(0, MakeMetadata(FrameMetrics::START_SCROLL_ID + 4));
  layers[2]->SetScrollMetadata(metadata);

  metadata.Clear();
  metadata.InsertElementAt(0, MakeMetadata(FrameMetrics::START_SCROLL_ID + 5));
  layers[4]->SetScrollMetadata(metadata);

  metadata.Clear();
  metadata.InsertElementAt(0, MakeMetadata(FrameMetrics::START_SCROLL_ID + 6));
  layers[5]->SetScrollMetadata(metadata);

  LayerMetricsWrapper wrapper(root, LayerMetricsWrapper::StartAt::TOP);
  nsTArray<Layer*> expectedLayers;
  expectedLayers.AppendElement(layers[0].get());
  expectedLayers.AppendElement(layers[0].get());
  expectedLayers.AppendElement(layers[0].get());
  expectedLayers.AppendElement(layers[0].get());
  expectedLayers.AppendElement(layers[0].get());
  expectedLayers.AppendElement(layers[0].get());
  expectedLayers.AppendElement(layers[1].get());
  expectedLayers.AppendElement(layers[2].get());
  expectedLayers.AppendElement(layers[2].get());
  expectedLayers.AppendElement(layers[3].get());
  nsTArray<FrameMetrics::ViewID> expectedIds;
  expectedIds.AppendElement(FrameMetrics::START_SCROLL_ID + 0);
  expectedIds.AppendElement(FrameMetrics::NULL_SCROLL_ID);
  expectedIds.AppendElement(FrameMetrics::START_SCROLL_ID + 1);
  expectedIds.AppendElement(FrameMetrics::START_SCROLL_ID + 2);
  expectedIds.AppendElement(FrameMetrics::NULL_SCROLL_ID);
  expectedIds.AppendElement(FrameMetrics::NULL_SCROLL_ID);
  expectedIds.AppendElement(FrameMetrics::START_SCROLL_ID + 3);
  expectedIds.AppendElement(FrameMetrics::NULL_SCROLL_ID);
  expectedIds.AppendElement(FrameMetrics::START_SCROLL_ID + 4);
  expectedIds.AppendElement(FrameMetrics::NULL_SCROLL_ID);
  for (int i = 0; i < 10; i++) {
    ASSERT_EQ(expectedLayers[i], wrapper.GetLayer());
    ASSERT_EQ(expectedIds[i], wrapper.Metrics().GetScrollId());
    wrapper = wrapper.GetFirstChild();
  }
  ASSERT_FALSE(wrapper.IsValid());

  wrapper = LayerMetricsWrapper(root, LayerMetricsWrapper::StartAt::BOTTOM);
  for (int i = 5; i < 10; i++) {
    ASSERT_EQ(expectedLayers[i], wrapper.GetLayer());
    ASSERT_EQ(expectedIds[i], wrapper.Metrics().GetScrollId());
    wrapper = wrapper.GetFirstChild();
  }
  ASSERT_FALSE(wrapper.IsValid());

  wrapper = LayerMetricsWrapper(layers[4], LayerMetricsWrapper::StartAt::BOTTOM);
  ASSERT_EQ(FrameMetrics::START_SCROLL_ID + 5, wrapper.Metrics().GetScrollId());
  wrapper = wrapper.GetParent();
  ASSERT_EQ(FrameMetrics::START_SCROLL_ID + 4, wrapper.Metrics().GetScrollId());
  ASSERT_EQ(layers[2].get(), wrapper.GetLayer());
  ASSERT_FALSE(wrapper.GetNextSibling().IsValid());
  wrapper = wrapper.GetParent();
  ASSERT_EQ(FrameMetrics::NULL_SCROLL_ID, wrapper.Metrics().GetScrollId());
  ASSERT_EQ(layers[2].get(), wrapper.GetLayer());
  wrapper = wrapper.GetNextSibling();
  ASSERT_EQ(FrameMetrics::START_SCROLL_ID + 6, wrapper.Metrics().GetScrollId());
  ASSERT_EQ(layers[5].get(), wrapper.GetLayer());
}
