// Various cases that used to assert, but will now throw (as they should).

if (!this.SharedUint16Array)
    quit();

var thrown = false;
try {
    new SharedUint16Array(2147483647); // Bug 1068458
}
catch (e) {
    thrown = true;
}
assertEq(thrown, true);

var thrown = false;
try {
    new SharedUint16Array(0xdeadbeef); // Bug 1072176
}
catch (e) {
    thrown = true;
}
assertEq(thrown, true);
