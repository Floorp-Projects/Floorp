/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/devtools/DominatorTree.h"
#include "mozilla/dom/DominatorTreeBinding.h"

namespace mozilla {
namespace devtools {

/*** Cycle Collection Boilerplate *****************************************************************/

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DominatorTree, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(DominatorTree)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DominatorTree)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DominatorTree)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* virtual */ JSObject*
DominatorTree::WrapObject(JSContext* aCx, JS::HandleObject aGivenProto)
{
  return dom::DominatorTreeBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace devtools
} // namespace mozilla
