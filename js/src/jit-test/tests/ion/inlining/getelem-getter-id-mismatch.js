// Ensure BaselineInspector properly handles mixed atom/symbols when determining
// whether or not a jsop_getelem access to a getter can be inlined.

// Defined outside of the test functions to ensure they're recognised as
// constants in Ion.
var atom1 = "prop1";
var atom2 = "prop2";
var sym1 = Symbol();
var sym2 = Symbol();

function testAtomAtom() {
    var holder = {
        get [atom1]() { return 1; },
        get [atom2]() { return 2; },
    };

    function get(name) {
        // Single access point called with different atoms.
        return holder[name];
    }

    function f() {
        for (var i = 0; i < 1000; ++i) {
            var x = get(atom1);
            var y = get(atom2);
            assertEq(x, 1);
            assertEq(y, 2);
        }
    }

    for (var i = 0; i < 2; i++) {
        f();
    }
}
testAtomAtom();

function testAtomSymbol() {
    var holder = {
        get [atom1]() { return 1; },
        get [sym2]() { return 2; },
    };

    function get(name) {
        // Single access point called with atom and symbol.
        return holder[name];
    }

    function f() {
        for (var i = 0; i < 1000; ++i) {
            var x = get(atom1);
            var y = get(sym2);
            assertEq(x, 1);
            assertEq(y, 2);
        }
    }

    for (var i = 0; i < 2; i++) {
        f();
    }
}
testAtomSymbol();

function testSymbolAtom() {
    var holder = {
        get [sym1]() { return 1; },
        get [atom2]() { return 2; },
    };

    function get(name) {
        // Single access point called with symbol and atom.
        return holder[name];
    }

    function f() {
        for (var i = 0; i < 1000; ++i) {
            var x = get(sym1);
            var y = get(atom2);
            assertEq(x, 1);
            assertEq(y, 2);
        }
    }

    for (var i = 0; i < 2; i++) {
        f();
    }
}
testSymbolAtom();

function testSymbolSymbol() {
    var holder = {
        get [sym1]() { return 1; },
        get [sym2]() { return 2; },
    };

    function get(name) {
        // Single access point called with different symbols.
        return holder[name];
    }

    function f() {
        for (var i = 0; i < 1000; ++i) {
            var x = get(sym1);
            var y = get(sym2);
            assertEq(x, 1);
            assertEq(y, 2);
        }
    }

    for (var i = 0; i < 2; i++) {
        f();
    }
}
testSymbolSymbol();
