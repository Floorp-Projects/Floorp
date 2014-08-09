setJitCompilerOption("baseline.usecount.trigger", 10);
setJitCompilerOption("ion.usecount.trigger", 20);

var y = 1;
for (var i = 0; i < 64; i++) {
  assertEq(Math.floor(y), y);
  y *= 2;
}
