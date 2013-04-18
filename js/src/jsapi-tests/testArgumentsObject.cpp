/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "tests.h"

#include "vm/Stack-inl.h"

using namespace js;

static const char NORMAL_ZERO[] =
    "function f() { return arguments; }";
static const char NORMAL_ONE[] =
    "function f(a) { return arguments; }";
static const char NORMAL_TWO[] =
    "function f(a, b) { return arguments; }";
static const char NORMAL_THREE[] =
    "function f(a, b, c) { return arguments; }";

static const char STRICT_ZERO[] =
    "function f() { 'use strict'; return arguments; }";
static const char STRICT_ONE[] =
    "function f() { 'use strict'; return arguments; }";
static const char STRICT_TWO[] =
    "function f() { 'use strict'; return arguments; }";
static const char STRICT_THREE[] =
    "function f() { 'use strict'; return arguments; }";

static const char *CALL_CODES[] =
    { "f()", "f(0)", "f(0, 1)", "f(0, 1, 2)", "f(0, 1, 2, 3)", "f(0, 1, 2, 3, 4)" };

static const size_t MAX_ELEMS = 6;

static void
ClearElements(Value elems[MAX_ELEMS])
{
    for (size_t i = 0; i < MAX_ELEMS - 1; i++)
        elems[i] = NullValue();
    elems[MAX_ELEMS - 1] = Int32Value(42);
}

BEGIN_TEST(testArgumentsObject)
{
    return ExhaustiveTest<0>(NORMAL_ZERO) &&
           ExhaustiveTest<1>(NORMAL_ZERO) &&
           ExhaustiveTest<2>(NORMAL_ZERO) &&
           ExhaustiveTest<0>(NORMAL_ONE) &&
           ExhaustiveTest<1>(NORMAL_ONE) &&
           ExhaustiveTest<2>(NORMAL_ONE) &&
           ExhaustiveTest<3>(NORMAL_ONE) &&
           ExhaustiveTest<0>(NORMAL_TWO) &&
           ExhaustiveTest<1>(NORMAL_TWO) &&
           ExhaustiveTest<2>(NORMAL_TWO) &&
           ExhaustiveTest<3>(NORMAL_TWO) &&
           ExhaustiveTest<4>(NORMAL_TWO) &&
           ExhaustiveTest<0>(NORMAL_THREE) &&
           ExhaustiveTest<1>(NORMAL_THREE) &&
           ExhaustiveTest<2>(NORMAL_THREE) &&
           ExhaustiveTest<3>(NORMAL_THREE) &&
           ExhaustiveTest<4>(NORMAL_THREE) &&
           ExhaustiveTest<5>(NORMAL_THREE) &&
           ExhaustiveTest<0>(STRICT_ZERO) &&
           ExhaustiveTest<1>(STRICT_ZERO) &&
           ExhaustiveTest<2>(STRICT_ZERO) &&
           ExhaustiveTest<0>(STRICT_ONE) &&
           ExhaustiveTest<1>(STRICT_ONE) &&
           ExhaustiveTest<2>(STRICT_ONE) &&
           ExhaustiveTest<3>(STRICT_ONE) &&
           ExhaustiveTest<0>(STRICT_TWO) &&
           ExhaustiveTest<1>(STRICT_TWO) &&
           ExhaustiveTest<2>(STRICT_TWO) &&
           ExhaustiveTest<3>(STRICT_TWO) &&
           ExhaustiveTest<4>(STRICT_TWO) &&
           ExhaustiveTest<0>(STRICT_THREE) &&
           ExhaustiveTest<1>(STRICT_THREE) &&
           ExhaustiveTest<2>(STRICT_THREE) &&
           ExhaustiveTest<3>(STRICT_THREE) &&
           ExhaustiveTest<4>(STRICT_THREE) &&
           ExhaustiveTest<5>(STRICT_THREE);
}

template<size_t ArgCount> bool
ExhaustiveTest(const char funcode[])
{
    RootedValue v(cx);
    EVAL(funcode, v.address());

    EVAL(CALL_CODES[ArgCount], v.address());
    Rooted<ArgumentsObject*> argsobj(cx, &JSVAL_TO_OBJECT(v)->asArguments());

    Value elems[MAX_ELEMS];

    for (size_t i = 0; i <= ArgCount; i++) {
        for (size_t j = 0; j <= ArgCount - i; j++) {
            ClearElements(elems);
            CHECK(argsobj->maybeGetElements(i, j, elems));
            for (size_t k = 0; k < j; k++)
                CHECK_SAME(elems[k], INT_TO_JSVAL(i + k));
            for (size_t k = j; k < MAX_ELEMS - 1; k++)
                CHECK_SAME(elems[k], JSVAL_NULL);
            CHECK_SAME(elems[MAX_ELEMS - 1], INT_TO_JSVAL(42));
        }
    }

    return true;
}
END_TEST(testArgumentsObject)
