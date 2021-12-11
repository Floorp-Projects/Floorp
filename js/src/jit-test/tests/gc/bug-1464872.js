gczeal(0);

var g = newGlobal({newCompartment: true});
var dbg = Debugger(g);
dbg.onEnterFrame = function(frame) {};

var g2 = newGlobal({newCompartment: true});
g2[g] = g;
g2.evaluate("grayRoot()")
g2 = undefined;

g = undefined;
dbg = undefined;

gc();
startgc(100000);
