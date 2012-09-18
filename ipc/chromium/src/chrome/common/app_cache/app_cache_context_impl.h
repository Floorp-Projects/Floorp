// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_APP_CACHE_APP_CACHE_CONTEXT_IMPL_H_
#define CHROME_COMMON_APP_CACHE_APP_CACHE_CONTEXT_IMPL_H_

#include "base/id_map.h"
#include "chrome/common/ipc_message.h"
#include "webkit/glue/webappcachecontext.h"

// A concrete implemenation of WebAppCacheContext for use in a child process.
class AppCacheContextImpl : public WebAppCacheContext {
 public:
  // Returns the context having given id or NULL if there is no such context.
  static AppCacheContextImpl* FromContextId(int id);

  AppCacheContextImpl(IPC::Message::Sender* sender);
  virtual ~AppCacheContextImpl();

  // WebAppCacheContext implementation
  virtual int GetContextID() { return context_id_; }
  virtual int64_t GetAppCacheID() { return app_cache_id_; }
  virtual void Initialize(WebAppCacheContext::ContextType context_type,
                          WebAppCacheContext* opt_parent);
  virtual void SelectAppCacheWithoutManifest(
                                     const GURL& document_url,
                                     int64_t cache_document_was_loaded_from);
  virtual void SelectAppCacheWithManifest(
                                     const GURL& document_url,
                                     int64_t cache_document_was_loaded_from,
                                     const GURL& manifest_url);

  // Called by AppCacheDispatcher when the browser has selected an appcache.
  void OnAppCacheSelected(int select_request_id, int64_t app_cache_id);

 private:
  void UnInitializeContext();

  int context_id_;
  int64_t app_cache_id_;
  int pending_select_request_id_;
  IPC::Message::Sender* sender_;

  static IDMap<AppCacheContextImpl> all_contexts;
};

#endif  // CHROME_COMMON_APP_CACHE_APP_CACHE_CONTEXT_IMPL_H_
