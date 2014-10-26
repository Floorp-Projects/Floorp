/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DOMStringList.h"
#include "mozilla/dom/DOMStringListBinding.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(DOMStringList)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMStringList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMStringList)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMStringList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

DOMStringList::~DOMStringList()
{
}

JSObject*
DOMStringList::WrapObject(JSContext* aCx)
{
  return DOMStringListBinding::Wrap(aCx, this);
}

} // namespace dom
} // namespace mozilla
