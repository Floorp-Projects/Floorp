// |jit-test| error: ReferenceError; --ion-warmup-threshold=50
setJitCompilerOption("offthread-compilation.enable", 0);
gcPreserveCode();

new Function(`
  while (true) {
    try {
        var buf = new Uint8ClampedArray(-1);
    } catch (e) {
        break;
    }
  }
  var caughtInvalidArguments = false;
  while (true) {
    var a = inIon() ? -true.get : 0;
    while (x > 7 & 0) {}
  }
`)();

