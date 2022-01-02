/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
#include "mozilla/dom/GamepadHandle.h"

namespace mozilla {
namespace dom {

void StartGamepadMonitoring() {}

void StopGamepadMonitoring() {}

void SetGamepadLightIndicatorColor(const Tainted<GamepadHandle>&,
                                   const Tainted<uint32_t>&, const uint8_t&,
                                   const uint8_t&, const uint8_t&) {}

}  // namespace dom
}  // namespace mozilla
