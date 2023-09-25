// |jit-test| --fuzzing-safe; --ion-offthread-compile=off; --ion-warmup-threshold=10; skip-if: (getBuildConfiguration("android") && getBuildConfiguration("debug") && getBuildConfiguration("arm64"))

// Test that Nursery::disable() waits for poisoning to finish before
// discarding and re-poisoning its chunks.

for(var i = 0; i < 100; i++) {
  try {
    evalInWorker(`
      function testOneSize(current_size) {
        var eval_string = 'obj = {';
        for (var current = 0; current <= current_size; ++current)
          eval_string += 'k' + current + ':' + current + ','
      }
      testOneSize(1023);
      testOneSize(1024);
      gczeal(4);
    `);
  } catch (exc) {}
}

