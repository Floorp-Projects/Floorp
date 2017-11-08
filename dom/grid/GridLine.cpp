/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GridLine.h"

#include "GridLines.h"
#include "mozilla/dom/GridBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(GridLine, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(GridLine)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GridLine)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GridLine)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

GridLine::GridLine(GridLines *aParent)
  : mParent(aParent)
  , mStart(0.0)
  , mBreadth(0.0)
  , mType(GridDeclaration::Implicit)
  , mNumber(0)
{
  MOZ_ASSERT(aParent, "Should never be instantiated with a null GridLines");
}

GridLine::~GridLine()
{
}

void
GridLine::GetNames(nsTArray<nsString>& aNames) const
{
  aNames = mNames;
}

JSObject*
GridLine::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return GridLineBinding::Wrap(aCx, this, aGivenProto);
}

double
GridLine::Start() const
{
  return mStart;
}

double
GridLine::Breadth() const
{
  return mBreadth;
}

GridDeclaration
GridLine::Type() const
{
  return mType;
}

uint32_t
GridLine::Number() const
{
  return mNumber;
}

int32_t
GridLine::NegativeNumber() const
{
  return mNegativeNumber;
}

void
GridLine::SetLineValues(const nsTArray<nsString>& aNames,
                        double aStart,
                        double aBreadth,
                        uint32_t aNumber,
                        int32_t aNegativeNumber,
                        GridDeclaration aType)
{
  mNames = aNames;
  mStart = aStart;
  mBreadth = aBreadth;
  mNumber = aNumber;
  mNegativeNumber = aNegativeNumber;
  mType = aType;
}

} // namespace dom
} // namespace mozilla
