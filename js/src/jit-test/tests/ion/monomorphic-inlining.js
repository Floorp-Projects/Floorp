// |jit-test| --ion-offthread-compile=off;

function foo(f, a, b) {
  return f(a, b);
}

function bar(a, b) {
  let result = a + b;
  if (result >= fns.length) {
    return b + a;
  }
  return result;
}

function baz(a, b) {
  return a + b;
}

let fns = [];

// This is pretty fiddly. What we are trying to test here is a specific path
// in the bailout code which needs to know which ICScript to load, and has to
// decide between the script's own ICScript, or the trial-inlined ICScript
// which belongs to the outer script. It uses the ICFallbackStub's
// trialInliningState to make this decision, which can change out from
// underneath us if the inlined call fails. So what were doing here is getting
// into a state where we've monomorphic inlined a function, and gone to Ion
// with it. We then cause the inlined call to fail by calling a function which
// doesn't match what we expect, which transitions us to a failed
// trialInliningState. We then will bail out *inside* bar, due to the
// previously unseen inside of the result >= fns.length check, exercising the
// bailout code in question.
for (let i = 0; i < 2000; i++) {
  fns.push(bar);
}

fns.push(baz);
fns.push(bar);

for (let i = 0; i < fns.length; i++) {
  assertEq(foo(fns[i], i, 1), i + 1);
}
