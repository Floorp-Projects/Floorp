// |jit-test| skip-if: !('oomTest' in this); --fuzzing-safe

oomTest(Debugger);
oomTest(Debugger);
async function* f() {}
f().return();
dumpHeap(f);
