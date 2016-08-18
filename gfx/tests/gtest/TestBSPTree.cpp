/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BSPTree.h"
#include "Polygon.h"

#include "gtest/gtest.h"

#include <deque>

using mozilla::layers::BSPTree;
using mozilla::gfx::Point3D;
using mozilla::gfx::Polygon3D;

namespace
{

// Compares two points while allowing some numerical inaccuracy.
static bool FuzzyCompare(const Point3D& lhs, const Point3D& rhs)
{
  const float epsilon = 0.001f;
  const Point3D d = lhs - rhs;

  return (abs(d.x) > epsilon || abs(d.y) > epsilon || abs(d.z) > epsilon);
}

// Compares the points of two polygons and ensures
// that the points are in the same winding order.
static bool operator==(const Polygon3D& lhs, const Polygon3D& rhs)
{
  const nsTArray<Point3D>& left = lhs.GetPoints();
  const nsTArray<Point3D>& right = rhs.GetPoints();

  // Polygons do not have the same amount of points.
  if (left.Length() != right.Length()) {
    return false;
  }

  const size_t pointCount = left.Length();

  // Find the first vertex of the first polygon from the second polygon.
  // This assumes that the polygons do not contain duplicate vertices.
  int start = -1;
  for (size_t i = 0; i < pointCount; ++i) {
    if (FuzzyCompare(left[0], right[i])) {
      start = i;
      break;
    }
  }

  // Found at least one different vertex.
  if (start == -1) {
    return false;
  }

  // Verify that the polygons have the same points.
  for (size_t i = 0; i < pointCount; ++i) {
    size_t j = (start + i) % pointCount;

    if (!FuzzyCompare(left[i], right[j])) {
      return false;
    }
  }

  return true;
}

static void RunTest(std::deque<Polygon3D> aPolygons,
                    std::deque<Polygon3D> aExpected)
{
  const BSPTree tree(aPolygons);
  const nsTArray<Polygon3D> order = tree.GetDrawOrder();

  EXPECT_EQ(order.Length(), aExpected.size());

  for (size_t i = 0; i < order.Length(); ++i) {
    EXPECT_TRUE(order[i] == aExpected[i]);
  }
}

}

TEST(BSPTree, SameNode)
{
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(0.0f, 0.0f, 0.0f),
      Point3D(1.0f, 0.0f, 0.0f),
      Point3D(1.0f, 1.0f, 0.0f),
      Point3D(0.0f, 1.0f, 0.0f)
    },
    Polygon3D {
      Point3D(0.0f, 0.0f, 0.0f),
      Point3D(1.0f, 0.0f, 0.0f),
      Point3D(1.0f, 1.0f, 0.0f),
      Point3D(0.0f, 1.0f, 0.0f)
    }
  };

  ::RunTest(polygons, polygons);
}

TEST(BSPTree, OneChild)
{
  const Polygon3D p1 {
    Point3D(0.0f, 0.0f, 0.0f),
    Point3D(1.0f, 0.0f, 0.0f),
    Point3D(1.0f, 1.0f, 0.0f),
    Point3D(0.0f, 1.0f, 0.0f)
  };

  const Polygon3D p2 {
    Point3D(0.0f, 0.0f, 1.0f),
    Point3D(1.0f, 0.0f, 1.0f),
    Point3D(1.0f, 1.0f, 1.0f),
    Point3D(0.0f, 1.0f, 1.0f)
  };

  ::RunTest(std::deque<Polygon3D> {p1, p2}, {p1, p2});
  ::RunTest(std::deque<Polygon3D> {p2, p1}, {p1, p2});
}

TEST(BSPTree, SplitSimple1)
{
  Polygon3D p1 {
    Point3D(0.0f, 0.0f, 1.0f),
    Point3D(1.0f, 0.0f, 1.0f),
    Point3D(1.0f, 1.0f, 1.0f),
    Point3D(0.0f, 1.0f, 1.0f)
  };

  Polygon3D p2 {
    Point3D(0.0f, 0.0f, 2.0f),
    Point3D(1.0f, 0.0f, 2.0f),
    Point3D(1.0f, 1.0f, 0.0f),
    Point3D(0.0f, 1.0f, 0.0f)
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.0f, 1.0f, 0.0f),
      Point3D(0.0f, 0.5f, 1.0f),
      Point3D(1.0f, 0.5f, 1.0f),
      Point3D(1.0f, 1.0f, 0.0f)
    },
    p1,
    Polygon3D {
      Point3D(0.0f, 0.0f, 2.0f),
      Point3D(1.0f, 0.0f, 2.0f),
      Point3D(1.0f, 0.5f, 1.0f),
      Point3D(0.0f, 0.5f, 1.0f)
    }
  };

  ::RunTest({p1, p2}, expected);
}

TEST(BSPTree, SplitSimple2) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-5.00000f, -5.00000f, 0.00000f),
      Point3D(-5.00000f, 5.00000f, 0.00000f),
      Point3D(5.00000f, 5.00000f, 0.00000f),
      Point3D(5.00000f, -5.00000f, 0.00000f)
    },
    Polygon3D {
      Point3D(0.00000f, -5.00000f, -5.00000f),
      Point3D(0.00000f, 5.00000f, -5.00000f),
      Point3D(0.00000f, 5.00000f, 5.00000f),
      Point3D(0.00000f, -5.00000f, 5.00000f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00000f, -5.00000f, 0.00000f),
      Point3D(0.00000f, -5.00000f, -5.00000f),
      Point3D(0.00000f, 5.00000f, -5.00000f),
      Point3D(0.00000f, 5.00000f, 0.00000f)
    },
    Polygon3D {
      Point3D(-5.00000f, -5.00000f, 0.00000f),
      Point3D(-5.00000f, 5.00000f, 0.00000f),
      Point3D(5.00000f, 5.00000f, 0.00000f),
      Point3D(5.00000f, -5.00000f, 0.00000f)
    },
    Polygon3D {
      Point3D(0.00000f, 5.00000f, 0.00000f),
      Point3D(0.00000f, 5.00000f, 5.00000f),
      Point3D(0.00000f, -5.00000f, 5.00000f),
      Point3D(0.00000f, -5.00000f, 0.00000f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, NoSplit1) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(0.00000f, 10.00000f, 0.00000f),
      Point3D(0.00000f, 0.00000f, 0.00000f),
      Point3D(10.00000f, 0.00000f, 0.00000f),
      Point3D(10.00000f, 10.00000f, 0.00000f)
    },
    Polygon3D {
      Point3D(0.00000f, 10.00000f, -5.00000f),
      Point3D(0.00000f, 0.00000f, -5.00000f),
      Point3D(10.00000f, 0.00000f, -5.00000f),
      Point3D(10.00000f, 10.00000f, -5.00000f)
    },
    Polygon3D {
      Point3D(0.00000f, 10.00000f, 5.00000f),
      Point3D(0.00000f, 0.00000f, 5.00000f),
      Point3D(10.00000f, 0.00000f, 5.00000f),
      Point3D(10.00000f, 10.00000f, 5.00000f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00000f, 10.00000f, -5.00000f),
      Point3D(0.00000f, 0.00000f, -5.00000f),
      Point3D(10.00000f, 0.00000f, -5.00000f),
      Point3D(10.00000f, 10.00000f, -5.00000f)
    },
    Polygon3D {
      Point3D(0.00000f, 10.00000f, 0.00000f),
      Point3D(0.00000f, 0.00000f, 0.00000f),
      Point3D(10.00000f, 0.00000f, 0.00000f),
      Point3D(10.00000f, 10.00000f, 0.00000f)
    },
    Polygon3D {
      Point3D(0.00000f, 10.00000f, 5.00000f),
      Point3D(0.00000f, 0.00000f, 5.00000f),
      Point3D(10.00000f, 0.00000f, 5.00000f),
      Point3D(10.00000f, 10.00000f, 5.00000f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, NoSplit2) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-5.00000f, -5.00000f, 0.00000f),
      Point3D(-5.00000f, 5.00000f, 0.00000f),
      Point3D(5.00000f, 5.00000f, 0.00000f),
      Point3D(5.00000f, -5.00000f, 0.00000f)
    },
    Polygon3D {
      Point3D(0.00000f, 5.00000f, -15.00000f),
      Point3D(0.00000f, -5.00000f, -15.00000f),
      Point3D(0.00000f, -5.00000f, -10.00000f),
      Point3D(0.00000f, 5.00000f, -10.00000f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00000f, 5.00000f, -15.00000f),
      Point3D(0.00000f, -5.00000f, -15.00000f),
      Point3D(0.00000f, -5.00000f, -10.00000f),
      Point3D(0.00000f, 5.00000f, -10.00000f)
    },
    Polygon3D {
      Point3D(-5.00000f, -5.00000f, 0.00000f),
      Point3D(-5.00000f, 5.00000f, 0.00000f),
      Point3D(5.00000f, 5.00000f, 0.00000f),
      Point3D(5.00000f, -5.00000f, 0.00000f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, SplitComplex1) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(0.00000f, -5.00000f, -15.00000f),
      Point3D(0.00000f, 5.00000f, -15.00000f),
      Point3D(0.00000f, 5.00000f, -10.00000f),
      Point3D(0.00000f, -5.00000f, -10.00000f)
    },
    Polygon3D {
      Point3D(-5.00000f, -5.00000f, 0.00000f),
      Point3D(-5.00000f, 5.00000f, 0.00000f),
      Point3D(5.00000f, 5.00000f, 0.00000f),
      Point3D(5.00000f, -5.00000f, 0.00000f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00000f, 5.00000f, 0.00000f),
      Point3D(5.00000f, 5.00000f, 0.00000f),
      Point3D(5.00000f, -5.00000f, 0.00000f),
      Point3D(0.00000f, -5.00000f, 0.00000f)
    },
    Polygon3D {
      Point3D(0.00000f, -5.00000f, -15.00000f),
      Point3D(0.00000f, 5.00000f, -15.00000f),
      Point3D(0.00000f, 5.00000f, -10.00000f),
      Point3D(0.00000f, -5.00000f, -10.00000f)
    },
    Polygon3D {
      Point3D(0.00000f, -5.00000f, 0.00000f),
      Point3D(-5.00000f, -5.00000f, 0.00000f),
      Point3D(-5.00000f, 5.00000f, 0.00000f),
      Point3D(0.00000f, 5.00000f, 0.00000f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, SplitComplex2) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-5.00000f, -5.00000f, 0.00000f),
      Point3D(-5.00000f, 5.00000f, 0.00000f),
      Point3D(5.00000f, 5.00000f, 0.00000f),
      Point3D(5.00000f, -5.00000f, 0.00000f)
    },
    Polygon3D {
      Point3D(0.00000f, -5.00000f, -5.00000f),
      Point3D(0.00000f, 5.00000f, -5.00000f),
      Point3D(0.00000f, 5.00000f, 5.00000f),
      Point3D(0.00000f, -5.00000f, 5.00000f)
    },
    Polygon3D {
      Point3D(-5.00000f, 0.00000f, -5.00000f),
      Point3D(-5.00000f, 0.00000f, 5.00000f),
      Point3D(5.00000f, 0.00000f, 5.00000f),
      Point3D(5.00000f, 0.00000f, -5.00000f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00000f, 0.00000f, 0.00000f),
      Point3D(5.00000f, 0.00000f, 0.00000f),
      Point3D(5.00000f, 0.00000f, -5.00000f),
      Point3D(0.00000f, 0.00000f, -5.00000f)
    },
    Polygon3D {
      Point3D(0.00000f, -5.00000f, 0.00000f),
      Point3D(0.00000f, -5.00000f, -5.00000f),
      Point3D(0.00000f, 5.00000f, -5.00000f),
      Point3D(0.00000f, 5.00000f, 0.00000f)
    },
    Polygon3D {
      Point3D(0.00000f, 0.00000f, -5.00000f),
      Point3D(-5.00000f, 0.00000f, -5.00000f),
      Point3D(-5.00000f, 0.00000f, 0.00000f),
      Point3D(0.00000f, 0.00000f, 0.00000f)
    },
    Polygon3D {
      Point3D(-5.00000f, -5.00000f, 0.00000f),
      Point3D(-5.00000f, 5.00000f, 0.00000f),
      Point3D(5.00000f, 5.00000f, 0.00000f),
      Point3D(5.00000f, -5.00000f, 0.00000f)
    },
    Polygon3D {
      Point3D(0.00000f, 0.00000f, 5.00000f),
      Point3D(5.00000f, 0.00000f, 5.00000f),
      Point3D(5.00000f, 0.00000f, 0.00000f),
      Point3D(0.00000f, 0.00000f, 0.00000f)
    },
    Polygon3D {
      Point3D(0.00000f, 5.00000f, 0.00000f),
      Point3D(0.00000f, 5.00000f, 5.00000f),
      Point3D(0.00000f, -5.00000f, 5.00000f),
      Point3D(0.00000f, -5.00000f, 0.00000f)
    },
    Polygon3D {
      Point3D(0.00000f, 0.00000f, 0.00000f),
      Point3D(-5.00000f, 0.00000f, 0.00000f),
      Point3D(-5.00000f, 0.00000f, 5.00000f),
      Point3D(0.00000f, 0.00000f, 5.00000f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate0degrees) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-0.00000f, 2.00000f, 2.00000f),
      Point3D(-0.00000f, -2.00000f, 2.00000f),
      Point3D(0.00010f, -2.00000f, -2.00000f),
      Point3D(0.00010f, 2.00000f, -2.00000f)
    },
    Polygon3D {
      Point3D(2.00000f, 0.00000f, 2.00000f),
      Point3D(2.00000f, -0.00000f, -2.00000f),
      Point3D(-2.00000f, 0.00000f, -2.00000f),
      Point3D(-2.00000f, 0.00010f, 2.00000f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00010f, 0.00000f, -2.00000f),
      Point3D(-2.00000f, 0.00000f, -2.00000f),
      Point3D(-2.00000f, 0.00010f, 2.00000f),
      Point3D(0.00000f, 0.00005f, 2.00000f)
    },
    Polygon3D {
      Point3D(-0.00000f, 2.00000f, 2.00000f),
      Point3D(-0.00000f, -2.00000f, 2.00000f),
      Point3D(0.00010f, -2.00000f, -2.00000f),
      Point3D(0.00010f, 2.00000f, -2.00000f)
    },
    Polygon3D {
      Point3D(0.00000f, 0.00005f, 2.00000f),
      Point3D(2.00000f, 0.00000f, 2.00000f),
      Point3D(2.00000f, -0.00000f, -2.00000f),
      Point3D(0.00010f, 0.00000f, -2.00000f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate20degrees) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-0.00000f, 1.19540f, 2.56350f),
      Point3D(-0.00000f, -2.56340f, 1.19540f),
      Point3D(0.00010f, -1.19530f, -2.56340f),
      Point3D(0.00010f, 2.56350f, -1.19530f)
    },
    Polygon3D {
      Point3D(2.00000f, -0.68400f, 1.87940f),
      Point3D(2.00000f, 0.68410f, -1.87930f),
      Point3D(-2.00000f, 0.68410f, -1.87930f),
      Point3D(-2.00000f, -0.68400f, 1.87940f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00010f, 0.68410f, -1.87930f),
      Point3D(-2.00000f, 0.68410f, -1.87930f),
      Point3D(-2.00000f, -0.68400f, 1.87940f),
      Point3D(0.00000f, -0.68400f, 1.87940f)
    },
    Polygon3D {
      Point3D(-0.00000f, 1.19540f, 2.56350f),
      Point3D(-0.00000f, -2.56340f, 1.19540f),
      Point3D(0.00010f, -1.19530f, -2.56340f),
      Point3D(0.00010f, 2.56350f, -1.19530f)
    },
    Polygon3D {
      Point3D(0.00000f, -0.68400f, 1.87940f),
      Point3D(2.00000f, -0.68400f, 1.87940f),
      Point3D(2.00000f, 0.68410f, -1.87930f),
      Point3D(0.00010f, 0.68410f, -1.87930f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate40degrees) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-0.00000f, -0.73200f, 2.73210f),
      Point3D(-0.00000f, -2.73200f, -0.73200f),
      Point3D(0.00010f, 0.73210f, -2.73200f),
      Point3D(0.00010f, 2.73210f, 0.73210f)
    },
    Polygon3D {
      Point3D(2.00000f, -1.73200f, 1.00000f),
      Point3D(2.00000f, 1.73210f, -0.99990f),
      Point3D(-2.00000f, 1.73210f, -0.99990f),
      Point3D(-2.00000f, -1.73200f, 1.00000f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00010f, 1.73210f, -0.99990f),
      Point3D(-2.00000f, 1.73210f, -0.99990f),
      Point3D(-2.00000f, -1.73200f, 1.00000f),
      Point3D(0.00000f, -1.73200f, 1.00000f)
    },
    Polygon3D {
      Point3D(-0.00000f, -0.73200f, 2.73210f),
      Point3D(-0.00000f, -2.73200f, -0.73200f),
      Point3D(0.00010f, 0.73210f, -2.73200f),
      Point3D(0.00010f, 2.73210f, 0.73210f)
    },
    Polygon3D {
      Point3D(0.00000f, -1.73200f, 1.00000f),
      Point3D(2.00000f, -1.73200f, 1.00000f),
      Point3D(2.00000f, 1.73210f, -0.99990f),
      Point3D(0.00010f, 1.73210f, -0.99990f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate60degrees) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-0.00000f, -2.73200f, 0.73210f),
      Point3D(-0.00000f, -0.73200f, -2.73200f),
      Point3D(0.00010f, 2.73210f, -0.73200f),
      Point3D(0.00010f, 0.73210f, 2.73210f)
    },
    Polygon3D {
      Point3D(2.00000f, -1.73200f, -1.00000f),
      Point3D(2.00000f, 1.73210f, 1.00010f),
      Point3D(-2.00000f, 1.73210f, 1.00010f),
      Point3D(-2.00000f, -1.73200f, -1.00000f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00000f, -1.73200f, -1.00000f),
      Point3D(2.00000f, -1.73200f, -1.00000f),
      Point3D(2.00000f, 1.73210f, 1.00010f),
      Point3D(0.00010f, 1.73210f, 1.00010f)
    },
    Polygon3D {
      Point3D(-0.00000f, -2.73200f, 0.73210f),
      Point3D(-0.00000f, -0.73200f, -2.73200f),
      Point3D(0.00010f, 2.73210f, -0.73200f),
      Point3D(0.00010f, 0.73210f, 2.73210f)
    },
    Polygon3D {
      Point3D(0.00010f, 1.73210f, 1.00010f),
      Point3D(-2.00000f, 1.73210f, 1.00010f),
      Point3D(-2.00000f, -1.73200f, -1.00000f),
      Point3D(0.00000f, -1.73200f, -1.00000f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate80degrees) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-0.00000f, -1.19530f, -2.56340f),
      Point3D(-0.00000f, 2.56350f, -1.19530f),
      Point3D(0.00010f, 1.19540f, 2.56350f),
      Point3D(0.00010f, -2.56340f, 1.19540f)
    },
    Polygon3D {
      Point3D(2.00000f, 0.68410f, -1.87930f),
      Point3D(2.00000f, -0.68400f, 1.87940f),
      Point3D(-2.00000f, -0.68400f, 1.87940f),
      Point3D(-2.00000f, 0.68410f, -1.87930f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00000f, 0.68410f, -1.87930f),
      Point3D(2.00000f, 0.68410f, -1.87930f),
      Point3D(2.00000f, -0.68400f, 1.87940f),
      Point3D(0.00010f, -0.68400f, 1.87940f)
    },
    Polygon3D {
      Point3D(-0.00000f, -1.19530f, -2.56340f),
      Point3D(-0.00000f, 2.56350f, -1.19530f),
      Point3D(0.00010f, 1.19540f, 2.56350f),
      Point3D(0.00010f, -2.56340f, 1.19540f)
    },
    Polygon3D {
      Point3D(0.00010f, -0.68400f, 1.87940f),
      Point3D(-2.00000f, -0.68400f, 1.87940f),
      Point3D(-2.00000f, 0.68410f, -1.87930f),
      Point3D(0.00000f, 0.68410f, -1.87930f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate100degrees) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-0.00000f, 2.73210f, -0.73200f),
      Point3D(-0.00000f, 0.73210f, 2.73210f),
      Point3D(0.00010f, -2.73200f, 0.73210f),
      Point3D(0.00010f, -0.73200f, -2.73200f)
    },
    Polygon3D {
      Point3D(2.00000f, 1.73210f, 1.00010f),
      Point3D(2.00000f, -1.73200f, -1.00000f),
      Point3D(-2.00000f, -1.73200f, -1.00000f),
      Point3D(-2.00000f, 1.73210f, 1.00010f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00010f, -1.73200f, -1.00000f),
      Point3D(-2.00000f, -1.73200f, -1.00000f),
      Point3D(-2.00000f, 1.73210f, 1.00010f),
      Point3D(0.00000f, 1.73210f, 1.00010f)
    },
    Polygon3D {
      Point3D(-0.00000f, 2.73210f, -0.73200f),
      Point3D(-0.00000f, 0.73210f, 2.73210f),
      Point3D(0.00010f, -2.73200f, 0.73210f),
      Point3D(0.00010f, -0.73200f, -2.73200f)
    },
    Polygon3D {
      Point3D(0.00000f, 1.73210f, 1.00010f),
      Point3D(2.00000f, 1.73210f, 1.00010f),
      Point3D(2.00000f, -1.73200f, -1.00000f),
      Point3D(0.00010f, -1.73200f, -1.00000f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate120degrees) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-0.00000f, -0.73200f, 2.73210f),
      Point3D(-0.00000f, -2.73200f, -0.73200f),
      Point3D(0.00010f, 0.73210f, -2.73200f),
      Point3D(0.00010f, 2.73210f, 0.73210f)
    },
    Polygon3D {
      Point3D(2.00000f, -1.73200f, 1.00000f),
      Point3D(2.00000f, 1.73210f, -0.99990f),
      Point3D(-2.00000f, 1.73210f, -0.99990f),
      Point3D(-2.00000f, -1.73200f, 1.00000f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00010f, 1.73210f, -0.99990f),
      Point3D(-2.00000f, 1.73210f, -0.99990f),
      Point3D(-2.00000f, -1.73200f, 1.00000f),
      Point3D(0.00000f, -1.73200f, 1.00000f)
    },
    Polygon3D {
      Point3D(-0.00000f, -0.73200f, 2.73210f),
      Point3D(-0.00000f, -2.73200f, -0.73200f),
      Point3D(0.00010f, 0.73210f, -2.73200f),
      Point3D(0.00010f, 2.73210f, 0.73210f)
    },
    Polygon3D {
      Point3D(0.00000f, -1.73200f, 1.00000f),
      Point3D(2.00000f, -1.73200f, 1.00000f),
      Point3D(2.00000f, 1.73210f, -0.99990f),
      Point3D(0.00010f, 1.73210f, -0.99990f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate140degrees) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-0.00000f, -1.19530f, -2.56340f),
      Point3D(-0.00000f, 2.56350f, -1.19530f),
      Point3D(0.00010f, 1.19540f, 2.56350f),
      Point3D(0.00010f, -2.56340f, 1.19540f)
    },
    Polygon3D {
      Point3D(2.00000f, 0.68410f, -1.87930f),
      Point3D(2.00000f, -0.68400f, 1.87940f),
      Point3D(-2.00000f, -0.68400f, 1.87940f),
      Point3D(-2.00000f, 0.68410f, -1.87930f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00000f, 0.68410f, -1.87930f),
      Point3D(2.00000f, 0.68410f, -1.87930f),
      Point3D(2.00000f, -0.68400f, 1.87940f),
      Point3D(0.00010f, -0.68400f, 1.87940f)
    },
    Polygon3D {
      Point3D(-0.00000f, -1.19530f, -2.56340f),
      Point3D(-0.00000f, 2.56350f, -1.19530f),
      Point3D(0.00010f, 1.19540f, 2.56350f),
      Point3D(0.00010f, -2.56340f, 1.19540f)
    },
    Polygon3D {
      Point3D(0.00010f, -0.68400f, 1.87940f),
      Point3D(-2.00000f, -0.68400f, 1.87940f),
      Point3D(-2.00000f, 0.68410f, -1.87930f),
      Point3D(0.00000f, 0.68410f, -1.87930f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate160degrees) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-0.00000f, 2.00000f, 2.00000f),
      Point3D(-0.00000f, -2.00000f, 2.00000f),
      Point3D(0.00010f, -2.00000f, -2.00000f),
      Point3D(0.00010f, 2.00000f, -2.00000f)
    },
    Polygon3D {
      Point3D(2.00000f, -0.00000f, 2.00000f),
      Point3D(2.00000f, 0.00010f, -2.00000f),
      Point3D(-2.00000f, 0.00010f, -2.00000f),
      Point3D(-2.00000f, -0.00000f, 2.00000f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00010f, 0.00010f, -2.00000f),
      Point3D(-2.00000f, 0.00010f, -2.00000f),
      Point3D(-2.00000f, -0.00000f, 2.00000f),
      Point3D(0.00000f, 0.00000f, 2.00000f)
    },
    Polygon3D {
      Point3D(-0.00000f, 2.00000f, 2.00000f),
      Point3D(-0.00000f, -2.00000f, 2.00000f),
      Point3D(0.00010f, -2.00000f, -2.00000f),
      Point3D(0.00010f, 2.00000f, -2.00000f)
    },
    Polygon3D {
      Point3D(0.00000f, 0.00000f, 2.00000f),
      Point3D(2.00000f, -0.00000f, 2.00000f),
      Point3D(2.00000f, 0.00010f, -2.00000f),
      Point3D(0.00010f, 0.00010f, -2.00000f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate180degrees) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-0.00000f, -2.00000f, -2.00000f),
      Point3D(-0.00000f, 2.00000f, -2.00000f),
      Point3D(0.00010f, 2.00000f, 2.00000f),
      Point3D(0.00010f, -2.00000f, 2.00000f)
    },
    Polygon3D {
      Point3D(2.00000f, 0.00010f, -2.00000f),
      Point3D(2.00000f, -0.00000f, 2.00000f),
      Point3D(-2.00000f, -0.00000f, 2.00000f),
      Point3D(-2.00000f, 0.00010f, -2.00000f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00000f, 0.00010f, -2.00000f),
      Point3D(2.00000f, 0.00010f, -2.00000f),
      Point3D(2.00000f, -0.00000f, 2.00000f),
      Point3D(0.00010f, 0.00000f, 2.00000f)
    },
    Polygon3D {
      Point3D(-0.00000f, -2.00000f, -2.00000f),
      Point3D(-0.00000f, 2.00000f, -2.00000f),
      Point3D(0.00010f, 2.00000f, 2.00000f),
      Point3D(0.00010f, -2.00000f, 2.00000f)
    },
    Polygon3D {
      Point3D(0.00010f, 0.00000f, 2.00000f),
      Point3D(-2.00000f, -0.00000f, 2.00000f),
      Point3D(-2.00000f, 0.00010f, -2.00000f),
      Point3D(0.00000f, 0.00010f, -2.00000f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate200degrees) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-0.00000f, 1.19540f, 2.56350f),
      Point3D(-0.00000f, -2.56340f, 1.19540f),
      Point3D(0.00010f, -1.19530f, -2.56340f),
      Point3D(0.00010f, 2.56350f, -1.19530f)
    },
    Polygon3D {
      Point3D(2.00000f, -0.68400f, 1.87940f),
      Point3D(2.00000f, 0.68410f, -1.87930f),
      Point3D(-2.00000f, 0.68410f, -1.87930f),
      Point3D(-2.00000f, -0.68400f, 1.87940f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00010f, 0.68410f, -1.87930f),
      Point3D(-2.00000f, 0.68410f, -1.87930f),
      Point3D(-2.00000f, -0.68400f, 1.87940f),
      Point3D(0.00000f, -0.68400f, 1.87940f)
    },
    Polygon3D {
      Point3D(-0.00000f, 1.19540f, 2.56350f),
      Point3D(-0.00000f, -2.56340f, 1.19540f),
      Point3D(0.00010f, -1.19530f, -2.56340f),
      Point3D(0.00010f, 2.56350f, -1.19530f)
    },
    Polygon3D {
      Point3D(0.00000f, -0.68400f, 1.87940f),
      Point3D(2.00000f, -0.68400f, 1.87940f),
      Point3D(2.00000f, 0.68410f, -1.87930f),
      Point3D(0.00010f, 0.68410f, -1.87930f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate220degrees) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-0.00000f, 0.73210f, -2.73200f),
      Point3D(-0.00000f, 2.73210f, 0.73210f),
      Point3D(0.00010f, -0.73200f, 2.73210f),
      Point3D(0.00010f, -2.73200f, -0.73200f)
    },
    Polygon3D {
      Point3D(2.00000f, 1.73210f, -0.99990f),
      Point3D(2.00000f, -1.73200f, 1.00000f),
      Point3D(-2.00000f, -1.73200f, 1.00000f),
      Point3D(-2.00000f, 1.73210f, -0.99990f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00000f, 1.73210f, -0.99990f),
      Point3D(2.00000f, 1.73210f, -0.99990f),
      Point3D(2.00000f, -1.73200f, 1.00000f),
      Point3D(0.00010f, -1.73200f, 1.00000f)
    },
    Polygon3D {
      Point3D(-0.00000f, 0.73210f, -2.73200f),
      Point3D(-0.00000f, 2.73210f, 0.73210f),
      Point3D(0.00010f, -0.73200f, 2.73210f),
      Point3D(0.00010f, -2.73200f, -0.73200f)
    },
    Polygon3D {
      Point3D(0.00010f, -1.73200f, 1.00000f),
      Point3D(-2.00000f, -1.73200f, 1.00000f),
      Point3D(-2.00000f, 1.73210f, -0.99990f),
      Point3D(0.00000f, 1.73210f, -0.99990f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate240degrees) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-0.00000f, -2.73200f, 0.73210f),
      Point3D(-0.00000f, -0.73200f, -2.73200f),
      Point3D(0.00010f, 2.73210f, -0.73200f),
      Point3D(0.00010f, 0.73210f, 2.73210f)
    },
    Polygon3D {
      Point3D(2.00000f, -1.73200f, -1.00000f),
      Point3D(2.00000f, 1.73210f, 1.00010f),
      Point3D(-2.00000f, 1.73210f, 1.00010f),
      Point3D(-2.00000f, -1.73200f, -1.00000f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00000f, -1.73200f, -1.00000f),
      Point3D(2.00000f, -1.73200f, -1.00000f),
      Point3D(2.00000f, 1.73210f, 1.00010f),
      Point3D(0.00010f, 1.73210f, 1.00010f)
    },
    Polygon3D {
      Point3D(-0.00000f, -2.73200f, 0.73210f),
      Point3D(-0.00000f, -0.73200f, -2.73200f),
      Point3D(0.00010f, 2.73210f, -0.73200f),
      Point3D(0.00010f, 0.73210f, 2.73210f)
    },
    Polygon3D {
      Point3D(0.00010f, 1.73210f, 1.00010f),
      Point3D(-2.00000f, 1.73210f, 1.00010f),
      Point3D(-2.00000f, -1.73200f, -1.00000f),
      Point3D(0.00000f, -1.73200f, -1.00000f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate260degrees) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-0.00000f, 1.19540f, 2.56350f),
      Point3D(-0.00000f, -2.56340f, 1.19540f),
      Point3D(0.00010f, -1.19530f, -2.56340f),
      Point3D(0.00010f, 2.56350f, -1.19530f)
    },
    Polygon3D {
      Point3D(2.00000f, -0.68400f, 1.87940f),
      Point3D(2.00000f, 0.68410f, -1.87930f),
      Point3D(-2.00000f, 0.68410f, -1.87930f),
      Point3D(-2.00000f, -0.68400f, 1.87940f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00010f, 0.68410f, -1.87930f),
      Point3D(-2.00000f, 0.68410f, -1.87930f),
      Point3D(-2.00000f, -0.68400f, 1.87940f),
      Point3D(0.00000f, -0.68400f, 1.87940f)
    },
    Polygon3D {
      Point3D(-0.00000f, 1.19540f, 2.56350f),
      Point3D(-0.00000f, -2.56340f, 1.19540f),
      Point3D(0.00010f, -1.19530f, -2.56340f),
      Point3D(0.00010f, 2.56350f, -1.19530f)
    },
    Polygon3D {
      Point3D(0.00000f, -0.68400f, 1.87940f),
      Point3D(2.00000f, -0.68400f, 1.87940f),
      Point3D(2.00000f, 0.68410f, -1.87930f),
      Point3D(0.00010f, 0.68410f, -1.87930f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate280degrees) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-0.00000f, 2.73210f, -0.73200f),
      Point3D(-0.00000f, 0.73210f, 2.73210f),
      Point3D(0.00010f, -2.73200f, 0.73210f),
      Point3D(0.00010f, -0.73200f, -2.73200f)
    },
    Polygon3D {
      Point3D(2.00000f, 1.73210f, 1.00010f),
      Point3D(2.00000f, -1.73200f, -1.00000f),
      Point3D(-2.00000f, -1.73200f, -1.00000f),
      Point3D(-2.00000f, 1.73210f, 1.00010f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00010f, -1.73200f, -1.00000f),
      Point3D(-2.00000f, -1.73200f, -1.00000f),
      Point3D(-2.00000f, 1.73210f, 1.00010f),
      Point3D(0.00000f, 1.73210f, 1.00010f)
    },
    Polygon3D {
      Point3D(-0.00000f, 2.73210f, -0.73200f),
      Point3D(-0.00000f, 0.73210f, 2.73210f),
      Point3D(0.00010f, -2.73200f, 0.73210f),
      Point3D(0.00010f, -0.73200f, -2.73200f)
    },
    Polygon3D {
      Point3D(0.00000f, 1.73210f, 1.00010f),
      Point3D(2.00000f, 1.73210f, 1.00010f),
      Point3D(2.00000f, -1.73200f, -1.00000f),
      Point3D(0.00010f, -1.73200f, -1.00000f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate300degrees) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-0.00000f, 0.73210f, -2.73200f),
      Point3D(-0.00000f, 2.73210f, 0.73210f),
      Point3D(0.00010f, -0.73200f, 2.73210f),
      Point3D(0.00010f, -2.73200f, -0.73200f)
    },
    Polygon3D {
      Point3D(2.00000f, 1.73210f, -0.99990f),
      Point3D(2.00000f, -1.73200f, 1.00000f),
      Point3D(-2.00000f, -1.73200f, 1.00000f),
      Point3D(-2.00000f, 1.73210f, -0.99990f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00000f, 1.73210f, -0.99990f),
      Point3D(2.00000f, 1.73210f, -0.99990f),
      Point3D(2.00000f, -1.73200f, 1.00000f),
      Point3D(0.00010f, -1.73200f, 1.00000f)
    },
    Polygon3D {
      Point3D(-0.00000f, 0.73210f, -2.73200f),
      Point3D(-0.00000f, 2.73210f, 0.73210f),
      Point3D(0.00010f, -0.73200f, 2.73210f),
      Point3D(0.00010f, -2.73200f, -0.73200f)
    },
    Polygon3D {
      Point3D(0.00010f, -1.73200f, 1.00000f),
      Point3D(-2.00000f, -1.73200f, 1.00000f),
      Point3D(-2.00000f, 1.73210f, -0.99990f),
      Point3D(0.00000f, 1.73210f, -0.99990f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate320degrees) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-0.00000f, -1.19530f, -2.56340f),
      Point3D(-0.00000f, 2.56350f, -1.19530f),
      Point3D(0.00010f, 1.19540f, 2.56350f),
      Point3D(0.00010f, -2.56340f, 1.19540f)
    },
    Polygon3D {
      Point3D(2.00000f, 0.68410f, -1.87930f),
      Point3D(2.00000f, -0.68400f, 1.87940f),
      Point3D(-2.00000f, -0.68400f, 1.87940f),
      Point3D(-2.00000f, 0.68410f, -1.87930f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00000f, 0.68410f, -1.87930f),
      Point3D(2.00000f, 0.68410f, -1.87930f),
      Point3D(2.00000f, -0.68400f, 1.87940f),
      Point3D(0.00010f, -0.68400f, 1.87940f)
    },
    Polygon3D {
      Point3D(-0.00000f, -1.19530f, -2.56340f),
      Point3D(-0.00000f, 2.56350f, -1.19530f),
      Point3D(0.00010f, 1.19540f, 2.56350f),
      Point3D(0.00010f, -2.56340f, 1.19540f)
    },
    Polygon3D {
      Point3D(0.00010f, -0.68400f, 1.87940f),
      Point3D(-2.00000f, -0.68400f, 1.87940f),
      Point3D(-2.00000f, 0.68410f, -1.87930f),
      Point3D(0.00000f, 0.68410f, -1.87930f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate340degrees) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-0.00000f, -2.00000f, -2.00000f),
      Point3D(-0.00000f, 2.00000f, -2.00000f),
      Point3D(0.00010f, 2.00000f, 2.00000f),
      Point3D(0.00010f, -2.00000f, 2.00000f)
    },
    Polygon3D {
      Point3D(2.00000f, 0.00010f, -2.00000f),
      Point3D(2.00000f, -0.00000f, 2.00000f),
      Point3D(-2.00000f, -0.00000f, 2.00000f),
      Point3D(-2.00000f, 0.00010f, -2.00000f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00000f, 0.00010f, -2.00000f),
      Point3D(2.00000f, 0.00010f, -2.00000f),
      Point3D(2.00000f, -0.00000f, 2.00000f),
      Point3D(0.00010f, 0.00000f, 2.00000f)
    },
    Polygon3D {
      Point3D(-0.00000f, -2.00000f, -2.00000f),
      Point3D(-0.00000f, 2.00000f, -2.00000f),
      Point3D(0.00010f, 2.00000f, 2.00000f),
      Point3D(0.00010f, -2.00000f, 2.00000f)
    },
    Polygon3D {
      Point3D(0.00010f, 0.00000f, 2.00000f),
      Point3D(-2.00000f, -0.00000f, 2.00000f),
      Point3D(-2.00000f, 0.00010f, -2.00000f),
      Point3D(0.00000f, 0.00010f, -2.00000f)
    }
  };
  ::RunTest(polygons, expected);
}

TEST(BSPTree, TwoPlaneIntersectRotate360degrees) {
  const std::deque<Polygon3D> polygons {
    Polygon3D {
      Point3D(-0.00000f, -2.00000f, -2.00000f),
      Point3D(-0.00000f, 2.00000f, -2.00000f),
      Point3D(0.00010f, 2.00000f, 2.00000f),
      Point3D(0.00010f, -2.00000f, 2.00000f)
    },
    Polygon3D {
      Point3D(2.00000f, 0.00010f, -2.00000f),
      Point3D(2.00000f, -0.00000f, 2.00000f),
      Point3D(-2.00000f, -0.00000f, 2.00000f),
      Point3D(-2.00000f, 0.00010f, -2.00000f)
    }
  };

  const std::deque<Polygon3D> expected {
    Polygon3D {
      Point3D(0.00000f, 0.00010f, -2.00000f),
      Point3D(2.00000f, 0.00010f, -2.00000f),
      Point3D(2.00000f, -0.00000f, 2.00000f),
      Point3D(0.00010f, 0.00000f, 2.00000f)
    },
    Polygon3D {
      Point3D(-0.00000f, -2.00000f, -2.00000f),
      Point3D(-0.00000f, 2.00000f, -2.00000f),
      Point3D(0.00010f, 2.00000f, 2.00000f),
      Point3D(0.00010f, -2.00000f, 2.00000f)
    },
    Polygon3D {
      Point3D(0.00010f, 0.00000f, 2.00000f),
      Point3D(-2.00000f, -0.00000f, 2.00000f),
      Point3D(-2.00000f, 0.00010f, -2.00000f),
      Point3D(0.00000f, 0.00010f, -2.00000f)
    }
  };
  ::RunTest(polygons, expected);
}
