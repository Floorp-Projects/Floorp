function convertToInt(str) {
    return str | 0;
}

function convertToIntOnTrace(str) {
    var z;
    for (var i = 0; i < 9; ++i) {
        z = str | 0;
    }
    return z;
}

function convertToDouble(str) {
    return str * 1.5;
}

function convertToDoubleOnTrace(str) {
    var z;
    for (var i = 0; i < 9; ++i) {
        z = str * 1.5;
    }
    return z;
}

assertEq(convertToInt("0x10"), 16);
assertEq(convertToInt("-0x10"), 0);

assertEq(convertToIntOnTrace("0x10"), 16);
assertEq(convertToIntOnTrace("-0x10"), 0);

assertEq(convertToDouble("0x10"), 24);
assertEq(convertToDouble("-0x10"), NaN);

assertEq(convertToDoubleOnTrace("0x10"), 24);
assertEq(convertToDoubleOnTrace("-0x10"), NaN);
