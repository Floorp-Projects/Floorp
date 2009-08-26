#include "tests.h"

BEGIN_TEST(selfTest_NaNsAreSame)
{
    jsvalRoot v1(cx), v2(cx);
    EVAL("0/0", v1.addr());  // NaN
    CHECK_SAME(v1, v1);

    EVAL("Math.sin('no')", v2.addr());  // also NaN
    CHECK_SAME(v1, v2);
    return true;
}
END_TEST(selfTest_NaNsAreSame)

BEGIN_TEST(selfTest_negativeZeroIsNotTheSameAsZero)
{
    jsvalRoot negativeZero(cx);
    EVAL("-0", negativeZero.addr());
    CHECK(!sameValue(negativeZero, JSVAL_ZERO));
    return true;
}
END_TEST(selfTest_negativeZeroIsNotTheSameAsZero)
