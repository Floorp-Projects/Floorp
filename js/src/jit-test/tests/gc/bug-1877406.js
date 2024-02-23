// |jit-test| --fuzzing-safe

oomTest(Debugger);
oomTest(Debugger);
async function* f() {}
f().return();
dumpHeap(f);
