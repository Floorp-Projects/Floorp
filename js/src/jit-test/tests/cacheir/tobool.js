var success         = 0;
var expected_bool   = 0;
var expected_int    = 0;
var expected_double = 0;
var expected_string = 0;
var expected_object = 0;
var expected_symbol = 0;

function test_type_stable_ic() {
    // Initialize as falsy where possible.
    var a = undefined; // No Change, never succeeds
    var b = null;      // No Change, never succeeds
    var c = false;     // Alternate between true and false.
    var d = 0;         // Int32 cache checker, change int values
    var e = "";        // String cache checker, change string values
    var f = Symbol();  // No change, always succeed, no cache.
    var g = {};        // Change objects, always succeed.
    var h = -0;        // Double cache checker, change double values.

    for (var i =0; i < 30; i++) {
        // Switch between values to ensure the caches fire.
        switch (i % 3) {
            case 0:
                d = 0;
                e = "hi"; expected_string++;
                c = true; expected_bool++;
                h = 0;
                break;
            case 1:
                d = 1; expected_int++;
                e = "";
                c = false;
                h = NaN;
                break;
            case 2:
                d = 2; expected_int++;
                h = 1.234; expected_double++;
                g = {};
                break;
        }

        if (a) { success++; }
        if (b) { success++; }
        if (c) { success++; }
        if (d) { success++; }
        if (e) { success++; }
        if (f) { success++; } expected_symbol++; // Symbol succeed
        if (g) { success++; } expected_object++; // Object success
        if (h) { success++; }
    }
}

test_type_stable_ic();

assertEq(success, expected_bool + expected_double + expected_int + expected_object + expected_string + expected_symbol);


// Test cache failures.
function helper(fun, arg, n)
{
    var r = 0;
    for (var i = 0; i < n; i++) {
        r = fun(arg);
    }
    return r ? 1 : 0;
}

function test_transition(fun, load, test, before, after) {
    var a = helper(fun, load, 30);
    var x = helper(fun, test, 5)
    assertEq(a, before);
    assertEq(x, after)
}

var fun1 = (x) => { if (x) return true; else return false; };
var fun2 = (x) => { if (x) return true; else return false; };
var fun3 = (x) => { if (x) return true; else return false; };
var fun4 = (x) => { if (x) return true; else return false; };
var fun5 = (x) => { if (x) return true; else return false; };
var fun6 = (x) => { if (x) return true; else return false; };
var fun7 = (x) => { if (x) return true; else return false; };
var fun8 = (x) => { if (x) return true; else return false; };

// NaN -> Int32
test_transition(fun1, NaN, 1, 0, 1);
test_transition(fun2, 1, NaN, 1, 0);
// NaN -> Object / Object -> NaN
test_transition(fun3, NaN, {}, 0, 1);
test_transition(fun4, {}, NaN, 1, 0);
// Object -> null / null -> Object
test_transition(fun5, {}, null, 1, 0);
test_transition(fun6, null, {},  0, 1);
// Symbol -> null, null -> Symbol
test_transition(fun7, Symbol('hi'), null, 1, 0);
test_transition(fun8, null, Symbol('lo'),  0, 1);