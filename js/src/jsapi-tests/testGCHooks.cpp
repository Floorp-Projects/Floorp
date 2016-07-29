/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/UniquePtr.h"

#include "js/GCAPI.h"

#include "jsapi-tests/tests.h"

static unsigned gSliceCallbackCount = 0;

static void
NonIncrementalGCSliceCallback(JSContext* cx, JS::GCProgress progress, const JS::GCDescription& desc)
{
    ++gSliceCallbackCount;
    MOZ_RELEASE_ASSERT(progress == JS::GC_CYCLE_BEGIN || progress == JS::GC_CYCLE_END);
    MOZ_RELEASE_ASSERT(desc.isCompartment_ == false);
    MOZ_RELEASE_ASSERT(desc.invocationKind_ == GC_NORMAL);
    MOZ_RELEASE_ASSERT(desc.reason_ == JS::gcreason::API);
    if (progress == JS::GC_CYCLE_END) {
        mozilla::UniquePtr<char16_t> summary(desc.formatSummaryMessage(cx));
        mozilla::UniquePtr<char16_t> message(desc.formatSliceMessage(cx));
        mozilla::UniquePtr<char16_t> json(desc.formatJSON(cx, 0));
    }
}

BEGIN_TEST(testGCSliceCallback)
{
    JS::SetGCSliceCallback(cx, NonIncrementalGCSliceCallback);
    JS_GC(cx);
    JS::SetGCSliceCallback(cx, nullptr);
    CHECK(gSliceCallbackCount == 2);
    return true;
}
END_TEST(testGCSliceCallback)
