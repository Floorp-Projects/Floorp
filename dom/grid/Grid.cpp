/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Grid.h"

#include "GridArea.h"
#include "GridDimension.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/GridBinding.h"
#include "nsGridContainerFrame.h"
#include "nsTHashSet.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Grid, mParent, mRows, mCols, mAreas)
NS_IMPL_CYCLE_COLLECTING_ADDREF(Grid)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Grid)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Grid)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Grid::Grid(nsISupports* aParent, nsGridContainerFrame* aFrame)
    : mParent(do_QueryInterface(aParent)),
      mRows(new GridDimension(this)),
      mCols(new GridDimension(this)) {
  MOZ_ASSERT(aFrame,
             "Should never be instantiated with a null nsGridContainerFrame");

  // Construct areas first, because lines may need to reference them
  // to extract additional names for boundary lines.

  // Add implicit areas first. Track the names that we add here, because
  // we will ignore future explicit areas with the same name.
  nsTHashSet<RefPtr<nsAtom>> namesSeen;
  nsGridContainerFrame::ImplicitNamedAreas* implicitAreas =
      aFrame->GetImplicitNamedAreas();
  if (implicitAreas) {
    for (auto iter = implicitAreas->iter(); !iter.done(); iter.next()) {
      auto& areaInfo = iter.get().value();
      namesSeen.Insert(areaInfo.name.AsAtom());
      GridArea* area =
          new GridArea(this, areaInfo.name.AsAtom(), GridDeclaration::Implicit,
                       areaInfo.rows.start, areaInfo.rows.end,
                       areaInfo.columns.start, areaInfo.columns.end);
      mAreas.AppendElement(area);
    }
  }

  // Add explicit areas next, as long as they don't have the same name
  // as the implicit areas, because the implicit values override what was
  // initially available in the explicit areas.
  if (auto* explicitAreas = aFrame->GetExplicitNamedAreas()) {
    for (auto& areaInfo : explicitAreas->AsSpan()) {
      if (!namesSeen.Contains(areaInfo.name.AsAtom())) {
        GridArea* area = new GridArea(
            this, areaInfo.name.AsAtom(), GridDeclaration::Explicit,
            areaInfo.rows.start, areaInfo.rows.end, areaInfo.columns.start,
            areaInfo.columns.end);
        mAreas.AppendElement(area);
      }
    }
  }

  // Now construct the tracks and lines.
  const ComputedGridTrackInfo* rowTrackInfo = aFrame->GetComputedTemplateRows();
  const ComputedGridLineInfo* rowLineInfo =
      aFrame->GetComputedTemplateRowLines();
  mRows->SetTrackInfo(rowTrackInfo);
  mRows->SetLineInfo(rowTrackInfo, rowLineInfo, mAreas, true);

  const ComputedGridTrackInfo* columnTrackInfo =
      aFrame->GetComputedTemplateColumns();
  const ComputedGridLineInfo* columnLineInfo =
      aFrame->GetComputedTemplateColumnLines();
  mCols->SetTrackInfo(columnTrackInfo);
  mCols->SetLineInfo(columnTrackInfo, columnLineInfo, mAreas, false);
}

Grid::~Grid() = default;

JSObject* Grid::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return Grid_Binding::Wrap(aCx, this, aGivenProto);
}

GridDimension* Grid::Rows() const { return mRows; }

GridDimension* Grid::Cols() const { return mCols; }

void Grid::GetAreas(nsTArray<RefPtr<GridArea>>& aAreas) const {
  aAreas = mAreas.Clone();
}

}  // namespace mozilla::dom
