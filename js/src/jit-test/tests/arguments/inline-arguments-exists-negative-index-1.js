// |jit-test| --fast-warmup

function inner(i) {
  return i in arguments;
}

function outer(i) {
  trialInline();

  // Loop header to trigger OSR.
  let r = 0;
  for (let j = 0; j < 1; ++j) {
    r += inner(i,
      // Add extra arguments to ensure we read the arguments from the frame.
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0,
    );
  }
  return r;
}

let count = 0;

for (let i = 0; i <= 100; ++i) {
    if (i === 50) {
      Object.prototype[-1] = 0;
    }
    count += outer(i < 100 ? i : -1);
}

assertEq(count, 1 + 21);
