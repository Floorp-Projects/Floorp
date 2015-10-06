/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PlatformDecoderModule.h"
#include "PDMFactory.h"

PRLogModuleInfo* GetPDMLog() {
  static PRLogModuleInfo* log = nullptr;
  if (!log) {
    log = PR_NewLogModule("PlatformDecoderModule");
  }
  return log;
}

namespace mozilla {

/* static */
void
PlatformDecoderModule::Init()
{
  MOZ_ASSERT(NS_IsMainThread());
  PDMFactory::Init();
}

/* static */
already_AddRefed<PlatformDecoderModule>
PlatformDecoderModule::Create()
{
  // Note: This (usually) runs on the decode thread.
  nsRefPtr<PlatformDecoderModule> m = new PDMFactory;
  return m.forget();
}

} // namespace mozilla
