/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GridArea.h"
#include "mozilla/dom/GridBinding.h"
#include "Grid.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(GridArea, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(GridArea)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GridArea)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GridArea)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

GridArea::GridArea(Grid* aParent,
                   const nsString& aName,
                   GridDeclaration aType,
                   uint32_t aRowStart,
                   uint32_t aRowEnd,
                   uint32_t aColumnStart,
                   uint32_t aColumnEnd)
  : mParent(aParent)
  , mName(aName)
  , mType(aType)
  , mRowStart(aRowStart)
  , mRowEnd(aRowEnd)
  , mColumnStart(aColumnStart)
  , mColumnEnd(aColumnEnd)
{
}

GridArea::~GridArea()
{
}

JSObject*
GridArea::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return GridArea_Binding::Wrap(aCx, this, aGivenProto);
}

void
GridArea::GetName(nsString& aName) const
{
  aName = mName;
}

GridDeclaration
GridArea::Type() const
{
  return mType;
}

uint32_t
GridArea::RowStart() const
{
  return mRowStart;
}

uint32_t
GridArea::RowEnd() const
{
  return mRowEnd;
}

uint32_t
GridArea::ColumnStart() const
{
  return mColumnStart;
}

uint32_t
GridArea::ColumnEnd() const
{
  return mColumnEnd;
}

} // namespace dom
} // namespace mozilla
