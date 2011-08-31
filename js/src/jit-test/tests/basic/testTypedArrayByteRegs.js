// Uint8Array and Int8Array need single byte registers.
// Test for id and value having the same backing.
function f() {
    var x = new Uint8Array(30);
    for (var i=0; i<x.length; i++) {
        x[i] = i;
    }

    var res = 0;
    for (var i=0; i<x.length; i++) {
        res += x[i];
    }
    assertEq(res, 435);
}
f();

// Test for id and value having a different backing.
function g() {
    var x = new Int8Array(30);
    for (var i=1; i<x.length; i++) {
        x[i-1] = i;
    }

    var res = 0;
    for (var i=0; i<x.length; i++) {
        res += x[i];
    }
    assertEq(res, 435);
}
g();
