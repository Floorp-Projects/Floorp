// An exception thrown from an iterate trap is propagated.

load(libdir + "asserts.js");

var p = Proxy.create({iterate: function () { throw "fit"; }});
assertThrowsValue(function () { for (var v of p) {} }, "fit");
