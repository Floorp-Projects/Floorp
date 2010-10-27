function test_JSOP_ARGCNT() {
    function f0() { return arguments.length; }
    function f1() { return arguments.length; }
    function f2() { return arguments.length; }
    function f3() { return arguments.length; }
    function f4() { return arguments.length; }
    function f5() { return arguments.length; }
    function f6() { return arguments.length; }
    function f7() { return arguments.length; }
    function f8() { return arguments.length; }
    function f9() { return arguments.length; }
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
    return a.join(",");
}
assertEq(test_JSOP_ARGCNT(), "1,2,3,4,5,6,7,8,9,10");
