// We detect and stop the runaway recursion caused by making onEnterFrame a
// wrapper of a debuggee function.

// This is all a bit silly. In any reasonable design, both debugger re-entry
// (the second onEnterFrame invocation) and debuggee re-entry (the call to g.f
// from within the debugger, not via a Debugger invocation function) would raise
// errors immediately. We have plans to do so, but in the mean time, we settle
// for at least detecting the recursion.

load(libdir + 'asserts.js');

var g = newGlobal();
g.eval("function f(frame) { n++; return 42; }");
g.n = 0;

var dbg = Debugger();
var gw = dbg.addDebuggee(g);

// Register the debuggee function as the onEnterFrame handler. When we first
// call or eval in the debuggee:
//
// - The onEnterFrame call reporting that frame's creation is itself an event
//   that must be reported, so we call onEnterFrame again.
//
// - SpiderMonkey detects the out-of-control recursion, and generates a "too
//   much recursion" InternalError in the youngest onEnterFrame call.
//
// - We don't catch it, so the onEnterFrame handler call itself throws.
//
// - Since the Debugger doesn't have an uncaughtExceptionHook (it can't; such a
//   hook would itself raise a "too much recursion" exception), Spidermonkey
//   reports the exception immediately and terminates the debuggee --- which is
//   the next-older onEnterFrame call.
//
// - This termination propagates all the way out to the initial attempt to
//   create a frame in the debuggee.
dbg.onEnterFrame = g.f;

// Get a Debugger.Object instance referring to f.
var debuggeeF = gw.makeDebuggeeValue(g.f);

// Using f.call allows us to catch the termination.
assertEq(debuggeeF.call(), null);

// We should never actually begin execution of the function.
assertEq(g.n, 0);

// When an error is reported, the shell usually exits with a nonzero exit code.
// If we get here, the test passed, so override that behavior.
quit(0);
