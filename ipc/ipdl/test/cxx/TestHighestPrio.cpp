/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHighestPrio.h"

#include "IPDLUnitTests.h"      // fail etc.
#if defined(OS_POSIX)
#include <unistd.h>
#else
#include <windows.h>
#endif

namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// parent

TestHighestPrioParent::TestHighestPrioParent()
  : msg_num_(0)
{
    MOZ_COUNT_CTOR(TestHighestPrioParent);
}

TestHighestPrioParent::~TestHighestPrioParent()
{
    MOZ_COUNT_DTOR(TestHighestPrioParent);
}

void
TestHighestPrioParent::Main()
{
    if (!SendStart())
        fail("sending Start");
}

mozilla::ipc::IPCResult
TestHighestPrioParent::RecvMsg1()
{
    MOZ_ASSERT(msg_num_ == 0);
    msg_num_ = 1;
    return IPC_OK();
}

mozilla::ipc::IPCResult
TestHighestPrioParent::RecvMsg2()
{

    MOZ_ASSERT(msg_num_ == 1);
    msg_num_ = 2;

    if (!SendStartInner())
        fail("sending StartInner");

    return IPC_OK();
}

mozilla::ipc::IPCResult
TestHighestPrioParent::RecvMsg3()
{
    MOZ_ASSERT(msg_num_ == 2);
    msg_num_ = 3;
    return IPC_OK();
}

mozilla::ipc::IPCResult
TestHighestPrioParent::RecvMsg4()
{
    MOZ_ASSERT(msg_num_ == 3);
    msg_num_ = 4;
    return IPC_OK();
}

//-----------------------------------------------------------------------------
// child


TestHighestPrioChild::TestHighestPrioChild()
{
    MOZ_COUNT_CTOR(TestHighestPrioChild);
}

TestHighestPrioChild::~TestHighestPrioChild()
{
    MOZ_COUNT_DTOR(TestHighestPrioChild);
}

mozilla::ipc::IPCResult
TestHighestPrioChild::RecvStart()
{
    if (!SendMsg1())
        fail("sending Msg1");

    if (!SendMsg2())
        fail("sending Msg2");

    Close();
    return IPC_OK();
}

mozilla::ipc::IPCResult
TestHighestPrioChild::RecvStartInner()
{
    if (!SendMsg3())
        fail("sending Msg3");

    if (!SendMsg4())
        fail("sending Msg4");

    return IPC_OK();
}

} // namespace _ipdltest
} // namespace mozilla
