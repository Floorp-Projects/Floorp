function test() {
    var re = /abc.+de/y;
    for (var i = 0; i < 100; i++) {
        re.lastIndex = 10;
        assertEq(re.exec("abcXdef"), null);
        assertEq(re.lastIndex, 0);

        re.lastIndex = 10;
        assertEq(re.test("abcXdef"), false);
        assertEq(re.lastIndex, 0);
    }
}
test();
