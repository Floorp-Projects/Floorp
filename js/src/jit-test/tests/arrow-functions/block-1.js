// Braces after => indicate a block body as opposed to an expression body.

var f = () => {};
assertEq(f(), undefined);
var g = () => ({});
assertEq(typeof g(), 'object');
