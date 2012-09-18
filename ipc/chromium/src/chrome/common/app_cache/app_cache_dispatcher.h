// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_APP_CACHE_APP_CACHE_DISPATCHER_H_
#define CHROME_COMMON_APP_CACHE_APP_CACHE_DISPATCHER_H_

#include "base/basictypes.h"
#include "chrome/common/ipc_message.h"

// Dispatches app cache related messages sent to a child process from the
// main browser process. There is one instance per child process. Messages
// are dispatched on the main child thread. The ChildThread base class
// creates an instance and delegates calls to it.
class AppCacheDispatcher {
 public:
  bool OnMessageReceived(const IPC::Message& msg);

 private:
  // AppCacheContextImpl related messages
  void OnAppCacheSelected(int context_id,
                          int select_request_id,
                          int64_t app_cache_id);
};

#endif  // CHROME_COMMON_APP_CACHE_APP_CACHE_DISPATCHER_H_
