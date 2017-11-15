/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FlexItem.h"

#include "mozilla/dom/FlexBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FlexItem, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(FlexItem)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FlexItem)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FlexItem)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

FlexItem::FlexItem(FlexLine* aParent)
  : mParent(aParent)
{
}

JSObject*
FlexItem::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FlexItemBinding::Wrap(aCx, this, aGivenProto);
}

nsINode*
FlexItem::GetNode() const
{
  return nullptr;
}

double
FlexItem::MainBaseSize() const
{
  return 0;
}

double
FlexItem::MainDeltaSize() const
{
  return 0;
}

double
FlexItem::MainMinSize() const
{
  return 0;
}

double
FlexItem::MainMaxSize() const
{
  return 0;
}

double
FlexItem::CrossMinSize() const
{
  return 0;
}

double
FlexItem::CrossMaxSize() const
{
  return 0;
}

} // namespace dom
} // namespace mozilla
