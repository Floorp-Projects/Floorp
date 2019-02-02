// With-Statements are not supported in Ion, therefore functions containing
// them can't be inlined in Ion. However it's still possible to inline the
// property access to the getter into a simple guard-shape instruction.

// Defined outside of the test functions to ensure they're recognised as
// constants in Ion.
var atom = "prop";
var symbol = Symbol();

function testAtom() {
    var holder = {
        get [atom]() {
            with ({}) {
                return 1;
            }
        }
    };

    function f() {
        for (var i = 0; i < 1000; ++i) {
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
            with ({}) {
                return 1;
            }
        }
    };

    function f() {
        for (var i = 0; i < 1000; ++i) {
            var x = holder[symbol];
            assertEq(x, 1);
        }
    }

    for (var i = 0; i < 2; i++) {
        f();
    }
}
testSymbol();
