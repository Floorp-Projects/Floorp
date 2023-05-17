function test() {
    var re = /abc(.+)z/;
    for (var i = 0; i < 100; i++) {
        assertEq(re.exec(newRope("abcd", "xyz"))[1], "dxy");
        assertEq(re.test(newRope("abcd", "xyz")), true);
        assertEq(re[Symbol.search](newRope(".abcd", "xyz")), 1);
        assertEq(re[Symbol.match](newRope("abcd", "xyz"))[1], "dxy");
    }
}
test();
