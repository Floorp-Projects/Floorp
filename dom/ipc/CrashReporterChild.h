/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set sw=4 ts=8 et tw=80 : 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CrashReporterChild_h
#define mozilla_dom_CrashReporterChild_h

#include "mozilla/dom/PCrashReporterChild.h"
#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#include "nsXULAppAPI.h"
#endif

namespace mozilla {
namespace dom {
class CrashReporterChild :
    public PCrashReporterChild
{
 public:
    CrashReporterChild() {
        MOZ_COUNT_CTOR(CrashReporterChild);
    }
    ~CrashReporterChild() {
        MOZ_COUNT_DTOR(CrashReporterChild);
    }

    static PCrashReporterChild* GetCrashReporter();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CrashReporterChild_h
