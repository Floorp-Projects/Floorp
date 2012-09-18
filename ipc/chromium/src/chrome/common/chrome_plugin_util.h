// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_PLUGIN_UTIL_H_
#define CHROME_COMMON_CHROME_PLUGIN_UTIL_H_

#include <string>

#include "base/basictypes.h"
#include "base/non_thread_safe.h"
#include "base/ref_counted.h"
#include "chrome/common/chrome_plugin_api.h"
#include "chrome/common/notification_observer.h"

class ChromePluginLib;
class MessageLoop;
namespace net {
class HttpResponseHeaders;
}

// A helper struct to ensure the CPRequest data is cleaned up when done.
// This class is reused for requests made by the browser (and intercepted by the
// plugin) as well as those made by the plugin.
struct ScopableCPRequest : public CPRequest {
  template<class T>
  static T GetData(CPRequest* request) {
    return static_cast<T>(static_cast<ScopableCPRequest*>(request)->data);
  }

  ScopableCPRequest(const char* url, const char* method,
                    CPBrowsingContext context);
  ~ScopableCPRequest();

  void* data;
};

// This is a base class for plugin-related objects that need to go away when
// the plugin unloads.  This object also verifies that it is created and
// destroyed on the same thread.
class PluginHelper : public NotificationObserver, public NonThreadSafe {
 public:
  static void DestroyAllHelpersForPlugin(ChromePluginLib* plugin);

  PluginHelper(ChromePluginLib* plugin);
  virtual ~PluginHelper();

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 protected:
  scoped_refptr<ChromePluginLib> plugin_;

  DISALLOW_COPY_AND_ASSIGN(PluginHelper);
};

// A class of utility functions for dealing with request responses.
class PluginResponseUtils {
public:
  // Helper to convert request load flags from the plugin API to the net API
  // versions.
  static uint32_t CPLoadFlagsToNetFlags(uint32_t flags);

  // Common implementation of a CPR_GetResponseInfo call.
  static int GetResponseInfo(
      const net::HttpResponseHeaders* response_headers,
      CPResponseInfoType type, void* buf, size_t buf_size);
};

// Helper to allocate a string using the given CPB_Alloc function.
inline char* CPB_StringDup(CPB_AllocFunc alloc, const std::string& str) {
  char* cstr = static_cast<char*>(alloc(static_cast<uint32_t>(str.length() + 1)));
  memcpy(cstr, str.c_str(), str.length() + 1);  // Include null terminator.
  return cstr;
}

CPError CPB_GetCommandLineArgumentsCommon(const char* url,
                                          std::string* arguments);

void* STDCALL CPB_Alloc(uint32_t size);
void STDCALL CPB_Free(void* memory);

#endif  // CHROME_COMMON_CHROME_PLUGIN_UTIL_H_
