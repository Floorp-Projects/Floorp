/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsImageModule.h"

#include "mozilla/ModuleUtils.h"

#include "DecodePool.h"
#include "ImageFactory.h"
#include "ShutdownTracker.h"
#include "SurfaceCache.h"

#include "imgLoader.h"

using namespace mozilla::image;

static bool sInitialized = false;
nsresult mozilla::image::EnsureModuleInitialized() {
  MOZ_ASSERT(NS_IsMainThread());

  if (sInitialized) {
    return NS_OK;
  }

  mozilla::image::ShutdownTracker::Initialize();
  mozilla::image::ImageFactory::Initialize();
  mozilla::image::DecodePool::Initialize();
  mozilla::image::SurfaceCache::Initialize();
  imgLoader::GlobalInit();
  sInitialized = true;
  return NS_OK;
}

void mozilla::image::ShutdownModule() {
  if (!sInitialized) {
    return;
  }
  imgLoader::Shutdown();
  mozilla::image::SurfaceCache::Shutdown();
  sInitialized = false;
}
