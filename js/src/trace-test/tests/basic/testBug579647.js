expected = "TypeError: NaN is not a function";
actual = "";

try {
    a = ""
    for each(x in [0, 0, 0, 0]) {
        a %= x
    } ( - a)()
} catch (e) {
    actual = '' + e;
}

assertEq(expected, actual);
