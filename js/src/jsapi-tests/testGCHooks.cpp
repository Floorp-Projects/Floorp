/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"

#include "js/GCAPI.h"

#include "jsapi-tests/tests.h"

static unsigned gSliceCallbackCount = 0;

static void
NonIncrementalGCSliceCallback(JSContext* cx, JS::GCProgress progress, const JS::GCDescription& desc)
{
    using namespace JS;
    static GCProgress expect[] =
        { GC_CYCLE_BEGIN, GC_SLICE_BEGIN, GC_SLICE_END, GC_CYCLE_END };

    MOZ_RELEASE_ASSERT(progress == expect[gSliceCallbackCount++]);
    MOZ_RELEASE_ASSERT(desc.isZone_ == false);
    MOZ_RELEASE_ASSERT(desc.invocationKind_ == GC_NORMAL);
    MOZ_RELEASE_ASSERT(desc.reason_ == JS::gcreason::API);
    if (progress == GC_CYCLE_END) {
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
    CHECK(gSliceCallbackCount == 4);
    return true;
}
END_TEST(testGCSliceCallback)
