// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/thread.h"
#include "chrome/common/ipc_channel_proxy.h"
#include "chrome/common/ipc_logging.h"
#include "chrome/common/ipc_message_utils.h"

namespace IPC {

//-----------------------------------------------------------------------------

ChannelProxy::Context::Context(Channel::Listener* listener,
                               MessageFilter* filter,
                               MessageLoop* ipc_message_loop)
    : listener_message_loop_(MessageLoop::current()),
      listener_(listener),
      ipc_message_loop_(ipc_message_loop),
      channel_(NULL),
      peer_pid_(0),
      channel_connected_called_(false) {
  if (filter)
    filters_.push_back(filter);
}

void ChannelProxy::Context::CreateChannel(const std::wstring& id,
                                          const Channel::Mode& mode) {
  DCHECK(channel_ == NULL);
  channel_id_ = id;
  channel_ = new Channel(id, mode, this);
}

bool ChannelProxy::Context::TryFilters(const Message& message) {
#ifdef IPC_MESSAGE_LOG_ENABLED
  Logging* logger = Logging::current();
  if (logger->Enabled())
    logger->OnPreDispatchMessage(message);
#endif

  for (size_t i = 0; i < filters_.size(); ++i) {
    if (filters_[i]->OnMessageReceived(message)) {
#ifdef IPC_MESSAGE_LOG_ENABLED
      if (logger->Enabled())
        logger->OnPostDispatchMessage(message, channel_id_);
#endif
      return true;
    }
  }
  return false;
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnMessageReceived(const Message& message) {
  // First give a chance to the filters to process this message.
  if (!TryFilters(message))
    OnMessageReceivedNoFilter(message);
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnMessageReceivedNoFilter(const Message& message) {
  // NOTE: This code relies on the listener's message loop not going away while
  // this thread is active.  That should be a reasonable assumption, but it
  // feels risky.  We may want to invent some more indirect way of referring to
  // a MessageLoop if this becomes a problem.
  listener_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &Context::OnDispatchMessage, message));
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelConnected(int32_t peer_pid) {
  peer_pid_ = peer_pid;
  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnChannelConnected(peer_pid);

  // See above comment about using listener_message_loop_ here.
  listener_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &Context::OnDispatchConnected));
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelError() {
  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnChannelError();

  // See above comment about using listener_message_loop_ here.
  listener_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &Context::OnDispatchError));
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelOpened() {
  DCHECK(channel_ != NULL);

  // Assume a reference to ourselves on behalf of this thread.  This reference
  // will be released when we are closed.
  AddRef();

  if (!channel_->Connect()) {
    OnChannelError();
    return;
  }

  for (size_t i = 0; i < filters_.size(); ++i)
    filters_[i]->OnFilterAdded(channel_);
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnChannelClosed() {
  // It's okay for IPC::ChannelProxy::Close to be called more than once, which
  // would result in this branch being taken.
  if (!channel_)
    return;

  for (size_t i = 0; i < filters_.size(); ++i) {
    filters_[i]->OnChannelClosing();
    filters_[i]->OnFilterRemoved();
  }

  // We don't need the filters anymore.
  filters_.clear();

  delete channel_;
  channel_ = NULL;

  // Balance with the reference taken during startup.  This may result in
  // self-destruction.
  Release();
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnSendMessage(Message* message) {
  if (!channel_->Send(message))
    OnChannelError();
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnAddFilter(MessageFilter* filter) {
  filters_.push_back(filter);

  // If the channel has already been created, then we need to send this message
  // so that the filter gets access to the Channel.
  if (channel_)
    filter->OnFilterAdded(channel_);

  // Balances the AddRef in ChannelProxy::AddFilter.
  filter->Release();
}

// Called on the IPC::Channel thread
void ChannelProxy::Context::OnRemoveFilter(MessageFilter* filter) {
  for (size_t i = 0; i < filters_.size(); ++i) {
    if (filters_[i].get() == filter) {
      filter->OnFilterRemoved();
      filters_.erase(filters_.begin() + i);
      return;
    }
  }

  NOTREACHED() << "filter to be removed not found";
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchMessage(const Message& message) {
  if (!listener_)
    return;

  OnDispatchConnected();

#ifdef IPC_MESSAGE_LOG_ENABLED
  Logging* logger = Logging::current();
  if (message.type() == IPC_LOGGING_ID) {
    logger->OnReceivedLoggingMessage(message);
    return;
  }

  if (logger->Enabled())
    logger->OnPreDispatchMessage(message);
#endif

  listener_->OnMessageReceived(message);

#ifdef IPC_MESSAGE_LOG_ENABLED
  if (logger->Enabled())
    logger->OnPostDispatchMessage(message, channel_id_);
#endif
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchConnected() {
  if (channel_connected_called_)
    return;

  channel_connected_called_ = true;
  if (listener_)
    listener_->OnChannelConnected(peer_pid_);
}

// Called on the listener's thread
void ChannelProxy::Context::OnDispatchError() {
  if (listener_)
    listener_->OnChannelError();
}

//-----------------------------------------------------------------------------

ChannelProxy::ChannelProxy(const std::wstring& channel_id, Channel::Mode mode,
                           Channel::Listener* listener, MessageFilter* filter,
                           MessageLoop* ipc_thread)
    : context_(new Context(listener, filter, ipc_thread)) {
  Init(channel_id, mode, ipc_thread, true);
}

ChannelProxy::ChannelProxy(const std::wstring& channel_id, Channel::Mode mode,
                           MessageLoop* ipc_thread, Context* context,
                           bool create_pipe_now)
    : context_(context) {
  Init(channel_id, mode, ipc_thread, create_pipe_now);
}

void ChannelProxy::Init(const std::wstring& channel_id, Channel::Mode mode,
                        MessageLoop* ipc_thread_loop, bool create_pipe_now) {
  if (create_pipe_now) {
    // Create the channel immediately.  This effectively sets up the
    // low-level pipe so that the client can connect.  Without creating
    // the pipe immediately, it is possible for a listener to attempt
    // to connect and get an error since the pipe doesn't exist yet.
    context_->CreateChannel(channel_id, mode);
  } else {
#if defined(OS_POSIX)
    // TODO(playmobil): On POSIX, IPC::Channel uses a socketpair(), one side of
    // which needs to be mapped into the child process' address space.
    // To know the value of the client side FD we need to have already
    // created a socketpair which currently occurs in IPC::Channel's
    // constructor.
    // If we lazilly construct the IPC::Channel then the caller has no way
    // of knowing the FD #.
    //
    // We can solve this either by having the Channel's creation launch the
    // subprocess itself or by creating the socketpair() externally.
    NOTIMPLEMENTED();
#endif  // defined(OS_POSIX)
    context_->ipc_message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        context_.get(), &Context::CreateChannel, channel_id, mode));
  }

  // complete initialization on the background thread
  context_->ipc_message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      context_.get(), &Context::OnChannelOpened));
}

void ChannelProxy::Close() {
  // Clear the backpointer to the listener so that any pending calls to
  // Context::OnDispatchMessage or OnDispatchError will be ignored.  It is
  // possible that the channel could be closed while it is receiving messages!
  context_->Clear();

  if (context_->ipc_message_loop()) {
    context_->ipc_message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        context_.get(), &Context::OnChannelClosed));
  }
}

bool ChannelProxy::Send(Message* message) {
#ifdef IPC_MESSAGE_LOG_ENABLED
  Logging::current()->OnSendMessage(message, context_->channel_id());
#endif

  context_->ipc_message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      context_.get(), &Context::OnSendMessage, message));
  return true;
}

void ChannelProxy::AddFilter(MessageFilter* filter) {
  // We want to addref the filter to prevent it from
  // being destroyed before the OnAddFilter call is invoked.
  filter->AddRef();
  context_->ipc_message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      context_.get(), &Context::OnAddFilter, filter));
}

void ChannelProxy::RemoveFilter(MessageFilter* filter) {
  context_->ipc_message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      context_.get(), &Context::OnRemoveFilter, filter));
}

#if defined(OS_POSIX)
// See the TODO regarding lazy initialization of the channel in
// ChannelProxy::Init().
// We assume that IPC::Channel::GetClientFileDescriptorMapping() is thread-safe.
void ChannelProxy::GetClientFileDescriptorMapping(int *src_fd,
                                                  int *dest_fd) const {
  Channel *channel = context_.get()->channel_;
  DCHECK(channel); // Channel must have been created first.
  channel->GetClientFileDescriptorMapping(src_fd, dest_fd);
}
#endif

//-----------------------------------------------------------------------------

}  // namespace IPC
