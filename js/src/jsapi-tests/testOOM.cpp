/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "jsapi-tests/tests.h"

BEGIN_TEST(testOOM)
{
    JS::RootedValue v(cx, JS::Int32Value(9));
    JS::RootedString jsstr(cx, JS::ToString(cx, v));
    mozilla::DebugOnly<const jschar *> s = JS_GetStringCharsZ(cx, jsstr);
    JS_ASSERT(s[0] == '9' && s[1] == '\0');
    return true;
}

virtual JSRuntime * createRuntime()
{
    JSRuntime *rt = JS_NewRuntime(0, JS_USE_HELPER_THREADS);
    if (!rt)
        return nullptr;
    JS_SetGCParameter(rt, JSGC_MAX_BYTES, (uint32_t)-1);
    setNativeStackQuota(rt);
    return rt;
}
END_TEST(testOOM)
