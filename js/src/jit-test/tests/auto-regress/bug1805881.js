// |jit-test| --baseline-warmup-threshold=10; --ion-warmup-threshold=100

function blackhole() {
  with ({});
}

function main() {
  for (let i = 0; i < 100; i++) {
    try {
      const v3 = 0..toString(i);
      for (let j = 0; j < 100; j++) {
      }
      blackhole(v3);
    } catch {
    }
    for (let j = 0; j < 6; ++j) {
    }
  }
}

main();
