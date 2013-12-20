/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

let { seq, iterate, filter, map, reductions, reduce, count,
      isEmpty, every, isEvery, some, take, takeWhile, drop,
      dropWhile, concat, first, rest, nth, last, dropLast,
      distinct, remove, mapcat, fromEnumerator, string,
      object, pairs, keys, values, each
    } = require("sdk/util/sequence");

const boom = () => { throw new Error("Boom!"); };
const broken = seq(function*() {
  yield 1;
  throw new Error("Boom!");
});

exports["test seq"] = assert => {
  let xs = seq(function*() {
    yield 1;
    yield 2;
    yield 3;
  });

  assert.deepEqual([...seq(null)], [], "seq of null is empty");
  assert.deepEqual([...seq(void(0))], [], "seq of void is empty");
  assert.deepEqual([...xs], [1, 2, 3], "seq of 1 2 3");
  assert.deepEqual([...seq(xs)], [1, 2, 3], "seq of seq is seq");

  assert.deepEqual([...seq([])], [], "seq of emtpy array is empty");
  assert.deepEqual([...seq([1])], [1], "seq of lonly array is single element");
  assert.deepEqual([...seq([1, 2, 3])], [1, 2, 3], "seq of array is it's elements");

  assert.deepEqual([...seq("")], [], "seq of emtpy string is empty");
  assert.deepEqual([...seq("o")], ["o"], "seq of char is single char seq");
  assert.deepEqual([...seq("hello")], ["h", "e", "l", "l", "o"],
                   "seq of string are chars");

  assert.deepEqual([...seq(new Set())], [], "seq of emtpy set is empty");
  assert.deepEqual([...seq(new Set([1]))], [1], "seq of lonely set is single");
  assert.deepEqual([...seq(new Set([1, 2, 3]))], [1, 2, 3], "seq of lonely set is single");

  assert.deepEqual([...seq(new Map())], [], "seq of emtpy map is empty");
  assert.deepEqual([...seq(new Map([[1, 2]]))], [[1, 2]], "seq single mapping is that mapping");
  assert.deepEqual([...seq(new Map([[1, 2], [3, 4], [5, 6]]))],
                   [[1, 2], [3, 4], [5, 6]],
                   "seq of map is key value mappings");

  [function(){}, 1, /foo/, true].forEach(x => {
    assert.throws(() => seq(x), "Type is not seq-able");
  });

  assert.throws(() => [...broken],
                /Boom/,
                "broken sequence errors propagate");
};

exports["test seq casting"] = assert => {
  const xs = seq(function*() { yield 1; yield 2; yield 3; });
  const ys = seq(function*() { yield 1; });
  const zs = seq(function*() {});
  const kvs = seq(function*() { yield ["a", 1]; yield ["b", 2]; });
  const kv = seq(function*() { yield ["a", 1]; });

  assert.deepEqual([...xs], [1, 2, 3], "cast to array");
  assert.deepEqual([...ys], [1], "cast to of one element");
  assert.deepEqual([...zs], [], "cast empty array");

  assert.deepEqual(string(...xs), "123", "cast to string");
  assert.deepEqual(string(...ys), "1", "cast to char");
  assert.deepEqual(string(...zs), "", "cast to empty string");

  assert.deepEqual(new Set(xs), new Set([1, 2, 3]),
                   "cast to set of items");
  assert.deepEqual(new Set(ys), new Set([1]),
                   "cast to set of one item");
  assert.deepEqual(new Set(zs), new Set(),
                   "cast to set of one item");

  assert.deepEqual(new Map(kvs), new Map([["a", 1], ["b", 2]]),
                   "cast to map");
  assert.deepEqual(new Map(kv), new Map([["a", 1]]),
                   "cast to single mapping");
  assert.deepEqual(new Map(zs), new Map(),
                   "cast to empty map");

  assert.deepEqual(object(...kvs), {a: 1, b: 2},
                  "cast to object");
  assert.deepEqual(object(...kv), {a: 1},
                  "cast to single pair");
  assert.deepEqual(object(...zs), {},
                  "cast to empty object");
};

exports["test pairs"] = assert => {
  assert.deepEqual([...pairs(null)], [], "pairs on null is empty");
  assert.deepEqual([...pairs(void(0))], [], "pairs on void is empty");
  assert.deepEqual([...pairs({})], [], "empty sequence");
  assert.deepEqual([...pairs({a: 1})], [["a", 1]], "single pair");
  assert.deepEqual([...pairs({a: 1, b: 2, c: 3})].sort(),
                   [["a", 1], ["b", 2], ["c", 3]],
                   "creates pairs");
  let items = [];
  for (let [key, value] of pairs({a: 1, b: 2, c: 3}))
    items.push([key, value]);

  assert.deepEqual(items.sort(),
                   [["a", 1], ["b", 2], ["c", 3]],
                   "for of works on pairs");


  assert.deepEqual([...pairs([])], [], "pairs on empty array is empty");
  assert.deepEqual([...pairs([1])], [[0, 1]], "pairs on array is [index, element]");
  assert.deepEqual([...pairs([1, 2, 3])],
                   [[0, 1], [1, 2], [2, 3]],
                   "for arrays it pair of [index, element]");

  assert.deepEqual([...pairs("")], [], "pairs on empty string is empty");
  assert.deepEqual([...pairs("a")], [[0, "a"]], "pairs on char is [0, char]");
  assert.deepEqual([...pairs("hello")],
                   [[0, "h"], [1, "e"], [2, "l"], [3, "l"], [4, "o"]],
                   "for strings it's pair of [index, char]");

  assert.deepEqual([...pairs(new Map())],
                   [],
                   "pairs on empty map is empty");
  assert.deepEqual([...pairs(new Map([[1, 3]]))],
                   [[1, 3]],
                   "pairs on single mapping single mapping");
  assert.deepEqual([...pairs(new Map([[1, 2], [3, 4]]))],
                   [[1, 2], [3, 4]],
                   "pairs on map returs key vaule pairs");

  assert.throws(() => pairs(new Set()),
                "can't pair set");

  assert.throws(() => pairs(4),
                "can't pair number");

  assert.throws(() => pairs(true),
                "can't pair boolean");
};

exports["test keys"] = assert => {
  assert.deepEqual([...keys(null)], [], "keys on null is empty");
  assert.deepEqual([...keys(void(0))], [], "keys on void is empty");
  assert.deepEqual([...keys({})], [], "empty sequence");
  assert.deepEqual([...keys({a: 1})], ["a"], "single key");
  assert.deepEqual([...keys({a: 1, b: 2, c: 3})].sort(),
                   ["a", "b", "c"],
                   "all keys");

  let items = [];
  for (let key of keys({a: 1, b: 2, c: 3}))
    items.push(key);

  assert.deepEqual(items.sort(),
                   ["a", "b", "c"],
                   "for of works on keys");


  assert.deepEqual([...keys([])], [], "keys on empty array is empty");
  assert.deepEqual([...keys([1])], [0], "keys on array is indexes");
  assert.deepEqual([...keys([1, 2, 3])],
                   [0, 1, 2],
                   "keys on arrays returns indexes");

  assert.deepEqual([...keys("")], [], "keys on empty string is empty");
  assert.deepEqual([...keys("a")], [0], "keys on char is 0");
  assert.deepEqual([...keys("hello")],
                   [0, 1, 2, 3, 4],
                   "keys on strings is char indexes");

  assert.deepEqual([...keys(new Map())],
                   [],
                   "keys on empty map is empty");
  assert.deepEqual([...keys(new Map([[1, 3]]))],
                   [1],
                   "keys on single mapping single mapping is single key");
  assert.deepEqual([...keys(new Map([[1, 2], [3, 4]]))],
                   [1, 3],
                   "keys on map is keys from map");

  assert.throws(() => keys(new Set()),
                "can't keys set");

  assert.throws(() => keys(4),
                "can't keys number");

  assert.throws(() => keys(true),
                "can't keys boolean");
};

exports["test values"] = assert => {
  assert.deepEqual([...values({})], [], "empty sequence");
  assert.deepEqual([...values({a: 1})], [1], "single value");
  assert.deepEqual([...values({a: 1, b: 2, c: 3})].sort(),
                   [1, 2, 3],
                   "all values");

  let items = [];
  for (let value of values({a: 1, b: 2, c: 3}))
    items.push(value);

  assert.deepEqual(items.sort(),
                   [1, 2, 3],
                   "for of works on values");

  assert.deepEqual([...values([])], [], "values on empty array is empty");
  assert.deepEqual([...values([1])], [1], "values on array elements");
  assert.deepEqual([...values([1, 2, 3])],
                   [1, 2, 3],
                   "values on arrays returns elements");

  assert.deepEqual([...values("")], [], "values on empty string is empty");
  assert.deepEqual([...values("a")], ["a"], "values on char is char");
  assert.deepEqual([...values("hello")],
                   ["h", "e", "l", "l", "o"],
                   "values on strings is chars");

  assert.deepEqual([...values(new Map())],
                   [],
                   "values on empty map is empty");
  assert.deepEqual([...values(new Map([[1, 3]]))],
                   [3],
                   "keys on single mapping single mapping is single key");
  assert.deepEqual([...values(new Map([[1, 2], [3, 4]]))],
                   [2, 4],
                   "values on map is values from map");

  assert.deepEqual([...values(new Set())], [], "values on empty set is empty");
  assert.deepEqual([...values(new Set([1]))], [1], "values on set is it's items");
  assert.deepEqual([...values(new Set([1, 2, 3]))],
                   [1, 2, 3],
                   "values on set is it's items");


  assert.throws(() => values(4),
                "can't values number");

  assert.throws(() => values(true),
                "can't values boolean");
};

exports["test fromEnumerator"] = assert => {
  const { Cc, Ci } = require("chrome");
  const { enumerateObservers,
          addObserver,
          removeObserver } = Cc["@mozilla.org/observer-service;1"].
                               getService(Ci.nsIObserverService);


  const topic = "sec:" + Math.random().toString(32).substr(2);
  const [a, b, c] = [{wrappedJSObject: {}},
                     {wrappedJSObject: {}},
                     {wrappedJSObject: {}}];
  const unwrap = x => x.wrappedJSObject;

  [a, b, c].forEach(x => addObserver(x, topic, false));

  const xs = fromEnumerator(() => enumerateObservers(topic));
  const ys = map(unwrap, xs);

  assert.deepEqual([...ys], [a, b, c].map(unwrap),
                   "all observers are there");

  removeObserver(b, topic);

  assert.deepEqual([...ys], [a, c].map(unwrap),
                   "b was removed");

  removeObserver(a, topic);

  assert.deepEqual([...ys], [c].map(unwrap),
                   "a was removed");

  removeObserver(c, topic);

  assert.deepEqual([...ys], [],
                   "c was removed, now empty");

  addObserver(a, topic, false);

  assert.deepEqual([...ys], [a].map(unwrap),
                   "a was added");

  removeObserver(a, topic);

  assert.deepEqual([...ys], [].map(unwrap),
                   "a was removed, now empty");

};

exports["test filter"] = assert => {
  const isOdd = x => x % 2;
  const odds = seq(function*() { yield 1; yield 3; yield 5; });
  const evens = seq(function*() { yield 2; yield 4; yield 6; });
  const mixed = seq(function*() {
    yield 1;
    yield 2;
    yield 3;
    yield 4;
  });

  assert.deepEqual([...filter(isOdd, mixed)], [1, 3],
                   "filtered odds");
  assert.deepEqual([...filter(isOdd, odds)], [1, 3, 5],
                   "kept all");
  assert.deepEqual([...filter(isOdd, evens)], [],
                   "kept none");


  let xs = filter(boom, mixed);
  assert.throws(() => [...xs], /Boom/, "errors propagate");

  assert.throws(() => [...filter(isOdd, broken)], /Boom/,
                "sequence errors propagate");
};

exports["test filter array"] = assert => {
  let isOdd = x => x % 2;
  let xs = filter(isOdd, [1, 2, 3, 4]);
  let ys = filter(isOdd, [1, 3, 5]);
  let zs = filter(isOdd, [2, 4, 6]);

  assert.deepEqual([...xs], [1, 3], "filteres odds");
  assert.deepEqual([...ys], [1, 3, 5], "kept all");
  assert.deepEqual([...zs], [], "kept none");
  assert.ok(!Array.isArray(xs));
};

exports["test filter set"] = assert => {
  let isOdd = x => x % 2;
  let xs = filter(isOdd, new Set([1, 2, 3, 4]));
  let ys = filter(isOdd, new Set([1, 3, 5]));
  let zs = filter(isOdd, new Set([2, 4, 6]));

  assert.deepEqual([...xs], [1, 3], "filteres odds");
  assert.deepEqual([...ys], [1, 3, 5], "kept all");
  assert.deepEqual([...zs], [], "kept none");
};

exports["test filter string"] = assert => {
  let isUpperCase = x => x.toUpperCase() === x;
  let xs = filter(isUpperCase, "aBcDe");
  let ys = filter(isUpperCase, "ABC");
  let zs = filter(isUpperCase, "abcd");

  assert.deepEqual([...xs], ["B", "D"], "filteres odds");
  assert.deepEqual([...ys], ["A", "B", "C"], "kept all");
  assert.deepEqual([...zs], [], "kept none");
};

exports["test filter lazy"] = assert => {
  const x = 1;
  let y = 2;

  const xy = seq(function*() { yield x; yield y; });
  const isOdd = x => x % 2;
  const actual = filter(isOdd, xy);

  assert.deepEqual([...actual], [1], "only one odd number");
  y = 3;
  assert.deepEqual([...actual], [1, 3], "filter is lazy");
};

exports["test filter non sequences"] = assert => {
  const False = _ => false;
  assert.throws(() => [...filter(False, 1)],
                "can't iterate number");
  assert.throws(() => [...filter(False, {a: 1, b:2})],
                "can't iterate object");
};

exports["test map"] = assert => {
  let inc = x => x + 1;
  let xs = seq(function*() { yield 1; yield 2; yield 3; });
  let ys = map(inc, xs);

  assert.deepEqual([...ys], [2, 3, 4], "incremented each item");

  assert.deepEqual([...map(inc, null)], [], "mapping null is empty");
  assert.deepEqual([...map(inc, void(0))], [], "mapping void is empty");
  assert.deepEqual([...map(inc, new Set([1, 2, 3]))], [2, 3, 4], "maps set items");
};

exports["test map two inputs"] = assert => {
  let sum = (x, y) => x + y;
  let xs = seq(function*() { yield 1; yield 2; yield 3; });
  let ys = seq(function*() { yield 4; yield 5; yield 6; });

  let zs = map(sum, xs, ys);

  assert.deepEqual([...zs], [5, 7, 9], "summed numbers");
};

exports["test map diff sized inputs"] = assert => {
  let sum = (x, y) => x + y;
  let xs = seq(function*() { yield 1; yield 2; yield 3; });
  let ys = seq(function*() { yield 4; yield 5; yield 6; yield 7; yield 8; });

  let zs = map(sum, xs, ys);

  assert.deepEqual([...zs], [5, 7, 9], "summed numbers");
  assert.deepEqual([...map(sum, ys, xs)], [5, 7, 9],
                   "index of exhasting input is irrelevant");
};

exports["test map multi"] = assert => {
  let sum = (x, y, z, w) => x + y + z + w;
  let xs = seq(function*() { yield 1; yield 2; yield 3; yield 4; });
  let ys = seq(function*() { yield 4; yield 5; yield 6; yield 7; yield 8; });
  let zs = seq(function*() { yield 10; yield 11; yield 12; });
  let ws = seq(function*() { yield 0; yield 20; yield 40; yield 60; });

  let actual = map(sum, xs, ys, zs, ws);

  assert.deepEqual([...actual], [15, 38, 61], "summed numbers");
};

exports["test map errors"] = assert => {
  assert.deepEqual([...map(boom, [])], [],
                   "won't throw if empty");

  const xs = map(boom, [1, 2, 4]);

  assert.throws(() => [...xs], /Boom/, "propagates errors");

  assert.throws(() => [...map(x => x, broken)], /Boom/,
                "sequence errors propagate");
};

exports["test reductions"] = assert => {
  let sum = (...xs) => xs.reduce((x, y) => x + y, 0);

  assert.deepEqual([...reductions(sum, [1, 1, 1, 1])],
                   [1, 2, 3, 4],
                   "works with arrays");
  assert.deepEqual([...reductions(sum, 5, [1, 1, 1, 1])],
                   [5, 6, 7, 8, 9],
                   "array with initial");

  assert.deepEqual([...reductions(sum, seq(function*() {
    yield 1;
    yield 2;
    yield 3;
  }))],
  [1, 3, 6],
  "works with sequences");

  assert.deepEqual([...reductions(sum, 10, seq(function*() {
    yield 1;
    yield 2;
    yield 3;
  }))],
  [10, 11, 13, 16],
  "works with sequences");

  assert.deepEqual([...reductions(sum, [])], [0],
                   "invokes accumulator with no args");

  assert.throws(() => [...reductions(boom, 1, [1])],
                /Boom/,
                "arg errors errors propagate");
  assert.throws(() => [...reductions(sum, 1, broken)],
                /Boom/,
                "sequence errors propagate");
};

exports["test reduce"] = assert => {
 let sum = (...xs) => xs.reduce((x, y) => x + y, 0);

  assert.deepEqual(reduce(sum, [1, 2, 3, 4, 5]),
                   15,
                   "works with arrays");

  assert.deepEqual(reduce(sum, seq(function*() {
                     yield 1;
                     yield 2;
                     yield 3;
                   })),
                   6,
                   "works with sequences");

  assert.deepEqual(reduce(sum, 10, [1, 2, 3, 4, 5]),
                   25,
                   "works with array & initial");

  assert.deepEqual(reduce(sum, 5, seq(function*() {
                     yield 1;
                     yield 2;
                     yield 3;
                   })),
                   11,
                   "works with sequences & initial");

  assert.deepEqual(reduce(sum, []), 0, "reduce with no args");
  assert.deepEqual(reduce(sum, "a", []), "a", "reduce with initial");
  assert.deepEqual(reduce(sum, 1, [1]), 2, "reduce with single & initial");

  assert.throws(() => [...reduce(boom, 1, [1])],
                /Boom/,
                "arg errors errors propagate");
  assert.throws(() => [...reduce(sum, 1, broken)],
                /Boom/,
                "sequence errors propagate");
};

exports["test each"] = assert => {
  const collect = xs => {
    let result = [];
    each((...etc) => result.push(...etc), xs);
    return result;
  };

  assert.deepEqual(collect(null), [], "each ignores null");
  assert.deepEqual(collect(void(0)), [], "each ignores void");

  assert.deepEqual(collect([]), [], "each ignores empty");
  assert.deepEqual(collect([1]), [1], "each works on single item arrays");
  assert.deepEqual(collect([1, 2, 3, 4, 5]),
                   [1, 2, 3, 4, 5],
                   "works with arrays");

  assert.deepEqual(collect(seq(function*() {
                     yield 1;
                     yield 2;
                     yield 3;
                   })),
                   [1, 2, 3],
                   "works with sequences");

  assert.deepEqual(collect(""), [], "ignores empty strings");
  assert.deepEqual(collect("a"), ["a"], "works on chars");
  assert.deepEqual(collect("hello"), ["h", "e", "l", "l", "o"],
                   "works on strings");

  assert.deepEqual(collect(new Set()), [], "ignores empty sets");
  assert.deepEqual(collect(new Set(["a"])), ["a"],
                   "works on single item sets");
  assert.deepEqual(collect(new Set([1, 2, 3])), [1, 2, 3],
                   "works on muti item tests");

  assert.deepEqual(collect(new Map()), [], "ignores empty maps");
  assert.deepEqual(collect(new Map([["a", 1]])), [["a", 1]],
                   "works on single mapping maps");
  assert.deepEqual(collect(new Map([[1, 2], [3, 4], [5, 6]])),
                   [[1, 2], [3, 4], [5, 6]],
                   "works on muti mapping maps");

  assert.throws(() => collect({}), "objects arn't supported");
  assert.throws(() => collect(1), "numbers arn't supported");
  assert.throws(() => collect(true), "booleas arn't supported");
};

exports["test count"] = assert => {
  assert.equal(count(null), 0, "null counts to 0");
  assert.equal(count(), 0, "undefined counts to 0");
  assert.equal(count([]), 0, "empty array");
  assert.equal(count([1, 2, 3]), 3, "non-empty array");
  assert.equal(count(""), 0, "empty string");
  assert.equal(count("hello"), 5, "non-empty string");
  assert.equal(count(new Map()), 0, "empty map");
  assert.equal(count(new Map([[1, 2], [2, 3]])), 2, "non-empty map");
  assert.equal(count(new Set()), 0, "empty set");
  assert.equal(count(new Set([1, 2, 3, 4])), 4, "non-empty set");
  assert.equal(count(seq(function*() {})), 0, "empty sequence");
  assert.equal(count(seq(function*() { yield 1; yield 2; })), 2,
               "non-empty sequence");

  assert.throws(() => count(broken),
                /Boom/,
                "sequence errors propagate");
};

exports["test isEmpty"] = assert => {
  assert.equal(isEmpty(null), true, "null is empty");
  assert.equal(isEmpty(), true, "undefined is empty");
  assert.equal(isEmpty([]), true, "array is array");
  assert.equal(isEmpty([1, 2, 3]), false, "array isn't empty");
  assert.equal(isEmpty(""), true, "string is empty");
  assert.equal(isEmpty("hello"), false, "non-empty string");
  assert.equal(isEmpty(new Map()), true, "empty map");
  assert.equal(isEmpty(new Map([[1, 2], [2, 3]])), false, "non-empty map");
  assert.equal(isEmpty(new Set()), true, "empty set");
  assert.equal(isEmpty(new Set([1, 2, 3, 4])), false , "non-empty set");
  assert.equal(isEmpty(seq(function*() {})), true, "empty sequence");
  assert.equal(isEmpty(seq(function*() { yield 1; yield 2; })), false,
               "non-empty sequence");

  assert.equal(isEmpty(broken), false, "hasn't reached error");
};

exports["test isEvery"] = assert => {
  let isOdd = x => x % 2;
  let isTrue = x => x === true;
  let isFalse = x => x === false;

  assert.equal(isEvery(isOdd, seq(function*() {
    yield 1;
    yield 3;
    yield 5;
  })), true, "all are odds");

  assert.equal(isEvery(isOdd, seq(function*() {
    yield 1;
    yield 2;
    yield 3;
  })), false, "contains even");

  assert.equal(isEvery(isTrue, seq(function*() {})), true, "true if empty");
  assert.equal(isEvery(isFalse, seq(function*() {})), true, "true if empty");

  assert.equal(isEvery(isTrue, null), true, "true for null");
  assert.equal(isEvery(isTrue, undefined), true, "true for undefined");

  assert.throws(() => isEvery(boom, [1, 2]),
                /Boom/,
                "arg errors errors propagate");
  assert.throws(() => isEvery(x => true, broken),
                /Boom/,
                "sequence errors propagate");

  assert.equal(isEvery(x => false, broken), false,
              "hasn't reached error");
};

exports["test some"] = assert => {
  let isOdd = x => x % 2;
  let isTrue = x => x === true;
  let isFalse = x => x === false;

  assert.equal(some(isOdd, seq(function*() {
    yield 2;
    yield 4;
    yield 6;
  })), null, "all are even");

  assert.equal(some(isOdd, seq(function*() {
    yield 2;
    yield 3;
    yield 4;
  })), true, "contains odd");

  assert.equal(some(isTrue, seq(function*() {})), null,
               "null if empty")
  assert.equal(some(isFalse, seq(function*() {})), null,
               "null if empty")

  assert.equal(some(isTrue, null), null, "null for null");
  assert.equal(some(isTrue, undefined), null, "null for undefined");

  assert.throws(() => some(boom, [1, 2]),
                /Boom/,
                "arg errors errors propagate");
  assert.throws(() => some(x => false, broken),
                /Boom/,
                "sequence errors propagate");

  assert.equal(some(x => true, broken), true,
              "hasn't reached error");
};

exports["test take"] = assert => {
  let xs = seq(function*() {
    yield 1;
    yield 2;
    yield 3;
    yield 4;
    yield 5;
    yield 6;
  });

  assert.deepEqual([...take(3, xs)], [1, 2, 3], "took 3 items");
  assert.deepEqual([...take(3, [1, 2, 3, 4, 5])], [1, 2, 3],
                   "took 3 from array");

  let ys = seq(function*() { yield 1; yield 2; });
  assert.deepEqual([...take(3, ys)], [1, 2], "takes at max n");
  assert.deepEqual([...take(3, [1, 2])], [1, 2],
                   "takes at max n from arary");

  let empty = seq(function*() {});
  assert.deepEqual([...take(5, empty)], [], "nothing to take");

  assert.throws(() => [...take(3, broken)],
                /Boom/,
                "sequence errors propagate");

  assert.deepEqual([...take(1, broken)], [1],
                   "hasn't reached error");
};

exports["test iterate"] = assert => {
  let inc = x => x + 1;
  let nums = iterate(inc, 0);

  assert.deepEqual([...take(5, nums)], [0, 1, 2, 3, 4], "took 5");
  assert.deepEqual([...take(10, nums)], [0, 1, 2, 3, 4, 5, 6, 7, 8, 9], "took 10");

  let xs = iterate(x => x * 3, 2);
  assert.deepEqual([...take(4, xs)], [2, 6, 18, 54], "took 4");

  assert.throws(() => [...iterate(boom, 0)],
                /Boom/,
                "function exceptions propagate");
};

exports["test takeWhile"] = assert => {
  let isNegative = x => x < 0;
  let xs = seq(function*() {
    yield -2;
    yield -1;
    yield 0;
    yield 1;
    yield 2;
    yield 3;
  });

  assert.deepEqual([...takeWhile(isNegative, xs)], [-2, -1],
                   "took until 0");

  let ys = seq(function*() {});
  assert.deepEqual([...takeWhile(isNegative, ys)], [],
                   "took none");

  let zs = seq(function*() {
    yield 0;
    yield 1;
    yield 2;
    yield 3;
  });

  assert.deepEqual([...takeWhile(isNegative, zs)], [],
                   "took none");

  assert.throws(() => [...takeWhile(boom, zs)],
                /Boom/,
                "function errors errors propagate");
  assert.throws(() => [...takeWhile(x => true, broken)],
                /Boom/,
                "sequence errors propagate");

  assert.deepEqual([...takeWhile(x => false, broken)],
                   [],
                   "hasn't reached error");
};

exports["test drop"] = assert => {
  let testDrop = xs => {
    assert.deepEqual([...drop(2, xs)],
                     [3, 4],
                     "dropped two elements");

    assert.deepEqual([...drop(1, xs)],
                     [2, 3, 4],
                     "dropped one");

    assert.deepEqual([...drop(0, xs)],
                     [1, 2, 3, 4],
                     "dropped 0");

    assert.deepEqual([...drop(-2, xs)],
                     [1, 2, 3, 4],
                     "dropped 0 on negative `n`");

    assert.deepEqual([...drop(5, xs)],
                     [],
                     "dropped all items");
  };

  testDrop([1, 2, 3, 4]);
  testDrop(seq(function*() {
    yield 1;
    yield 2;
    yield 3;
    yield 4;
  }));

  assert.throws(() => [...drop(1, broken)],
                /Boom/,
                "sequence errors propagate");
};


exports["test dropWhile"] = assert => {
  let isNegative = x => x < 0;
  let True = _ => true;
  let False = _ => false;

  let test = xs => {
    assert.deepEqual([...dropWhile(isNegative, xs)],
                     [0, 1, 2],
                     "dropped negative");

    assert.deepEqual([...dropWhile(True, xs)],
                     [],
                     "drop all");

    assert.deepEqual([...dropWhile(False, xs)],
                     [-2, -1, 0, 1, 2],
                     "keep all");
  };

  test([-2, -1, 0, 1, 2]);
  test(seq(function*() {
    yield -2;
    yield -1;
    yield 0;
    yield 1;
    yield 2;
  }));

  assert.throws(() => [...dropWhile(boom, [1, 2, 3])],
                /Boom/,
                "function errors errors propagate");
  assert.throws(() => [...dropWhile(x => true, broken)],
                /Boom/,
                "sequence errors propagate");
};


exports["test concat"] = assert => {
  let test = (a, b, c, d) => {
    assert.deepEqual([...concat()],
                     [],
                     "nothing to concat");
    assert.deepEqual([...concat(a)],
                     [1, 2, 3],
                     "concat with nothing returns same as first");
    assert.deepEqual([...concat(a, b)],
                     [1, 2, 3, 4, 5],
                     "concat items from both");
    assert.deepEqual([...concat(a, b, a)],
                     [1, 2, 3, 4, 5, 1, 2, 3],
                     "concat itself");
    assert.deepEqual([...concat(c)],
                     [],
                     "concat of empty is empty");
    assert.deepEqual([...concat(a, c)],
                     [1, 2, 3],
                     "concat with empty");
    assert.deepEqual([...concat(c, c, c)],
                     [],
                     "concat of empties is empty");
    assert.deepEqual([...concat(c, b)],
                     [4, 5],
                     "empty can be in front");
    assert.deepEqual([...concat(d)],
                     [7],
                     "concat singular");
    assert.deepEqual([...concat(d, d)],
                     [7, 7],
                     "concat singulars");

    assert.deepEqual([...concat(a, a, b, c, d, c, d, d)],
                     [1, 2, 3, 1, 2, 3, 4, 5, 7, 7, 7],
                     "many concats");

    let ab = concat(a, b);
    let abcd = concat(ab, concat(c, d));
    let cdabcd = concat(c, d, abcd);

    assert.deepEqual([...cdabcd],
                     [7, 1, 2, 3, 4, 5, 7],
                     "nested concats");
  };

  test([1, 2, 3],
       [4, 5],
       [],
       [7]);

  test(seq(function*() { yield 1; yield 2; yield 3; }),
       seq(function*() { yield 4; yield 5; }),
       seq(function*() { }),
       seq(function*() { yield 7; }));

  assert.throws(() => [...concat(broken, [1, 2, 3])],
                /Boom/,
                "function errors errors propagate");
};


exports["test first"] = assert => {
  let test = (xs, empty) => {
    assert.equal(first(xs), 1, "returns first");
    assert.equal(first(empty), null, "returns null empty");
  };

  test("1234", "");
  test([1, 2, 3], []);
  test([1, 2, 3], null);
  test([1, 2, 3], undefined);
  test(seq(function*() { yield 1; yield 2; yield 3; }),
       seq(function*() { }));
  assert.equal(first(broken), 1, "did not reached error");
};

exports["test rest"] = assert => {
  let test = (xs, x, nil) => {
    assert.deepEqual([...rest(xs)], ["b", "c"],
                     "rest items");
    assert.deepEqual([...rest(x)], [],
                     "empty when singular");
    assert.deepEqual([...rest(nil)], [],
                     "empty when empty");
  };

  test("abc", "a", "");
  test(["a", "b", "c"], ["d"], []);
  test(seq(function*() { yield "a"; yield "b"; yield "c"; }),
       seq(function*() { yield "d"; }),
       seq(function*() {}));
  test(["a", "b", "c"], ["d"], null);
  test(["a", "b", "c"], ["d"], undefined);

  assert.throws(() => [...rest(broken)],
                /Boom/,
                "sequence errors propagate");
};


exports["test nth"] = assert => {
  let notFound = {};
  let test = xs => {
    assert.equal(nth(xs, 0), "h", "first");
    assert.equal(nth(xs, 1), "e", "second");
    assert.equal(nth(xs, 5), void(0), "out of bound");
    assert.equal(nth(xs, 5, notFound), notFound, "out of bound");
    assert.equal(nth(xs, -1), void(0), "out of bound");
    assert.equal(nth(xs, -1, notFound), notFound, "out of bound");
    assert.equal(nth(xs, 4), "o", "5th");
  };

  let testEmpty = xs => {
    assert.equal(nth(xs, 0), void(0), "no first in empty");
    assert.equal(nth(xs, 5), void(0), "no 5th in empty");
    assert.equal(nth(xs, 0, notFound), notFound, "notFound on out of bound");
  };

  test("hello");
  test(["h", "e", "l", "l", "o"]);
  test(seq(function*() {
    yield "h";
    yield "e";
    yield "l";
    yield "l";
    yield "o";
  }));
  testEmpty(null);
  testEmpty(undefined);
  testEmpty([]);
  testEmpty("");
  testEmpty(seq(function*() {}));


  assert.throws(() => nth(broken, 1),
                /Boom/,
                "sequence errors propagate");
  assert.equal(nth(broken, 0), 1, "have not reached error");
};


exports["test last"] = assert => {
  assert.equal(last(null), null, "no last in null");
  assert.equal(last(void(0)), null, "no last in undefined");
  assert.equal(last([]), null, "no last in []");
  assert.equal(last(""), null, "no last in ''");
  assert.equal(last(seq(function*() { })), null, "no last in empty");

  assert.equal(last("hello"), "o", "last from string");
  assert.equal(last([1, 2, 3]), 3, "last from array");
  assert.equal(last([1]), 1, "last from singular");
  assert.equal(last(seq(function*() {
    yield 1;
    yield 2;
    yield 3;
  })), 3, "last from sequence");

  assert.throws(() => last(broken),
                /Boom/,
                "sequence errors propagate");
};


exports["test dropLast"] = assert => {
  let test = xs => {
    assert.deepEqual([...dropLast(xs)],
                     [1, 2, 3, 4],
                     "dropped last");
    assert.deepEqual([...dropLast(0, xs)],
                     [1, 2, 3, 4, 5],
                     "dropped none on 0");
    assert.deepEqual([...dropLast(-3, xs)],
                     [1, 2, 3, 4, 5],
                     "drop none on negative");
    assert.deepEqual([...dropLast(3, xs)],
                     [1, 2],
                     "dropped given number");
    assert.deepEqual([...dropLast(5, xs)],
                     [],
                     "dropped all");
  };

  let testEmpty = xs => {
    assert.deepEqual([...dropLast(xs)],
                     [],
                     "nothing to drop");
    assert.deepEqual([...dropLast(0, xs)],
                     [],
                     "dropped none on 0");
    assert.deepEqual([...dropLast(-3, xs)],
                     [],
                     "drop none on negative");
    assert.deepEqual([...dropLast(3, xs)],
                     [],
                     "nothing to drop");
  };

  test([1, 2, 3, 4, 5]);
  test(seq(function*() {
    yield 1;
    yield 2;
    yield 3;
    yield 4;
    yield 5;
  }));
  testEmpty([]);
  testEmpty("");
  testEmpty(seq(function*() {}));

  assert.throws(() => [...dropLast(broken)],
                /Boom/,
                "sequence errors propagate");
};


exports["test distinct"] = assert => {
  let test = (xs, message) => {
    assert.deepEqual([...distinct(xs)],
                     [1, 2, 3, 4, 5],
                     message);
  };

  test([1, 2, 1, 3, 1, 4, 1, 5], "works with arrays");
  test(seq(function*() {
    yield 1;
    yield 2;
    yield 1;
    yield 3;
    yield 1;
    yield 4;
    yield 1;
    yield 5;
  }), "works with sequences");
  test(new Set([1, 2, 1, 3, 1, 4, 1, 5]),
       "works with sets");
  test(seq(function*() {
    yield 1;
    yield 2;
    yield 2;
    yield 2;
    yield 1;
    yield 3;
    yield 1;
    yield 4;
    yield 4;
    yield 4;
    yield 1;
    yield 5;
  }), "works with multiple repeatitions");
  test([1, 2, 3, 4, 5], "work with distinct arrays");
  test(seq(function*() {
    yield 1;
    yield 2;
    yield 3;
    yield 4;
    yield 5;
  }), "works with distinct seqs");
};


exports["test remove"] = assert => {
  let isPositive = x => x > 0;
  let test = xs => {
    assert.deepEqual([...remove(isPositive, xs)],
                     [-2, -1, 0],
                     "removed positives");
  };

  test([1, -2, 2, -1, 3, 7, 0]);
  test(seq(function*() {
    yield 1;
    yield -2;
    yield 2;
    yield -1;
    yield 3;
    yield 7;
    yield 0;
  }));

  assert.throws(() => [...distinct(broken)],
                /Boom/,
                "sequence errors propagate");
};


exports["test mapcat"] = assert => {
  let upto = n => seq(function* () {
    let index = 0;
    while (index < n) {
      yield index;
      index = index + 1;
    }
  });

  assert.deepEqual([...mapcat(upto, [1, 2, 3, 4])],
                   [0, 0, 1, 0, 1, 2, 0, 1, 2, 3],
                   "expands given sequence");

  assert.deepEqual([...mapcat(upto, [0, 1, 2, 0])],
                   [0, 0, 1],
                   "expands given sequence");

  assert.deepEqual([...mapcat(upto, [0, 0, 0])],
                   [],
                   "expands given sequence");

  assert.deepEqual([...mapcat(upto, [])],
                   [],
                   "nothing to expand");

  assert.deepEqual([...mapcat(upto, null)],
                   [],
                   "nothing to expand");

  assert.deepEqual([...mapcat(upto, void(0))],
                   [],
                   "nothing to expand");

  let xs = seq(function*() {
    yield 0;
    yield 1;
    yield 0;
    yield 2;
    yield 0;
  });

  assert.deepEqual([...mapcat(upto, xs)],
                   [0, 0, 1],
                   "expands given sequence");

  assert.throws(() => [...mapcat(boom, xs)],
                /Boom/,
                "function errors errors propagate");
  assert.throws(() => [...mapcat(upto, broken)],
                /Boom/,
                "sequence errors propagate");
};

require("sdk/test").run(exports);
