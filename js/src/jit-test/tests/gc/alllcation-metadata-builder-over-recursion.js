// |jit-test| allow-unhandlable-oom

// Over-recursion should suppress alloation metadata builder, to avoid another
// over-recursion while generating an error object for the first over-recursion.
//
// This test should catch the error for the "load" testing function's arguments,
// or crash with unhandlable OOM inside allocation metadata builder.

const g = newGlobal();
g.enableShellAllocationMetadataBuilder();
function run() {
    const g_load = g.load;
    g_load.toString = run;
    return g_load(g_load);
}
let caught = false;
try {
  run();
} catch (e) {
  caught = true;
}
assertEq(caught, true);
