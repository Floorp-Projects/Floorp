/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tests.h"
#include <limits>
#include <math.h>

using namespace std;

struct LooseEqualityFixture : public JSAPITest
{
    jsval qNaN;
    jsval sNaN;
    jsval d42;
    jsval i42;
    jsval undef;
    jsval null;
    jsval obj;
    jsval poszero;
    jsval negzero;

    virtual ~LooseEqualityFixture() {}

    virtual bool init() {
        if (!JSAPITest::init())
            return false;
        qNaN = DOUBLE_TO_JSVAL(numeric_limits<double>::quiet_NaN());
        sNaN = DOUBLE_TO_JSVAL(numeric_limits<double>::signaling_NaN());
        d42 = DOUBLE_TO_JSVAL(42.0);
        i42 = INT_TO_JSVAL(42);
        undef = JSVAL_VOID;
        null = JSVAL_NULL;
        obj = OBJECT_TO_JSVAL(global);
        poszero = DOUBLE_TO_JSVAL(0.0);
        negzero = DOUBLE_TO_JSVAL(-0.0);
#ifdef XP_WIN
# define copysign _copysign
#endif
        JS_ASSERT(copysign(1.0, JSVAL_TO_DOUBLE(poszero)) == 1.0);
        JS_ASSERT(copysign(1.0, JSVAL_TO_DOUBLE(negzero)) == -1.0);
#ifdef XP_WIN
# undef copysign
#endif
        return true;
    }

    bool leq(jsval x, jsval y) {
        JSBool equal;
        CHECK(JS_LooselyEqual(cx, x, y, &equal) && equal);
        CHECK(JS_LooselyEqual(cx, y, x, &equal) && equal);
        return true;
    }

    bool nleq(jsval x, jsval y) {
        JSBool equal;
        CHECK(JS_LooselyEqual(cx, x, y, &equal) && !equal);
        CHECK(JS_LooselyEqual(cx, y, x, &equal) && !equal);
        return true;
    }
};

// 11.9.3 1a
BEGIN_FIXTURE_TEST(LooseEqualityFixture, test_undef_leq_undef)
{
    CHECK(leq(JSVAL_VOID, JSVAL_VOID));
    return true;
}
END_FIXTURE_TEST(LooseEqualityFixture, test_undef_leq_undef)

// 11.9.3 1b
BEGIN_FIXTURE_TEST(LooseEqualityFixture, test_null_leq_null)
{
    CHECK(leq(JSVAL_NULL, JSVAL_NULL));
    return true;
}
END_FIXTURE_TEST(LooseEqualityFixture, test_null_leq_null)

// 11.9.3 1ci
BEGIN_FIXTURE_TEST(LooseEqualityFixture, test_nan_nleq_all)
{
    CHECK(nleq(qNaN, qNaN));
    CHECK(nleq(qNaN, sNaN));

    CHECK(nleq(sNaN, sNaN));
    CHECK(nleq(sNaN, qNaN));

    CHECK(nleq(qNaN, d42));
    CHECK(nleq(qNaN, i42));
    CHECK(nleq(qNaN, undef));
    CHECK(nleq(qNaN, null));
    CHECK(nleq(qNaN, obj));

    CHECK(nleq(sNaN, d42));
    CHECK(nleq(sNaN, i42));
    CHECK(nleq(sNaN, undef));
    CHECK(nleq(sNaN, null));
    CHECK(nleq(sNaN, obj));
    return true;
}
END_FIXTURE_TEST(LooseEqualityFixture, test_nan_nleq_all)

// 11.9.3 1cii
BEGIN_FIXTURE_TEST(LooseEqualityFixture, test_all_nleq_nan)
{
    CHECK(nleq(qNaN, qNaN));
    CHECK(nleq(qNaN, sNaN));

    CHECK(nleq(sNaN, sNaN));
    CHECK(nleq(sNaN, qNaN));

    CHECK(nleq(d42,   qNaN));
    CHECK(nleq(i42,   qNaN));
    CHECK(nleq(undef, qNaN));
    CHECK(nleq(null,  qNaN));
    CHECK(nleq(obj,   qNaN));

    CHECK(nleq(d42,   sNaN));
    CHECK(nleq(i42,   sNaN));
    CHECK(nleq(undef, sNaN));
    CHECK(nleq(null,  sNaN));
    CHECK(nleq(obj,   sNaN));
    return true;
}
END_FIXTURE_TEST(LooseEqualityFixture, test_all_nleq_nan)

// 11.9.3 1ciii
BEGIN_FIXTURE_TEST(LooseEqualityFixture, test_leq_same_nums)
{
    CHECK(leq(d42, d42));
    CHECK(leq(i42, i42));
    CHECK(leq(d42, i42));
    CHECK(leq(i42, d42));
    return true;
}
END_FIXTURE_TEST(LooseEqualityFixture, test_leq_same_nums)

// 11.9.3 1civ
BEGIN_FIXTURE_TEST(LooseEqualityFixture, test_pz_leq_nz)
{
    CHECK(leq(poszero, negzero));
    return true;
}
END_FIXTURE_TEST(LooseEqualityFixture, test_pz_leq_nz)

// 11.9.3 1cv
BEGIN_FIXTURE_TEST(LooseEqualityFixture, test_nz_leq_pz)
{
    CHECK(leq(negzero, poszero));
    return true;
}
END_FIXTURE_TEST(LooseEqualityFixture, test_nz_leq_pz)

// 1cvi onwards NOT TESTED

// 11.9.3 2
BEGIN_FIXTURE_TEST(LooseEqualityFixture, test_null_leq_undef)
{
    CHECK(leq(null, undef));
    return true;
}
END_FIXTURE_TEST(LooseEqualityFixture, test_null_leq_undef)

// 11.9.3 3
BEGIN_FIXTURE_TEST(LooseEqualityFixture, test_undef_leq_null)
{
    CHECK(leq(undef, null));
    return true;
}
END_FIXTURE_TEST(LooseEqualityFixture, test_undef_leq_null)

// 4 onwards NOT TESTED
