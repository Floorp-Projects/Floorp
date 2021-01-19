// |jit-test| skip-if: !this.SharedArrayBuffer

var gb = 1 * 1024 * 1024 * 1024;
var sab = new SharedArrayBuffer(4 * gb + 10);
var ta = new Uint8Array(sab);

function testSlice() {
    // Large |start| and |end|.
    ta[4 * gb + 0] = 11;
    ta[4 * gb + 1] = 22;
    ta[4 * gb + 2] = 33;
    ta[4 * gb + 3] = 44;
    var ta2 = new Uint8Array(sab.slice(4 * gb, 4 * gb + 4));
    assertEq(ta2.toString(), "11,22,33,44");

    // Large |start|.
    ta[ta.length - 3] = 99;
    ta[ta.length - 2] = 88;
    ta[ta.length - 1] = 77;
    ta2 = new Uint8Array(sab.slice(4 * gb + 8));
    assertEq(ta2.toString(), "88,77");

    // Relative values.
    ta2 = new Uint8Array(sab.slice(-3, -1));
    assertEq(ta2.toString(), "99,88");

    // Large relative values.
    ta[0] = 100;
    ta[1] = 101;
    ta[2] = 102;
    ta2 = new Uint8Array(sab.slice(-ta.length, -ta.length + 3));
    assertEq(ta2.toString(), "100,101,102");
}
testSlice();

function testSharedMailbox() {
    setSharedObject(sab);
    assertEq(getSharedObject().byteLength, 4 * gb + 10);
}
testSharedMailbox();
