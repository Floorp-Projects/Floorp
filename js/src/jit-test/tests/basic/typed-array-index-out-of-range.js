
// Out of bounds writes on typed arrays are uneffectful for all integers.

var x = new Int32Array(10);

// 7.1.16 CanonicalNumericIndexString ( argument )
function CanonicalNumericIndexString(argument) {
    // Step 1.
    assertEq(typeof argument, "string");

    // Step 2.
    if (argument === "-0") {
        return -0;
    }

    // Step 3.
    var n = Number(argument);

    // Step 4.
    if (String(n) !== argument) {
        return undefined;
    }

    // Step 5.
    return n;
}

function CanonicalIndexInRange(argument) {
    return Number.isInteger(argument) && !Object.is(argument, -0) &&
           argument >= 0 && argument < x.length;
}

function assertCanonicalNumericIndexString(i) {
    var canonical = CanonicalNumericIndexString(i);
    assertEq(canonical !== undefined, true,
            `Argument ${i} is not a canonical numeric index string`);

    x[i] = 0; // Clear any previous value.
    x[i] = 1;

    var expected = CanonicalIndexInRange(canonical) ? 1 : undefined;
    assertEq(x[i], expected, `${i} is a canonical index string`);

    // Also test when accessing with a number instead of a string.

    x[canonical] = 0; // Clear any previous value.
    x[canonical] = 2;

    // Add +0 to convert -0 to +0.
    var expected = CanonicalIndexInRange(canonical + 0) ? 2 : undefined;
    assertEq(x[canonical], expected, `${i} is a canonical index string`);
}

function assertNotCanonicalNumericIndexString(i) {
    assertEq(CanonicalNumericIndexString(i) === undefined, true,
             `Argument ${i} is a canonical numeric index string`);

    x[i] = ""; // Clear any previous value.
    x[i] = "test";

    assertEq(x[i], "test", `${i} isn't a canonical index string`);
}

function f() {
    for (var i = -100; i < 100; i++) {
        assertCanonicalNumericIndexString(String(i));
    }
}
f();

// INT32_MAX and UINT32_MAX are canonical numeric indices.

assertCanonicalNumericIndexString("2147483647");
assertCanonicalNumericIndexString("4294967295");

// Neither INT64_MAX nor UINT64_MAX are canonical numeric indices.

assertNotCanonicalNumericIndexString("9223372036854775807");
assertNotCanonicalNumericIndexString("18446744073709551615");

// "18446744073709552000" and "18446744073709556000" are canonical numeric indices.

assertEq(String(18446744073709551615), "18446744073709552000");

assertCanonicalNumericIndexString("18446744073709552000");
assertCanonicalNumericIndexString("18446744073709556000");

// Number.MAX_SAFE_INTEGER and Number.MAX_SAFE_INTEGER + 1 are canonical numeric
// indices, but Number.MAX_SAFE_INTEGER + 2 is not.

assertEq(String(Number.MAX_SAFE_INTEGER), "9007199254740991");

assertCanonicalNumericIndexString("9007199254740991");
assertCanonicalNumericIndexString("9007199254740992");
assertNotCanonicalNumericIndexString("9007199254740993");

// Number.MIN_SAFE_INTEGER and Number.MIN_SAFE_INTEGER - 1 are canonical numeric
// indices, but Number.MIN_SAFE_INTEGER - 2 is not.

assertEq(String(Number.MIN_SAFE_INTEGER), "-9007199254740991");

assertCanonicalNumericIndexString("-9007199254740991");
assertCanonicalNumericIndexString("-9007199254740992");
assertNotCanonicalNumericIndexString("-9007199254740993");

// -0 is a canonical numeric index.

assertCanonicalNumericIndexString("-0");

// 0.1 and -0.1 are canonical numeric indices.

assertCanonicalNumericIndexString("0.1");
assertCanonicalNumericIndexString("-0.1");

// 0.10 and -0.10 aren't canonical numeric indices.

assertNotCanonicalNumericIndexString("0.10");
assertNotCanonicalNumericIndexString("-0.10");

// Number.MIN_VALUE and -Number.MIN_VALUE are canonical numeric indices.

assertEq(String(Number.MIN_VALUE), "5e-324");
assertEq(String(-Number.MIN_VALUE), "-5e-324");

assertCanonicalNumericIndexString("5e-324");
assertCanonicalNumericIndexString("-5e-324");

assertNotCanonicalNumericIndexString("5E-324");
assertNotCanonicalNumericIndexString("-5E-324");

// 1e2, -1e2, 1e-2, and -1e-2 aren't canonical numeric indices.

assertNotCanonicalNumericIndexString("1e2");
assertNotCanonicalNumericIndexString("-1e2");
assertNotCanonicalNumericIndexString("1e+2");
assertNotCanonicalNumericIndexString("-1e+2");
assertNotCanonicalNumericIndexString("1e-2");
assertNotCanonicalNumericIndexString("-1e-2");

assertNotCanonicalNumericIndexString("1E2");
assertNotCanonicalNumericIndexString("-1E2");
assertNotCanonicalNumericIndexString("1E+2");
assertNotCanonicalNumericIndexString("-1E+2");
assertNotCanonicalNumericIndexString("1E-2");
assertNotCanonicalNumericIndexString("-1E-2");

// Number.MAX_VALUE and -Number.MAX_VALUE are canonical numeric indices.

assertEq(String(Number.MAX_VALUE), "1.7976931348623157e+308");
assertEq(String(-Number.MAX_VALUE), "-1.7976931348623157e+308");

assertCanonicalNumericIndexString("1.7976931348623157e+308");
assertCanonicalNumericIndexString("-1.7976931348623157e+308");

assertNotCanonicalNumericIndexString("1.7976931348623157E+308");
assertNotCanonicalNumericIndexString("-1.7976931348623157E+308");

// Infinity, -Infinity, and NaN are canonical numeric indices and are handled
// the same way as other out-of-bounds accesses.

assertCanonicalNumericIndexString("Infinity");
assertCanonicalNumericIndexString("-Infinity");
assertCanonicalNumericIndexString("NaN");

// +Infinity, +NaN, or -NaN are not canonical numeric indices.

assertNotCanonicalNumericIndexString("+Infinity");
assertNotCanonicalNumericIndexString("+NaN");
assertNotCanonicalNumericIndexString("-NaN");
