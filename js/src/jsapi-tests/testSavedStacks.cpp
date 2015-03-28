/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscompartment.h"
#include "jsfriendapi.h"
#include "jsstr.h"

#include "jsapi-tests/tests.h"

#include "vm/ArrayObject.h"
#include "vm/SavedStacks.h"

BEGIN_TEST(testSavedStacks_withNoStack)
{
    JSCompartment* compartment = js::GetContextCompartment(cx);
    compartment->setObjectMetadataCallback(js::SavedStacksMetadataCallback);
    JS::RootedObject obj(cx, js::NewDenseEmptyArray(cx));
    compartment->setObjectMetadataCallback(nullptr);
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
