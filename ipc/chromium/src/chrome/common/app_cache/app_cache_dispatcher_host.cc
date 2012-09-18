// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/app_cache/app_cache_dispatcher_host.h"

#include "chrome/common/render_messages.h"

AppCacheDispatcherHost::~AppCacheDispatcherHost() {
  if (sender_) {
    // TODO(michaeln): plumb to request_context_->app_cache_service
    // to remove contexts for the child process that is going away.
  }
}

void AppCacheDispatcherHost::Initialize(IPC::Message::Sender* sender) {
  DCHECK(sender);
  sender_ = sender;
  // TODO(michaeln): plumb to request_context_->app_cache_service to
  // tell it about this child process coming into existance
}

bool AppCacheDispatcherHost::OnMessageReceived(const IPC::Message& msg,
                                               bool *msg_ok) {
  DCHECK(sender_);
  *msg_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(AppCacheDispatcherHost, msg, *msg_ok)
    IPC_MESSAGE_HANDLER(AppCacheMsg_ContextCreated, OnContextCreated);
    IPC_MESSAGE_HANDLER(AppCacheMsg_ContextDestroyed, OnContextDestroyed);
    IPC_MESSAGE_HANDLER(AppCacheMsg_SelectAppCache, OnSelectAppCache);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void AppCacheDispatcherHost::OnContextCreated(
                                 WebAppCacheContext::ContextType context_type,
                                 int context_id,
                                 int opt_parent_id) {
  // TODO(michaeln): implement me, plumb to request_context->app_cache_service
  DCHECK(context_id != WebAppCacheContext::kNoAppCacheContextId);
}

void AppCacheDispatcherHost::OnContextDestroyed(int context_id) {
  // TODO(michaeln): implement me, plumb to request_context->app_cache_service
  DCHECK(context_id != WebAppCacheContext::kNoAppCacheContextId);
}

void AppCacheDispatcherHost::OnSelectAppCache(
                                int context_id,
                                int select_request_id,
                                const GURL& document_url,
                                int64_t cache_document_was_loaded_from,
                                const GURL& opt_manifest_url) {
  // TODO(michaeln): implement me, plumb to request_context->app_cache_service
  DCHECK(context_id != WebAppCacheContext::kNoAppCacheContextId);
  Send(new AppCacheMsg_AppCacheSelected(context_id, select_request_id,
                                        WebAppCacheContext::kNoAppCacheId));
}
