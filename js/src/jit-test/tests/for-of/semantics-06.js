// Deleting the .next method makes for-of stop working on arrays.

load(libdir + "asserts.js");
var iterProto = Object.getPrototypeOf([].iterator());
delete iterProto.next;
assertThrowsInstanceOf(function () { for (var v of []) ; }, TypeError);
