var BUGNUMBER = 992617;
var summary = "Implement arguments[@@iterator].";

print(BUGNUMBER + ": " + summary);

// MappedArgumentsObject
let mapped = [
  function(a, b, c) {
    assertEq(Symbol.iterator in arguments, true);
    delete arguments[Symbol.iterator];
    assertEq(Symbol.iterator in arguments, false);
  },
  function(a, b, c) {
    delete arguments[Symbol.iterator];
    assertEq(Symbol.iterator in arguments, false);
  },
  function(a, b, c) {
    arguments[Symbol.iterator] = 10;
    delete arguments[Symbol.iterator];
    assertEq(Symbol.iterator in arguments, false);
  },
  function(a, b, c) {
    Object.defineProperty(arguments, Symbol.iterator, {
      value: 10, writable: true, enumerable: true, configurable: true
    });
    delete arguments[Symbol.iterator];
    assertEq(Symbol.iterator in arguments, false);
  },
  function(a, b, c) {
    assertEq(arguments[Symbol.iterator], Array.prototype[Symbol.iterator]);
  },
  function(a, b, c) {
    assertEq(arguments[Symbol.iterator].name, "values");
  },
  function(a, b, c) {
    var desc = Object.getOwnPropertyDescriptor(arguments, Symbol.iterator);
    assertEq("value" in desc, true);
    assertEq(desc.value, Array.prototype[Symbol.iterator]);
    assertEq(desc.writable, true);
    assertEq(desc.enumerable, false);
    assertEq(desc.configurable, true);
  },
  function(a, b, c) {
    var iter = arguments[Symbol.iterator]();
    assertDeepEq(iter.next(), { value: 10, done: false });
    assertDeepEq(iter.next(), { value: 20, done: false });
    assertDeepEq(iter.next(), { value: 30, done: false });
    assertDeepEq(iter.next(), { value: undefined, done: true });
  },
  function(a, b, c) {
    assertDeepEq([...arguments], [10, 20, 30]);
  },
  function(a, b, c) {
    b = 40;
    assertDeepEq([...arguments], [10, 40, 30]);
  },
  function(a, b, c) {
    arguments.length = 4;
    assertDeepEq([...arguments], [10, 20, 30, undefined]);
  },
  function(a, b, c) {
    arguments[5] = 50;
    assertDeepEq([...arguments], [10, 20, 30]);
  },
  function(a, b, c) {
    arguments[Symbol.iterator] = function*() {
      yield 40;
      yield 50;
      yield 60;
    };
    assertDeepEq([...arguments], [40, 50, 60]);
  },
];
for (let f of mapped) {
  f(10, 20, 30);
}

var g1 = newGlobal();
assertEq(g1.eval(`
function f(a, b, c) {
  return arguments[Symbol.iterator].name;
}
f(1, 2, 3);
`), "values");

// UnmappedArgumentsObject
let unmapped = [
  function([a], b, c) {
    assertEq(Symbol.iterator in arguments, true);
    delete arguments[Symbol.iterator];
    assertEq(Symbol.iterator in arguments, false);
  },
  function([a], b, c) {
    delete arguments[Symbol.iterator];
    assertEq(Symbol.iterator in arguments, false);
  },
  function([a], b, c) {
    arguments[Symbol.iterator] = 10;
    delete arguments[Symbol.iterator];
    assertEq(Symbol.iterator in arguments, false);
  },
  function([a], b, c) {
    Object.defineProperty(arguments, Symbol.iterator, {
      value: 10, writable: true, enumerable: true, configurable: true
    });
    delete arguments[Symbol.iterator];
    assertEq(Symbol.iterator in arguments, false);
  },
  function([a], b, c) {
    assertEq(arguments[Symbol.iterator], Array.prototype[Symbol.iterator]);
  },
  function([a], b, c) {
    assertEq(arguments[Symbol.iterator].name, "values");
  },
  function([a], b, c) {
    var desc = Object.getOwnPropertyDescriptor(arguments, Symbol.iterator);
    assertEq("value" in desc, true);
    assertEq(desc.value, Array.prototype[Symbol.iterator]);
    assertEq(desc.writable, true);
    assertEq(desc.enumerable, false);
    assertEq(desc.configurable, true);
  },
  function([a], b, c) {
    var iter = arguments[Symbol.iterator]();
    assertDeepEq(iter.next(), { value: [10], done: false });
    assertDeepEq(iter.next(), { value: 20, done: false });
    assertDeepEq(iter.next(), { value: 30, done: false });
    assertDeepEq(iter.next(), { value: undefined, done: true });
  },
  function([a], b, c) {
    assertDeepEq([...arguments], [[10], 20, 30]);
  },
  function([a], b, c) {
    b = 40;
    assertDeepEq([...arguments], [[10], 20, 30]);
  },
  function([a], b, c) {
    arguments.length = 4;
    assertDeepEq([...arguments], [[10], 20, 30, undefined]);
  },
  function([a], b, c) {
    arguments[5] = 50;
    assertDeepEq([...arguments], [[10], 20, 30]);
  },
  function([a], b, c) {
    arguments[Symbol.iterator] = function*() {
      yield 40;
      yield 50;
      yield 60;
    };
    assertDeepEq([...arguments], [40, 50, 60]);
  },
];
for (let f of unmapped) {
  f([10], 20, 30);
}

var g2 = newGlobal();
assertEq(g2.eval(`
function f([a], b, c) {
  return arguments[Symbol.iterator].name;
}
f([1], 2, 3);
`), "values");

if (typeof reportCompare === "function")
    reportCompare(true, true);
