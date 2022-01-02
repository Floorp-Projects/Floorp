
Object.defineProperty(Object.prototype, "a", {});
function t2(b) { this.a = b; }
var x = new t2(3);
assertEq(x.a, undefined);
