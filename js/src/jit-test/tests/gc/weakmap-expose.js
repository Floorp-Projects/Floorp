// Test that WeakMap.get() doesn't return a gray GC thing.

function checkNotGray(value) {
  // Assigning a gray GC thing to an object propery will assert.
  let test = {};
  test.property = value;
}

// 1. Make a black WeakMap with a gray key and gray value.

gczeal(0);

let key = {};
let value = {};

let map = new WeakMap();
map.set(key, value);

let gray = grayRoot();
gray.key = key;

addMarkObservers([map, key, value]);

gray = null;
key = null;
value = null;

gc();

let marks = getMarks();
assertEq(marks[0], "black");
assertEq(marks[1], "gray");
assertEq(marks[2], "gray");

// 2. Get our key back, which will expose it and mark it black.

key = nondeterministicGetWeakMapKeys(map)[0];
checkNotGray(key);

// 3. Look up the value in the map and check it's not gray.

value = map.get(key);
checkNotGray(value);
