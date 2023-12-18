/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 */

/*
Based partially on original code from nICEr and nrappkit.

nICEr copyright:

Copyright (c) 2007, Adobe Systems, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of Adobe Systems, Network Resonance nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


nrappkit copyright:

   Copyright (C) 2001-2003, Network Resonance, Inc.
   Copyright (C) 2006, Network Resonance, Inc.
   All Rights Reserved

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of Network Resonance, Inc. nor the name of any
      contributors to this software may be used to endorse or promote
      products derived from this software without specific prior written
      permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.


   ekr@rtfm.com  Thu Dec 20 20:14:49 2001
*/

// Original author: bcampen@mozilla.com [:bwc]

extern "C" {
#include "stun_msg.h"  // for NR_STUN_MAX_MESSAGE_SIZE
#include "async_wait.h"
#include "async_timer.h"
#include "nr_socket.h"
#include "stun.h"
#include "transport_addr.h"
}

#include "mozilla/RefPtr.h"
#include "test_nr_socket.h"

namespace mozilla {

static int test_nat_socket_create(void* obj, nr_transport_addr* addr,
                                  nr_socket** sockp) {
  RefPtr<NrSocketBase> sock = new TestNrSocket(static_cast<TestNat*>(obj));

  int r, _status;

  r = sock->create(addr);
  if (r) ABORT(r);

  r = nr_socket_create_int(static_cast<void*>(sock), sock->vtbl(), sockp);
  if (r) ABORT(r);

  _status = 0;

  {
    // We will release this reference in destroy(), not exactly the normal
    // ownership model, but it is what it is.
    NrSocketBase* dummy = sock.forget().take();
    (void)dummy;
  }

abort:
  return _status;
}

static int test_nat_socket_factory_destroy(void** obj) {
  TestNat* nat = static_cast<TestNat*>(*obj);
  *obj = nullptr;
  nat->Release();
  return 0;
}

static nr_socket_factory_vtbl test_nat_socket_factory_vtbl = {
    test_nat_socket_create, test_nat_socket_factory_destroy};

/* static */
TestNat::NatBehavior TestNat::ToNatBehavior(const std::string& type) {
  if (type.empty() || !type.compare("ENDPOINT_INDEPENDENT")) {
    return TestNat::ENDPOINT_INDEPENDENT;
  }
  if (!type.compare("ADDRESS_DEPENDENT")) {
    return TestNat::ADDRESS_DEPENDENT;
  }
  if (!type.compare("PORT_DEPENDENT")) {
    return TestNat::PORT_DEPENDENT;
  }

  MOZ_ASSERT(false, "Invalid NAT behavior");
  return TestNat::ENDPOINT_INDEPENDENT;
}

bool TestNat::has_port_mappings() const {
  for (TestNrSocket* sock : sockets_) {
    if (sock->has_port_mappings()) {
      return true;
    }
  }
  return false;
}

bool TestNat::is_my_external_tuple(const nr_transport_addr& addr) const {
  for (TestNrSocket* sock : sockets_) {
    if (sock->is_my_external_tuple(addr)) {
      return true;
    }
  }

  return false;
}

bool TestNat::is_an_internal_tuple(const nr_transport_addr& addr) const {
  for (TestNrSocket* sock : sockets_) {
    nr_transport_addr addr_behind_nat;
    if (sock->getaddr(&addr_behind_nat)) {
      MOZ_CRASH("TestNrSocket::getaddr failed!");
    }

    if (!nr_transport_addr_cmp(&addr, &addr_behind_nat,
                               NR_TRANSPORT_ADDR_CMP_MODE_ALL)) {
      return true;
    }
  }
  return false;
}

int TestNat::create_socket_factory(nr_socket_factory** factorypp) {
  int r = nr_socket_factory_create_int(this, &test_nat_socket_factory_vtbl,
                                       factorypp);
  if (!r) {
    AddRef();
  }
  return r;
}

void TestNat::set_proxy_config(
    std::shared_ptr<NrSocketProxyConfig> aProxyConfig) {
  proxy_config_ = std::move(aProxyConfig);
}

TestNrSocket::TestNrSocket(TestNat* nat)
    : nat_(nat), tls_(false), timer_handle_(nullptr) {
  nat_->insert_socket(this);
}

TestNrSocket::~TestNrSocket() { nat_->erase_socket(this); }

RefPtr<NrSocketBase> TestNrSocket::create_external_socket(
    const nr_transport_addr& dest_addr) const {
  MOZ_ASSERT(nat_->enabled_);
  MOZ_ASSERT(!nat_->is_an_internal_tuple(dest_addr));

  int r;
  nr_transport_addr nat_external_addr;

  // Open the socket on an arbitrary port, on the same address.
  if ((r = nr_transport_addr_copy(&nat_external_addr,
                                  &internal_socket_->my_addr()))) {
    r_log(LOG_GENERIC, LOG_CRIT, "%s: Failure in nr_transport_addr_copy: %d",
          __FUNCTION__, r);
    return nullptr;
  }

  if ((r = nr_transport_addr_set_port(&nat_external_addr, 0))) {
    r_log(LOG_GENERIC, LOG_CRIT,
          "%s: Failure in nr_transport_addr_set_port: %d", __FUNCTION__, r);
    return nullptr;
  }

  RefPtr<NrSocketBase> external_socket;
  r = NrSocketBase::CreateSocket(&nat_external_addr, &external_socket,
                                 nat_->proxy_config_);

  if (r) {
    r_log(LOG_GENERIC, LOG_CRIT, "%s: Failure in NrSocket::create: %d",
          __FUNCTION__, r);
    return nullptr;
  }

  return external_socket;
}

int TestNrSocket::create(nr_transport_addr* addr) {
  tls_ = addr->tls;

  r_log(LOG_GENERIC, LOG_DEBUG, "TestNrSocket %p create %s", this,
        addr->as_string);
  return NrSocketBase::CreateSocket(addr, &internal_socket_, nullptr);
}

int TestNrSocket::getaddr(nr_transport_addr* addrp) {
  return internal_socket_->getaddr(addrp);
}

void TestNrSocket::close() {
  r_log(LOG_GENERIC, LOG_DEBUG, "TestNrSocket %p %s closing", this,
        internal_socket_->my_addr().as_string);
  if (timer_handle_) {
    NR_async_timer_cancel(timer_handle_);
    timer_handle_ = nullptr;
  }
  internal_socket_->close();
  for (RefPtr<PortMapping>& port_mapping : port_mappings_) {
    port_mapping->external_socket_->close();
  }
}

int TestNrSocket::listen(int backlog) {
  MOZ_ASSERT(internal_socket_->my_addr().protocol == IPPROTO_TCP);
  r_log(LOG_GENERIC, LOG_DEBUG, "TestNrSocket %p %s listening", this,
        internal_socket_->my_addr().as_string);

  return internal_socket_->listen(backlog);
}

int TestNrSocket::accept(nr_transport_addr* addrp, nr_socket** sockp) {
  MOZ_ASSERT(internal_socket_->my_addr().protocol == IPPROTO_TCP);
  int r = internal_socket_->accept(addrp, sockp);
  if (r) {
    return r;
  }

  if (nat_->enabled_ && !nat_->is_an_internal_tuple(*addrp)) {
    nr_socket_destroy(sockp);
    return R_IO_ERROR;
  }

  return 0;
}

void TestNrSocket::process_delayed_cb(NR_SOCKET s, int how, void* cb_arg) {
  DeferredPacket* op = static_cast<DeferredPacket*>(cb_arg);
  op->socket_->timer_handle_ = nullptr;
  r_log(LOG_GENERIC, LOG_DEBUG, "TestNrSocket %s sending delayed STUN response",
        op->internal_socket_->my_addr().as_string);
  op->internal_socket_->sendto(op->buffer_.data(), op->buffer_.len(),
                               op->flags_, &op->to_);

  delete op;
}

int TestNrSocket::sendto(const void* msg, size_t len, int flags,
                         const nr_transport_addr* to) {
  MOZ_ASSERT(internal_socket_->my_addr().protocol != IPPROTO_TCP);
  r_log(LOG_GENERIC, LOG_DEBUG, "TestNrSocket %p %s %s", this, __FUNCTION__,
        to->as_string);

  if (nat_->nat_delegate_ &&
      nat_->nat_delegate_->on_sendto(nat_, msg, len, flags, to)) {
    return nat_->error_code_for_drop_;
  }

  UCHAR* buf = static_cast<UCHAR*>(const_cast<void*>(msg));
  if (nat_->block_stun_ && nr_is_stun_message(buf, len)) {
    return nat_->error_code_for_drop_;
  }

  if (nr_is_stun_request_message(buf, len) &&
      maybe_send_fake_response(buf, len, to)) {
    return 0;
  }

  /* TODO: improve the functionality of this in bug 1253657 */
  if (!nat_->enabled_ || nat_->is_an_internal_tuple(*to)) {
    if (nat_->delay_stun_resp_ms_ && nr_is_stun_response_message(buf, len)) {
      NR_ASYNC_TIMER_SET(
          nat_->delay_stun_resp_ms_, process_delayed_cb,
          new DeferredPacket(this, msg, len, flags, to, internal_socket_),
          &timer_handle_);
      return 0;
    }
    return internal_socket_->sendto(msg, len, flags, to);
  }

  destroy_stale_port_mappings();

  if (to->protocol == IPPROTO_UDP && nat_->block_udp_) {
    return nat_->error_code_for_drop_;
  }

  // Choose our port mapping based on our most selective criteria
  PortMapping* port_mapping = get_port_mapping(
      *to, std::max(nat_->filtering_type_, nat_->mapping_type_));

  if (!port_mapping) {
    // See if we have already made the external socket we need to use.
    PortMapping* similar_port_mapping =
        get_port_mapping(*to, nat_->mapping_type_);
    RefPtr<NrSocketBase> external_socket;

    if (similar_port_mapping) {
      external_socket = similar_port_mapping->external_socket_;
    } else {
      external_socket = create_external_socket(*to);
      if (!external_socket) {
        MOZ_ASSERT(false);
        return R_INTERNAL;
      }
    }

    port_mapping = create_port_mapping(*to, external_socket);
    port_mappings_.push_back(port_mapping);

    if (poll_flags() & PR_POLL_READ) {
      // Make sure the new port mapping is ready to receive traffic if the
      // TestNrSocket is already waiting.
      port_mapping->async_wait(NR_ASYNC_WAIT_READ, socket_readable_callback,
                               this, (char*)__FUNCTION__, __LINE__);
    }
  }

  // We probably don't want to propagate the flags, since this is a simulated
  // external IP address.
  return port_mapping->sendto(msg, len, *to);
}

int TestNrSocket::recvfrom(void* buf, size_t maxlen, size_t* len, int flags,
                           nr_transport_addr* from) {
  MOZ_ASSERT(internal_socket_->my_addr().protocol != IPPROTO_TCP);

  if (!read_buffer_.empty()) {
    UdpPacket& packet = read_buffer_.front();
    *len = std::min(maxlen, packet.buffer_->len());
    memcpy(buf, packet.buffer_->data(), *len);
    nr_transport_addr_copy(from, &packet.remote_address_);
    read_buffer_.pop_front();
    return 0;
  }

  int r;
  bool ingress_allowed = false;

  if (readable_socket_) {
    // If any of the external sockets got data, see if it will be passed through
    r = readable_socket_->recvfrom(buf, maxlen, len, 0, from);
    const nr_transport_addr to = readable_socket_->my_addr();
    readable_socket_ = nullptr;
    if (!r) {
      PortMapping* port_mapping_used;
      ingress_allowed = allow_ingress(to, *from, &port_mapping_used);
      if (ingress_allowed) {
        r_log(LOG_GENERIC, LOG_DEBUG, "TestNrSocket %s received from %s via %s",
              internal_socket_->my_addr().as_string, from->as_string,
              port_mapping_used->external_socket_->my_addr().as_string);
        if (nat_->refresh_on_ingress_) {
          port_mapping_used->last_used_ = PR_IntervalNow();
        }
      }
    }
  } else {
    // If no external socket has data, see if there's any data that was sent
    // directly to the TestNrSocket, and eat it if it isn't supposed to get
    // through.
    r = internal_socket_->recvfrom(buf, maxlen, len, flags, from);
    if (!r) {
      // We do not use allow_ingress() here because that only handles traffic
      // landing on an external port.
      ingress_allowed = (!nat_->enabled_ || nat_->is_an_internal_tuple(*from));
      if (!ingress_allowed) {
        r_log(LOG_GENERIC, LOG_INFO,
              "TestNrSocket %s denying ingress from %s: "
              "Not behind the same NAT",
              internal_socket_->my_addr().as_string, from->as_string);
      } else {
        r_log(LOG_GENERIC, LOG_DEBUG, "TestNrSocket %s received from %s",
              internal_socket_->my_addr().as_string, from->as_string);
      }
    }
  }

  // Kinda bad that we are forced to give the app a readable callback and then
  // say "Oh, never mind...", but the alternative is to totally decouple the
  // callbacks from STS and the callbacks the app sets. On the bright side, this
  // speeds up unit tests where we are verifying that ingress is forbidden,
  // since they'll get a readable callback and then an error, instead of having
  // to wait for a timeout.
  if (!ingress_allowed) {
    *len = 0;
    r = R_WOULDBLOCK;
  }

  return r;
}

bool TestNrSocket::allow_ingress(const nr_transport_addr& to,
                                 const nr_transport_addr& from,
                                 PortMapping** port_mapping_used) const {
  // This is only called for traffic arriving at a port mapping
  MOZ_ASSERT(nat_->enabled_);
  MOZ_ASSERT(!nat_->is_an_internal_tuple(from));

  // Find the port mapping (if any) that this packet landed on
  *port_mapping_used = nullptr;
  for (PortMapping* port_mapping : port_mappings_) {
    if (!nr_transport_addr_cmp(&to, &port_mapping->external_socket_->my_addr(),
                               NR_TRANSPORT_ADDR_CMP_MODE_ALL) &&
        !is_port_mapping_stale(*port_mapping)) {
      *port_mapping_used = port_mapping;
      // TODO: Bug 1857149
      // Adding a break here causes test failures, but we would not expect to
      // find more than one matching mapping at a time.
    }
  }

  if (!(*port_mapping_used)) {
    r_log(LOG_GENERIC, LOG_INFO,
          "TestNrSocket %s denying ingress from %s: "
          "No non-stale port mapping for this local port.",
          internal_socket_->my_addr().as_string, from.as_string);
    return false;
  }

  if (!port_mapping_matches(**port_mapping_used, from, nat_->filtering_type_)) {
    r_log(LOG_GENERIC, LOG_INFO,
          "TestNrSocket %s denying ingress from %s: "
          "Filtered (no port mapping for source)",
          internal_socket_->my_addr().as_string, from.as_string);
    return false;
  }

  if (!nat_->allow_hairpinning_ && nat_->is_my_external_tuple(from)) {
    r_log(LOG_GENERIC, LOG_INFO,
          "TestNrSocket %s denying ingress from %s: "
          "Hairpinning disallowed",
          internal_socket_->my_addr().as_string, from.as_string);
    return false;
  }

  return true;
}

int TestNrSocket::connect(const nr_transport_addr* addr) {
  r_log(LOG_GENERIC, LOG_DEBUG, "TestNrSocket %p %s connecting to %s", this,
        internal_socket_->my_addr().as_string, addr->as_string);

  if (connect_invoked_ || !port_mappings_.empty()) {
    MOZ_CRASH("TestNrSocket::connect() called more than once!");
    return R_INTERNAL;
  }

  if (maybe_get_redirect_targets(addr).isSome()) {
    // If we are simulating STUN redirects for |addr|, we need to pretend that
    // the TCP connection worked, since |addr| probably does not actually point
    // at something that exists.
    connect_fake_stun_address_.reset(new nr_transport_addr);
    nr_transport_addr_copy(connect_fake_stun_address_.get(), addr);

    // We dispatch this, otherwise nICEr can trip over its shoelaces
    GetCurrentSerialEventTarget()->Dispatch(
        NS_NewRunnableFunction("Async writeable callback for TestNrSocket",
                               [this, self = RefPtr<TestNrSocket>(this)] {
                                 if (poll_flags() & PR_POLL_WRITE) {
                                   fire_callback(NR_ASYNC_WAIT_WRITE);
                                 }
                               }));

    return R_WOULDBLOCK;
  }

  if (!nat_->enabled_ ||
      addr->protocol == IPPROTO_UDP  // Horrible hack to allow default address
                                     // discovery to work. Only works because
                                     // we don't normally connect on UDP.
      || nat_->is_an_internal_tuple(*addr)) {
    // This will set connect_invoked_
    return internal_socket_->connect(addr);
  }

  RefPtr<NrSocketBase> external_socket(create_external_socket(*addr));
  if (!external_socket) {
    return R_INTERNAL;
  }

  PortMapping* port_mapping = create_port_mapping(*addr, external_socket);
  port_mappings_.push_back(port_mapping);
  int r = port_mapping->external_socket_->connect(addr);
  if (r && r != R_WOULDBLOCK) {
    return r;
  }

  port_mapping->last_used_ = PR_IntervalNow();

  if (poll_flags() & PR_POLL_READ) {
    port_mapping->async_wait(NR_ASYNC_WAIT_READ,
                             port_mapping_tcp_passthrough_callback, this,
                             (char*)__FUNCTION__, __LINE__);
  }

  return r;
}

int TestNrSocket::write(const void* msg, size_t len, size_t* written) {
  r_log(LOG_GENERIC, LOG_DEBUG, "TestNrSocket %p %s writing", this,
        internal_socket_->my_addr().as_string);

  UCHAR* buf = static_cast<UCHAR*>(const_cast<void*>(msg));

  if (nat_->nat_delegate_ &&
      nat_->nat_delegate_->on_write(nat_, msg, len, written)) {
    return R_INTERNAL;
  }

  if (nat_->block_stun_ && nr_is_stun_message(buf, len)) {
    // Should cause this socket to be abandoned
    r_log(LOG_GENERIC, LOG_DEBUG,
          "TestNrSocket %s dropping outgoing TCP "
          "because it is configured to drop STUN",
          my_addr().as_string);
    return R_INTERNAL;
  }

  if (nr_is_stun_request_message(buf, len) && connect_fake_stun_address_ &&
      maybe_send_fake_response(buf, len, connect_fake_stun_address_.get())) {
    return 0;
  }

  if (nat_->block_tcp_ && !tls_) {
    // Should cause this socket to be abandoned
    r_log(LOG_GENERIC, LOG_DEBUG,
          "TestNrSocket %s dropping outgoing TCP "
          "because it is configured to drop TCP",
          my_addr().as_string);
    return R_INTERNAL;
  }

  if (nat_->block_tls_ && tls_) {
    // Should cause this socket to be abandoned
    r_log(LOG_GENERIC, LOG_DEBUG,
          "TestNrSocket %s dropping outgoing TLS "
          "because it is configured to drop TLS",
          my_addr().as_string);
    return R_INTERNAL;
  }

  if (port_mappings_.empty()) {
    // The no-nat case, just pass call through.
    r_log(LOG_GENERIC, LOG_DEBUG, "TestNrSocket %s writing",
          my_addr().as_string);

    return internal_socket_->write(msg, len, written);
  }
  destroy_stale_port_mappings();
  if (port_mappings_.empty()) {
    r_log(LOG_GENERIC, LOG_DEBUG,
          "TestNrSocket %s dropping outgoing TCP "
          "because the port mapping was stale",
          my_addr().as_string);
    return R_INTERNAL;
  }
  // This is TCP only
  MOZ_ASSERT(port_mappings_.size() == 1);
  r_log(LOG_GENERIC, LOG_DEBUG, "PortMapping %s -> %s writing",
        port_mappings_.front()->external_socket_->my_addr().as_string,
        port_mappings_.front()->remote_address_.as_string);
  port_mappings_.front()->last_used_ = PR_IntervalNow();
  return port_mappings_.front()->external_socket_->write(msg, len, written);
}

int TestNrSocket::read(void* buf, size_t maxlen, size_t* len) {
  r_log(LOG_GENERIC, LOG_DEBUG, "TestNrSocket %p %s reading", this,
        internal_socket_->my_addr().as_string);

  if (!read_buffer_.empty()) {
    r_log(LOG_GENERIC, LOG_DEBUG,
          "TestNrSocket %p %s has stuff in read_buffer_", this,
          internal_socket_->my_addr().as_string);
    UdpPacket packet(std::move(read_buffer_.front()));
    read_buffer_.pop_front();
    *len = std::min(maxlen, packet.buffer_->len());
    memcpy(buf, packet.buffer_->data(), *len);
    if (*len != packet.buffer_->len()) {
      // Put remaining bytes in new packet, at the front.
      read_buffer_.emplace_front(packet.buffer_->data() + *len,
                                 packet.buffer_->len() - *len,
                                 packet.remote_address_);
    }
    return 0;
  }

  if (connect_fake_stun_address_) {
    return R_WOULDBLOCK;
  }

  int r;

  if (port_mappings_.empty()) {
    r = internal_socket_->read(buf, maxlen, len);
  } else {
    MOZ_ASSERT(port_mappings_.size() == 1);
    r = port_mappings_.front()->external_socket_->read(buf, maxlen, len);
    if (!r && nat_->refresh_on_ingress_) {
      port_mappings_.front()->last_used_ = PR_IntervalNow();
    }
  }

  if (r) {
    return r;
  }

  if (nat_->nat_delegate_ &&
      nat_->nat_delegate_->on_read(nat_, buf, maxlen, len)) {
    return R_INTERNAL;
  }

  if (nat_->block_tcp_ && !tls_) {
    // Should cause this socket to be abandoned
    return R_INTERNAL;
  }

  if (nat_->block_tls_ && tls_) {
    // Should cause this socket to be abandoned
    return R_INTERNAL;
  }

  UCHAR* cbuf = static_cast<UCHAR*>(const_cast<void*>(buf));
  if (nat_->block_stun_ && nr_is_stun_message(cbuf, *len)) {
    // Should cause this socket to be abandoned
    return R_INTERNAL;
  }

  return r;
}

int TestNrSocket::async_wait(int how, NR_async_cb cb, void* cb_arg,
                             char* function, int line) {
  r_log(LOG_GENERIC, LOG_DEBUG, "TestNrSocket %s waiting for %s",
        internal_socket_->my_addr().as_string,
        how == NR_ASYNC_WAIT_READ ? "read" : "write");

  int r;

  if (how == NR_ASYNC_WAIT_READ) {
    NrSocketBase::async_wait(how, cb, cb_arg, function, line);
    if (!read_buffer_.empty()) {
      fire_readable_callback();
      return 0;
    }

    // Make sure we're waiting on the socket for the internal address
    r = internal_socket_->async_wait(how, socket_readable_callback, this,
                                     function, line);
  } else {
    if (connect_fake_stun_address_) {
      // Fake TCP connection case; register the callback on this socket, not
      // a real one.
      return NrSocketBase::async_wait(how, cb, cb_arg, function, line);
    }

    // For write, just use the readiness of the internal socket, since we queue
    // everything for the port mappings.
    r = internal_socket_->async_wait(how, cb, cb_arg, function, line);
  }

  if (r) {
    r_log(LOG_GENERIC, LOG_ERR,
          "TestNrSocket %s failed to async_wait for "
          "internal socket: %d\n",
          internal_socket_->my_addr().as_string, r);
    return r;
  }

  if (is_tcp_connection_behind_nat()) {
    // Bypass all port-mapping related logic
    return 0;
  }

  if (internal_socket_->my_addr().protocol == IPPROTO_TCP) {
    // For a TCP connection through a simulated NAT, these signals are
    // just passed through.
    MOZ_ASSERT(port_mappings_.size() == 1);

    return port_mappings_.front()->async_wait(
        how, port_mapping_tcp_passthrough_callback, this, function, line);
  }
  if (how == NR_ASYNC_WAIT_READ) {
    // For UDP port mappings, we decouple the writeable callbacks
    for (PortMapping* port_mapping : port_mappings_) {
      // Be ready to receive traffic on our port mappings
      r = port_mapping->async_wait(how, socket_readable_callback, this,
                                   function, line);
      if (r) {
        r_log(LOG_GENERIC, LOG_ERR,
              "TestNrSocket %s failed to async_wait for "
              "port mapping: %d\n",
              internal_socket_->my_addr().as_string, r);
        return r;
      }
    }
  }

  return 0;
}

void TestNrSocket::cancel_port_mapping_async_wait(int how) {
  for (PortMapping* port_mapping : port_mappings_) {
    port_mapping->cancel(how);
  }
}

int TestNrSocket::cancel(int how) {
  r_log(LOG_GENERIC, LOG_DEBUG, "TestNrSocket %s stop waiting for %s",
        internal_socket_->my_addr().as_string,
        how == NR_ASYNC_WAIT_READ ? "read" : "write");

  if (connect_fake_stun_address_) {
    return NrSocketBase::cancel(how);
  }

  // Writable callbacks are decoupled except for the TCP case
  if (how == NR_ASYNC_WAIT_READ ||
      internal_socket_->my_addr().protocol == IPPROTO_TCP) {
    cancel_port_mapping_async_wait(how);
  }

  return internal_socket_->cancel(how);
}

bool TestNrSocket::has_port_mappings() const { return !port_mappings_.empty(); }

bool TestNrSocket::is_my_external_tuple(const nr_transport_addr& addr) const {
  for (PortMapping* port_mapping : port_mappings_) {
    nr_transport_addr port_mapping_addr;
    if (port_mapping->external_socket_->getaddr(&port_mapping_addr)) {
      MOZ_CRASH("NrSocket::getaddr failed!");
    }

    if (!nr_transport_addr_cmp(&addr, &port_mapping_addr,
                               NR_TRANSPORT_ADDR_CMP_MODE_ALL)) {
      return true;
    }
  }
  return false;
}

bool TestNrSocket::is_port_mapping_stale(
    const PortMapping& port_mapping) const {
  PRIntervalTime now = PR_IntervalNow();
  PRIntervalTime elapsed_ticks = now - port_mapping.last_used_;
  uint32_t idle_duration = PR_IntervalToMilliseconds(elapsed_ticks);
  r_log(LOG_GENERIC, LOG_INFO,
        "TestNrSocket %s port mapping %s -> %s last used %u",
        internal_socket_->my_addr().as_string,
        port_mapping.external_socket_->my_addr().as_string,
        port_mapping.remote_address_.as_string,
        static_cast<unsigned>(idle_duration));
  return idle_duration > nat_->mapping_timeout_;
}

void TestNrSocket::destroy_stale_port_mappings() {
  for (auto i = port_mappings_.begin(); i != port_mappings_.end();) {
    auto temp = i;
    ++i;
    if (is_port_mapping_stale(**temp)) {
      r_log(LOG_GENERIC, LOG_INFO,
            "TestNrSocket %s destroying port mapping %s -> %s",
            internal_socket_->my_addr().as_string,
            (*temp)->external_socket_->my_addr().as_string,
            (*temp)->remote_address_.as_string);

      port_mappings_.erase(temp);
    }
  }
}

void TestNrSocket::socket_readable_callback(void* real_sock_v, int how,
                                            void* test_sock_v) {
  TestNrSocket* test_socket = static_cast<TestNrSocket*>(test_sock_v);
  NrSocketBase* real_socket = static_cast<NrSocketBase*>(real_sock_v);

  test_socket->on_socket_readable(real_socket);
}

void TestNrSocket::on_socket_readable(NrSocketBase* real_socket) {
  if (!readable_socket_ && (real_socket != internal_socket_)) {
    readable_socket_ = real_socket;
  }

  fire_readable_callback();
}

void TestNrSocket::fire_readable_callback() {
  MOZ_ASSERT(poll_flags() & PR_POLL_READ);
  r_log(LOG_GENERIC, LOG_DEBUG, "TestNrSocket %p %s ready for read", this,
        internal_socket_->my_addr().as_string);
  fire_callback(NR_ASYNC_WAIT_READ);
}

void TestNrSocket::port_mapping_writeable_callback(void* ext_sock_v, int how,
                                                   void* test_sock_v) {
  TestNrSocket* test_socket = static_cast<TestNrSocket*>(test_sock_v);
  NrSocketBase* external_socket = static_cast<NrSocketBase*>(ext_sock_v);

  test_socket->write_to_port_mapping(external_socket);
}

void TestNrSocket::write_to_port_mapping(NrSocketBase* external_socket) {
  MOZ_ASSERT(internal_socket_->my_addr().protocol != IPPROTO_TCP);

  int r = 0;
  for (PortMapping* port_mapping : port_mappings_) {
    if (port_mapping->external_socket_ == external_socket) {
      // If the send succeeds, or if there was nothing to send, we keep going
      r = port_mapping->send_from_queue();
      if (r) {
        break;
      }
    }
  }

  if (r == R_WOULDBLOCK) {
    // Re-register for writeable callbacks, since we still have stuff to send
    NR_ASYNC_WAIT(external_socket, NR_ASYNC_WAIT_WRITE,
                  &TestNrSocket::port_mapping_writeable_callback, this);
  }
}

void TestNrSocket::port_mapping_tcp_passthrough_callback(void* ext_sock_v,
                                                         int how,
                                                         void* test_sock_v) {
  TestNrSocket* test_socket = static_cast<TestNrSocket*>(test_sock_v);
  r_log(LOG_GENERIC, LOG_DEBUG, "TestNrSocket %s firing %s callback",
        test_socket->internal_socket_->my_addr().as_string,
        how == NR_ASYNC_WAIT_READ ? "readable" : "writeable");

  test_socket->internal_socket_->fire_callback(how);
}

bool TestNrSocket::is_tcp_connection_behind_nat() const {
  return internal_socket_->my_addr().protocol == IPPROTO_TCP &&
         port_mappings_.empty();
}

TestNrSocket::PortMapping* TestNrSocket::get_port_mapping(
    const nr_transport_addr& remote_address,
    TestNat::NatBehavior filter) const {
  for (PortMapping* port_mapping : port_mappings_) {
    if (port_mapping_matches(*port_mapping, remote_address, filter)) {
      return port_mapping;
    }
  }
  return nullptr;
}

/* static */
bool TestNrSocket::port_mapping_matches(const PortMapping& port_mapping,
                                        const nr_transport_addr& remote_addr,
                                        TestNat::NatBehavior filter) {
  int compare_flags;
  switch (filter) {
    case TestNat::ENDPOINT_INDEPENDENT:
      compare_flags = NR_TRANSPORT_ADDR_CMP_MODE_PROTOCOL;
      break;
    case TestNat::ADDRESS_DEPENDENT:
      compare_flags = NR_TRANSPORT_ADDR_CMP_MODE_ADDR;
      break;
    case TestNat::PORT_DEPENDENT:
      compare_flags = NR_TRANSPORT_ADDR_CMP_MODE_ALL;
      break;
  }

  return !nr_transport_addr_cmp(&remote_addr, &port_mapping.remote_address_,
                                compare_flags);
}

TestNrSocket::PortMapping* TestNrSocket::create_port_mapping(
    const nr_transport_addr& remote_address,
    const RefPtr<NrSocketBase>& external_socket) const {
  r_log(LOG_GENERIC, LOG_INFO, "TestNrSocket %s creating port mapping %s -> %s",
        internal_socket_->my_addr().as_string,
        external_socket->my_addr().as_string, remote_address.as_string);

  return new PortMapping(remote_address, external_socket);
}

TestNrSocket::PortMapping::PortMapping(
    const nr_transport_addr& remote_address,
    const RefPtr<NrSocketBase>& external_socket)
    : external_socket_(external_socket) {
  nr_transport_addr_copy(&remote_address_, &remote_address);
}

int TestNrSocket::PortMapping::send_from_queue() {
  MOZ_ASSERT(remote_address_.protocol != IPPROTO_TCP);
  int r = 0;

  while (!send_queue_.empty()) {
    UdpPacket& packet = send_queue_.front();
    r_log(LOG_GENERIC, LOG_DEBUG,
          "PortMapping %s -> %s sending from queue to %s",
          external_socket_->my_addr().as_string, remote_address_.as_string,
          packet.remote_address_.as_string);

    r = external_socket_->sendto(packet.buffer_->data(), packet.buffer_->len(),
                                 0, &packet.remote_address_);

    if (r) {
      if (r != R_WOULDBLOCK) {
        r_log(LOG_GENERIC, LOG_ERR, "%s: Fatal error %d, stop trying",
              __FUNCTION__, r);
        send_queue_.clear();
      } else {
        r_log(LOG_GENERIC, LOG_DEBUG, "Would block, will retry later");
      }
      break;
    }

    send_queue_.pop_front();
  }

  return r;
}

int TestNrSocket::PortMapping::sendto(const void* msg, size_t len,
                                      const nr_transport_addr& to) {
  MOZ_ASSERT(remote_address_.protocol != IPPROTO_TCP);
  r_log(LOG_GENERIC, LOG_DEBUG, "PortMapping %s -> %s sending to %s",
        external_socket_->my_addr().as_string, remote_address_.as_string,
        to.as_string);

  last_used_ = PR_IntervalNow();
  int r = external_socket_->sendto(msg, len, 0, &to);

  if (r == R_WOULDBLOCK) {
    r_log(LOG_GENERIC, LOG_DEBUG, "Enqueueing UDP packet to %s", to.as_string);
    send_queue_.emplace_back(msg, len, to);
    return 0;
  }
  if (r) {
    r_log(LOG_GENERIC, LOG_ERR, "Error: %d", r);
  }

  return r;
}

int TestNrSocket::PortMapping::async_wait(int how, NR_async_cb cb, void* cb_arg,
                                          char* function, int line) {
  r_log(LOG_GENERIC, LOG_DEBUG, "PortMapping %s -> %s waiting for %s",
        external_socket_->my_addr().as_string, remote_address_.as_string,
        how == NR_ASYNC_WAIT_READ ? "read" : "write");

  return external_socket_->async_wait(how, cb, cb_arg, function, line);
}

int TestNrSocket::PortMapping::cancel(int how) {
  r_log(LOG_GENERIC, LOG_DEBUG, "PortMapping %s -> %s stop waiting for %s",
        external_socket_->my_addr().as_string, remote_address_.as_string,
        how == NR_ASYNC_WAIT_READ ? "read" : "write");

  return external_socket_->cancel(how);
}

class nr_stun_message_deleter {
 public:
  nr_stun_message_deleter() = default;
  void operator()(nr_stun_message* msg) const { nr_stun_message_destroy(&msg); }
};

bool TestNrSocket::maybe_send_fake_response(const void* msg, size_t len,
                                            const nr_transport_addr* to) {
  Maybe<nsTArray<nsCString>> redirect_targets = maybe_get_redirect_targets(to);
  if (!redirect_targets.isSome()) {
    return false;
  }

  std::unique_ptr<nr_stun_message, nr_stun_message_deleter> request;
  {
    nr_stun_message* temp = nullptr;
    if (NS_WARN_IF(nr_stun_message_create2(&temp, (unsigned char*)msg, len))) {
      return false;
    }
    request.reset(temp);
  }

  if (NS_WARN_IF(nr_stun_decode_message(request.get(), nullptr, nullptr))) {
    return false;
  }

  std::unique_ptr<nr_stun_message, nr_stun_message_deleter> response;
  {
    nr_stun_message* temp = nullptr;
    if (nr_stun_message_create(&temp)) {
      MOZ_CRASH("nr_stun_message_create failed!");
    }
    response.reset(temp);
  }

  nr_stun_form_error_response(request.get(), response.get(), 300,
                              (char*)"Try alternate");

  int port = 0;
  if (nr_transport_addr_get_port(to, &port)) {
    MOZ_CRASH();
  }

  for (const nsCString& address : *redirect_targets) {
    r_log(LOG_GENERIC, LOG_DEBUG,
          "TestNrSocket attempting to add alternate server %s", address.Data());
    nr_transport_addr addr;
    if (NS_WARN_IF(nr_str_port_to_transport_addr(address.Data(), port,
                                                 IPPROTO_UDP, &addr))) {
      continue;
    }
    if (nr_stun_message_add_alternate_server_attribute(response.get(), &addr)) {
      MOZ_CRASH("nr_stun_message_add_alternate_server_attribute failed!");
    }
  }

  if (nr_stun_encode_message(response.get())) {
    MOZ_CRASH("nr_stun_encode_message failed!");
  }

  nr_transport_addr response_from;
  if (nr_transport_addr_is_wildcard(to)) {
    // |to| points to an FQDN, and nICEr is delegating DNS lookup to us; we
    // aren't _actually_ going to do that though, so we select a bogus address
    // for the response to come from. TEST-NET is a fairly reasonable thing to
    // use for this.
    int port = 0;
    if (nr_transport_addr_get_port(to, &port)) {
      MOZ_CRASH();
    }
    switch (to->ip_version) {
      case NR_IPV4:
        if (nr_str_port_to_transport_addr("198.51.100.1", port, to->protocol,
                                          &response_from)) {
          MOZ_CRASH();
        }
        break;
      case NR_IPV6:
        if (nr_str_port_to_transport_addr("::ffff:198.51.100.1", port,
                                          to->protocol, &response_from)) {
          MOZ_CRASH();
        }
        break;
      default:
        MOZ_CRASH();
    }
  } else {
    nr_transport_addr_copy(&response_from, to);
  }

  read_buffer_.emplace_back(response->buffer, response->length, response_from);

  // We dispatch this, otherwise nICEr can trip over its shoelaces
  r_log(LOG_GENERIC, LOG_DEBUG,
        "TestNrSocket %p scheduling callback for redirect response", this);
  GetCurrentSerialEventTarget()->Dispatch(NS_NewRunnableFunction(
      "Async readable callback for TestNrSocket",
      [this, self = RefPtr<TestNrSocket>(this)] {
        if (poll_flags() & PR_POLL_READ) {
          fire_readable_callback();
        } else {
          r_log(LOG_GENERIC, LOG_DEBUG,
                "TestNrSocket %p deferring callback for redirect response",
                this);
        }
      }));

  return true;
}

Maybe<nsTArray<nsCString>> TestNrSocket::maybe_get_redirect_targets(
    const nr_transport_addr* to) const {
  Maybe<nsTArray<nsCString>> result;

  // 256 is overkill, but it hardly matters
  char addrstring[256];
  if (nr_transport_addr_get_addrstring(to, addrstring, 256)) {
    MOZ_CRASH("nr_transport_addr_get_addrstring failed!");
  }

  r_log(LOG_GENERIC, LOG_DEBUG, "TestNrSocket checking redirect rules for %s",
        addrstring);
  auto it = nat_->stun_redirect_map_.find(nsCString(addrstring));
  if (it != nat_->stun_redirect_map_.end()) {
    result = Some(it->second);
  }

  return result;
}

}  // namespace mozilla
