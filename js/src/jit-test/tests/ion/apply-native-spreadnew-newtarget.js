load(libdir + "array-compare.js");

const xs = [
  // Zero arguments.
  [],

  // Single argument.
  [1],

  // Few arguments. Even number of arguments.
  [1, 2],

  // Few arguments. Odd number of arguments.
  [1, 2, 3],

  // Many arguments. Even number of arguments.
  [1, 2, 3, 4, 5, 6, 7, 8, 9, 0],

  // Many arguments. Odd number of arguments.
  [1, 2, 3, 4, 5, 6, 7, 8, 9],
];

class ArrayWithExplicitConstructor extends Array {
  constructor(...args) {
    super(...args);
  }
}

class ArrayWithImplicitConstructor extends Array {
  constructor(...args) {
    super(...args);
  }
}

function f(...x) {
  return new ArrayWithExplicitConstructor(...x);
}

function g(...x) {
  return new ArrayWithImplicitConstructor(...x);
}

// Don't inline |f| and |g| into the top-level script.
with ({}) ;

for (let i = 0; i < 400; ++i) {
  let x = xs[i % xs.length];

  // NB: Array(1) creates the array `[,]`.
  let expected = x.length !== 1 ? x : [,];

  let result = f.apply(null, x);
  assertEq(arraysEqual(result, expected), true);
  assertEq(Object.getPrototypeOf(result), ArrayWithExplicitConstructor.prototype);
}

for (let i = 0; i < 400; ++i) {
  let x = xs[i % xs.length];

  // NB: Array(1) creates the array `[,]`.
  let expected = x.length !== 1 ? x : [,];

  let result = g.apply(null, x);
  assertEq(arraysEqual(result, expected), true);
  assertEq(Object.getPrototypeOf(result), ArrayWithImplicitConstructor.prototype);
}
