// nactuals > nformals
function testOverflow() {
    var called = false;
    function f(a) {
        assertEq(a, 173);
        assertEq(arguments.length, 2);
        assertEq(arguments[0], a);
        assertEq(arguments[1], a);
        called = true;
    }

    for (var i=0; i<10; i++)
        [173, 173, 173].sort(f);
    assertEq(called, true);
}
testOverflow();

// nactuals == nformals
function testEqual() {
    var called = false;
    function f(a, b) {
        assertEq(a, 173);
        assertEq(arguments.length, 2);
        assertEq(arguments[0], a);
        assertEq(arguments[1], b);
        called = true;
    }

    for (var i=0; i<10; i++)
        [173, 173, 173].sort(f);
    assertEq(called, true);
}
testEqual();

// nactuals < nformals
function testUnderflow() {
    var called = false;
    function f(a, b, c) {
        assertEq(a, 173);
        assertEq(c, undefined);
        assertEq(arguments.length, 2);
        assertEq(arguments[0], a);
        assertEq(arguments[1], b);
        called = true;
    }

    for (var i=0; i<10; i++)
        [173, 173, 173].sort(f);
    assertEq(called, true);
}
testUnderflow();

function testUnderflowMany() {
    var called = 0;
    function f(a, b, c, d, e, f, g, h) {
        assertEq(a, 173);
        assertEq(arguments.length, 3);
        assertEq(arguments[0], a);
        assertEq(arguments[1] < 3, true);
        assertEq(c.length, 3);
        assertEq(d, undefined);
        assertEq(e, undefined);
        assertEq(f, undefined);
        assertEq(g, undefined);
        assertEq(h, undefined);
        called += 1;
    }

    for (var i=0; i<10; i++)
        [173, 173, 173].map(f);
    assertEq(called, 30);
}
testUnderflowMany();
