/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

assertEq(Number("0b11"), 3);
assertEq(Number("0B11"), 3);
assertEq(Number(" 0b11 "), 3);
assertEq(Number("0b12"), NaN);
assertEq(Number("-0b11"), NaN);
assertEq(+"0b11", 3);

assertEq(Number("0o66"), 54);
assertEq(Number("0O66"), 54);
assertEq(Number(" 0o66 "), 54);
assertEq(Number("0o88"), NaN);
assertEq(Number("-0o66"), NaN);
assertEq(+"0o66", 54);

if(typeof getSelfHostedValue === "function"){
    assertEq(getSelfHostedValue("ToNumber")("0b11"), 3);
    assertEq(getSelfHostedValue("ToNumber")("0o66"), 54);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
