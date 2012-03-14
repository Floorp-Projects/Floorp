/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 */

#include "tests.h"

#include "jsatom.h"

#include "vm/StringBuffer.h"

#include "jsobjinlines.h"

BEGIN_TEST(testStringBuffer_finishString)
{
    JSString *str = JS_NewStringCopyZ(cx, "foopy");
    CHECK(str);

    JSAtom *atom = js_AtomizeString(cx, str);
    CHECK(atom);

    js::StringBuffer buffer(cx);
    CHECK(buffer.append("foopy"));

    JSAtom *finishedAtom = buffer.finishAtom();
    CHECK(finishedAtom);
    CHECK_EQUAL(atom, finishedAtom);
    return true;
}
END_TEST(testStringBuffer_finishString)
