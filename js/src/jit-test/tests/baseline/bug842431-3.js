// |jit-test| no-jm
// (disable with JM, see bug 845276)

// If an exception unwind hook performs a forced return, and this calls an
// onPop handler that throws, the exception should not be handled in the
// current frame but propagated to the caller.
var g = newGlobal('new-compartment');
var dbg = new Debugger(g);

g.eval('function f() { try { var y; throw 123; } catch(e) { assertEq(0, 1); } }');

dbg.onExceptionUnwind = function(frame) {
    frame.onPop = function() {
	return {throw: 321};
    }

    return {return: 11};
};

try {
    g.f();
    assertEq(0, 1);
} catch(e) {
    assertEq(e.toString().contains("321"), true);
}
