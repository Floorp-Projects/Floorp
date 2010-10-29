function test_JSOP_ARGSUB() {
    function f0() { return arguments[0]; }
    function f1() { return arguments[1]; }
    function f2() { return arguments[2]; }
    function f3() { return arguments[3]; }
    function f4() { return arguments[4]; }
    function f5() { return arguments[5]; }
    function f6() { return arguments[6]; }
    function f7() { return arguments[7]; }
    function f8() { return arguments[8]; }
    function f9() { return arguments[9]; }
    var a = [];
    for (var i = 0; i < 10; i++) {
        a[0] = f0('a');
        a[1] = f1('a','b');
        a[2] = f2('a','b','c');
        a[3] = f3('a','b','c','d');
        a[4] = f4('a','b','c','d','e');
        a[5] = f5('a','b','c','d','e','f');
        a[6] = f6('a','b','c','d','e','f','g');
        a[7] = f7('a','b','c','d','e','f','g','h');
        a[8] = f8('a','b','c','d','e','f','g','h','i');
        a[9] = f9('a','b','c','d','e','f','g','h','i','j');
    }
    return a.join("");
}
assertEq(test_JSOP_ARGSUB(), "abcdefghij");
