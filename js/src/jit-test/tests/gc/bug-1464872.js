var g = newGlobal();
var dbg = Debugger(g);
dbg.onEnterFrame = function(frame) {};

var g2 = newGlobal();
g2[g] = g;
g2.evaluate("grayRoot()")
g2 = undefined;

g = undefined;
dbg = undefined;

gc();
startgc(100000);
