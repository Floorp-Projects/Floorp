// Test that we can save stacks with self-hosted function frames in them.

const map = (function () {
  return [3].map(n => saveStack()).pop();
}());

assertEq(map.parent.functionDisplayName, "map");
assertEq(map.parent.source, "self-hosted");

const reduce = (function () {
  return [3].reduce(() => saveStack(), 3);
}());

assertEq(reduce.parent.functionDisplayName, "reduce");
assertEq(reduce.parent.source, "self-hosted");

const forEach = (function () {
  try {
    [3].forEach(n => { throw saveStack() });
  } catch (s) {
    return s;
  }
}());

assertEq(forEach.parent.functionDisplayName, "forEach");
assertEq(forEach.parent.source, "self-hosted");
