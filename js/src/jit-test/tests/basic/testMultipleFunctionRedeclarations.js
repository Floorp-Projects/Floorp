function f1() {
    assertEq(g(), 3);
    function g() { return 1 }
    assertEq(g(), 3);
    function g() { return 2 }
    assertEq(g(), 3);
    function g() { return 3 }
    assertEq(g(), 3);
}
f1();

function f2() {
    assertEq(g(), 2);
    var g = 3;
    assertEq(g, 3);
    function g() { return 1 }
    function g() { return 2 }
}
f2();

function f3() {
    assertEq(g(), 2);
    var g = 3;
    assertEq(g, 3);
    function g() { return 1 }
    var g = 4;
    assertEq(g, 4);
    function g() { return 2 }
}
f3();

function f4() {
    assertEq(g(), 4);
    function g() { return 1 }
    assertEq(g(), 4);
    function g() { return 2 }
    var g = 9;
    assertEq(g, 9);
    function g() { return 3 }
    assertEq(g, 9);
    function g() { return 4 }
    assertEq(g, 9);
}
f4();
