gczeal(0);
var x = newGlobal();
x.evaluate("grayRoot()");
x = 0;
gcparam("markStackLimit", 4);
gc();
