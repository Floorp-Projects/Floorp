var g = newGlobal();
var w1 = g.Math;
var w2 = g.evaluate("new Array");
recomputeWrappers(this, g);
recomputeWrappers();
