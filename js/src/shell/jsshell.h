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

void
WarningReporter(JSContext* cx, const char* message, JSErrorReport* report);

class MOZ_STACK_CLASS AutoReportException
{
    JSContext* cx;
  public:
    explicit AutoReportException(JSContext* cx)
      : cx(cx)
    {}
    ~AutoReportException();
};

bool
GenerateInterfaceHelp(JSContext* cx, JS::HandleObject obj, const char* name);

JSString*
FileAsString(JSContext* cx, JS::HandleString pathnameStr);

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

// Reference counted file.
struct RCFile {
    FILE* fp;
    uint32_t numRefs;

    RCFile() : fp(nullptr), numRefs(0) {}
    explicit RCFile(FILE* fp) : fp(fp), numRefs(0) {}

    void acquire() { numRefs++; }

    // Starts out with a ref count of zero.
    static RCFile* create(JSContext* cx, const char* filename, const char* mode);

    void close();
    bool isOpen() const { return fp; }
    bool release();
};

} /* namespace shell */
} /* namespace js */

#endif
