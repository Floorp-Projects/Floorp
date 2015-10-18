/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InputPortListeners_h
#define mozilla_dom_InputPortListeners_h

#include "nsCycleCollectionParticipant.h"
#include "nsIInputPortService.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class InputPort;

class InputPortListener final : public nsIInputPortListener
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(InputPortListener)
  NS_DECL_NSIINPUTPORTLISTENER

  void RegisterInputPort(InputPort* aPort);

  void UnregisterInputPort(InputPort* aPort);

private:
  ~InputPortListener() {}

  nsTArray<RefPtr<InputPort>> mInputPorts;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_InputPortListeners_h
