function f(array, method, args) {
  var called = false;
  var keys = [];
  for (var key in array) {
    keys.push(key);
    if (!called) {
      called = true;
      Reflect.apply(method, array, args);
    }
  }
  return keys;
}

assertEqArray(f([1, /* hole */, 3], Array.prototype.unshift, [0]), ["0"]);
assertEqArray(f([1, /* hole */, 3], Array.prototype.splice, [0, 0, 0]), ["0"]);

if (typeof reportCompare === "function")
  reportCompare(true, true);
