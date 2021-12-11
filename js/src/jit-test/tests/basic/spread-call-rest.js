// bug 1235092
// Optimize spread call with rest parameter.

load(libdir + "asserts.js");

function makeArray(...args) {
  return args;
}

// Optimizable Case.
function test(...args) {
  return makeArray(...args);
}
assertDeepEq(test(1, 2, 3), [1, 2, 3]);

// Not optimizable case 1: the array has hole.
function hole1(...args) {
  args[4] = 5;
  return makeArray(...args);
}
assertDeepEq(hole1(1, 2, 3), [1, 2, 3, undefined, 5]);

function hole2(...args) {
  args.length = 5;
  return makeArray(...args);
}
assertDeepEq(hole2(1, 2, 3), [1, 2, 3, undefined, undefined]);

function hole3(...args) {
  delete args[1];
  return makeArray(...args);
}
assertDeepEq(hole3(1, 2, 3), [1, undefined, 3]);

// Not optimizable case 2: array[@@iterator] is modified.
function modifiedIterator(...args) {
  args[Symbol.iterator] = function*() {
    for (let i = 0; i < this.length; i++)
      yield this[i] * 10;
  };
  return makeArray(...args);
}
assertDeepEq(modifiedIterator(1, 2, 3), [10, 20, 30]);

// Not optimizable case 3: the array's prototype is modified.
function modifiedProto(...args) {
  args.__proto__ = {
    __proto__: Array.prototype,
    *[Symbol.iterator]() {
      for (let i = 0; i < this.length; i++)
        yield this[i] * 10;
    }
  };
  return makeArray(...args);
}
assertDeepEq(modifiedProto(1, 2, 3), [10, 20, 30]);

// Not optimizable case 4: Array.prototype[@@iterator] is modified.
let ArrayValues = Array.prototype[Symbol.iterator];
Array.prototype[Symbol.iterator] = function*() {
  for (let i = 0; i < this.length; i++)
    yield this[i] * 10;
};
assertDeepEq(test(1, 2, 3), [10, 20, 30]);
Array.prototype[Symbol.iterator] = ArrayValues;

// Not optimizable case 5: %ArrayIteratorPrototype%.next is modified.
let ArrayIteratorPrototype = Object.getPrototypeOf(Array.prototype[Symbol.iterator]());
let i = 1;
ArrayIteratorPrototype.next = function() {
  return { done: i % 4 == 0, value: 10 * i++ };
};
assertDeepEq(test(1, 2, 3), [10, 20, 30]);
