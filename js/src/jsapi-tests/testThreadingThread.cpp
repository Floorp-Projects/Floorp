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

#include "js/AllocPolicy.h"
#include "jsapi-tests/tests.h"
#include "threading/Thread.h"

BEGIN_TEST(testThreadingThreadJoin)
{
    bool flag = false;
    js::Thread thread;
    CHECK(thread.init([](bool* flagp){*flagp = true;}, &flag));
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
    js::Thread thread;
    CHECK(thread.init([](bool* flag){*flag = true; js_delete(flag);}, std::move(flag)));
    CHECK(thread.joinable());
    thread.detach();
    CHECK(!thread.joinable());

    return true;
}
END_TEST(testThreadingThreadDetach)

BEGIN_TEST(testThreadingThreadSetName)
{
    js::Thread thread;
    CHECK(thread.init([](){js::ThisThread::SetName("JSAPI Test Thread");}));
    thread.detach();
    return true;
}
END_TEST(testThreadingThreadSetName)

BEGIN_TEST(testThreadingThreadId)
{
    CHECK(js::Thread::Id() == js::Thread::Id());
    js::Thread::Id fromOther;
    js::Thread thread;
    CHECK(thread.init([](js::Thread::Id* idp){*idp = js::ThisThread::GetId();}, &fromOther));
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
    mozilla::Vector<js::Thread, 0, js::SystemAllocPolicy> v;
    for (auto i : mozilla::IntegerRange(N)) {
        CHECK(v.emplaceBack());
        CHECK(v.back().init([](mozilla::Atomic<int>* countp){(*countp)++;}, &count));
        CHECK(v.length() == i + 1);
    }
    for (auto& th : v)
        th.join();
    CHECK(count == 10);
    return true;
}
END_TEST(testThreadingThreadVectorMoveConstruct)

// This test is checking that args are using "decay" copy, per spec. If we do
// not use decay copy properly, the rvalue reference |bool&& b| in the
// constructor will automatically become an lvalue reference |bool& b| in the
// trampoline, causing us to read through the reference when passing |bool bb|
// from the trampoline. If the parent runs before the child, the bool may have
// already become false, causing the trampoline to read the changed value, thus
// causing the child's assertion to fail.
BEGIN_TEST(testThreadingThreadArgCopy)
{
    for (size_t i = 0; i < 10000; ++i) {
        bool b = true;
        js::Thread thread;
        CHECK(thread.init([](bool bb){MOZ_RELEASE_ASSERT(bb);}, b));
        b = false;
        thread.join();
    }
    return true;
}
END_TEST(testThreadingThreadArgCopy)
