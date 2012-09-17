// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_APP_CACHE_APP_CACHE_DISPATCHER_HOST_H_
#define CHROME_COMMON_APP_CACHE_APP_CACHE_DISPATCHER_HOST_H_

#include "base/id_map.h"
#include "chrome/common/ipc_message.h"
#include "webkit/glue/webappcachecontext.h"

class GURL;

// Handles app cache related messages sent to the main browser process from
// its child processes. There is a distinct host for each child process.
// Messages are handled on the IO thread. The ResourceMessageFilter creates
// an instance and delegates calls to it.
class AppCacheDispatcherHost {
 public:
  AppCacheDispatcherHost() : sender_(NULL) {}
  ~AppCacheDispatcherHost();
  void Initialize(IPC::Message::Sender* sender);
  bool OnMessageReceived(const IPC::Message& msg, bool* msg_is_ok);

 private:
  // AppCacheContextImpl related messages
  void OnContextCreated(WebAppCacheContext::ContextType context_type,
                        int context_id, int opt_parent_id);
  void OnContextDestroyed(int context_id);
  void OnSelectAppCache(int context_id,
                        int select_request_id,
                        const GURL& document_url,
                        int64_t cache_document_was_loaded_from,
                        const GURL& opt_manifest_url);

  bool Send(IPC::Message* msg) {
    return sender_->Send(msg);
  }

  IPC::Message::Sender* sender_;
};

#endif  // CHROME_COMMON_APP_CACHE_APP_CACHE_DISPATCHER_HOST_H_
