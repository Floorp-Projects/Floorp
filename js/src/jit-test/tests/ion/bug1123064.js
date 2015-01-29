

function toint32() {

    // The test case to trigger MToInt32 operation.
    var ToInteger = getSelfHostedValue("ToInteger");

    // Case1: The input operand is constant int32.
    var result = ToInteger(1);
    assertEq(result, 1);

    // Case2: The input operand is constant double.
    result = ToInteger(0.12);
    assertEq(result, 0);

    // Case3: The input operand is constant float.
    result = ToInteger(Math.fround(0.13));
    assertEq(result, 0);

    // Case4: The input operand is constant boolean.
    result = ToInteger(true);
    assertEq(result, 1);

    // Case5: The input operand is null.
    result = ToInteger(null);
    assertEq(result, 0);
}

toint32();
toint32();
