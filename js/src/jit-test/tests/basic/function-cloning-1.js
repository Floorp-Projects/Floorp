var g = newGlobal();
var f1 = g.evaluate("(function (x) { function inner() {}; })");
gczeal(2, 1); // Exercise all the edge cases in cloning, please.
var f2 = clone(f1);
