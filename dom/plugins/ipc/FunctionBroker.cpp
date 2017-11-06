/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FunctionBroker.h"
#include "PluginQuirks.h"
#include <commdlg.h>

using namespace mozilla;
using namespace mozilla::ipc;
using namespace mozilla::plugins;

namespace mozilla {
namespace plugins {

template <int QuirkFlag>
static bool CheckQuirks(int aQuirks)
{
  return static_cast<bool>(aQuirks & QuirkFlag);
}

void FreeDestructor(void* aObj) { free(aObj); }

/*****************************************************************************/

#define FUN_HOOK(x) static_cast<FunctionHook*>(x)

void
AddBrokeredFunctionHooks(FunctionHookArray& aHooks)
{
}

#undef FUN_HOOK

} // namespace plugins
} // namespace mozilla
