/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Grid_h
#define mozilla_dom_Grid_h

#include "GridArea.h"
#include "mozilla/dom/Element.h"
#include "nsGridContainerFrame.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class GridDimension;

class Grid : public nsISupports
           , public nsWrapperCache
{
public:
  explicit Grid(nsISupports* aParent, nsGridContainerFrame* aFrame);

protected:
  virtual ~Grid();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Grid)

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
  Element* GetParentObject()
  {
    return mParent;
  }

  GridDimension* Rows() const;
  GridDimension* Cols() const;
  void GetAreas(nsTArray<RefPtr<GridArea>>& aAreas) const;

protected:
  nsCOMPtr<Element> mParent;
  RefPtr<GridDimension> mRows;
  RefPtr<GridDimension> mCols;
  nsTArray<RefPtr<GridArea>> mAreas;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_Grid_h */
