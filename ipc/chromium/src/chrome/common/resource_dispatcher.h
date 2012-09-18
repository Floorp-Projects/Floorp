// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#ifndef CHROME_COMMON_RESOURCE_DISPATCHER_H__
#define CHROME_COMMON_RESOURCE_DISPATCHER_H__

#include <deque>
#include <queue>
#include <string>

#include "base/hash_tables.h"
#include "base/shared_memory.h"
#include "base/task.h"
#include "chrome/common/filter_policy.h"
#include "chrome/common/ipc_channel.h"
#include "webkit/glue/resource_loader_bridge.h"

struct ResourceResponseHead;

// This class serves as a communication interface between the
// ResourceDispatcherHost in the browser process and the ResourceLoaderBridge in
// the child process.  It can be used from any child process.
class ResourceDispatcher {
 public:
  explicit ResourceDispatcher(IPC::Message::Sender* sender);
  ~ResourceDispatcher();

  // Called to possibly handle the incoming IPC message.  Returns true if
  // handled, else false.
  bool OnMessageReceived(const IPC::Message& message);

  // Creates a ResourceLoaderBridge for this type of dispatcher, this is so
  // this can be tested regardless of the ResourceLoaderBridge::Create
  // implementation.
  webkit_glue::ResourceLoaderBridge* CreateBridge(const std::string& method,
    const GURL& url,
    const GURL& policy_url,
    const GURL& referrer,
    const std::string& frame_origin,
    const std::string& main_frame_origin,
    const std::string& headers,
    int load_flags,
    int origin_pid,
    ResourceType::Type resource_type,
    uint32_t request_context /* used for plugin->browser requests */,
    int app_cache_context_id,
    int route_id);

  // Adds a request from the pending_requests_ list, returning the new
  // requests' ID
  int AddPendingRequest(webkit_glue::ResourceLoaderBridge::Peer* callback,
                        ResourceType::Type resource_type);

  // Removes a request from the pending_requests_ list, returning true if the
  // request was found and removed.
  bool RemovePendingRequest(int request_id);

  IPC::Message::Sender* message_sender() const {
    return message_sender_;
  }

  // Toggles the is_deferred attribute for the specified request.
  void SetDefersLoading(int request_id, bool value);

 private:
  friend class ResourceDispatcherTest;

  typedef std::deque<IPC::Message*> MessageQueue;
  struct PendingRequestInfo {
    PendingRequestInfo() { }
    PendingRequestInfo(webkit_glue::ResourceLoaderBridge::Peer* peer,
                       ResourceType::Type resource_type)
        : peer(peer),
          resource_type(resource_type),
          filter_policy(FilterPolicy::DONT_FILTER),
          is_deferred(false) {
    }
    ~PendingRequestInfo() { }
    webkit_glue::ResourceLoaderBridge::Peer* peer;
    ResourceType::Type resource_type;
    FilterPolicy::Type filter_policy;
    MessageQueue deferred_message_queue;
    bool is_deferred;
  };
  typedef base::hash_map<int, PendingRequestInfo> PendingRequestList;

  // Message response handlers, called by the message handler for this process.
  void OnUploadProgress(const IPC::Message& message,
                        int request_id,
                        int64_t position,
                        int64_t size);
  void OnDownloadProgress(const IPC::Message& message,
                          int request_id, int64_t position, int64_t size);
  void OnReceivedResponse(int request_id, const ResourceResponseHead&);
  void OnReceivedRedirect(int request_id, const GURL& new_url);
  void OnReceivedData(const IPC::Message& message,
                      int request_id,
                      base::SharedMemoryHandle data,
                      int data_len);
  void OnRequestComplete(int request_id,
                         const URLRequestStatus& status,
                         const std::string& security_info);

  // Dispatch the message to one of the message response handlers.
  void DispatchMessage(const IPC::Message& message);

  // Dispatch any deferred messages for the given request, provided it is not
  // again in the deferred state.
  void FlushDeferredMessages(int request_id);

  // Returns true if the message passed in is a resource related message.
  static bool IsResourceDispatcherMessage(const IPC::Message& message);

  IPC::Message::Sender* message_sender_;

  // All pending requests issued to the host
  PendingRequestList pending_requests_;

  ScopedRunnableMethodFactory<ResourceDispatcher> method_factory_;

  DISALLOW_EVIL_CONSTRUCTORS(ResourceDispatcher);
};

#endif  // CHROME_COMMON_RESOURCE_DISPATCHER_H__
