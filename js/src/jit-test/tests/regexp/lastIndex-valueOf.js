function test() {
    var re = /abc.+de/g;
    var c = 0;
    var weird = {valueOf() { c++; return 0; }};
    for (var i = 0; i < 100; i++) {
        re.lastIndex = (i > 60) ? weird : 0;
        assertEq(typeof re.exec("abcXdef"), "object");
        assertEq(re.lastIndex, 6);

        re.lastIndex = (i > 60) ? weird : 0;
        assertEq(re.test("abcXdef"), true);
        assertEq(re.lastIndex, 6);
    }
    assertEq(c, 78);
}
test();
