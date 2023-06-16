// A negative lastIndex value must be treated as 0.
function test() {
    var re = /abc.+de/g;
    for (var i = 0; i < 100; i++) {
        re.lastIndex = (i > 60) ? -1 : 0;
        assertEq(typeof re.exec("abcXdef"), "object");
        assertEq(re.lastIndex, 6);

        re.lastIndex = (i > 60) ? -1 : 0;
        assertEq(re.test("abcXdef"), true);
        assertEq(re.lastIndex, 6);
    }
}
test();
