/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "Layers.h"

using namespace mozilla;
using namespace mozilla::layers;

class TestLayer: public Layer {
public:
  TestLayer(LayerManager* aManager)
    : Layer(aManager, nullptr)
  {}

  virtual const char* Name() const {
    return "TestLayer";
  }

  virtual LayerType GetType() const {
    return TYPE_CONTAINER;
  }

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface) {
    MOZ_CRASH();
  }
};

class TestUserData: public LayerUserData {
public:
  MOCK_METHOD0(Die, void());
  virtual ~TestUserData() { Die(); }
};


TEST(Layers, LayerConstructor) {
  TestLayer layer(nullptr);
}

TEST(Layers, Defaults) {
  TestLayer layer(nullptr);
  ASSERT_EQ(1.0, layer.GetOpacity());
  ASSERT_EQ(1.0f, layer.GetPostXScale());
  ASSERT_EQ(1.0f, layer.GetPostYScale());

  ASSERT_EQ(nullptr, layer.GetNextSibling());
  ASSERT_EQ(nullptr, layer.GetPrevSibling());
  ASSERT_EQ(nullptr, layer.GetFirstChild());
  ASSERT_EQ(nullptr, layer.GetLastChild());
}

TEST(Layers, Transform) {
  TestLayer layer(nullptr);

  gfx3DMatrix identity;
  ASSERT_EQ(true, identity.IsIdentity());

  ASSERT_EQ(identity, layer.GetTransform());
}

TEST(Layers, Type) {
  TestLayer layer(nullptr);
  ASSERT_EQ(nullptr, layer.AsThebesLayer());
  ASSERT_EQ(nullptr, layer.AsRefLayer());
  ASSERT_EQ(nullptr, layer.AsColorLayer());
}

TEST(Layers, UserData) {
  TestLayer* layerPtr = new TestLayer(nullptr);
  TestLayer& layer = *layerPtr;

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

