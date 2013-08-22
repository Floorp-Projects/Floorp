/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "TestLayers.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

using namespace mozilla;
using namespace mozilla::layers;

class TestLayerManager: public LayerManager {
public:
  TestLayerManager()
    : LayerManager()
  {}

  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) { return false; }
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer() { return nullptr; }
  virtual void GetBackendName(nsAString& aName) {}
  virtual LayersBackend GetBackendType() { return LAYERS_BASIC; }
  virtual void BeginTransaction() {}
  virtual already_AddRefed<ImageLayer> CreateImageLayer() { return nullptr; }
  virtual void SetRoot(Layer* aLayer) {}
  virtual already_AddRefed<ColorLayer> CreateColorLayer() { return nullptr; }
  virtual void BeginTransactionWithTarget(gfxContext* aTarget) {}
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer() { return nullptr; }
  virtual void EndTransaction(DrawThebesLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT) {}
  virtual int32_t GetMaxTextureSize() const { return 0; }
  virtual already_AddRefed<ThebesLayer> CreateThebesLayer() { return nullptr; }
};

class TestContainerLayer: public ContainerLayer {
public:
  TestContainerLayer(LayerManager* aManager)
    : ContainerLayer(aManager, nullptr)
  {}

  virtual const char* Name() const {
    return "TestContainerLayer";
  }

  virtual LayerType GetType() const {
    return TYPE_CONTAINER;
  }

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface) {
    DefaultComputeEffectiveTransforms(aTransformToSurface);
  }

  virtual void RepositionChild(Layer* aChild, Layer* aAfter) {
    MOZ_CRASH();
  }

  virtual void InsertAfter(Layer* aChild, Layer* aAfter) {
    // Bad implementation but it should be fine for testing
    if (this == aChild) {
      MOZ_CRASH();
    }
    if (aAfter != nullptr && aAfter != mLastChild) {
      // Fix the implementation to support this if you need it
      MOZ_CRASH();
    }
    if (!mFirstChild) {
      mFirstChild = aChild;
    }
    if (mLastChild) {
      mLastChild->SetNextSibling(aChild);
    }
    aChild->SetPrevSibling(mLastChild);
    mLastChild = aChild;
  }

  virtual void RemoveChild(Layer* aChild) {
    MOZ_CRASH();
  }
};

class TestThebesLayer: public ThebesLayer {
public:
  TestThebesLayer(LayerManager* aManager)
    : ThebesLayer(aManager, nullptr)
  {}

  virtual const char* Name() const {
    return "TestThebesLayer";
  }

  virtual LayerType GetType() const {
    return TYPE_THEBES;
  }

  virtual void InvalidateRegion(const nsIntRegion& aRegion) {
    MOZ_CRASH();
  }
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

  gfx3DMatrix identity;
  ASSERT_EQ(true, identity.IsIdentity());

  ASSERT_EQ(identity, layer.GetTransform());
}

TEST(Layers, Type) {
  TestContainerLayer layer(nullptr);
  ASSERT_EQ(nullptr, layer.AsThebesLayer());
  ASSERT_EQ(nullptr, layer.AsRefLayer());
  ASSERT_EQ(nullptr, layer.AsColorLayer());
}

TEST(Layers, UserData) {
  TestContainerLayer* layerPtr = new TestContainerLayer(nullptr);
  TestContainerLayer& layer = *layerPtr;

  void* key1 = (void*)1;
  void* key2 = (void*)2;
  void* key3 = (void*)3;

  TestUserData* data1 = new TestUserData;
  TestUserData* data2 = new TestUserData;
  TestUserData* data3 = new TestUserData;

  ASSERT_EQ(nullptr, layer.GetUserData(key1));
  ASSERT_EQ(nullptr, layer.GetUserData(key2));
  ASSERT_EQ(nullptr, layer.GetUserData(key3));

  layer.SetUserData(key1, data1);
  layer.SetUserData(key2, data2);
  layer.SetUserData(key3, data3);

  // Also checking that the user data is returned but not free'd
  ASSERT_EQ(data1, layer.RemoveUserData(key1).forget());
  ASSERT_EQ(data2, layer.RemoveUserData(key2).forget());
  ASSERT_EQ(data3, layer.RemoveUserData(key3).forget());

  layer.SetUserData(key1, data1);
  layer.SetUserData(key2, data2);
  layer.SetUserData(key3, data3);

  // Layer has ownership of data1-3, check that they are destroyed
  EXPECT_CALL(*data1, Die());
  EXPECT_CALL(*data2, Die());
  EXPECT_CALL(*data3, Die());
  delete layerPtr;

}

static
already_AddRefed<Layer> CreateLayer(char aLayerType, LayerManager* aManager) {
  nsRefPtr<Layer> layer = nullptr;
  if (aLayerType == 'c') {
    layer = new TestContainerLayer(aManager);
  } else if (aLayerType == 't') {
    layer = new TestThebesLayer(aManager);
  }
  return layer.forget();
}

already_AddRefed<Layer> CreateLayerTree(
    const char* aLayerTreeDescription,
    nsIntRegion* aVisibleRegions,
    const gfx3DMatrix* aTransforms,
    nsRefPtr<LayerManager>& manager,
    nsTArray<nsRefPtr<Layer> >& aLayersOut) {

  aLayersOut.Clear();

  manager = new TestLayerManager();

  nsRefPtr<Layer> rootLayer = nullptr;
  nsRefPtr<ContainerLayer> parentContainerLayer = nullptr;
  nsRefPtr<Layer> lastLayer = nullptr;
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
      nsRefPtr<Layer> layer = CreateLayer(aLayerTreeDescription[i], manager.get());
      layer->SetVisibleRegion(aVisibleRegions[layerNumber]);
      layer->SetBaseTransform(aTransforms[layerNumber]);
      aLayersOut.AppendElement(layer);
      layerNumber++;
      if (rootLayer && !parentContainerLayer) {
        MOZ_CRASH();
      }
      if (!rootLayer) {
        rootLayer = layer;
      }
      if (parentContainerLayer) {
        parentContainerLayer->InsertAfter(layer, nullptr);
        layer->SetParent(parentContainerLayer);
      }
      lastLayer = layer;
    }
  }
  if (rootLayer) {
    rootLayer->ComputeEffectiveTransforms(gfx3DMatrix());
  }
  return rootLayer.forget();
}

TEST(Layers, LayerTree) {
  const char* layerTreeSyntax = "c(c(tt))";
  nsIntRegion layerVisibleRegion[] = {
    nsIntRegion(nsIntRect(0,0,100,100)),
    nsIntRegion(nsIntRect(0,0,100,100)),
    nsIntRegion(nsIntRect(0,0,100,100)),
    nsIntRegion(nsIntRect(10,10,20,20)),
  };
  gfx3DMatrix transforms[] = {
    gfx3DMatrix(),
    gfx3DMatrix(),
    gfx3DMatrix(),
    gfx3DMatrix(),
  };
  nsTArray<nsRefPtr<Layer> > layers;

  nsRefPtr<LayerManager> lm;
  nsRefPtr<Layer> root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, transforms, lm, layers);

  // B2G g++ doesn't like ASSERT_NE with nullptr directly. It thinks it's
  // an int.
  Layer* nullLayer = nullptr;
  ASSERT_NE(nullLayer, layers[0]->AsContainerLayer());
  ASSERT_NE(nullLayer, layers[1]->AsContainerLayer());
  ASSERT_NE(nullLayer, layers[2]->AsThebesLayer());
  ASSERT_NE(nullLayer, layers[3]->AsThebesLayer());
}

