/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PlatformDecoderModule.h"
#ifdef XP_WIN
#include "WMFDecoderModule.h"
#endif
#include "mozilla/Preferences.h"

namespace mozilla {

extern PlatformDecoderModule* CreateBlankDecoderModule();

bool PlatformDecoderModule::sUseBlankDecoder = false;

/* static */
void
PlatformDecoderModule::Init()
{
  MOZ_ASSERT(NS_IsMainThread());
  static bool alreadyInitialized = false;
  if (alreadyInitialized) {
    return;
  }
  alreadyInitialized = true;
  sUseBlankDecoder = Preferences::GetBool("media.fragmented-mp4.use-blank-decoder");
}

/* static */
PlatformDecoderModule*
PlatformDecoderModule::Create()
{
  if (sUseBlankDecoder) {
    return CreateBlankDecoderModule();
  }
#ifdef XP_WIN
  nsAutoPtr<WMFDecoderModule> m(new WMFDecoderModule());
  if (NS_SUCCEEDED(m->Startup())) {
    return m.forget();
  }
#endif
  return nullptr;
}

} // namespace mozilla
