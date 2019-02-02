// Test bailouts in inlined jsop_getelem accesses.

// Defined outside of the test functions to ensure they're recognised as
// constants in Ion.
var atom = "prop";
var symbol = Symbol();

function testAtom() {
    var holder = {
        get [atom]() {
            new Error().stack;
            return 1;
        }
    };

    function f() {
        for (var i = 0; i < 2000; ++i) {
            var x = holder[atom];
            assertEq(x, 1);
        }
    }

    for (var i = 0; i < 2; i++) {
        f();
    }
}
testAtom();

function testSymbol() {
    var holder = {
        get [symbol]() {
            new Error().stack;
            return 1;
        }
    };

    function f() {
        for (var i = 0; i < 2000; ++i) {
            var x = holder[symbol];
            assertEq(x, 1);
        }
    }

    for (var i = 0; i < 2; i++) {
        f();
    }
}
testSymbol();
