/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_SharedMemoryBasic_mach_h
#define mozilla_ipc_SharedMemoryBasic_mach_h

#include "base/file_descriptor_posix.h"
#include "base/process.h"

#include "mozilla/ipc/SharedMemory.h"
#include <mach/port.h>
#include "chrome/common/mach_ipc_mac.h"

#ifdef FUZZING
#  include "mozilla/ipc/SharedMemoryFuzzer.h"
#endif

//
// This is a low-level wrapper around platform shared memory.  Don't
// use it directly; use Shmem allocated through IPDL interfaces.
//

class MachPortSender;
class ReceivePort;

namespace mozilla {
namespace ipc {

enum {
  kGetPortsMsg = 1,
  kSharePortsMsg,
  kWaitForTexturesMsg,
  kUpdateTextureLocksMsg,
  kReturnIdMsg,
  kReturnWaitForTexturesMsg,
  kReturnPortsMsg,
  kShutdownMsg,
  kCleanupMsg,
};

struct MemoryPorts {
  MachPortSender* mSender;
  ReceivePort* mReceiver;

  MemoryPorts() = default;
  MemoryPorts(MachPortSender* sender, ReceivePort* receiver)
      : mSender(sender), mReceiver(receiver) {}
};

class SharedMemoryBasic final : public SharedMemoryCommon<mach_port_t> {
 public:
  static void SetupMachMemory(pid_t pid, ReceivePort* listen_port,
                              MachPortSender* listen_port_ack,
                              MachPortSender* send_port,
                              ReceivePort* send_port_ack, bool pidIsParent);

  static void CleanupForPid(pid_t pid);
  static void CleanupForPidWithLock(pid_t pid);

  static void Shutdown();

  static bool SendMachMessage(pid_t pid, MachSendMessage& message,
                              MachReceiveMessage* response);

  SharedMemoryBasic();

  virtual bool SetHandle(const Handle& aHandle, OpenRights aRights) override;

  virtual bool Create(size_t aNbytes) override;

  virtual bool Map(size_t nBytes, void* fixed_address = nullptr) override;

  virtual void Unmap() override;

  virtual void CloseHandle() override;

  virtual void* memory() const override {
#ifdef FUZZING
    return SharedMemoryFuzzer::MutateSharedMemory(mMemory, mAllocSize);
#else
    return mMemory;
#endif
  }

  virtual SharedMemoryType Type() const override { return TYPE_BASIC; }

  static Handle NULLHandle() { return Handle(); }

  static void* FindFreeAddressSpace(size_t aSize);

  virtual bool IsHandleValid(const Handle& aHandle) const override;

  virtual bool ShareToProcess(base::ProcessId aProcessId,
                              Handle* aNewHandle) override;

 private:
  ~SharedMemoryBasic();

  mach_port_t mPort;
  // Pointer to mapped region, null if unmapped.
  void* mMemory;
  // Access rights to map an existing region with.
  OpenRights mOpenRights;
};

}  // namespace ipc
}  // namespace mozilla

#endif  // ifndef mozilla_ipc_SharedMemoryBasic_mach_h
