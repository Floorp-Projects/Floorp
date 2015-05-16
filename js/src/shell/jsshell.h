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

class AutoCloseInputFile
{
  private:
    FILE* f_;
  public:
    explicit AutoCloseInputFile(FILE* f) : f_(f) {}
    ~AutoCloseInputFile() {
        if (f_ && f_ != stdin)
            fclose(f_);
    }
};

} /* namespace shell */
} /* namespace js */

#endif
