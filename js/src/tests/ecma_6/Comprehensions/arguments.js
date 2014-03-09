

function values(g) {
  return [for (x of g) x];
}

function argumentsTest() {
  return values((for (i of [0,1,2]) arguments[i]));
}

assertDeepEq(argumentsTest('a', 'b', 'c'), ['a', 'b', 'c']);

if (typeof reportCompare === "function")
  reportCompare(true, true);
