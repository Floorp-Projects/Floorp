#ifndef nsConfigTriggers_h
#define nsConfigTriggers_h

#include "nscore.h"
#include "js/TypeDecls.h"

nsresult EvaluateAdminConfigScript(const char *js_buffer, size_t length,
                                   const char *filename,
                                   bool bGlobalContext,
                                   bool bCallbacks,
                                   bool skipFirstLine,
                                   bool isPrivileged = false);

nsresult EvaluateAdminConfigScript(JS::HandleObject sandbox,
                                   const char *js_buffer, size_t length,
                                   const char *filename,
                                   bool bGlobalContext,
                                   bool bCallbacks,
                                   bool skipFirstLine);

#endif // nsConfigTriggers_h
