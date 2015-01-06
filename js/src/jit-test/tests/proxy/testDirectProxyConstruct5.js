load(libdir + "asserts.js");

// Make sure that a proxy only has a [[Construct]] if the target does

var handler = {};
var p = new Proxy(Math.sin, handler);
var r = Proxy.revocable(Math.sin, handler).proxy;

assertThrowsInstanceOf(() => new p, TypeError, "Can't use 'new' on proxy with non-constructor target");
assertThrowsInstanceOf(() => new r, TypeError, "Can't use 'new' on proxy with non-constructor target");
// Better throw regardless of whether we have a handler trap.
handler.construct = (() => ({}));
assertThrowsInstanceOf(() => new p, TypeError, "Can't use 'new' on proxy with non-constructor target");
assertThrowsInstanceOf(() => new r, TypeError, "Can't use 'new' on proxy with non-constructor target");
