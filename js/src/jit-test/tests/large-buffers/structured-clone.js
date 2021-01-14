var gb = 1 * 1024 * 1024 * 1024;
var ab = new ArrayBuffer(5 * gb);

var ta1 = new Uint8Array(ab);

for (var i = 0; i < 5; i++) {
    ta1[i * gb + 0] = i + 1;
    ta1[i * gb + 1] = i + 2;
    ta1[i * gb + 2] = i + 3;
    ta1[i * gb + 3] = i + 4;
}

function test(transferables, scope) {
    var ta2 = new Int32Array(ab, ab.byteLength - 8, 2);
    ta2[0] = 1234567;
    var dv1 = new DataView(ab);
    var dv2 = new DataView(ab, ab.byteLength - 8);
    dv2.setInt32(4, -987654);
    var objects = [ab, ta1, ta2, dv1, dv2];

    var clonebuf = serialize(objects, transferables, {scope});
    check(clonebuf);
}

function check(clonebuf) {
    var objects = deserialize(clonebuf);
    assertEq(objects.length, 5);

    var ab = objects[0];
    assertEq(ab instanceof ArrayBuffer, true);
    assertEq(ab.byteLength, 5 * gb);

    var ta1 = objects[1];
    assertEq(ta1 instanceof Uint8Array, true);
    assertEq(ta1.buffer, ab);
    assertEq(ta1.byteOffset, 0);
    assertEq(ta1.length, 5 * gb);

    for (var i = 0; i < 5; i++) {
        assertEq(ta1[i * gb + 0], i + 1);
        assertEq(ta1[i * gb + 1], i + 2);
        assertEq(ta1[i * gb + 2], i + 3);
        assertEq(ta1[i * gb + 3], i + 4);
    }

    var ta2 = objects[2];
    assertEq(ta2 instanceof Int32Array, true);
    assertEq(ta2.buffer, ab);
    assertEq(ta2.byteOffset, 5 * gb - 8);
    assertEq(ta2.length, 2);
    assertEq(ta2[0], 1234567);

    var dv1 = objects[3];
    assertEq(dv1 instanceof DataView, true);
    assertEq(dv1.buffer, ab);
    assertEq(dv1.byteOffset, 0);
    assertEq(dv1.byteLength, 5 * gb);

    var dv2 = objects[4];
    assertEq(dv2 instanceof DataView, true);
    assertEq(dv2.buffer, ab);
    assertEq(dv2.byteOffset, 5 * gb - 8);
    assertEq(dv2.byteLength, 8);
    assertEq(dv2.getInt32(4), -987654);
}

// It would be nice to test serialization of the ArrayBuffer's contents, but it
// causes OOMs and/or timeouts in automation so for now only test the more
// efficient version that transfers the underlying buffer.
//test([], "DifferentProcessForIndexedDB");
//assertEq(ab.byteLength, 5 * gb);

test([ab], "SameProcess");
assertEq(ab.byteLength, 0);
