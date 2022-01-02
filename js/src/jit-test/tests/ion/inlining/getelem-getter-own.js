// Test for inlined getters for jsop_getelem accesses, where the getter is an
// own property.

// Defined outside of the test functions to ensure they're recognised as
// constants in Ion.
var atom1 = "prop1";
var atom2 = "prop2";
var sym1 = Symbol();
var sym2 = Symbol();

function testAtom() {
    var holder = {
        get [atom1]() { return 1; },
        get [atom2]() { return 2; },
    };

    function f() {
        for (var i = 0; i < 1000; ++i) {
            var x = holder[atom1];
            var y = holder[atom2];
            assertEq(x, 1);
            assertEq(y, 2);
        }
    }

    for (var i = 0; i < 2; i++) {
        f();
    }
}
testAtom();

function testSymbol() {
    var holder = {
        get [sym1]() { return 1; },
        get [sym2]() { return 2; },
    };

    function f() {
        for (var i = 0; i < 1000; ++i) {
            var x = holder[sym1];
            var y = holder[sym2];
            assertEq(x, 1);
            assertEq(y, 2);
        }
    }

    for (var i = 0; i < 2; i++) {
        f();
    }
}
testSymbol();
