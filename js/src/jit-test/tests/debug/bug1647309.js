// |jit-test| skip-if: !('oomTest' in this)

const g = newGlobal({ newCompartment: true });
const dbg = new Debugger(g);

// Define async generator in debuggee compartment.
g.eval("async function* f() { }");

// Use onEnterFrame hook to create generatorFrames entry.
dbg.onEnterFrame = () => {};

// Trigger failure in AsyncGeneratorNext.
ignoreUnhandledRejections();
oomTest(function() { g.f().next(); });

// Trigger DebugAPI::onSingleStep to check generatorFrames.
dbg.onDebuggerStatement = frame => { frame.onStep = () => {}; }
g.eval("debugger");
