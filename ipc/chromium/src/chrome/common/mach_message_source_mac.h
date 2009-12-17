// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MACH_MESSAGE_SOURCE_MAC_H_
#define CHROME_COMMON_MACH_MESSAGE_SOURCE_MAC_H_

#include <CoreServices/CoreServices.h>

#include "base/scoped_cftyperef.h"

// Handles registering and cleaning up after a CFRunloopSource for a Mach port.
// Messages received on the port are piped through to a delegate.
//
// Example:
//  class MyListener : public MachMessageSource::MachPortListener {
//   public:
//    void OnMachMessageReceived(void* mach_msg, size_t size) {
//      printf("received message on Mach port\n");
//    }
//  };
//
//  mach_port_t a_port = ...;
//  MyListener listener;
//  bool success = false;
//  MachMessageSource message_source(port, listener, &success);
//
//  if (!success) {
//    exit(1);  // Couldn't register mach runloop source.
//  }
//
//  CFRunLoopRun();  // Process messages on runloop...
class MachMessageSource {
 public:
  // Classes that want to listen on a Mach port can implement
  // OnMachMessageReceived, |mach_msg| is a pointer to the raw message data and
  // |size| is the buffer size;
  class MachPortListener {
   public:
    virtual void OnMachMessageReceived(void* mach_msg, size_t size) = 0;
  };

  // |listener| is a week reference passed to CF, it needs to remain in
  // existence till this object is destroeyd.
  MachMessageSource(mach_port_t port,
                    MachPortListener* listener,
                    bool* success);
  ~MachMessageSource();

 private:
  // Called by CF when a new message arrives on the Mach port.
  static void OnReceiveMachMessage(CFMachPortRef port, void* msg, CFIndex size,
                                   void* closure);

  scoped_cftyperef<CFRunLoopSourceRef> machport_runloop_ref_;
  DISALLOW_COPY_AND_ASSIGN(MachMessageSource);
};

#endif // CHROME_COMMON_MACH_MESSAGE_SOURCE_MAC_H_
