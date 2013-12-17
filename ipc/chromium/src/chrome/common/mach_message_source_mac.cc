// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/mach_message_source_mac.h"

#include "base/logging.h"

MachMessageSource::MachMessageSource(mach_port_t port,
                                     MachPortListener* msg_listener,
                                     bool* success) {
  DCHECK(msg_listener);
  DCHECK(success);
  DCHECK(port != MACH_PORT_NULL);

  CFMachPortContext port_context = {0};
  port_context.info = msg_listener;

  scoped_cftyperef<CFMachPortRef> cf_mach_port_ref(
     CFMachPortCreateWithPort(kCFAllocatorDefault,
                              port,
                              MachMessageSource::OnReceiveMachMessage,
                              &port_context,
                              NULL));

  if (cf_mach_port_ref.get() == NULL) {
   CHROMIUM_LOG(WARNING) << "CFMachPortCreate failed";
   *success = false;
   return;
  }

  // Create a RL source.
  machport_runloop_ref_.reset(
     CFMachPortCreateRunLoopSource(kCFAllocatorDefault,
                                   cf_mach_port_ref.get(),
                                   0));

  if (machport_runloop_ref_.get() == NULL) {
   CHROMIUM_LOG(WARNING) << "CFMachPortCreateRunLoopSource failed";
   *success = false;
   return;
  }

  CFRunLoopAddSource(CFRunLoopGetCurrent(),
                    machport_runloop_ref_.get(),
                    kCFRunLoopCommonModes);
  *success = true;
}

MachMessageSource::~MachMessageSource() {
  CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
                        machport_runloop_ref_.get(),
                        kCFRunLoopCommonModes);
}

// static
void MachMessageSource::OnReceiveMachMessage(CFMachPortRef port, void* msg,
                                             CFIndex size, void* closure) {
  MachPortListener *msg_listener = static_cast<MachPortListener*>(closure);
  size_t msg_size = (size < 0) ? 0 : static_cast<size_t>(size);
  DCHECK(msg && msg_size > 0);  // this should never happen!

  if (msg_listener && msg && msg_size > 0) {
    msg_listener->OnMachMessageReceived(msg, msg_size);
  }
}
