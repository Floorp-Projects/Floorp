/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ControllerConnectionCollection.h"

#include "mozilla/ClearOnShutdown.h"
#include "nsIPresentationService.h"
#include "PresentationConnection.h"

namespace mozilla {
namespace dom {

/* static */
StaticAutoPtr<ControllerConnectionCollection>
ControllerConnectionCollection::sSingleton;

/* static */ ControllerConnectionCollection*
ControllerConnectionCollection::GetSingleton()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!sSingleton) {
    sSingleton = new ControllerConnectionCollection();
    ClearOnShutdown(&sSingleton);
  }

  return sSingleton;
}

ControllerConnectionCollection::ControllerConnectionCollection()
{
  MOZ_COUNT_CTOR(ControllerConnectionCollection);
}

ControllerConnectionCollection::~ControllerConnectionCollection()
{
  MOZ_COUNT_DTOR(ControllerConnectionCollection);
}

void
ControllerConnectionCollection::AddConnection(
  PresentationConnection* aConnection,
  const uint8_t aRole)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (aRole != nsIPresentationService::ROLE_CONTROLLER) {
    MOZ_ASSERT(false, "This is allowed only to be called at controller side.");
    return;
  }

  if (!aConnection) {
    return;
  }

  WeakPtr<PresentationConnection> connection = aConnection;
  if (mConnections.Contains(connection)) {
    return;
  }

  mConnections.AppendElement(connection);
}

void
ControllerConnectionCollection::RemoveConnection(
  PresentationConnection* aConnection,
  const uint8_t aRole)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (aRole != nsIPresentationService::ROLE_CONTROLLER) {
    MOZ_ASSERT(false, "This is allowed only to be called at controller side.");
    return;
  }

  if (!aConnection) {
    return;
  }

  WeakPtr<PresentationConnection> connection = aConnection;
  mConnections.RemoveElement(connection);
}

already_AddRefed<PresentationConnection>
ControllerConnectionCollection::FindConnection(
  uint64_t aWindowId,
  const nsAString& aId,
  const uint8_t aRole)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (aRole != nsIPresentationService::ROLE_CONTROLLER) {
    MOZ_ASSERT(false, "This is allowed only to be called at controller side.");
    return nullptr;
  }

  // Loop backwards to allow removing elements in the loop.
  for (int i = mConnections.Length() - 1; i >= 0; --i) {
    WeakPtr<PresentationConnection> connection = mConnections[i];
    if (!connection) {
      // The connection was destroyed. Remove it from the list.
      mConnections.RemoveElementAt(i);
      continue;
    }

    if (connection->Equals(aWindowId, aId)) {
      RefPtr<PresentationConnection> matchedConnection = connection.get();
      return matchedConnection.forget();
    }
  }

  return nullptr;
}

} // namespace dom
} // namespace mozilla
