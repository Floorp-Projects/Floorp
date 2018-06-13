/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LogEntry.h"

#include "Device.h"
#include "mozilla/dom/WebGPUBinding.h"

namespace mozilla {
namespace webgpu {

LogEntry::~LogEntry() = default;

dom::WebGPULogEntryType
LogEntry::Type() const
{
    MOZ_CRASH("todo");
}

void
LogEntry::GetObj(JSContext* cx, JS::MutableHandleValue out) const
{
    MOZ_CRASH("todo");
}

void
LogEntry::GetReason(nsString& out) const
{
    MOZ_CRASH("todo");
}

WEBGPU_IMPL_GOOP_0(LogEntry)

} // namespace webgpu
} // namespace mozilla
