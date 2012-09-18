// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/app_cache/app_cache_dispatcher.h"

#include "chrome/common/app_cache/app_cache_context_impl.h"
#include "chrome/common/render_messages.h"

bool AppCacheDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AppCacheDispatcher, msg)
    IPC_MESSAGE_HANDLER(AppCacheMsg_AppCacheSelected, OnAppCacheSelected)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AppCacheDispatcher::OnAppCacheSelected(int context_id,
                                            int select_request_id,
                                            int64_t app_cache_id) {
  AppCacheContextImpl *context = AppCacheContextImpl::FromContextId(context_id);
  if (context) {
    context->OnAppCacheSelected(select_request_id, app_cache_id);
  }
}
