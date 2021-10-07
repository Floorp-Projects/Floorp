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

class TestLayerManager : public LayerManager {
 public:
  TestLayerManager() : LayerManager() {}

  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) {
    return false;
  }
  virtual void GetBackendName(nsAString& aName) {}
  virtual LayersBackend GetBackendType() { return LayersBackend::LAYERS_BASIC; }
  virtual bool BeginTransaction(const nsCString& = nsCString()) { return true; }
  virtual void SetRoot(Layer* aLayer) {}
  virtual bool BeginTransactionWithTarget(gfxContext* aTarget,
                                          const nsCString& = nsCString()) {
    return true;
  }
  virtual int32_t GetMaxTextureSize() const { return 0; }
};

class TestUserData : public LayerUserData {
 public:
  MOCK_METHOD0(Die, void());
  virtual ~TestUserData() { Die(); }
};
