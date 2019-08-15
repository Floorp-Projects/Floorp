/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "base/shared_memory.h"

#include "base/process_util.h"
#include "mozilla/ipc/SharedMemory.h"

namespace mozilla {

// Try to map a frozen shm for writing.  Threat model: the process is
// compromised and then receives a frozen handle.
TEST(IPCSharedMemory, FreezeAndMapRW)
{
  base::SharedMemory shm;

  // Create and initialize
  ASSERT_TRUE(shm.CreateFreezeable(1));
  ASSERT_TRUE(shm.Map(1));
  auto mem = reinterpret_cast<char*>(shm.memory());
  ASSERT_TRUE(mem);
  *mem = 'A';

  // Freeze
  ASSERT_TRUE(shm.Freeze());
  ASSERT_FALSE(shm.memory());

  // Re-create as writeable
  auto handle = base::SharedMemory::NULLHandle();
  ASSERT_TRUE(shm.GiveToProcess(base::GetCurrentProcId(), &handle));
  ASSERT_TRUE(shm.IsHandleValid(handle));
  ASSERT_FALSE(shm.IsValid());
  ASSERT_TRUE(shm.SetHandle(handle, /* read-only */ false));
  ASSERT_TRUE(shm.IsValid());

  // This should fail
  EXPECT_FALSE(shm.Map(1));
}

// Try to restore write permissions to a frozen mapping.  Threat
// model: the process has mapped frozen shm normally and then is
// compromised, or as for FreezeAndMapRW (see also the
// proof-of-concept at https://crbug.com/project-zero/1671 ).
TEST(IPCSharedMemory, FreezeAndReprotect)
{
  base::SharedMemory shm;

  // Create and initialize
  ASSERT_TRUE(shm.CreateFreezeable(1));
  ASSERT_TRUE(shm.Map(1));
  auto mem = reinterpret_cast<char*>(shm.memory());
  ASSERT_TRUE(mem);
  *mem = 'A';

  // Freeze
  ASSERT_TRUE(shm.Freeze());
  ASSERT_FALSE(shm.memory());

  // Re-map
  ASSERT_TRUE(shm.Map(1));
  mem = reinterpret_cast<char*>(shm.memory());
  ASSERT_EQ(*mem, 'A');

  // Try to alter protection; should fail
  EXPECT_FALSE(ipc::SharedMemory::SystemProtectFallible(
      mem, 1, ipc::SharedMemory::RightsReadWrite));
}

#ifndef XP_WIN
// This essentially tests whether FreezeAndReprotect would have failed
// without the freeze.  It doesn't work on Windows: VirtualProtect
// can't exceed the permissions set in MapViewOfFile regardless of the
// security status of the original handle.
TEST(IPCSharedMemory, Reprotect)
{
  base::SharedMemory shm;

  // Create and initialize
  ASSERT_TRUE(shm.CreateFreezeable(1));
  ASSERT_TRUE(shm.Map(1));
  auto mem = reinterpret_cast<char*>(shm.memory());
  ASSERT_TRUE(mem);
  *mem = 'A';

  // Re-create as read-only
  auto handle = base::SharedMemory::NULLHandle();
  ASSERT_TRUE(shm.GiveToProcess(base::GetCurrentProcId(), &handle));
  ASSERT_TRUE(shm.IsHandleValid(handle));
  ASSERT_FALSE(shm.IsValid());
  ASSERT_TRUE(shm.SetHandle(handle, /* read-only */ true));
  ASSERT_TRUE(shm.IsValid());

  // Re-map
  ASSERT_TRUE(shm.Map(1));
  mem = reinterpret_cast<char*>(shm.memory());
  ASSERT_EQ(*mem, 'A');

  // Try to alter protection; should succeed, because not frozen
  EXPECT_TRUE(ipc::SharedMemory::SystemProtectFallible(
      mem, 1, ipc::SharedMemory::RightsReadWrite));
}
#endif

}  // namespace mozilla
