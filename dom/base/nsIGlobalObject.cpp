/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIGlobalObject.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "nsHostObjectProtocolHandler.h"

nsIGlobalObject::~nsIGlobalObject()
{
  UnlinkHostObjectURIs();
}

nsIPrincipal*
nsIGlobalObject::PrincipalOrNull()
{
  JSObject *global = GetGlobalJSObject();
  if (NS_WARN_IF(!global))
    return nullptr;

  return nsContentUtils::ObjectPrincipal(global);
}

void
nsIGlobalObject::RegisterHostObjectURI(const nsACString& aURI)
{
  MOZ_ASSERT(!mHostObjectURIs.Contains(aURI));
  mHostObjectURIs.AppendElement(aURI);
}

void
nsIGlobalObject::UnregisterHostObjectURI(const nsACString& aURI)
{
  mHostObjectURIs.RemoveElement(aURI);
}

namespace {

class UnlinkHostObjectURIsRunnable final : public nsRunnable
{
public:
  explicit UnlinkHostObjectURIsRunnable(nsTArray<nsCString>& aURIs)
  {
    mURIs.SwapElements(aURIs);
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    for (uint32_t index = 0; index < mURIs.Length(); ++index) {
      nsHostObjectProtocolHandler::RemoveDataEntry(mURIs[index]);
    }

    return NS_OK;
  }

private:
  ~UnlinkHostObjectURIsRunnable() {}

  nsTArray<nsCString> mURIs;
};

} // namespace

void
nsIGlobalObject::UnlinkHostObjectURIs()
{
  if (mHostObjectURIs.IsEmpty()) {
    return;
  }

  if (NS_IsMainThread()) {
    for (uint32_t index = 0; index < mHostObjectURIs.Length(); ++index) {
      nsHostObjectProtocolHandler::RemoveDataEntry(mHostObjectURIs[index]);
    }

    mHostObjectURIs.Clear();
    return;
  }

  // nsHostObjectProtocolHandler is main-thread only.

  nsRefPtr<UnlinkHostObjectURIsRunnable> runnable =
    new UnlinkHostObjectURIsRunnable(mHostObjectURIs);
  MOZ_ASSERT(mHostObjectURIs.IsEmpty());

  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch a runnable to the main-thread.");
  }
}

void
nsIGlobalObject::TraverseHostObjectURIs(nsCycleCollectionTraversalCallback &aCb)
{
  if (mHostObjectURIs.IsEmpty()) {
    return;
  }

  // Currently we only store BlobImpl objects off the the main-thread and they
  // are not CCed.
  if (!NS_IsMainThread()) {
    return;
  }

  for (uint32_t index = 0; index < mHostObjectURIs.Length(); ++index) {
    nsHostObjectProtocolHandler::Traverse(mHostObjectURIs[index], aCb);
  }
}
