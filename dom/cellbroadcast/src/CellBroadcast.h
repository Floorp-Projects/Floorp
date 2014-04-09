/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CellBroadcast_h__
#define mozilla_dom_CellBroadcast_h__

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "nsICellBroadcastProvider.h"
#include "js/TypeDecls.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class CellBroadcast MOZ_FINAL : public DOMEventTargetHelper
{
  /**
   * Class CellBroadcast doesn't actually inherit nsICellBroadcastListener.
   * Instead, it owns an nsICellBroadcastListener derived instance mListener
   * and passes it to nsICellBroadcastProvider. The onreceived events are first
   * delivered to mListener and then forwarded to its owner, CellBroadcast. See
   * also bug 775997 comment #51.
   */
  class Listener;

public:
  NS_DECL_NSICELLBROADCASTLISTENER

  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(DOMEventTargetHelper)

  static already_AddRefed<CellBroadcast>
  Create(nsPIDOMWindow* aOwner, ErrorResult& aRv);

  CellBroadcast() MOZ_DELETE;
  CellBroadcast(nsPIDOMWindow *aWindow,
                nsICellBroadcastProvider* aProvider);
  // MOZ_FINAL suppresses -Werror,-Wdelete-non-virtual-dtor
  ~CellBroadcast();

  nsPIDOMWindow*
  GetParentObject() const { return GetOwner(); }

  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  IMPL_EVENT_HANDLER(received)

private:
  nsCOMPtr<nsICellBroadcastProvider> mProvider;
  nsRefPtr<Listener> mListener;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_CellBroadcast_h__ */
