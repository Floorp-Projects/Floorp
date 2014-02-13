// |jit-test| exitstatus: 6;

setJitCompilerOption("baseline.usecount.trigger", 1);
setJitCompilerOption("ion.usecount.trigger", 2);

// The timepout function is made to trigger the interruption callback. The
// interruption callback will protect the code while a GC might be
// marking/updating pointers in it.
var x = 0;
function interrupt_gc() {
  if (x++ >= 20)
    return;
  timeout(0.1, interrupt_gc);
  while(x < 20)
    gc();
}

interrupt_gc();
