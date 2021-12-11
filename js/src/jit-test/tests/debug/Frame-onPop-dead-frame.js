// Don't crash trying to fire a dead frame's onPop handler.

var g = newGlobal({newCompartment: true});
g.eval('function f() { debugger; }');

var log = '';

// Create two Debuggers debugging the same global `g`. Both will put onPop
// handlers on the same frame.
var dbg1 = Debugger(g);
dbg1.onDebuggerStatement = frame1 => {
  frame1.onPop = completion => {
    log += 'A';
    dbg2.removeDebuggee(g); // kills frame2, so frame2.onPop should not fire
    log += 'B';
  };
};

var dbg2 = Debugger(g);
dbg2.onDebuggerStatement = frame2 => {
  frame2.onPop = completion => {
    log += 'C';
  };
};

g.f();

assertEq(log, 'AB');
