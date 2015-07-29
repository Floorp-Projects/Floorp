// Test that the SavedFrame caching doesn't get messed up in the presence of
// cross-compartment calls.

const funcSource = "function call(f) { return f(); }";

const g1 = newGlobal();
const g2 = newGlobal();

g1.eval(funcSource);
g2.eval(funcSource);
eval(funcSource);

function doSaveStack() {
  return saveStack();
}

const captureStacksAcrossCompartmens = () =>
  [this, g1, g2].map(g => g.call(doSaveStack));

(function f0() {
  const stacks = [];

  for (var i = 0; i < 2; i++)
    stacks.push(...captureStacksAcrossCompartmens());

  const [s1, s2, s3, s4, s5, s6] = stacks;

  assertEq(s1 != s2, true);
  assertEq(s2 != s3, true);
  assertEq(s3 != s1, true);

  assertEq(s1, s4);
  assertEq(s2, s5);
  assertEq(s3, s6);
}());
