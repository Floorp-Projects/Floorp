/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental"
};

// Disclamer:
// In this module we'll have some common argument / variable names
// to hint their type or behavior.
//
// - `f` stands for "function" that is intended to be side effect
//   free.
// - `p` stands for "predicate" that is function which returns logical
//   true or false and is intended to be side effect free.
// - `x` / `y` single item of the sequence.
// - `xs` / `ys` sequence of `x` / `y` items where `x` / `y` signifies
//    type of the items in sequence, so sequence is not of the same item.
// - `_` used for argument(s) or variable(s) who's values are ignored.

const { complement, flip, identity } = require("../lang/functional");
const { isArray, isArguments, isMap, isSet, isGenerator,
        isString, isBoolean, isNumber } = require("../lang/type");

const Sequence = function Sequence(iterator) {
  if (!isGenerator(iterator)) {
    throw TypeError("Expected generator argument");
  }

  this[Symbol.iterator] = iterator;
};
exports.Sequence = Sequence;

const polymorphic = dispatch => x =>
  x === null ? dispatch.null(null) :
  x === void(0) ? dispatch.void(void(0)) :
  isArray(x) ? (dispatch.array || dispatch.indexed)(x) :
  isString(x) ? (dispatch.string || dispatch.indexed)(x) :
  isArguments(x) ? (dispatch.arguments || dispatch.indexed)(x) :
  isMap(x) ? dispatch.map(x) :
  isSet(x) ? dispatch.set(x) :
  isNumber(x) ? dispatch.number(x) :
  isBoolean(x) ? dispatch.boolean(x) :
  dispatch.default(x);

const nogen = function*() {};
const empty = () => new Sequence(nogen);
exports.empty = empty;

const seq = polymorphic({
  null: empty,
  void: empty,
  array: identity,
  string: identity,
  arguments: identity,
  map: identity,
  set: identity,
  default: x => x instanceof Sequence ? x : new Sequence(x)
});
exports.seq = seq;

// Function to cast seq to string.
const string = (...etc) => "".concat(...etc);
exports.string = string;

// Function for casting seq to plain object.
const object = (...pairs) => {
  let result = {};
  for (let [key, value] of pairs)
    result[key] = value;

  return result;
};
exports.object = object;

// Takes `getEnumerator` function that returns `nsISimpleEnumerator`
// and creates lazy sequence of it's items. Note that function does
// not take `nsISimpleEnumerator` itslef because that would allow
// single iteration, which would not be consistent with rest of the
// lazy sequences.
const fromEnumerator = getEnumerator => seq(function* () {
  const enumerator = getEnumerator();
  while (enumerator.hasMoreElements())
   yield enumerator.getNext();
});
exports.fromEnumerator = fromEnumerator;

// Takes `object` and returns lazy sequence of own `[key, value]`
// pairs (does not include inherited and non enumerable keys).
const pairs = polymorphic({
  null: empty,
  void: empty,
  map: identity,
  indexed: indexed => seq(function* () {
    const count = indexed.length;
    let index = 0;
    while (index < count) {
      yield [index, indexed[index]];
      index = index + 1;
    }
  }),
  default: object => seq(function* () {
    for (let key of Object.keys(object))
      yield [key, object[key]];
  })
});
exports.pairs = pairs;

const names = polymorphic({
  null: empty,
  void: empty,
  default: object => seq(function*() {
    for (let name of Object.getOwnPropertyNames(object)) {
      yield name;
    }
  })
});
exports.names = names;

const symbols = polymorphic({
  null: empty,
  void: empty,
  default: object => seq(function* () {
    for (let symbol of Object.getOwnPropertySymbols(object)) {
      yield symbol;
    }
  })
});
exports.symbols = symbols;

const keys = polymorphic({
  null: empty,
  void: empty,
  indexed: indexed => seq(function* () {
    const count = indexed.length;
    let index = 0;
    while (index < count) {
      yield index;
      index = index + 1;
    }
  }),
  map: map => seq(function* () {
    for (let [key, _] of map)
      yield key;
  }),
  default: object => seq(function* () {
    for (let key of Object.keys(object))
      yield key;
  })
});
exports.keys = keys;


const values = polymorphic({
  null: empty,
  void: empty,
  set: identity,
  indexed: indexed => seq(function* () {
    const count = indexed.length;
    let index = 0;
    while (index < count) {
      yield indexed[index];
      index = index + 1;
    }
  }),
  map: map => seq(function* () {
    for (let [_, value] of map) yield value;
  }),
  default: object => seq(function* () {
    for (let key of Object.keys(object)) yield object[key];
  })
});
exports.values = values;



// Returns a lazy sequence of `x`, `f(x)`, `f(f(x))` etc.
// `f` must be free of side-effects. Note that returned
// sequence is infinite so it must be consumed partially.
//
// Implements clojure iterate:
// http://clojuredocs.org/clojure_core/clojure.core/iterate
const iterate = (f, x) => seq(function* () {
  let state = x;
  while (true) {
    yield state;
    state = f(state);
  }
});
exports.iterate = iterate;

// Returns a lazy sequence of the items in sequence for which `p(item)`
// returns `true`. `p` must be free of side-effects.
//
// Implements clojure filter:
// http://clojuredocs.org/clojure_core/clojure.core/filter
const filter = (p, sequence) => seq(function* () {
  if (sequence !== null && sequence !== void(0)) {
    for (let item of sequence) {
      if (p(item))
        yield item;
    }
  }
});
exports.filter = filter;

// Returns a lazy sequence consisting of the result of applying `f` to the
// set of first items of each sequence, followed by applying f to the set
// of second items in each sequence, until any one of the sequences is
// exhausted. Any remaining items in other sequences are ignored. Function
// `f` should accept number-of-sequences arguments.
//
// Implements clojure map:
// http://clojuredocs.org/clojure_core/clojure.core/map
const map = (f, ...sequences) => seq(function* () {
  const count = sequences.length;
  // Optimize a single sequence case
  if (count === 1) {
    let [sequence] = sequences;
    if (sequence !== null && sequence !== void(0)) {
      for (let item of sequence)
        yield f(item);
    }
  }
  else {
    // define args array that will be recycled on each
    // step to aggregate arguments to be passed to `f`.
    let args = [];
    // define inputs to contain started generators.
    let inputs = [];

    let index = 0;
    while (index < count) {
      inputs[index] = sequences[index][Symbol.iterator]();
      index = index + 1;
    }

    // Run loop yielding of applying `f` to the set of
    // items at each step until one of the `inputs` is
    // exhausted.
    let done = false;
    while (!done) {
      let index = 0;
      let value = void(0);
      while (index < count && !done) {
        ({ done, value }) = inputs[index].next();

        // If input is not exhausted yet store value in args.
        if (!done) {
          args[index] = value;
          index = index + 1;
        }
      }

      // If none of the inputs is exhasted yet, `args` contain items
      // from each input so we yield application of `f` over them.
      if (!done)
        yield f(...args);
    }
  }
});
exports.map = map;

// Returns a lazy sequence of the intermediate values of the reduction (as
// per reduce) of sequence by `f`, starting with `initial` value if provided.
//
// Implements clojure reductions:
// http://clojuredocs.org/clojure_core/clojure.core/reductions
const reductions = (...params) => {
  const count = params.length;
  let hasInitial = false;
  let f, initial, source;
  if (count === 2) {
    ([f, source]) = params;
  }
  else if (count === 3) {
    ([f, initial, source]) = params;
    hasInitial = true;
  }
  else {
    throw Error("Invoked with wrong number of arguments: " + count);
  }

  const sequence = seq(source);

  return seq(function* () {
    let started = hasInitial;
    let result = void(0);

    // If initial is present yield it.
    if (hasInitial)
      yield (result = initial);

    // For each item of the sequence accumulate new result.
    for (let item of sequence) {
      // If nothing has being yield yet set result to first
      // item and yield it.
      if (!started) {
        started = true;
        yield (result = item);
      }
      // Otherwise accumulate new result and yield it.
      else {
        yield (result = f(result, item));
      }
    }

    // If nothing has being yield yet it's empty sequence and no
    // `initial` was provided in which case we need to yield `f()`.
    if (!started)
      yield f();
  });
};
exports.reductions = reductions;

// `f` should be a function of 2 arguments. If `initial` is not supplied,
// returns the result of applying `f` to the first 2 items in sequence, then
// applying `f` to that result and the 3rd item, etc. If sequence contains no
// items, `f` must accept no arguments as well, and reduce returns the
// result of calling f with no arguments. If sequence has only 1 item, it
// is returned and `f` is not called. If `initial` is supplied, returns the
// result of applying `f` to `initial` and the first item in  sequence, then
// applying `f` to that result and the 2nd item, etc. If sequence contains no
// items, returns `initial` and `f` is not called.
//
// Implements clojure reduce:
// http://clojuredocs.org/clojure_core/clojure.core/reduce
const reduce = (...args) => {
  const xs = reductions(...args);
  let x;
  for (x of xs) void(0);
  return x;
};
exports.reduce = reduce;

const each = (f, sequence) => {
  for (let x of seq(sequence)) void(f(x));
};
exports.each = each;


const inc = x => x + 1;
// Returns the number of items in the sequence. `count(null)` && `count()`
// returns `0`. Also works on strings, arrays, Maps & Sets.

// Implements clojure count:
// http://clojuredocs.org/clojure_core/clojure.core/count
const count = polymorphic({
  null: _ => 0,
  void: _ => 0,
  indexed: indexed => indexed.length,
  map: map => map.size,
  set: set => set.size,
  default: xs => reduce(inc, 0, xs)
});
exports.count = count;

// Returns `true` if sequence has no items.

// Implements clojure empty?:
// http://clojuredocs.org/clojure_core/clojure.core/empty_q
const isEmpty = sequence => {
  // Treat `null` and `undefined` as empty sequences.
  if (sequence === null || sequence === void(0))
    return true;

  // If contains any item non empty so return `false`.
  for (let _ of sequence)
    return false;

  // If has not returned yet, there was nothing to iterate
  // so it's empty.
  return true;
};
exports.isEmpty = isEmpty;

const and = (a, b) => a && b;

// Returns true if `p(x)` is logical `true` for every `x` in sequence, else
// `false`.
//
// Implements clojure every?:
// http://clojuredocs.org/clojure_core/clojure.core/every_q
const isEvery = (p, sequence) => {
  if (sequence !== null && sequence !== void(0)) {
    for (let item of sequence) {
      if (!p(item))
        return false;
    }
  }
  return true;
};
exports.isEvery = isEvery;

// Returns the first logical true value of (p x) for any x in sequence,
// else `null`.
//
// Implements clojure some:
// http://clojuredocs.org/clojure_core/clojure.core/some
const some = (p, sequence) => {
  if (sequence !== null && sequence !== void(0)) {
    for (let item of sequence) {
      if (p(item))
        return true;
    }
  }
  return null;
};
exports.some = some;

// Returns a lazy sequence of the first `n` items in sequence, or all items if
// there are fewer than `n`.
//
// Implements clojure take:
// http://clojuredocs.org/clojure_core/clojure.core/take
const take = (n, sequence) => n <= 0 ? empty() : seq(function* () {
  let count = n;
  for (let item of sequence) {
    yield item;
    count = count - 1;
    if (count === 0) break;
  }
});
exports.take = take;

// Returns a lazy sequence of successive items from sequence while
// `p(item)` returns `true`. `p` must be free of side-effects.
//
// Implements clojure take-while:
// http://clojuredocs.org/clojure_core/clojure.core/take-while
const takeWhile = (p, sequence) => seq(function* () {
  for (let item of sequence) {
    if (!p(item))
      break;

    yield item;
  }
});
exports.takeWhile = takeWhile;

// Returns a lazy sequence of all but the first `n` items in
// sequence.
//
// Implements clojure drop:
// http://clojuredocs.org/clojure_core/clojure.core/drop
const drop = (n, sequence) => seq(function* () {
  if (sequence !== null && sequence !== void(0)) {
    let count = n;
    for (let item of sequence) {
      if (count > 0)
        count = count - 1;
      else
        yield item;
    }
  }
});
exports.drop = drop;

// Returns a lazy sequence of the items in sequence starting from the
// first item for which `p(item)` returns falsy value.
//
// Implements clojure drop-while:
// http://clojuredocs.org/clojure_core/clojure.core/drop-while
const dropWhile = (p, sequence) => seq(function* () {
  let keep = false;
  for (let item of sequence) {
    keep = keep || !p(item);
    if (keep) yield item;
  }
});
exports.dropWhile = dropWhile;

// Returns a lazy sequence representing the concatenation of the
// suplied sequences.
//
// Implements clojure conact:
// http://clojuredocs.org/clojure_core/clojure.core/concat
const concat = (...sequences) => seq(function* () {
  for (let sequence of sequences)
    for (let item of sequence)
      yield item;
});
exports.concat = concat;

// Returns the first item in the sequence.
//
// Implements clojure first:
// http://clojuredocs.org/clojure_core/clojure.core/first
const first = sequence => {
  if (sequence !== null && sequence !== void(0)) {
    for (let item of sequence)
      return item;
  }
  return null;
};
exports.first = first;

// Returns a possibly empty sequence of the items after the first.
//
// Implements clojure rest:
// http://clojuredocs.org/clojure_core/clojure.core/rest
const rest = sequence => drop(1, sequence);
exports.rest = rest;

// Returns the value at the index. Returns `notFound` or `undefined`
// if index is out of bounds.
const nth = (xs, n, notFound) => {
  if (n >= 0) {
    if (isArray(xs) || isArguments(xs) || isString(xs)) {
      return n < xs.length ? xs[n] : notFound;
    }
    else if (xs !== null && xs !== void(0)) {
      let count = n;
      for (let x of xs) {
        if (count <= 0)
          return x;

        count = count - 1;
      }
    }
  }
  return notFound;
};
exports.nth = nth;

// Return the last item in sequence, in linear time.
// If `sequence` is an array or string or arguments
// returns in constant time.
// Implements clojure last:
// http://clojuredocs.org/clojure_core/clojure.core/last
const last = polymorphic({
  null: _ => null,
  void: _ => null,
  indexed: indexed => indexed[indexed.length - 1],
  map: xs => reduce((_, x) => x, xs),
  set: xs => reduce((_, x) => x, xs),
  default: xs => reduce((_, x) => x, xs)
});
exports.last = last;

// Return a lazy sequence of all but the last `n` (default 1) items
// from the give `xs`.
//
// Implements clojure drop-last:
// http://clojuredocs.org/clojure_core/clojure.core/drop-last
const dropLast = flip((xs, n=1) => seq(function* () {
  let ys = [];
  for (let x of xs) {
    ys.push(x);
    if (ys.length > n)
      yield ys.shift();
  }
}));
exports.dropLast = dropLast;

// Returns a lazy sequence of the elements of `xs` with duplicates
// removed
//
// Implements clojure distinct
// http://clojuredocs.org/clojure_core/clojure.core/distinct
const distinct = sequence => seq(function* () {
  let items = new Set();
  for (let item of sequence) {
    if (!items.has(item)) {
      items.add(item);
      yield item;
    }
  }
});
exports.distinct = distinct;

// Returns a lazy sequence of the items in `xs` for which
// `p(x)` returns false. `p` must be free of side-effects.
//
// Implements clojure remove
// http://clojuredocs.org/clojure_core/clojure.core/remove
const remove = (p, xs) => filter(complement(p), xs);
exports.remove = remove;

// Returns the result of applying concat to the result of
// `map(f, xs)`. Thus function `f` should return a sequence.
//
// Implements clojure mapcat
// http://clojuredocs.org/clojure_core/clojure.core/mapcat
const mapcat = (f, sequence) => seq(function* () {
  const sequences = map(f, sequence);
  for (let sequence of sequences)
    for (let item of sequence)
      yield item;
});
exports.mapcat = mapcat;
