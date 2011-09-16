#include "assert.h"
#include "ANPBase.h"
#include <android/log.h>

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_log_##name

void
anp_log_log(ANPLogType type, const char format[], ...) {

  va_list argp;
  va_start(argp,format);
  __android_log_vprint(type == kError_ANPLogType ? ANDROID_LOG_ERROR : type == kWarning_ANPLogType ?
                       ANDROID_LOG_WARN : ANDROID_LOG_INFO, "GeckoPluginLog", format, argp);
  va_end(argp);
}

void InitLogInterface(ANPLogInterfaceV0 *i) {
      _assert(i->inSize == sizeof(*i));
      ASSIGN(i, log);
}
