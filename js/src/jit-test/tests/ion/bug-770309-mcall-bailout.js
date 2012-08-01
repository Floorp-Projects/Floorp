
// Test various code paths associated with fused getprop/poly inlining.

function A(a) { this.a = a; }
A.prototype.foo = function (x) { return (x % 3) + this.a; };

function B(b) { this.b = b; }
B.prototype.foo = function (x) { return (x % 3) + this.b + 1; };

// c.foo() for some (c instanceof C) should always hit the fallback
// path of any fused poly inline cache created for it.
function C(c) { this.c = c; }
var GLOBX = {'x': function (x) {
    if (x > 29500)
        throw new Error("ERROR");
    return 2;
}};
function C_foo1(x) {
    return (x % 3) + this.c + GLOBX.x(x) + 1;
}
function C_foo2(x) {
    return (x % 3) + this.c + GLOBX.x(x) + 2;
}
C.prototype.foo = C_foo1;

// Create an array of As, Bs, and Cs.
function makeArray(n) {
    var classes = [A, B, C];
    var arr = [];
    for (var i = 0; i < n; i++) {
        arr.push(new classes[i % 3](i % 3));
    }
    return arr;
}

// Call foo on them, sum up results into first elem of resultArray
function runner(arr, resultArray, len) {
    for (var i = 0; i < len; i++) {
        // This changes the type of returned value from C.foo(), leading to
        // a bailout fater the call obj.foo() below.
        var obj = arr[i];
        resultArray[0] += obj.foo(i);
    }
}

// Make an array of instance.
var resultArray = [0];
var arr = makeArray(30000);

// Run runner for a bit with C.prototype.foo being C_foo1
runner(arr, resultArray, 100);

// Run runner for a bit with C.prototype.foo being C_foo2
C.prototype.foo = C_foo2;
runner(arr, resultArray, 100);

// Run runner for a bit longer to force GLOBX.x to raise
// an error inside a call to C.prototype.foo within runner.
var gotError = false;
try {
    runner(arr, resultArray, 30000);
} catch(err) {
    gotError = true;
}

// Check results.
assertEq(gotError, true);
assertEq(resultArray[0], 108859);
