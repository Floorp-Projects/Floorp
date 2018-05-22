var wm = new WeakMap();
grayRoot().map = wm;
wm = null;
gczeal(13, 7);
var lfOffThreadGlobal = newGlobal();

reportCompare('do not crash', 'do not crash', 'did not crash!');
