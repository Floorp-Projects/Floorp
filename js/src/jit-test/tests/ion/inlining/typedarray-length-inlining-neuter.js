var INLINE_INT8_AMOUNT = 4;
var OUT_OF_LINE_INT8_AMOUNT = 237;

// Small and inline

// Detaching and replacing data.
var ab1 = new ArrayBuffer(INLINE_INT8_AMOUNT);
var ta1 = new Int8Array(ab1);
function q1() { return ta1.length; }
q1();
q1();
detachArrayBuffer(ab1, "change-data");
assertEq(q1(), 0);

// Detaching preserving data pointer.
var ab2 = new ArrayBuffer(INLINE_INT8_AMOUNT);
var ta2 = new Int8Array(ab2);
function q2() { return ta2.length; }
q2();
q2();
detachArrayBuffer(ab2, "same-data");
assertEq(q2(), 0);

// Large and out-of-line

// Detaching and replacing data.
var ab3 = new ArrayBuffer(OUT_OF_LINE_INT8_AMOUNT);
var ta3 = new Int8Array(ab3);
function q3() { return ta3.length; }
q3();
q3();
detachArrayBuffer(ab3, "change-data");
assertEq(q3(), 0);

// Detaching preserving data pointer.
var ab4 = new ArrayBuffer(OUT_OF_LINE_INT8_AMOUNT);
var ta4 = new Int8Array(ab4);
function q4() { return ta4.length; }
q4();
q4();
detachArrayBuffer(ab4, "same-data");
assertEq(q4(), 0);
