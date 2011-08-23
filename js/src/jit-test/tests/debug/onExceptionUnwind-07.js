// Unwinding due to uncatchable errors does not trigger onExceptionUnwind.

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
var hits = 0;
dbg.onExceptionUnwind = function (frame, value) { hits = 'BAD'; };
dbg.onDebuggerStatement = function (frame) {
    if (hits++ === 0)
	assertEq(frame.eval("debugger;"), null);
    else
	return null;
}

assertEq(g.eval("debugger; 2"), 2);
assertEq(hits, 2);
