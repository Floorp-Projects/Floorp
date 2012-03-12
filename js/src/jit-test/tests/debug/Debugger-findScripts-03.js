// While eval code is running, findScripts returns its script.

var g = newGlobal('new-compartment');
var dbg = new Debugger(g);
var log;

g.check = function () {
    log += 'c';
    var frame = dbg.getNewestFrame();
    assertEq(frame.type, "eval");
    assertEq(dbg.findScripts().indexOf(frame.script) != -1, true);
};

log = '';
g.eval('check()');
assertEq(log, 'c');
