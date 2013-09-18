// |jit-test| error:AllDone
// When we leave debug mode in a compartment, we must throw away all
// analyses in that compartment (debug mode affects the results of
// analysis, so they become out of date). We cannot skip this step when
// there are debuggee frames on the stack.

var g = newGlobal();
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);

g.eval("" +
       function fib(n) {
         var a = 0, b = 1;
         while (n-- > 0)
           b = b+a, a = b-a;
         return b;
       });


// Cause g.fib to be jitted. This creates an analysis with debug mode on.
g.fib(20);

// Setting a breakpoint in g.f causes us to throw away the jit code, but
// not the analysis.
gw.makeDebuggeeValue(g.fib).script.setBreakpoint(0, { hit: function (f) { } });

// Take g out of debug mode, with debuggee code on the stack. In older
// code, this would not trigger a cleansing GC, so the script will
// retain its analysis.
dbg.onDebuggerStatement = function (f) {
  dbg.removeDebuggee(g);
};
g.eval('debugger');

// Run g.fib again, causing it to be re-jitted. If the original analysis is
// still present, JM will assert, because it is not in debug mode.
g.fib(20);

throw('AllDone');
