/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/UbiNode.h"

#include "jsapi-tests/tests.h"

using JS::RootedObject;
using JS::RootedScript;
using JS::RootedString;

// ubi::Node::zone works
BEGIN_TEST(test_ubiNodeZone)
{
    RootedObject global1(cx, JS::CurrentGlobalOrNull(cx));
    CHECK(global1);
    CHECK(JS::ubi::Node(global1).zone() == cx->zone());

    RootedObject global2(cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                                                JS::FireOnNewGlobalHook));
    CHECK(global2);
    CHECK(global1->zone() != global2->zone());
    CHECK(JS::ubi::Node(global2).zone() == global2->zone());
    CHECK(JS::ubi::Node(global2).zone() != global1->zone());

    JS::CompileOptions options(cx);

    // Create a string and a script in the original zone...
    RootedString string1(cx, JS_NewStringCopyZ(cx, "Simpson's Individual Stringettes!"));
    CHECK(string1);
    RootedScript script1(cx);
    CHECK(JS::Compile(cx, global1, options, "", 0, &script1));

    {
        // ... and then enter global2's zone and create a string and script
        // there, too.
        JSAutoCompartment ac(cx, global2);

        RootedString string2(cx, JS_NewStringCopyZ(cx, "A million household uses!"));
        CHECK(string2);
        RootedScript script2(cx);
        CHECK(JS::Compile(cx, global2, options, "", 0, &script2));

        CHECK(JS::ubi::Node(string1).zone() == global1->zone());
        CHECK(JS::ubi::Node(script1).zone() == global1->zone());

        CHECK(JS::ubi::Node(string2).zone() == global2->zone());
        CHECK(JS::ubi::Node(script2).zone() == global2->zone());
    }

    return true;
}
END_TEST(test_ubiNodeZone)

// ubi::Node::compartment works
BEGIN_TEST(test_ubiNodeCompartment)
{
    RootedObject global1(cx, JS::CurrentGlobalOrNull(cx));
    CHECK(global1);
    CHECK(JS::ubi::Node(global1).compartment() == cx->compartment());

    RootedObject global2(cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                                                JS::FireOnNewGlobalHook));
    CHECK(global2);
    CHECK(global1->compartment() != global2->compartment());
    CHECK(JS::ubi::Node(global2).compartment() == global2->compartment());
    CHECK(JS::ubi::Node(global2).compartment() != global1->compartment());

    JS::CompileOptions options(cx);

    // Create a script in the original compartment...
    RootedScript script1(cx);
    CHECK(JS::Compile(cx, global1, options, "", 0, &script1));

    {
        // ... and then enter global2's compartment and create a script
        // there, too.
        JSAutoCompartment ac(cx, global2);

        RootedScript script2(cx);
        CHECK(JS::Compile(cx, global2, options, "", 0, &script2));

        CHECK(JS::ubi::Node(script1).compartment() == global1->compartment());
        CHECK(JS::ubi::Node(script2).compartment() == global2->compartment());
    }

    return true;
}
END_TEST(test_ubiNodeCompartment)
