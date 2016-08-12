/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputPortListeners.h"
#include "mozilla/dom/InputPort.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION(InputPortListener, mInputPorts)

NS_IMPL_CYCLE_COLLECTING_ADDREF(InputPortListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(InputPortListener)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(InputPortListener)
  NS_INTERFACE_MAP_ENTRY(nsIInputPortListener)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

void
InputPortListener::RegisterInputPort(InputPort* aPort)
{
  MOZ_ASSERT(!mInputPorts.Contains(aPort));
  mInputPorts.AppendElement(aPort);
}

void
InputPortListener::UnregisterInputPort(InputPort* aPort)
{
  MOZ_ASSERT(mInputPorts.Contains(aPort));
  mInputPorts.RemoveElement(aPort);
}

NS_IMETHODIMP
InputPortListener::NotifyConnectionChanged(const nsAString& aPortId,
                                           bool aIsConnected)
{
  for (uint32_t i = 0; i < mInputPorts.Length(); ++i) {
    nsString id;
    mInputPorts[i]->GetId(id);
    if (aPortId.Equals(id)) {
      mInputPorts[i]->NotifyConnectionChanged(aIsConnected);
      break;
    }
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
