// |jit-test| error:AllDone
// When we enter debug mode in a compartment, we must throw away all
// analyses in that compartment (debug mode affects the results of
// analysis, so they become out of date). This is true even when we would
// otherwise be retaining jit code and its related data structures for
// animation timing.

if (typeof gcPreserveCode != "function")
  throw('AllDone');

var g = newGlobal();
var dbg = new Debugger;

g.eval("" +
       function fib(n) {
         var a = 0, b = 1;
         while (n-- > 0)
           b = b+a, a = b-a;
         return b;
       });

g.fib(20);                      // Cause g.fib to be jitted. This creates an analysis with
                                // debug mode off.

gcPreserveCode();               // Tell the gc to preserve JIT code and analyses by
                                // default. A recent call to js::NotifyAnimationActivity
                                // could have a similar effect in real life.

dbg.addDebuggee(g);             // Put g in debug mode. This triggers a GC which must
                                // clear all analyses. In the original buggy code, we also
                                // release all of g's scripts' JIT code, leading to a
                                // recompilation the next time it was called.

g.fib(20);                      // Run g.fib again, causing it to be re-jitted. If the
                                // original analysis is still present, JM will assert,
                                // because it is not in debug mode.

throw('AllDone');
