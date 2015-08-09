/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "TiledLayerBuffer.h"

#include "gtest/gtest.h"

namespace mozilla {
namespace layers {

TEST(TiledLayerBuffer, TileStart) {
  ASSERT_EQ(RoundDownToTileEdge(10, 256), 0);
  ASSERT_EQ(RoundDownToTileEdge(-10, 256), -256);
}

TEST(TiledLayerBuffer, TilesPlacement) {
  for (int firstY = -10; firstY < 10; ++firstY) {
    for (int firstX = -10; firstX < 10; ++firstX) {
      for (int height = 1; height < 10; ++height) {
        for (int width = 1; width < 10; ++width) {

          const TilesPlacement p1 = TilesPlacement(firstX, firstY, width, height);
          // Check that HasTile returns false with some positions that we know
          // not to be in the rectangle of the TilesPlacement.
          ASSERT_FALSE(p1.HasTile(TileIntPoint(firstX - 1, 0)));
          ASSERT_FALSE(p1.HasTile(TileIntPoint(0, firstY - 1)));
          ASSERT_FALSE(p1.HasTile(TileIntPoint(firstX + width + 1,  0)));
          ASSERT_FALSE(p1.HasTile(TileIntPoint(0, firstY + height + 1)));

          // Verify that all positions within the rect that defines the
          // TilesPlacement map to indices between 0 and width*height.
          for (int y = firstY; y < (firstY+height); ++y) {
            for (int x = firstX; x < (firstX+width); ++x) {
              ASSERT_TRUE(p1.HasTile(TileIntPoint(x,y)));
              ASSERT_TRUE(p1.TileIndex(TileIntPoint(x, y)) >= 0);
              ASSERT_TRUE(p1.TileIndex(TileIntPoint(x, y)) < width * height);
            }
          }

          // XXX - This causes some versions of gcc to warn that it optimizes
          // away the test, which gets caught in -WError in PGO builds.
          // The lazy thing to do is to just comment this out since this specific
          // test isn't critically important, but we should remove the warning instead.
          // cf. bug 1179287
          //
          // Verify that indices map to positions that are within the rect that
          // defines the TilesPlacement.
          // for (int i = 0; i < width * height; ++i) {
          //   ASSERT_TRUE(p1.TilePosition(i).x >= firstX);
          //   ASSERT_TRUE(p1.TilePosition(i).x < firstX + width);
          //   ASSERT_TRUE(p1.TilePosition(i).y >= firstY);
          //   ASSERT_TRUE(p1.TilePosition(i).y < firstY + height);
          // }

        }
      }
    }
  }
}

}
}
