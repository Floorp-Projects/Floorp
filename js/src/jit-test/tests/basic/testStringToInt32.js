function testStringToInt32() {
    var s = "";
    for (let j = 0; j < 5; ++j) s += ("1e+81" ^  3);
    return s;
}
assertEq(testStringToInt32(), "33333");
