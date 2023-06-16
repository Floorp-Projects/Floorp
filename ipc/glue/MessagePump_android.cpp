/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessagePump.h"

namespace mozilla::ipc {
void MessagePumpForAndroidUI::Run(Delegate* delegate) {
  MOZ_CRASH("MessagePumpForAndroidUI should never be Run.");
}

void MessagePumpForAndroidUI::Quit() {
  MOZ_CRASH("MessagePumpForAndroidUI should never be Quit.");
}

void MessagePumpForAndroidUI::ScheduleWork() {
  MOZ_CRASH("MessagePumpForAndroidUI should never ScheduleWork");
}

void MessagePumpForAndroidUI::ScheduleDelayedWork(
    const base::TimeTicks& delayed_work_time) {
  MOZ_CRASH("MessagePumpForAndroidUI should never ScheduleDelayedWork");
}
}  // namespace mozilla::ipc
