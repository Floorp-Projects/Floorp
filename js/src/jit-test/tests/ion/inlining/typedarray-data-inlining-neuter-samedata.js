var NONINLINABLE_AMOUNT = 40;
var SIZEOF_INT32 = 4;

var INLINABLE_INT8_AMOUNT = 4;

// Large arrays with non-inline data

// Neutering and replacing data.
var ab1 = new ArrayBuffer(NONINLINABLE_AMOUNT * SIZEOF_INT32);
var ta1 = new Int32Array(ab1);
for (var i = 0; i < ta1.length; i++)
  ta1[i] = i + 43;
function q1() { return ta1[NONINLINABLE_AMOUNT - 1]; }
assertEq(q1(), NONINLINABLE_AMOUNT - 1 + 43);
assertEq(q1(), NONINLINABLE_AMOUNT - 1 + 43);
neuter(ab1, "change-data");
assertEq(q1(), undefined);

// Neutering preserving data pointer.
var ab2 = new ArrayBuffer(NONINLINABLE_AMOUNT * SIZEOF_INT32);
var ta2 = new Int32Array(ab2);
for (var i = 0; i < ta2.length; i++)
  ta2[i] = i + 77;
function q2() { return ta2[NONINLINABLE_AMOUNT - 1]; }
assertEq(q2(), NONINLINABLE_AMOUNT - 1 + 77);
assertEq(q2(), NONINLINABLE_AMOUNT - 1 + 77);
neuter(ab2, "same-data");
assertEq(q2(), undefined);

// Small arrays with inline data

// Neutering and replacing data.
var ab3 = new ArrayBuffer(INLINABLE_INT8_AMOUNT);
var ta3 = new Int8Array(ab3);
for (var i = 0; i < ta3.length; i++)
  ta3[i] = i + 13;
function q3() { return ta3[INLINABLE_INT8_AMOUNT - 1]; }
assertEq(q3(), INLINABLE_INT8_AMOUNT - 1 + 13);
assertEq(q3(), INLINABLE_INT8_AMOUNT - 1 + 13);
neuter(ab3, "change-data");
assertEq(q3(), undefined);

// Neutering preserving data pointer.
var ab4 = new ArrayBuffer(4);
var ta4 = new Int8Array(ab4);
for (var i = 0; i < ta4.length; i++)
  ta4[i] = i + 17;
function q4() { return ta4[INLINABLE_INT8_AMOUNT - 1]; }
assertEq(q4(), INLINABLE_INT8_AMOUNT - 1 + 17);
assertEq(q4(), INLINABLE_INT8_AMOUNT - 1 + 17);
neuter(ab4, "same-data");
assertEq(q4(), undefined);
