// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/app_cache/app_cache_context_impl.h"

#include "base/logging.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/child_thread.h"
#include "googleurl/src/gurl.h"

IDMap<AppCacheContextImpl> AppCacheContextImpl::all_contexts;

// static
AppCacheContextImpl* AppCacheContextImpl::FromContextId(int id) {
  return all_contexts.Lookup(id);
}

AppCacheContextImpl::AppCacheContextImpl(IPC::Message::Sender *sender)
  : context_id_(kNoAppCacheContextId),
    app_cache_id_(kUnknownAppCacheId),
    pending_select_request_id_(0),
    sender_(sender) {
  DCHECK(sender_);
}

AppCacheContextImpl::~AppCacheContextImpl() {
  UnInitializeContext();
}

void AppCacheContextImpl::Initialize(ContextType context_type,
                                     WebAppCacheContext *parent) {
  DCHECK(context_id_ == kNoAppCacheContextId);
  DCHECK(((context_type == MAIN_FRAME) && !parent) ||
         ((context_type != MAIN_FRAME) && parent));

  context_id_ = all_contexts.Add(this);
  CHECK(context_id_ != kNoAppCacheContextId);

  sender_->Send(new AppCacheMsg_ContextCreated(context_type,
                                               context_id_,
                                               parent ? parent->GetContextID()
                                                      : kNoAppCacheContextId));
}

void AppCacheContextImpl::UnInitializeContext() {
  if (context_id_ != kNoAppCacheContextId) {
    sender_->Send(new AppCacheMsg_ContextDestroyed(context_id_));
    all_contexts.Remove(context_id_);
    context_id_ = kNoAppCacheContextId;
  }
}

void AppCacheContextImpl::SelectAppCacheWithoutManifest(
                              const GURL &document_url,
                              int64_t cache_document_was_loaded_from) {
  DCHECK(context_id_ != kNoAppCacheContextId);
  app_cache_id_ = kUnknownAppCacheId;  // unknown until we get a response
  sender_->Send(new AppCacheMsg_SelectAppCache(
                         context_id_, ++pending_select_request_id_,
                         document_url, cache_document_was_loaded_from,
                         GURL::EmptyGURL()));
}

void AppCacheContextImpl::SelectAppCacheWithManifest(
                              const GURL &document_url,
                              int64_t cache_document_was_loaded_from,
                              const GURL &manifest_url) {
  DCHECK(context_id_ != kNoAppCacheContextId);
  app_cache_id_ = kUnknownAppCacheId;  // unknown until we get a response
  sender_->Send(new AppCacheMsg_SelectAppCache(
                         context_id_, ++pending_select_request_id_,
                         document_url, cache_document_was_loaded_from,
                         manifest_url));
}

void AppCacheContextImpl::OnAppCacheSelected(int select_request_id,
                                             int64_t app_cache_id) {
  if (select_request_id == pending_select_request_id_) {
    DCHECK(app_cache_id_ == kUnknownAppCacheId);
    DCHECK(app_cache_id != kUnknownAppCacheId);
    app_cache_id_ = app_cache_id;
  }
}
