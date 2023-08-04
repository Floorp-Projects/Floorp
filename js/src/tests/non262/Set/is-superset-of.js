// |reftest| shell-option(--enable-new-set-methods) skip-if(!Set.prototype.isSupersetOf)

assertEq(typeof Set.prototype.isSupersetOf, "function");
assertDeepEq(Object.getOwnPropertyDescriptor(Set.prototype.isSupersetOf, "length"), {
  value: 1, writable: false, enumerable: false, configurable: true,
});
assertDeepEq(Object.getOwnPropertyDescriptor(Set.prototype.isSupersetOf, "name"), {
  value: "isSupersetOf", writable: false, enumerable: false, configurable: true,
});

const emptySet = new Set();
const emptySetLike = new SetLike();
const emptyMap = new Map();

function asMap(values) {
  return new Map(values.map(v => [v, v]));
}

// Empty set is a superset of the empty set.
assertEq(emptySet.isSupersetOf(emptySet), true);
assertEq(emptySet.isSupersetOf(emptySetLike), true);
assertEq(emptySet.isSupersetOf(emptyMap), true);

// Test native Set, Set-like, and Map objects.
for (let values of [
  [], [1], [1, 2], [1, true, null, {}],
]) {
  // Superset operation with an empty set.
  assertEq(new Set(values).isSupersetOf(emptySet), true);
  assertEq(new Set(values).isSupersetOf(emptySetLike), true);
  assertEq(new Set(values).isSupersetOf(emptyMap), true);
  assertEq(emptySet.isSupersetOf(new Set(values)), values.length === 0);
  assertEq(emptySet.isSupersetOf(new SetLike(values)), values.length === 0);
  assertEq(emptySet.isSupersetOf(asMap(values)), values.length === 0);

  // Two sets with the exact same values.
  assertEq(new Set(values).isSupersetOf(new Set(values)), true);
  assertEq(new Set(values).isSupersetOf(new SetLike(values)), true);
  assertEq(new Set(values).isSupersetOf(asMap(values)), true);

  // Superset operation of the same set object.
  let set = new Set(values);
  assertEq(set.isSupersetOf(set), true);
}

// Check property accesses are in the correct order.
{
  let log = [];

  let sizeValue = 0;

  let {proxy: keysIter} = LoggingProxy({
    next() {
      log.push("next()");
      return {done: true};
    }
  }, log);

  let {proxy: setLike} = LoggingProxy({
    size: {
      valueOf() {
        log.push("valueOf()");
        return sizeValue;
      }
    },
    has() {
      throw new Error("Unexpected call to |has| method");
    },
    keys() {
      log.push("keys()");
      return keysIter;
    },
  }, log);

  assertEq(emptySet.isSupersetOf(setLike), true);

  assertEqArray(log, [
    "[[get]]", "size",
    "valueOf()",
    "[[get]]", "has",
    "[[get]]", "keys",
    "keys()",
    "[[get]]", "next",
    "next()",
  ]);

  // |keys| isn't called when the this-value has fewer elements.
  sizeValue = 1;

  log.length = 0;
  assertEq(emptySet.isSupersetOf(setLike), false);

  assertEqArray(log, [
    "[[get]]", "size",
    "valueOf()",
    "[[get]]", "has",
    "[[get]]", "keys",
  ]);
}

// Check input validation.
{
  let log = [];

  const nonCallable = {};
  let sizeValue = 0;

  let {proxy: keysIter} = LoggingProxy({
    next: nonCallable,
  }, log);

  let {proxy: setLike, obj: setLikeObj} = LoggingProxy({
    size: {
      valueOf() {
        log.push("valueOf()");
        return sizeValue;
      }
    },
    has() {
      throw new Error("Unexpected call to |has| method");
    },
    keys() {
      log.push("keys()");
      return keysIter;
    },
  }, log);

  log.length = 0;
  assertThrowsInstanceOf(() => emptySet.isSupersetOf(setLike), TypeError);

  assertEqArray(log, [
    "[[get]]", "size",
    "valueOf()",
    "[[get]]", "has",
    "[[get]]", "keys",
    "keys()",
    "[[get]]", "next",
  ]);

  // Change |keys| to return a non-object value.
  setLikeObj.keys = () => 123;

  log.length = 0;
  assertThrowsInstanceOf(() => emptySet.isSupersetOf(setLike), TypeError);

  assertEqArray(log, [
    "[[get]]", "size",
    "valueOf()",
    "[[get]]", "has",
    "[[get]]", "keys",
  ]);

  // Change |keys| to a non-callable value.
  setLikeObj.keys = nonCallable;

  log.length = 0;
  assertThrowsInstanceOf(() => emptySet.isSupersetOf(setLike), TypeError);

  assertEqArray(log, [
    "[[get]]", "size",
    "valueOf()",
    "[[get]]", "has",
    "[[get]]", "keys",
  ]);

  // Change |has| to a non-callable value.
  setLikeObj.has = nonCallable;

  log.length = 0;
  assertThrowsInstanceOf(() => emptySet.isSupersetOf(setLike), TypeError);

  assertEqArray(log, [
    "[[get]]", "size",
    "valueOf()",
    "[[get]]", "has",
  ]);

  // Change |size| to NaN.
  sizeValue = NaN;

  log.length = 0;
  assertThrowsInstanceOf(() => emptySet.isSupersetOf(setLike), TypeError);

  assertEqArray(log, [
    "[[get]]", "size",
    "valueOf()",
  ]);

  // Change |size| to undefined.
  sizeValue = undefined;

  log.length = 0;
  assertThrowsInstanceOf(() => emptySet.isSupersetOf(setLike), TypeError);

  assertEqArray(log, [
    "[[get]]", "size",
    "valueOf()",
  ]);
}

// Doesn't accept Array as an input.
assertThrowsInstanceOf(() => emptySet.isSupersetOf([]), TypeError);

// Works with Set subclasses.
{
  class MySet extends Set {}

  let myEmptySet = new MySet;

  assertEq(emptySet.isSupersetOf(myEmptySet), true);
  assertEq(myEmptySet.isSupersetOf(myEmptySet), true);
  assertEq(myEmptySet.isSupersetOf(emptySet), true);
}

// Doesn't access any properties on the this-value.
{
  let log = [];

  let {proxy: setProto} = LoggingProxy(Set.prototype, log);

  let set = new Set([]);
  Object.setPrototypeOf(set, setProto);

  assertEq(Set.prototype.isSupersetOf.call(set, emptySet), true);

  assertEqArray(log, []);
}

// Throws a TypeError when the this-value isn't a Set object.
for (let thisValue of [
  null, undefined, true, "", {}, new Map, new Proxy(new Set, {}),
]) {
  assertThrowsInstanceOf(() => Set.prototype.isSupersetOf.call(thisValue, emptySet), TypeError);
}

// Doesn't call |has| nor |keys| when this-value has fewer elements.
{
  let set = new Set([1, 2, 3]);

  for (let size of [100, Infinity]) {
    let setLike = {
      size,
      has() {
        throw new Error("Unexpected call to |has| method");
      },
      keys() {
        throw new Error("Unexpected call to |keys| method");
      },
    };

    assertEq(set.isSupersetOf(setLike), false);
  }
}

// Test when this-value is modified during iteration.
{
  let set = new Set([]);

  let keys = [1, 2, 3];

  let keysIter = {
    next() {
      if (keys.length) {
        let value = keys.shift();
        return {
          done: false,
          get value() {
            assertEq(set.has(value), false);
            set.add(value);
            return value;
          }
        };
      }
      return {
        done: true,
        get value() {
          throw new Error("Unexpected call to |value| getter");
        }
      };
    }
  };

  let setLike = {
    size: 0,
    has() {
      throw new Error("Unexpected call to |has| method");
    },
    keys() {
      return keysIter;
    },
  };

  assertEq(set.isSupersetOf(setLike), true);
  assertSetContainsExactOrderedItems(set, [1, 2, 3]);
}

// IteratorClose is called for early returns.
{
  let log = [];

  let keysIter = {
    next() {
      log.push("next");
      return {done: false, value: 1};
    },
    return() {
      log.push("return");
      return {
        get value() { throw new Error("Unexpected call to |value| getter"); },
        get done() { throw new Error("Unexpected call to |done| getter"); },
      };
    }
  };

  let setLike = {
    size: 0,
    has() {
      throw new Error("Unexpected call to |has| method");
    },
    keys() {
      return keysIter;
    },
  };

  assertEq(new Set([2, 3, 4]).isSupersetOf(setLike), false);

  assertEqArray(log, ["next", "return"]);
}

// IteratorClose isn't called for non-early returns.
{
  let setLike = new SetLike([1, 2, 3]);

  assertEq(new Set([1, 2, 3]).isSupersetOf(setLike), true);
}

// Tests which modify any built-ins should appear last, because modifications may disable
// optimised code paths.

// Doesn't call the built-in |Set.prototype.{has, keys, size}| functions.
const SetPrototypeHas = Object.getOwnPropertyDescriptor(Set.prototype, "has");
const SetPrototypeKeys = Object.getOwnPropertyDescriptor(Set.prototype, "keys");
const SetPrototypeSize = Object.getOwnPropertyDescriptor(Set.prototype, "size");

delete Set.prototype.has;
delete Set.prototype.keys;
delete Set.prototype.size;

try {
  let set = new Set([1, 2, 3]);
  let other = new SetLike([1, 2, 3]);
  assertEq(set.isSupersetOf(other), true);
} finally {
  Object.defineProperty(Set.prototype, "has", SetPrototypeHas);
  Object.defineProperty(Set.prototype, "keys", SetPrototypeKeys);
  Object.defineProperty(Set.prototype, "size", SetPrototypeSize);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
