/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"

namespace mozilla {
namespace hal_impl {

nsresult
StartSystemService(const char* aSvcName, const char* aArgs)
{
  MOZ_ASSERT(NS_IsMainThread());

  return NS_ERROR_NOT_IMPLEMENTED;
}

void
StopSystemService(const char* aSvcName)
{
  MOZ_ASSERT(NS_IsMainThread());
}

bool
SystemServiceIsRunning(const char* aSvcName)
{
  MOZ_ASSERT(NS_IsMainThread());

  return false;
}

} // namespace hal_impl
} // namespace mozilla
