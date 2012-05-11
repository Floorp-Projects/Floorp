
load(libdir + "asserts.js");

function C(a, b) {}
var f = C.bind(null, 2);
var that = this;
assertThrowsInstanceOf(function () { g = clone(f, that)}, TypeError);
