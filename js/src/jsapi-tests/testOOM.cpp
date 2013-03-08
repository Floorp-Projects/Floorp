/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tests.h"

#include "mozilla/DebugOnly.h"

BEGIN_TEST(testOOM)
{
    JS::RootedString jsstr(cx, JS_ValueToString(cx, INT_TO_JSVAL(9)));
    mozilla::DebugOnly<const jschar *> s = JS_GetStringCharsZ(cx, jsstr);
    JS_ASSERT(s[0] == '9' && s[1] == '\0');
    return true;
}

virtual JSRuntime * createRuntime()
{
    JSRuntime *rt = JS_NewRuntime(0, JS_USE_HELPER_THREADS);
    JS_SetGCParameter(rt, JSGC_MAX_BYTES, (uint32_t)-1);
    return rt;
}
END_TEST(testOOM)
