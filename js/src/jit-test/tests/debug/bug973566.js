Object.prototype[1] = 'peek';
var g = newGlobal();
var dbg = Debugger(g);
dbg.onEnterFrame = function (frame) {
    var lines = frame.script.getAllOffsets();
};
g.eval("1;");
