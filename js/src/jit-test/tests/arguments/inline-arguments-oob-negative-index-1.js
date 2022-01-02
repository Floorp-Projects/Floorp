// |jit-test| --fast-warmup

function inner(i) {
  // Can't be eliminated because negative indices cause a bailout.
  arguments[i];
  arguments[i];
  arguments[i];
}

function outer(i) {
  trialInline();

  // Loop header to trigger OSR.
  for (let j = 0; j < 1; ++j) {
    inner(i,
      // Add extra arguments to ensure we read the arguments from the frame.
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
    );
  }
}

let count = 0;

for (let i = 0; i <= 100; ++i) {
    if (i === 50) {
      Object.defineProperty(Object.prototype, -1, {
        get() {
          count++;
        }
      });
    }
    outer(i < 100 ? i : -1);
}

assertEq(count, 3);
