// frame.eval works from an onPop handler.
var g = newGlobal('new-compartment');
g.eval('function f(a,b) { var x = "entablature", y; debugger; return x+y+a+b; }');

var dbg = new Debugger(g);
var log;

dbg.onDebuggerStatement = function handleDebugger(frame) {
    log += 'd';
    frame.onPop = handlePop;
};

function handlePop(c) {
    log += ')';

    // Arguments must be live.
    assertEq(this.eval('a').return, 'frieze');
    assertEq(this.eval('b = "architrave"').return, 'architrave');
    assertEq(this.eval('arguments[1]').return, 'architrave');
    assertEq(this.eval('b').return, 'architrave');

    // function-scope variables must be live.
    assertEq(this.eval('x').return, 'entablature');
    assertEq(this.eval('y = "cornice"').return, 'cornice');
    assertEq(this.eval('y').return, 'cornice');
}

log = '';
g.eval('f("frieze", "stylobate")');
assertEq(log, 'd)');
