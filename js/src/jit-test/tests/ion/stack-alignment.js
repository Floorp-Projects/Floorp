setJitCompilerOption("baseline.warmup.trigger", 10);
setJitCompilerOption("ion.warmup.trigger", 30);
var i;

// Check that an entry frame is always aligned properly.
function entryFrame_1() {
  assertValidJitStack();
}

for (i = 0; i < 40; i++) {
  entryFrame_1();
  entryFrame_1(0);
  entryFrame_1(0, 1);
}
