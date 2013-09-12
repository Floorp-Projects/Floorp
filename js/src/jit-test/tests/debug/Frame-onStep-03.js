// Setting onStep does not affect later calls to the same function.
// (onStep is per-frame, not per-function.)

var g = newGlobal();
g.a = 1;
g.eval("function f(a) {\n" +
       "    var x = 2 * a;\n" +
       "    return x * x;\n" +
       "}\n");

var dbg = Debugger(g);
var log = '';
dbg.onEnterFrame = function (frame) {
    log += '+';
    frame.onStep = function () {
        if (log.charAt(log.length - 1) != 's')
            log += 's';
    };
};

g.f(1);
log += '|';
g.f(2);
log += '|';
dbg.onEnterFrame = undefined;
g.f(3);

assertEq(log, '+s|+s|');
