/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sts=2 ts=8 sw=2 tw=99 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxVars.h"
#include "gfxVarReceiver.h"
#include "mozilla/dom/ContentChild.h"
#include "nsPrintfCString.h"


namespace mozilla {
namespace gfx {

StaticAutoPtr<gfxVars> gfxVars::sInstance;
StaticAutoPtr<nsTArray<gfxVars::VarBase*>> gfxVars::sVarList;
const char* gfxVars::sSerializedInitialVarUpdates = nullptr;

/* static */ void
gfxVars::Initialize()
{
  if (sInstance) {
    return;
  }

  // sVarList must be initialized first since it's used in the constructor for
  // sInstance.
  sVarList = new nsTArray<gfxVars::VarBase*>();
  sInstance = new gfxVars;

  // Like Preferences, we want content to synchronously get initial data on
  // init. Note the GPU process is not handled here - it cannot send sync
  // messages, so instead the initial data is pushed down.
  if (XRE_IsContentProcess()) {
    if (!ApplySerializedVarUpdates(sSerializedInitialVarUpdates)) {
      // No initial updates, or could not apply them -> Try sync retrieval.
      InfallibleTArray<GfxVarUpdate> vars;
      dom::ContentChild::GetSingleton()->SendGetGfxVars(&vars);
      for (const auto& var : vars) {
        ApplyUpdate(var);
      }
    }
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

/* static */ bool
gfxVars::SerializeNonDefaultVarUpdates(nsACString& aUpdates, size_t aMax)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!sVarList) {
    return false;
  }

  for (size_t i = 0; i < sVarList->Length(); i++) {
    VarBase* var = sVarList->ElementAt(i);
    if (var->HasDefaultValue()) {
      continue;
    }

    GfxVarValue value;
    var->GetValue(&value);

    switch (value.type()) {
      case mozilla::gfx::GfxVarValue::TBackendType: {
        nsPrintfCString t("T%zu:%d|", i, int(value.get_BackendType()));
        if (aUpdates.Length() + t.Length() > aMax) {
          return false;
        }
        aUpdates.Append(t);
      }
        break;
      case mozilla::gfx::GfxVarValue::Tbool: {
        nsPrintfCString b("B%zu:%d|",
                          i, int(value.get_bool()));
        if (aUpdates.Length() + b.Length() > aMax) {
          return false;
        }
        aUpdates.Append(b);
      }
        break;
      case mozilla::gfx::GfxVarValue::TgfxImageFormat: {
        nsPrintfCString f("F%zu:%d|", i, int(value.get_gfxImageFormat()));
        if (aUpdates.Length() + f.Length() > aMax) {
          return false;
        }
        aUpdates.Append(f);
      }
        break;
      case mozilla::gfx::GfxVarValue::TIntSize: {
        nsPrintfCString z("Z%zu:%dx%d|",
                          i,
                          int(value.get_IntSize().width),
                          int(value.get_IntSize().height));
        if (aUpdates.Length() + z.Length() > aMax) {
          return false;
        }
        aUpdates.Append(z);
      }
        break;
      case mozilla::gfx::GfxVarValue::TnsCString: {
        nsPrintfCString s("S%zu:%u;%s|",
                          i,
                          value.get_nsCString().Length(),
                          value.get_nsCString().get());
        if (aUpdates.Length() + s.Length() > aMax) {
          return false;
        }
        aUpdates.Append(s);
      }
        break;
      default:
        MOZ_ASSERT(false);
        return false;
    }
  }

  return true;
}

/* static */ void
gfxVars::GotSerializedInitialVarUpdates(const char* aUpdates)
{
  sSerializedInitialVarUpdates = aUpdates;
}

/* static */ bool
gfxVars::ApplySerializedVarUpdates(const char* aUpdates)
{
  if (!aUpdates) {
    return false;
  }

  for (const char* p = aUpdates; *p;) {
    switch (*p++) {
      case 'T': {
        int32_t index = strtol(p, const_cast<char**>(&p), 10);
        if (*p != ':') {
          return false;
        }
        p++;
        int32_t type = strtol(p, const_cast<char**>(&p), 10);
        if (*p != '|') {
          return false;
        }
        p++;
        ApplyUpdate(GfxVarUpdate(index, GfxVarValue(BackendType(type))));
      }
        break;
      case 'B': {
        int32_t index = strtol(p, const_cast<char**>(&p), 10);
        if (*p != ':') {
          return false;
        }
        p++;
        int32_t b = strtol(p, const_cast<char**>(&p), 10);
        if (*p != '|') {
          return false;
        }
        p++;
        ApplyUpdate(GfxVarUpdate(index, GfxVarValue(bool(b))));
      }
        break;
      case 'F': {
        int32_t index = strtol(p, const_cast<char**>(&p), 10);
        if (*p != ':') {
          return false;
        }
        p++;
        int32_t format = strtol(p, const_cast<char**>(&p), 10);
        if (*p != '|') {
          return false;
        }
        p++;
        ApplyUpdate(GfxVarUpdate(index, GfxVarValue(gfxImageFormat(format))));
      }
        break;
      case 'Z': {
        int32_t index = strtol(p, const_cast<char**>(&p), 10);
        if (*p != ':') {
          return false;
        }
        p++;
        int32_t width = strtol(p, const_cast<char**>(&p), 10);
        if (*p != 'x') {
          return false;
        }
        p++;
        int32_t height = strtol(p, const_cast<char**>(&p), 10);
        if (*p != '|') {
          return false;
        }
        p++;
        ApplyUpdate(GfxVarUpdate(index, GfxVarValue(IntSize(width, height))));
      }
        break;
      case 'S': {
        int32_t index = strtol(p, const_cast<char**>(&p), 10);
        if (*p != ':') {
          return false;
        }
        p++;
        int32_t length = strtol(p, const_cast<char**>(&p), 10);
        if (*p != ';') {
          return false;
        }
        p++;
        ApplyUpdate(GfxVarUpdate(index, GfxVarValue(nsCString(p, length))));
        p += length;
        if (*p++ != '|') {
          return false;
        }
      }
        break;
      default:
        return false;
    }
  }

  return true;
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
