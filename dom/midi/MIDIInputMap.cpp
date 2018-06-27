/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MIDIInputMap.h"
#include "mozilla/dom/MIDIInputMapBinding.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/BindingUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MIDIInputMap, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(MIDIInputMap)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MIDIInputMap)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MIDIInputMap)
NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

MIDIInputMap::MIDIInputMap(nsPIDOMWindowInner* aParent) :
  mParent(aParent)
{
}

JSObject*
MIDIInputMap::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MIDIInputMap_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
