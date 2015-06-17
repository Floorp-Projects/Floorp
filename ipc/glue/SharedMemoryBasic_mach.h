/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_SharedMemoryBasic_mach_h
#define mozilla_ipc_SharedMemoryBasic_mach_h

#include "base/file_descriptor_posix.h"
#include "base/process.h"

#include "SharedMemory.h"
#include <mach/port.h>

//
// This is a low-level wrapper around platform shared memory.  Don't
// use it directly; use Shmem allocated through IPDL interfaces.
//

class MachPortSender;
class ReceivePort;

namespace mozilla {
namespace ipc {

class SharedMemoryBasic final : public SharedMemory
{
public:
  typedef mach_port_t Handle;

  static void SetupMachMemory(pid_t pid,
                              ReceivePort* listen_port,
                              MachPortSender* listen_port_ack,
                              MachPortSender* send_port,
                              ReceivePort* send_port_ack,
                              bool pidIsParent);

  static void CleanupForPid(pid_t pid);

  static void Shutdown();

  SharedMemoryBasic();

  explicit SharedMemoryBasic(Handle aHandle);

  virtual bool Create(size_t aNbytes) override;

  virtual bool Map(size_t nBytes) override;

  virtual void* memory() const override
  {
    return mMemory;
  }

  virtual SharedMemoryType Type() const override
  {
    return TYPE_BASIC;
  }

  static Handle NULLHandle()
  {
    return Handle();
  }


  static bool IsHandleValid(const Handle &aHandle);

  bool ShareToProcess(base::ProcessId aProcessId,
                      Handle* aNewHandle);

private:
  ~SharedMemoryBasic();

  void Unmap();
  void Destroy();
  mach_port_t mPort;
  // Pointer to mapped region, null if unmapped.
  void *mMemory;
};

} // namespace ipc
} // namespace mozilla

#endif // ifndef mozilla_ipc_SharedMemoryBasic_mach_h
