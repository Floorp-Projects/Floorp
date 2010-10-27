function testReplace2() {
    var s = "H e l l o", s1;
    for (i = 0; i < 100; ++i)
        s1 = s.replace(" ", "");
    return s1;
}
assertEq(testReplace2(), "He l l o");
