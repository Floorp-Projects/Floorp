function f(i) {
    var a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a16,a17;
    var b = true;
    if (b)
        function f1() {}
    if (b)
        function f2() {}
    if (b)
        function f3() {}
    if (b)
        function f4() {}
    if (b)
        function f5() {}
    if (b)
        function f6() {}
    if (b)
        function f7() {}
    if (b)
        function f8() {}
    if (b)
        function f9() {}
    if (b)
        function f10() {}
    if (b)
        function f11() {}
    if (b)
        function f12() {}
    if (b)
        function f13() {}
    if (b)
        function f14() {}
    if (b)
        function f15() {}
    if (b)
        function f16() {}
    if (b)
        function f17() {}

    a1 = i;

    function f() {
        return a1;
    }

    return f();
}

for (var i = 0; i < 100; ++i)
    assertEq(f(i), i);
