// Test that we can save stacks with getter and setter function frames.

function assertStackLengthEq(stack, expectedLength) {
  let actual = 0;
  while (stack) {
    actual++;
    stack = stack.parent;
  }
  assertEq(actual, expectedLength);
}

const get = { get s() { return saveStack(); } }.s;
assertStackLengthEq(get, 2);

let set;
try {
  ({
    set s(v) {
      throw saveStack();
    }
  }).s = 1;
} catch (s) {
  set = s;
}
assertStackLengthEq(set, 2);
