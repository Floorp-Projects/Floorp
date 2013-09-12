// Within a series of evals and calls, all their frames' scripts appear in findScripts' result.
var g = newGlobal();
var dbg = new Debugger(g);
var log;

g.check = function () {
    log += 'c';
    var scripts = dbg.findScripts();

    var innerEvalFrame = dbg.getNewestFrame();
    assertEq(innerEvalFrame.type, "eval");
    assertEq(scripts.indexOf(innerEvalFrame.script) != -1, true);

    var callFrame = innerEvalFrame.older;
    assertEq(callFrame.type, "call");
    assertEq(scripts.indexOf(callFrame.script) != -1, true);

    var outerEvalFrame = callFrame.older;
    assertEq(outerEvalFrame.type, "eval");
    assertEq(scripts.indexOf(outerEvalFrame.script) != -1, true);
    assertEq(innerEvalFrame != outerEvalFrame, true);
};

g.eval('function f() { eval("check();") }');
log = '';
g.eval('f();');
assertEq(log, 'c');
