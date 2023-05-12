function test() {
    var re = /abc.+de/;
    var c = 0;
    for (var i = 0; i < 100; i++) {
        assertEq(re.test("abcXdef"), true);
        if (i === 60) {
            RegExp.prototype.exec = () => { c++; return {}; };
        }
    }
    assertEq(c, 39);
}
test();
