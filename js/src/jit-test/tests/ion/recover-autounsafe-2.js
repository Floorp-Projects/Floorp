// |jit-test| --fast-warmup; --ion-offthread-compile=off

function f() {
  "use strict";

  // Recoverable instruction. Uses AutoUnsafeCallWithABI, so no pending
  // exceptions are allowed.
  const recoverable = Math.sqrt(4);

  // Throws because |not_defined| isn't defined and we're in strict mode.
  // The thrown exception attempts to close the iterator, which will then
  // lead to recovering all instructions, including |Math.sqrt(4)|.
  [not_defined] = "a";
}

for (let i = 0; i < 50; i++) {
  try {
    f();
  } catch {}
}
