// Based on work by André Bargull

function f() {
    var x = [1,2,3];
    x[3] = 0xff;

    // Should have been defined on typed array.
    assertEq(x.length, 3);
    assertEq(x[3], -1);

    x[3] = 0;
}

Object.setPrototypeOf(Array.prototype, new Int8Array(4));
f();
f();

function g() {
    var x = [1,2,3,4];
    x[4] = 0xff;

    // OOB [[Set]] should have been ignored
    assertEq(x.length, 4);
    assertEq(x[4], undefined);
}

Object.setPrototypeOf(Array.prototype, new Int8Array(4));
g();
g();
