// Bug 744730.

var g = newGlobal();
var dbg = Debugger(g);
dbg.onDebuggerStatement = function (f) { return {return: 1234}; };
var hit = false;
dbg.onEnterFrame = function (f) {
    f.onPop = function () { hit = true};
};
g.eval("debugger;");
assertEq(hit, true);
