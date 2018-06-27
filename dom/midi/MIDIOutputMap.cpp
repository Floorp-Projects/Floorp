/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MIDIOutputMap.h"
#include "mozilla/dom/MIDIOutputMapBinding.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/BindingUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MIDIOutputMap, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(MIDIOutputMap)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MIDIOutputMap)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MIDIOutputMap)
NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

MIDIOutputMap::MIDIOutputMap(nsPIDOMWindowInner* aParent) :
  mParent(aParent)
{
}

JSObject*
MIDIOutputMap::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MIDIOutputMap_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
