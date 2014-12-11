function roundf(y) {
    return Math.round(Math.fround(y));
}

var x = -1;
assertEq(roundf(x), x);
assertEq(roundf(x), x);

var x = -2;
assertEq(roundf(x), x);
assertEq(roundf(x), x);

var x = -1024;
assertEq(roundf(x), x);

var x = -14680050;
assertEq(roundf(x), Math.fround(x));

var x = -8388610;
assertEq(roundf(x), Math.fround(x));
