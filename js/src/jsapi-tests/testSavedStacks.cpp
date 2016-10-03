/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscompartment.h"
#include "jsfriendapi.h"
#include "jsstr.h"

#include "builtin/TestingFunctions.h"
#include "jsapi-tests/tests.h"
#include "vm/ArrayObject.h"
#include "vm/SavedStacks.h"

BEGIN_TEST(testSavedStacks_withNoStack)
{
    JSCompartment* compartment = js::GetContextCompartment(cx);
    compartment->setAllocationMetadataBuilder(&js::SavedStacks::metadataBuilder);
    JS::RootedObject obj(cx, js::NewDenseEmptyArray(cx));
    compartment->setAllocationMetadataBuilder(nullptr);
    return true;
}
END_TEST(testSavedStacks_withNoStack)

BEGIN_TEST(testSavedStacks_ApiDefaultValues)
{
    js::RootedSavedFrame savedFrame(cx, nullptr);

    // Source
    JS::RootedString str(cx);
    JS::SavedFrameResult result = JS::GetSavedFrameSource(cx, savedFrame, &str);
    CHECK(result == JS::SavedFrameResult::AccessDenied);
    CHECK(str.get() == cx->runtime()->emptyString);

    // Line
    uint32_t line = 123;
    result = JS::GetSavedFrameLine(cx, savedFrame, &line);
    CHECK(result == JS::SavedFrameResult::AccessDenied);
    CHECK(line == 0);

    // Column
    uint32_t column = 123;
    result = JS::GetSavedFrameColumn(cx, savedFrame, &column);
    CHECK(result == JS::SavedFrameResult::AccessDenied);
    CHECK(column == 0);

    // Function display name
    result = JS::GetSavedFrameFunctionDisplayName(cx, savedFrame, &str);
    CHECK(result == JS::SavedFrameResult::AccessDenied);
    CHECK(str.get() == nullptr);

    // Parent
    JS::RootedObject parent(cx);
    result = JS::GetSavedFrameParent(cx, savedFrame, &parent);
    CHECK(result == JS::SavedFrameResult::AccessDenied);
    CHECK(parent.get() == nullptr);

    // Stack string
    CHECK(JS::BuildStackString(cx, savedFrame, &str));
    CHECK(str.get() == cx->runtime()->emptyString);

    return true;
}
END_TEST(testSavedStacks_ApiDefaultValues)

BEGIN_TEST(testSavedStacks_RangeBasedForLoops)
{
    CHECK(js::DefineTestingFunctions(cx, global, false, false));

    JS::RootedValue val(cx);
    CHECK(evaluate("(function one() {                      \n"  // 1
                   "  return (function two() {             \n"  // 2
                   "    return (function three() {         \n"  // 3
                   "      return saveStack();              \n"  // 4
                   "    }());                              \n"  // 5
                   "  }());                                \n"  // 6
                   "}());                                  \n", // 7
                   "filename.js",
                   1,
                   &val));

    CHECK(val.isObject());
    JS::RootedObject obj(cx, &val.toObject());

    CHECK(obj->is<js::SavedFrame>());
    JS::Rooted<js::SavedFrame*> savedFrame(cx, &obj->as<js::SavedFrame>());

    js::SavedFrame* f = savedFrame.get();
    for (auto& frame : *savedFrame.get()) {
        CHECK(&frame == f);
        f = f->getParent();
    }
    CHECK(f == nullptr);

    const js::SavedFrame* cf = savedFrame.get();
    for (const auto& frame : *savedFrame.get()) {
        CHECK(&frame == cf);
        cf = cf->getParent();
    }
    CHECK(cf == nullptr);

    JS::Rooted<js::SavedFrame*> rf(cx, savedFrame);
    for (JS::Handle<js::SavedFrame*> frame : js::SavedFrame::RootedRange(cx, rf)) {
        JS_GC(cx);
        CHECK(frame == rf);
        rf = rf->getParent();
    }
    CHECK(rf == nullptr);

    return true;
}
END_TEST(testSavedStacks_RangeBasedForLoops)

BEGIN_TEST(testSavedStacks_selfHostedFrames)
{
    CHECK(js::DefineTestingFunctions(cx, global, false, false));

    JS::RootedValue val(cx);
    //             0         1         2         3
    //             0123456789012345678901234567890123456789
    CHECK(evaluate("(function one() {                      \n"  // 1
                   "  try {                                \n"  // 2
                   "    [1].map(function two() {           \n"  // 3
                   "      throw saveStack();               \n"  // 4
                   "    });                                \n"  // 5
                   "  } catch (stack) {                    \n"  // 6
                   "    return stack;                      \n"  // 7
                   "  }                                    \n"  // 8
                   "}())                                   \n", // 9
                   "filename.js",
                   1,
                   &val));

    CHECK(val.isObject());
    JS::RootedObject obj(cx, &val.toObject());

    CHECK(obj->is<js::SavedFrame>());
    JS::Rooted<js::SavedFrame*> savedFrame(cx, &obj->as<js::SavedFrame>());

    JS::Rooted<js::SavedFrame*> selfHostedFrame(cx, savedFrame->getParent());
    CHECK(selfHostedFrame->isSelfHosted(cx));

    // Source
    JS::RootedString str(cx);
    JS::SavedFrameResult result = JS::GetSavedFrameSource(cx, selfHostedFrame, &str,
                                                          JS::SavedFrameSelfHosted::Exclude);
    CHECK(result == JS::SavedFrameResult::Ok);
    JSLinearString* lin = str->ensureLinear(cx);
    CHECK(lin);
    CHECK(js::StringEqualsAscii(lin, "filename.js"));

    // Source, including self-hosted frames
    result = JS::GetSavedFrameSource(cx, selfHostedFrame, &str, JS::SavedFrameSelfHosted::Include);
    CHECK(result == JS::SavedFrameResult::Ok);
    lin = str->ensureLinear(cx);
    CHECK(lin);
    CHECK(js::StringEqualsAscii(lin, "self-hosted"));

    // Line
    uint32_t line = 123;
    result = JS::GetSavedFrameLine(cx, selfHostedFrame, &line, JS::SavedFrameSelfHosted::Exclude);
    CHECK(result == JS::SavedFrameResult::Ok);
    CHECK_EQUAL(line, 3U);

    // Column
    uint32_t column = 123;
    result = JS::GetSavedFrameColumn(cx, selfHostedFrame, &column,
                                     JS::SavedFrameSelfHosted::Exclude);
    CHECK(result == JS::SavedFrameResult::Ok);
    CHECK_EQUAL(column, 5U);

    // Function display name
    result = JS::GetSavedFrameFunctionDisplayName(cx, selfHostedFrame, &str,
                                                  JS::SavedFrameSelfHosted::Exclude);
    CHECK(result == JS::SavedFrameResult::Ok);
    lin = str->ensureLinear(cx);
    CHECK(lin);
    CHECK(js::StringEqualsAscii(lin, "one"));

    // Parent
    JS::RootedObject parent(cx);
    result = JS::GetSavedFrameParent(cx, savedFrame, &parent, JS::SavedFrameSelfHosted::Exclude);
    CHECK(result == JS::SavedFrameResult::Ok);
    // JS::GetSavedFrameParent does this super funky and potentially unexpected
    // thing where it doesn't return the next subsumed parent but any next
    // parent. This so that callers can still get the "asyncParent" property
    // which is only on the first frame of the async parent stack and that frame
    // might not be subsumed by the caller. It is expected that callers will
    // still interact with the frame through the JSAPI accessors, so this should
    // be safe and should not leak privileged info to unprivileged
    // callers. However, because of that, we don't test that the parent we get
    // here is the selfHostedFrame's parent (because, as just explained, it
    // isn't) and instead check that asking for the source property gives us the
    // expected value.
    result = JS::GetSavedFrameSource(cx, parent, &str, JS::SavedFrameSelfHosted::Exclude);
    CHECK(result == JS::SavedFrameResult::Ok);
    lin = str->ensureLinear(cx);
    CHECK(lin);
    CHECK(js::StringEqualsAscii(lin, "filename.js"));

    return true;
}
END_TEST(testSavedStacks_selfHostedFrames)
