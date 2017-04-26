/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sts=2 ts=8 sw=2 tw=99 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxVars.h"
#include "gfxVarReceiver.h"
#include "mozilla/dom/ContentChild.h"

namespace mozilla {
namespace gfx {

StaticAutoPtr<gfxVars> gfxVars::sInstance;
StaticAutoPtr<nsTArray<gfxVars::VarBase*>> gfxVars::sVarList;

StaticAutoPtr<nsTArray<GfxVarUpdate>> gGfxVarInitUpdates;

void
gfxVars::GotInitialVarUpdates(const nsTArray<GfxVarUpdate>& aInitUpdates)
{
  // We expect aInitUpdates to be provided before any other gfxVars operation.
  MOZ_RELEASE_ASSERT(!sInstance, "Initial updates should not be provided after any gfxVars operation");

  gGfxVarInitUpdates = new nsTArray<GfxVarUpdate>(aInitUpdates);
}

void
gfxVars::Initialize()
{
  if (sInstance) {
    // We expect aInitUpdates to be provided before any other gfxVars operation.
    MOZ_RELEASE_ASSERT(!gGfxVarInitUpdates, "Initial updates should not be present after any gfxVars operation");
    return;
  }

  // sVarList must be initialized first since it's used in the constructor for
  // sInstance.
  sVarList = new nsTArray<gfxVars::VarBase*>();
  sInstance = new gfxVars;

  // Note the GPU process is not handled here - it cannot send sync
  // messages, so instead the initial data is pushed down.
  if (XRE_IsContentProcess()) {
    MOZ_RELEASE_ASSERT(gGfxVarInitUpdates, "Initial updates must be provided in content process");
    if (!gGfxVarInitUpdates) {
      // No provided initial updates, sync-request them from parent.
      InfallibleTArray<GfxVarUpdate> initUpdates;
      dom::ContentChild::GetSingleton()->SendGetGfxVars(&initUpdates);
      gGfxVarInitUpdates = new nsTArray<GfxVarUpdate>(Move(initUpdates));
    }
    for (const auto& varUpdate : *gGfxVarInitUpdates) {
      ApplyUpdate(varUpdate);
    }
    gGfxVarInitUpdates = nullptr;
  }
}

gfxVars::gfxVars()
{
}

void
gfxVars::Shutdown()
{
  sInstance = nullptr;
  sVarList = nullptr;
}

/* static */ void
gfxVars::ApplyUpdate(const GfxVarUpdate& aUpdate)
{
  // Only subprocesses receive updates and apply them locally.
  MOZ_ASSERT(!XRE_IsParentProcess());
  sVarList->ElementAt(aUpdate.index())->SetValue(aUpdate.value());
}

/* static */ void
gfxVars::AddReceiver(gfxVarReceiver* aReceiver)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Don't double-add receivers, in case a broken content process sends two
  // init messages.
  if (!sInstance->mReceivers.Contains(aReceiver)) {
    sInstance->mReceivers.AppendElement(aReceiver);
  }
}

/* static */ void
gfxVars::RemoveReceiver(gfxVarReceiver* aReceiver)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sInstance) {
    sInstance->mReceivers.RemoveElement(aReceiver);
  }
}

/* static */ nsTArray<GfxVarUpdate>
gfxVars::FetchNonDefaultVars()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sVarList);

  nsTArray<GfxVarUpdate> updates;
  for (size_t i = 0; i < sVarList->Length(); i++) {
    VarBase* var = sVarList->ElementAt(i);
    if (var->HasDefaultValue()) {
      continue;
    }

    GfxVarValue value;
    var->GetValue(&value);

    updates.AppendElement(GfxVarUpdate(i, value));
  }

  return updates;
}

gfxVars::VarBase::VarBase()
{
  mIndex = gfxVars::sVarList->Length();
  gfxVars::sVarList->AppendElement(this);
}

void
gfxVars::NotifyReceivers(VarBase* aVar)
{
  MOZ_ASSERT(NS_IsMainThread());

  GfxVarValue value;
  aVar->GetValue(&value);

  GfxVarUpdate update(aVar->Index(), value);
  for (auto& receiver : mReceivers) {
    receiver->OnVarChanged(update);
  }
}

} // namespace gfx
} // namespace mozilla
