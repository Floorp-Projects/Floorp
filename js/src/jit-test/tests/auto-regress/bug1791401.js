oomTest(function() {
  var f = Function(`
    // Don't actually enter the loop. This still causes the original bug and
    // additionally makes the test complete faster.
    //
    // Don't directly use |false|, otherwise the byte code emitter won't emit
    // the loop.
    var False = [false, false][0];
    if (False) {
      for (let x;;) {
        // Capture |x|, so it's freshened each loop iteration.
        Object(() => x);
      }
     }
  `);
  f();
});
