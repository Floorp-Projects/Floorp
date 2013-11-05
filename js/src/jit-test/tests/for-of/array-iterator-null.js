// Array.prototype.iterator applied to undefined or null throws directly.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

for (var v of [undefined, null]) {
    // ES6 draft 2013-09-05 section 22.1.5.1.
    assertThrowsInstanceOf(function () { Array.prototype[std_iterator].call(v); }, TypeError);
    assertThrowsInstanceOf(function () { Array.prototype.keys.call(v); }, TypeError);
    assertThrowsInstanceOf(function () { Array.prototype.entries.call(v); }, TypeError);
}
