var BUGNUMBER = 1263803;
var summary = 'CloneArrayBuffer should allocate ArrayBuffer even with individual byteLength';

print(BUGNUMBER + ": " + summary);

var abuf1 = new ArrayBuffer(38);
assertEq(abuf1.byteLength, 38);

var a1 = new Float64Array(abuf1, 24, 0);
assertEq(a1.buffer.byteLength, 38);
assertEq(a1.byteLength, 0);
assertEq(a1.byteOffset, 24);

var a2 = new Float64Array(a1);
// NOTE: There is a behavior difference on ArrayBuffer's byteLength, between
//       lazily allocated case and eagerly allocated case (bug 1264941).
//       This one is lazily allocated case, and it equals to a2.byteLength.
assertEq(a2.buffer.byteLength, 0);
assertEq(a2.byteLength, 0);
assertEq(a2.byteOffset, 0);

class MyArrayBuffer extends ArrayBuffer {}

var abuf2 = new MyArrayBuffer(38);
assertEq(abuf2.byteLength, 38);

var a3 = new Float64Array(abuf2, 24, 0);
assertEq(a3.buffer.byteLength, 38);
assertEq(a3.byteLength, 0);
assertEq(a3.byteOffset, 24);

var a4 = new Float64Array(a3);
// NOTE: This one is eagerly allocated case, and it differs from a4.byteLength.
//       Either one should be fixed once bug 1264941 is fixed.
assertEq(a4.buffer.byteLength, 14);
assertEq(a4.byteLength, 0);
assertEq(a4.byteOffset, 0);

if (typeof reportCompare === 'function')
  reportCompare(true, true);
