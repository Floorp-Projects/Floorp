/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Atomics.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/Move.h"
#include "mozilla/Vector.h"
#include "jsapi-tests/tests.h"
#include "threading/Thread.h"

BEGIN_TEST(testThreadingThreadJoin)
{
    bool flag = false;
    js::Thread thread([](bool* flagp){*flagp = true;}, &flag);
    CHECK(thread.joinable());
    thread.join();
    CHECK(flag);
    CHECK(!thread.joinable());
    return true;
}
END_TEST(testThreadingThreadJoin)

BEGIN_TEST(testThreadingThreadDetach)
{
    // We are going to detach this thread. Unlike join, we can't have it pointing at the stack
    // because it might do the write after we have returned and pushed a new frame.
    bool* flag = js_new<bool>(false);
    js::Thread thread([](bool* flag){*flag = true; js_delete(flag);}, mozilla::Move(flag));
    CHECK(thread.joinable());
    thread.detach();
    CHECK(!thread.joinable());

    return true;
}
END_TEST(testThreadingThreadDetach)

BEGIN_TEST(testThreadingThreadSetName)
{
    js::Thread thread([](){js::ThisThread::SetName("JSAPI Test Thread");});
    thread.detach();
    return true;
}
END_TEST(testThreadingThreadSetName)

BEGIN_TEST(testThreadingThreadId)
{
    CHECK(js::Thread::Id() == js::Thread::Id());
    js::Thread::Id fromOther;
    js::Thread thread([](js::Thread::Id* idp){*idp = js::ThisThread::GetId();}, &fromOther);
    js::Thread::Id fromMain = thread.get_id();
    thread.join();
    CHECK(fromOther == fromMain);
    return true;
}
END_TEST(testThreadingThreadId)

BEGIN_TEST(testThreadingThreadVectorMoveConstruct)
{
    const static size_t N = 10;
    mozilla::Atomic<int> count(0);
    mozilla::Vector<js::Thread> v;
    for (auto i : mozilla::MakeRange(N)) {
        CHECK(v.emplaceBack([](mozilla::Atomic<int>* countp){(*countp)++;}, &count));
        CHECK(v.length() == i + 1);
    }
    for (auto& th : v)
        th.join();
    CHECK(count == 10);
    return true;
}
END_TEST(testThreadingThreadVectorMoveConstruct)
