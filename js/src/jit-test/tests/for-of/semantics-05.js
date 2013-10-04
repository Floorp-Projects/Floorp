// Deleting String.prototype.iterator makes for-of stop working on strings.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

delete String.prototype[std_iterator];
assertThrowsInstanceOf(function () { for (var v of "abc") ; }, TypeError);
assertThrowsInstanceOf(function () { for (var v of new String("abc")) ; }, TypeError);
