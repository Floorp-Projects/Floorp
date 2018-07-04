/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_LogEntry_H_
#define WEBGPU_LogEntry_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace dom {
enum class WebGPULogEntryType : uint8_t;
} // namespace dom
namespace webgpu {

class Device;

class LogEntry final
    : public ChildOf<Device>
{
public:
    WEBGPU_DECL_GOOP(LogEntry)

private:
    LogEntry() = delete;
    virtual ~LogEntry();

public:
    dom::WebGPULogEntryType Type() const;
    void GetObj(JSContext* cx, JS::MutableHandleValue out) const;
    void GetReason(nsString& out) const;
};

} // namespace webgpu
} // namespace mozilla

#endif // WEBGPU_LogEntry_H_
