let obj = {
  a: 1,
  b: 1,
  c: 1,
  [Symbol.for("foo")]: 1,
};

function test(id) {
  return Object.hasOwn(obj, id);
}

let testKeys = [
  ["a", true],
  ["b", true],
  ["c", true],
  ["d", false],
  ["e", false],
  ["f", false],
  ["g", false],
  ["h", false],
];

with({});
for (var i = 0; i < 1000; i++) {
  let [key, has] = testKeys[i % testKeys.length];
  test(key);
}

testKeys.push([Symbol.for("foo"), true]);

for (var i = 0; i < 1000; i++) {
  let [key, has] = testKeys[i % testKeys.length];
  assertEq(test(key), has);
}
