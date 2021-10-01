/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "TestLayers.h"
#include "gfxPlatform.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "LayerUserData.h"
#include "mozilla/layers/CompositorBridgeParent.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;

class TestContainerLayer : public ContainerLayer {
 public:
  explicit TestContainerLayer(LayerManager* aManager)
      : ContainerLayer(aManager, nullptr) {}

  virtual const char* Name() const { return "TestContainerLayer"; }

  virtual LayerType GetType() const { return TYPE_CONTAINER; }

  virtual void ComputeEffectiveTransforms(
      const Matrix4x4& aTransformToSurface) {
    DefaultComputeEffectiveTransforms(aTransformToSurface);
  }
};

class TestLayerManager : public LayerManager {
 public:
  TestLayerManager() : LayerManager() {}

  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) {
    return false;
  }
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer() {
    RefPtr<ContainerLayer> layer = new TestContainerLayer(this);
    return layer.forget();
  }
  virtual void GetBackendName(nsAString& aName) {}
  virtual LayersBackend GetBackendType() { return LayersBackend::LAYERS_BASIC; }
  virtual bool BeginTransaction(const nsCString& = nsCString()) { return true; }
  virtual already_AddRefed<ColorLayer> CreateColorLayer() {
    MOZ_CRASH("Not implemented.");
  }
  virtual void SetRoot(Layer* aLayer) {}
  virtual bool BeginTransactionWithTarget(gfxContext* aTarget,
                                          const nsCString& = nsCString()) {
    return true;
  }
  virtual void EndTransaction(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT) {}
  virtual int32_t GetMaxTextureSize() const { return 0; }
};

class TestUserData : public LayerUserData {
 public:
  MOCK_METHOD0(Die, void());
  virtual ~TestUserData() { Die(); }
};

TEST(Layers, LayerConstructor)
{ TestContainerLayer layer(nullptr); }

TEST(Layers, Defaults)
{
  TestContainerLayer layer(nullptr);
  ASSERT_EQ(1.0, layer.GetOpacity());
  ASSERT_EQ(1.0f, layer.GetPostXScale());
  ASSERT_EQ(1.0f, layer.GetPostYScale());

  ASSERT_EQ(nullptr, layer.GetNextSibling());
  ASSERT_EQ(nullptr, layer.GetPrevSibling());
  ASSERT_EQ(nullptr, layer.GetFirstChild());
  ASSERT_EQ(nullptr, layer.GetLastChild());
}

TEST(Layers, Transform)
{
  TestContainerLayer layer(nullptr);

  Matrix4x4 identity;
  ASSERT_EQ(true, identity.IsIdentity());

  ASSERT_EQ(identity, layer.GetTransform());
}

TEST(Layers, Type)
{
  TestContainerLayer layer(nullptr);
  ASSERT_EQ(nullptr, layer.AsRefLayer());
  ASSERT_EQ(nullptr, layer.AsColorLayer());
}

TEST(Layers, UserData)
{
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
