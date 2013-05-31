/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "TiledLayerBuffer.h"

#include "gtest/gtest.h"

namespace mozilla {
namespace layers {

struct TestTiledLayerTile {
  int value;
  TestTiledLayerTile(int v = 0) {
    value = v;
  }
  bool operator== (const TestTiledLayerTile& o) const {
    return value == o.value;
  }
  bool operator!= (const TestTiledLayerTile& o) const {
    return value != o.value;
  }
};

class TestTiledLayerBuffer : public TiledLayerBuffer<TestTiledLayerBuffer, TestTiledLayerTile>
{
  friend class TiledLayerBuffer<TestTiledLayerBuffer, TestTiledLayerTile>;

public:
  TestTiledLayerTile GetPlaceholderTile() const {
    return TestTiledLayerTile(-1);
  }

  TestTiledLayerTile ValidateTile(TestTiledLayerTile aTile, const nsIntPoint& aTileOrigin, const nsIntRegion& aDirtyRect) {
    return TestTiledLayerTile();
  }

  void ReleaseTile(TestTiledLayerTile aTile)
  {

  }

  void SwapTiles(TestTiledLayerTile& aTileA, TestTiledLayerTile& aTileB)
  {
    TestTiledLayerTile oldTileA = aTileA;
    aTileA = aTileB;
    aTileB = oldTileA;
  }

  void TestUpdate(const nsIntRegion& aNewValidRegion, const nsIntRegion& aPaintRegion)
  {
    Update(aNewValidRegion, aPaintRegion);
  }
};

TEST(TiledLayerBuffer, TileConstructor) {
  TestTiledLayerBuffer buffer;
}

TEST(TiledLayerBuffer, TileStart) {
  TestTiledLayerBuffer buffer;

  ASSERT_EQ(buffer.RoundDownToTileEdge(10), 0);
  ASSERT_EQ(buffer.RoundDownToTileEdge(-10), -256);
}

TEST(TiledLayerBuffer, EmptyUpdate) {
  TestTiledLayerBuffer buffer;

  nsRegion::InitStatic();
  nsIntRegion validRegion(nsIntRect(0, 0, 10, 10));
  buffer.TestUpdate(validRegion, validRegion);

  ASSERT_EQ(buffer.GetValidRegion(), validRegion);
}

}
}
