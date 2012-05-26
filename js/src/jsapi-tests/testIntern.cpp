/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tests.h"
#include "jsatom.h"

#include "vm/String.h"

using namespace mozilla;

BEGIN_TEST(testAtomizedIsNotInterned)
{
    /* Try to pick a string that won't be interned by other tests in this runtime. */
    static const char someChars[] = "blah blah blah? blah blah blah";
    JSAtom *atom = js_Atomize(cx, someChars, ArrayLength(someChars));
    CHECK(!JS_StringHasBeenInterned(cx, atom));
    CHECK(JS_InternJSString(cx, atom));
    CHECK(JS_StringHasBeenInterned(cx, atom));
    return true;
}
END_TEST(testAtomizedIsNotInterned)

struct StringWrapper
{
    JSString *str;
    bool     strOk;
} sw;

void
FinalizeCallback(JSFreeOp *fop, JSFinalizeStatus status, JSBool isCompartmentGC)
{
    if (status == JSFINALIZE_START)
        sw.strOk = !JS_IsAboutToBeFinalized(sw.str);
}

BEGIN_TEST(testInternAcrossGC)
{
    sw.str = JS_InternString(cx, "wrapped chars that another test shouldn't be using");
    sw.strOk = false;
    CHECK(sw.str);
    JS_SetFinalizeCallback(rt, FinalizeCallback);
    JS_GC(rt);
    CHECK(sw.strOk);
    return true;
}
END_TEST(testInternAcrossGC)
