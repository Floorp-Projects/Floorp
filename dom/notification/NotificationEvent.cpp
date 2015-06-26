/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NotificationEvent.h"

using namespace mozilla::dom;

BEGIN_WORKERS_NAMESPACE

NotificationEvent::NotificationEvent(EventTarget* aOwner)
  : ExtendableEvent(aOwner)
{
}

NS_IMPL_ADDREF_INHERITED(NotificationEvent, ExtendableEvent)
NS_IMPL_RELEASE_INHERITED(NotificationEvent, ExtendableEvent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(NotificationEvent)
NS_INTERFACE_MAP_END_INHERITING(ExtendableEvent)

NS_IMPL_CYCLE_COLLECTION_INHERITED(NotificationEvent, ExtendableEvent, mNotification)

END_WORKERS_NAMESPACE
