/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits>
#include <math.h>

#include "jsapi-tests/tests.h"

using namespace std;

struct LooseEqualityFixture : public JSAPITest
{
    virtual ~LooseEqualityFixture() {}

    bool leq(JS::HandleValue x, JS::HandleValue y) {
        bool equal;
        CHECK(JS_LooselyEqual(cx, x, y, &equal) && equal);
        CHECK(JS_LooselyEqual(cx, y, x, &equal) && equal);
        return true;
    }

    bool nleq(JS::HandleValue x, JS::HandleValue y) {
        bool equal;
        CHECK(JS_LooselyEqual(cx, x, y, &equal) && !equal);
        CHECK(JS_LooselyEqual(cx, y, x, &equal) && !equal);
        return true;
    }
};

struct LooseEqualityData
{
    JS::RootedValue qNaN;
    JS::RootedValue sNaN;
    JS::RootedValue d42;
    JS::RootedValue i42;
    JS::RootedValue undef;
    JS::RootedValue null;
    JS::RootedValue obj;
    JS::RootedValue poszero;
    JS::RootedValue negzero;

    explicit LooseEqualityData(JSContext* cx)
      : qNaN(cx),
        sNaN(cx),
        d42(cx),
        i42(cx),
        undef(cx),
        null(cx),
        obj(cx),
        poszero(cx),
        negzero(cx)
    {
        qNaN = DOUBLE_TO_JSVAL(numeric_limits<double>::quiet_NaN());
        sNaN = DOUBLE_TO_JSVAL(numeric_limits<double>::signaling_NaN());
        d42 = DOUBLE_TO_JSVAL(42.0);
        i42 = JS::Int32Value(42);
        undef = JS::UndefinedValue();
        null = JS::NullValue();
        obj = JS::ObjectOrNullValue(JS::CurrentGlobalOrNull(cx));
        poszero = DOUBLE_TO_JSVAL(0.0);
        negzero = DOUBLE_TO_JSVAL(-0.0);
#ifdef XP_WIN
# define copysign _copysign
#endif
        MOZ_RELEASE_ASSERT(copysign(1.0, poszero.toDouble()) == 1.0);
        MOZ_RELEASE_ASSERT(copysign(1.0, negzero.toDouble()) == -1.0);
#ifdef XP_WIN
# undef copysign
#endif
    }
};

// 11.9.3 1a
BEGIN_FIXTURE_TEST(LooseEqualityFixture, test_undef_leq_undef)
{
    LooseEqualityData d(cx);
    CHECK(leq(d.undef, d.undef));
    return true;
}
END_FIXTURE_TEST(LooseEqualityFixture, test_undef_leq_undef)

// 11.9.3 1b
BEGIN_FIXTURE_TEST(LooseEqualityFixture, test_null_leq_null)
{
    LooseEqualityData d(cx);
    CHECK(leq(d.null, d.null));
    return true;
}
END_FIXTURE_TEST(LooseEqualityFixture, test_null_leq_null)

// 11.9.3 1ci
BEGIN_FIXTURE_TEST(LooseEqualityFixture, test_nan_nleq_all)
{
    LooseEqualityData d(cx);

    CHECK(nleq(d.qNaN, d.qNaN));
    CHECK(nleq(d.qNaN, d.sNaN));

    CHECK(nleq(d.sNaN, d.sNaN));
    CHECK(nleq(d.sNaN, d.qNaN));

    CHECK(nleq(d.qNaN, d.d42));
    CHECK(nleq(d.qNaN, d.i42));
    CHECK(nleq(d.qNaN, d.undef));
    CHECK(nleq(d.qNaN, d.null));
    CHECK(nleq(d.qNaN, d.obj));

    CHECK(nleq(d.sNaN, d.d42));
    CHECK(nleq(d.sNaN, d.i42));
    CHECK(nleq(d.sNaN, d.undef));
    CHECK(nleq(d.sNaN, d.null));
    CHECK(nleq(d.sNaN, d.obj));
    return true;
}
END_FIXTURE_TEST(LooseEqualityFixture, test_nan_nleq_all)

// 11.9.3 1cii
BEGIN_FIXTURE_TEST(LooseEqualityFixture, test_all_nleq_nan)
{
    LooseEqualityData d(cx);

    CHECK(nleq(d.qNaN, d.qNaN));
    CHECK(nleq(d.qNaN, d.sNaN));

    CHECK(nleq(d.sNaN, d.sNaN));
    CHECK(nleq(d.sNaN, d.qNaN));

    CHECK(nleq(d.d42,   d.qNaN));
    CHECK(nleq(d.i42,   d.qNaN));
    CHECK(nleq(d.undef, d.qNaN));
    CHECK(nleq(d.null,  d.qNaN));
    CHECK(nleq(d.obj,   d.qNaN));

    CHECK(nleq(d.d42,   d.sNaN));
    CHECK(nleq(d.i42,   d.sNaN));
    CHECK(nleq(d.undef, d.sNaN));
    CHECK(nleq(d.null,  d.sNaN));
    CHECK(nleq(d.obj,   d.sNaN));
    return true;
}
END_FIXTURE_TEST(LooseEqualityFixture, test_all_nleq_nan)

// 11.9.3 1ciii
BEGIN_FIXTURE_TEST(LooseEqualityFixture, test_leq_same_nums)
{
    LooseEqualityData d(cx);

    CHECK(leq(d.d42, d.d42));
    CHECK(leq(d.i42, d.i42));
    CHECK(leq(d.d42, d.i42));
    CHECK(leq(d.i42, d.d42));
    return true;
}
END_FIXTURE_TEST(LooseEqualityFixture, test_leq_same_nums)

// 11.9.3 1civ
BEGIN_FIXTURE_TEST(LooseEqualityFixture, test_pz_leq_nz)
{
    LooseEqualityData d(cx);
    CHECK(leq(d.poszero, d.negzero));
    return true;
}
END_FIXTURE_TEST(LooseEqualityFixture, test_pz_leq_nz)

// 11.9.3 1cv
BEGIN_FIXTURE_TEST(LooseEqualityFixture, test_nz_leq_pz)
{
    LooseEqualityData d(cx);
    CHECK(leq(d.negzero, d.poszero));
    return true;
}
END_FIXTURE_TEST(LooseEqualityFixture, test_nz_leq_pz)

// 1cvi onwards NOT TESTED

// 11.9.3 2
BEGIN_FIXTURE_TEST(LooseEqualityFixture, test_null_leq_undef)
{
    LooseEqualityData d(cx);
    CHECK(leq(d.null, d.undef));
    return true;
}
END_FIXTURE_TEST(LooseEqualityFixture, test_null_leq_undef)

// 11.9.3 3
BEGIN_FIXTURE_TEST(LooseEqualityFixture, test_undef_leq_null)
{
    LooseEqualityData d(cx);
    CHECK(leq(d.undef, d.null));
    return true;
}
END_FIXTURE_TEST(LooseEqualityFixture, test_undef_leq_null)

// 4 onwards NOT TESTED
