gczeal(0);
var x = newGlobal();
x.evaluate("grayRoot()");
x = 0;
setMarkStackLimit(4);
gc();
