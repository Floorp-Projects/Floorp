/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Grid.h"

#include "GridArea.h"
#include "GridDimension.h"
#include "mozilla/dom/GridBinding.h"
#include "nsGridContainerFrame.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Grid, mParent, mRows, mCols, mAreas)
NS_IMPL_CYCLE_COLLECTING_ADDREF(Grid)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Grid)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Grid)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Grid::Grid(nsISupports* aParent,
           nsGridContainerFrame* aFrame)
  : mParent(do_QueryInterface(aParent))
  , mRows(new GridDimension(this))
  , mCols(new GridDimension(this))
{
  MOZ_ASSERT(aFrame,
    "Should never be instantiated with a null nsGridContainerFrame");

  const ComputedGridTrackInfo* rowTrackInfo =
    aFrame->GetComputedTemplateRows();
  const ComputedGridLineInfo* rowLineInfo =
    aFrame->GetComputedTemplateRowLines();
  mRows->SetTrackInfo(rowTrackInfo);
  mRows->SetLineInfo(rowTrackInfo, rowLineInfo);

  const ComputedGridTrackInfo* columnTrackInfo =
    aFrame->GetComputedTemplateColumns();
  const ComputedGridLineInfo* columnLineInfo =
    aFrame->GetComputedTemplateColumnLines();
  mCols->SetTrackInfo(columnTrackInfo);
  mCols->SetLineInfo(columnTrackInfo, columnLineInfo);

  // Add implicit areas first.
  nsGridContainerFrame::ImplicitNamedAreas* implicitAreas =
    aFrame->GetImplicitNamedAreas();
  if (implicitAreas) {
    for (auto iter = implicitAreas->Iter(); !iter.Done(); iter.Next()) {
      nsStringHashKey* entry = iter.Get();

      GridArea* area = new GridArea(this,
                                    nsString(entry->GetKey()),
                                    GridDeclaration::Implicit,
                                    0,
                                    0,
                                    0,
                                    0);
      mAreas.AppendElement(area);
    }
  }

  // Add explicit areas next.
  nsGridContainerFrame::ExplicitNamedAreas* explicitAreas =
    aFrame->GetExplicitNamedAreas();
  if (explicitAreas) {
    for (auto areaInfo : *explicitAreas) {
      GridArea* area = new GridArea(this,
                                    areaInfo.mName,
                                    GridDeclaration::Explicit,
                                    areaInfo.mRowStart,
                                    areaInfo.mRowEnd,
                                    areaInfo.mColumnStart,
                                    areaInfo.mColumnEnd);
      mAreas.AppendElement(area);
    }
  }
}

Grid::~Grid()
{
}

JSObject*
Grid::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return GridBinding::Wrap(aCx, this, aGivenProto);
}

GridDimension*
Grid::Rows() const
{
  return mRows;
}

GridDimension*
Grid::Cols() const
{
  return mCols;
}

void
Grid::GetAreas(nsTArray<RefPtr<GridArea>>& aAreas) const
{
  aAreas = mAreas;
}

} // namespace dom
} // namespace mozilla
