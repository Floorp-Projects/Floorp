// Test for OOM hitting a breakpoint in a generator.
//
// (The purpose is to test OOM-handling in the code that creates the
// Debugger.Frame object and associates it with the generator object.)

if (!('oomTest' in this))
    quit();

let g = newGlobal();
g.eval(`\
    function* gen(x) {  // line 1
        x++;            // 2
        yield x;        // 3
    }                   // 4
`);

let dbg = new Debugger;

// On OOM in the debugger, propagate it to the debuggee.
dbg.uncaughtExceptionHook = exc => exc === "out of memory" ? {throw: exc} : null;

let gw = dbg.addDebuggee(g);
let script = gw.makeDebuggeeValue(g.gen).script;
let hits = 0;
let handler = {
    hit(frame) {
        hits++;
        print("x=", frame.environment.getVariable("x"));
    }
};
for (let offset of script.getLineOffsets(2))
    script.setBreakpoint(offset, handler);

let result;
oomTest(() => {
    hits = 0;
    result = g.gen(1).next();
}, false);
assertEq(hits, 1);
assertEq(result.done, false);
assertEq(result.value, 2);

