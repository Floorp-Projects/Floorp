// Test cases for arguments.length optimization.

function f1() {
    return arguments.length;
}

function f2(a, b, c) {
    return arguments.length;
}

// arrow functions don't have their own arguments, and so capture the enclosing
// scope.
function f3(a, b, c, d) {
    return (() => arguments.length)();
}

// Test a function which mutates arguments.length
function f4(a, b, c, d) {
    arguments.length = 42;
    return arguments.length;
}

// Manually read out arguments; should disable the length opt
function f5() {
    for (var i = 0; i < arguments.length; i++) {
        if (arguments[i] == 10) { return true }
    }
    return false;
}

function f6() {
    function inner() {
        return arguments.length;
    }
    return inner(1, 2, 3);
}

// edge cases of the arguments bindings:
function f7() {
    var arguments = 42;
    return arguments;
}

function f8() {
    var arguments = [1, 2];
    return arguments.length;
}

function f9() {
    eval("arguments.length = 42");
    return arguments.length;
}

function test() {
    assertEq(f1(), 0);
    assertEq(f1(1), 1);
    assertEq(f1(1, 2), 2);
    assertEq(f1(1, 2, 3), 3);

    assertEq(f2(), 0);
    assertEq(f2(1, 2, 3), 3);

    assertEq(f3(), 0);
    assertEq(f3(1, 2, 3), 3);

    assertEq(f4(), 42);
    assertEq(f4(1, 2, 3), 42);

    assertEq(f5(), false);
    assertEq(f5(1, 2, 3, 10), true);
    assertEq(f5(1, 2, 3, 10, 20), true);
    assertEq(f5(1, 2, 3, 9, 20, 30), false);

    assertEq(f6(), 3)
    assertEq(f6(1, 2, 3, 4), 3)

    assertEq(f7(), 42);

    assertEq(f8(), 2);

    assertEq(f9(), 42);
}

test();

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
