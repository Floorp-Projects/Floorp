// A proxy P whose target is an object X whose prototype is an array V inherits V.length.

var V = [1, 2, 3];
var X = Object.create(V);
var P = new Proxy(X, {});
assertEq(P.length, 3);
