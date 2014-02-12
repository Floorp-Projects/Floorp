expected = "TypeError: a is not a function";
actual = "";

try {
    a = ""
    for each(x in [0, 0, 0, 0]) {
        a %= x
    } ( let (a=-a) a)()
} catch (e) {
    actual = '' + e;
}

assertEq(actual, expected);
