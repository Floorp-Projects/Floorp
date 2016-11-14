// ArrayBuffer.slice should be warned once and only once.

enableLastWarning();

ArrayBuffer.slice(new ArrayBuffer(10), 1);
var warning = getLastWarning();
assertEq(warning !== null, true, "warning should be generated");
assertEq(warning.name, "Warning");

clearLastWarning();
ArrayBuffer.slice(new ArrayBuffer(10), 1);
warning = getLastWarning();
assertEq(warning, null, "warning should not generated for 2nd ocurrence");
