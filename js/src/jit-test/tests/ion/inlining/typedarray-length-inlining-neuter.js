var INLINE_INT8_AMOUNT = 4;
var OUT_OF_LINE_INT8_AMOUNT = 237;

// Small and inline

var ab1 = new ArrayBuffer(INLINE_INT8_AMOUNT);
var ta1 = new Int8Array(ab1);
function q1() { return ta1.length; }
q1();
q1();
detachArrayBuffer(ab1);
assertEq(q1(), 0);

// Large and out-of-line

var ab2 = new ArrayBuffer(OUT_OF_LINE_INT8_AMOUNT);
var ta2 = new Int8Array(ab2);
function q2() { return ta2.length; }
q2();
q2();
detachArrayBuffer(ab2);
assertEq(q2(), 0);
