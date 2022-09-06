/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GridArea_h
#define mozilla_dom_GridArea_h

#include "mozilla/dom/GridBinding.h"
#include "nsWrapperCache.h"

class nsAtom;

namespace mozilla::dom {

class Grid;

class GridArea : public nsISupports, public nsWrapperCache {
 public:
  explicit GridArea(Grid* aParent, nsAtom* aName, GridDeclaration aType,
                    uint32_t aRowStart, uint32_t aRowEnd, uint32_t aColumnStart,
                    uint32_t aColumnEnd);

 protected:
  virtual ~GridArea();

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(GridArea)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  Grid* GetParentObject() { return mParent; }

  void GetName(nsString& aName) const;
  GridDeclaration Type() const;
  uint32_t RowStart() const;
  uint32_t RowEnd() const;
  uint32_t ColumnStart() const;
  uint32_t ColumnEnd() const;

 protected:
  RefPtr<Grid> mParent;
  const RefPtr<nsAtom> mName;
  const GridDeclaration mType;
  const uint32_t mRowStart;
  const uint32_t mRowEnd;
  const uint32_t mColumnStart;
  const uint32_t mColumnEnd;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_GridTrack_h */
