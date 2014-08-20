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
    JSCompartment *compartment = js::GetContextCompartment(cx);
    compartment->setObjectMetadataCallback(js::SavedStacksMetadataCallback);
    JS::RootedObject obj(cx, js::NewDenseEmptyArray(cx));
    compartment->setObjectMetadataCallback(nullptr);
    return true;
}
END_TEST(testSavedStacks_withNoStack)
