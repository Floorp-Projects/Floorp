function test() {
    var re = /abc(.+)z/;
    for (var i = 0; i < 100; i++) {
        assertEq(re.exec(newRope("abcdefghijklmnopqrstuvw", "xyz"))[1], "defghijklmnopqrstuvwxy");
        assertEq(re.test(newRope("abcdefghijklmnopqrstuvw", "xyz")), true);
        assertEq(re[Symbol.search](newRope(".abcdefghijklmnopqrstuvw", "xyz")), 1);
        assertEq(re[Symbol.match](newRope("abcdefghijklmnopqrstuvw", "xyz"))[1], "defghijklmnopqrstuvwxy");
    }
}
test();
