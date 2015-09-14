/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsshell_js_h
#define jsshell_js_h

#include "jsapi.h"

namespace js {
namespace shell {

enum JSShellErrNum {
#define MSG_DEF(name, count, exception, format) \
    name,
#include "jsshell.msg"
#undef MSG_DEF
    JSShellErr_Limit
};

const JSErrorFormatString*
my_GetErrorMessage(void* userRef, const unsigned errorNumber);

static void
my_ErrorReporter(JSContext* cx, const char* message, JSErrorReport* report);

JSString*
FileAsString(JSContext* cx, const char* pathname);

class AutoCloseFile
{
  private:
    FILE* f_;
  public:
    explicit AutoCloseFile(FILE* f) : f_(f) {}
    ~AutoCloseFile() {
        (void) release();
    }
    bool release() {
        bool success = true;
        if (f_ && f_ != stdin && f_ != stdout && f_ != stderr)
            success = !fclose(f_);
        f_ = nullptr;
        return success;
    }
};

} /* namespace shell */
} /* namespace js */

#endif
