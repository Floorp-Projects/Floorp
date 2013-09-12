
var g = newGlobal("same-compartment");
g.eval("this.f = function(a) {" +
       "assertEq(a instanceof Array, false);" +
       "a = Array.prototype.slice.call(a);" +
       "assertEq(a instanceof Array, true); }");
g.f([1, 2, 3]);

var g2 = newGlobal();
g2.a = g2.Array(10);
assertEq(g2.a instanceof Array, false);
g2.a = Array.prototype.slice(g2.a);
assertEq(g2.a instanceof Array, true);
