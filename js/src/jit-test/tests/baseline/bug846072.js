// |jit-test| error: TypeError
toString = undefined;
if (!(this in ParallelArray)) {}
