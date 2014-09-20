// This used to assert, now it will throw because the limit
// (in bytes) on a SharedArrayBuffer is INT32_MAX.

if (!this.SharedUint16Array)
    quit();

var thrown = false;
try {
    new SharedUint16Array(2147483647);
}
catch (e) {
    thrown = true;
}
assertEq(thrown, true);
