/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestBugs.h"
#include "2D.h"
#include <string.h>

using namespace mozilla;
using namespace mozilla::gfx;

TestBugs::TestBugs() {
  REGISTER_TEST(TestBugs, CairoClip918671);
  REGISTER_TEST(TestBugs, PushPopClip950550);
}

void TestBugs::CairoClip918671() {
  RefPtr<DrawTarget> dt = Factory::CreateDrawTarget(
      BackendType::CAIRO, IntSize(100, 100), SurfaceFormat::B8G8R8A8);
  RefPtr<DrawTarget> ref = Factory::CreateDrawTarget(
      BackendType::CAIRO, IntSize(100, 100), SurfaceFormat::B8G8R8A8);
  // Create a path that extends around the center rect but doesn't intersect it.
  RefPtr<PathBuilder> pb1 = dt->CreatePathBuilder();
  pb1->MoveTo(Point(10, 10));
  pb1->LineTo(Point(90, 10));
  pb1->LineTo(Point(90, 20));
  pb1->LineTo(Point(10, 20));
  pb1->Close();
  pb1->MoveTo(Point(90, 90));
  pb1->LineTo(Point(91, 90));
  pb1->LineTo(Point(91, 91));
  pb1->LineTo(Point(91, 90));
  pb1->Close();

  RefPtr<Path> path1 = pb1->Finish();
  dt->PushClip(path1);

  // This center rect must NOT be rectilinear!
  RefPtr<PathBuilder> pb2 = dt->CreatePathBuilder();
  pb2->MoveTo(Point(50, 50));
  pb2->LineTo(Point(55, 51));
  pb2->LineTo(Point(54, 55));
  pb2->LineTo(Point(50, 56));
  pb2->Close();

  RefPtr<Path> path2 = pb2->Finish();
  dt->PushClip(path2);

  dt->FillRect(Rect(0, 0, 100, 100), ColorPattern(DeviceColor(1, 0, 0)));

  RefPtr<SourceSurface> surf1 = dt->Snapshot();
  RefPtr<SourceSurface> surf2 = ref->Snapshot();

  RefPtr<DataSourceSurface> dataSurf1 = surf1->GetDataSurface();
  RefPtr<DataSourceSurface> dataSurf2 = surf2->GetDataSurface();

  DataSourceSurface::ScopedMap map1(dataSurf1, DataSourceSurface::READ);
  DataSourceSurface::ScopedMap map2(dataSurf2, DataSourceSurface::READ);
  for (int y = 0; y < dt->GetSize().height; y++) {
    VERIFY(memcmp(map1.GetData() + y * map1.GetStride(),
                  map2.GetData() + y * map2.GetStride(),
                  dataSurf1->GetSize().width * 4) == 0);
  }
}

void TestBugs::PushPopClip950550() {
  RefPtr<DrawTarget> dt = Factory::CreateDrawTarget(
      BackendType::CAIRO, IntSize(500, 500), SurfaceFormat::B8G8R8A8);
  dt->PushClipRect(Rect(0, 0, 100, 100));
  Matrix m(1, 0, 0, 1, 45, -100);
  dt->SetTransform(m);
  dt->PopClip();

  // We fail the test if we assert in this call because our draw target's
  // transforms are out of sync.
  dt->FillRect(Rect(50, 50, 50, 50),
               ColorPattern(DeviceColor(0.5f, 0, 0, 1.0f)));
}
