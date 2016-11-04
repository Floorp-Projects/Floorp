if (!this.SharedArrayBuffer)
    quit(0);

var ia = new Int32Array(new SharedArrayBuffer(4));

// Atomics.store() returns the input value converted to integer as if
// by ToInteger.
//
// JIT and interpreter have different paths here, so loop a little to
// trigger the JIT properly.

function f() {
    assertEq(Atomics.store(ia, 0, 3.5), 3);
    assertEq(ia[0], 3);

    assertEq(Atomics.store(ia, 0, -0), -0);
    assertEq(ia[0], 0);

    assertEq(Atomics.store(ia, 0, '4.6'), 4);
    assertEq(ia[0], 4);

    assertEq(Atomics.store(ia, 0, '-4.6'), -4);
    assertEq(ia[0], -4);

    assertEq(Atomics.store(ia, 0, undefined), 0);
    assertEq(ia[0], 0);

    assertEq(Atomics.store(ia, 0, Infinity), Infinity);
    assertEq(ia[0], 0);

    assertEq(Atomics.store(ia, 0, -Infinity), -Infinity);
    assertEq(ia[0], 0);

    assertEq(Atomics.store(ia, 0, Math.pow(2, 32)+5), Math.pow(2, 32)+5);
    assertEq(ia[0], 5);

    assertEq(Atomics.store(ia, 0, { valueOf: () => 3.7 }), 3);
    assertEq(ia[0], 3);
}

for ( var i=0 ; i < 10 ; i++ )
    f();
