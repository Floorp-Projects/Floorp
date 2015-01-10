/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { setTimeout } = require('sdk/timers');
const utils = require('sdk/lang/functional');
const { invoke, defer, partial, compose, memoize, once, is, isnt,
  delay, wrap, curry, chainable, field, query, isInstance, debounce, throttle
} = utils;
const { LoaderWithHookedConsole } = require('sdk/test/loader');

exports['test forwardApply'] = function(assert) {
  function sum(b, c) { return this.a + b + c; }
  assert.equal(invoke(sum, [2, 3], { a: 1 }), 6,
               'passed arguments and pseoude-variable are used');

  assert.equal(invoke(sum.bind({ a: 2 }), [2, 3], { a: 1 }), 7,
               'bounded `this` pseoudo variable is used');
};

exports['test deferred function'] = function(assert, done) {
  let nextTurn = false;
  function sum(b, c) {
    assert.ok(nextTurn, 'enqueued is called in next turn of event loop');
    assert.equal(this.a + b + c, 6,
                 'passed arguments an pseoude-variable are used');
    done();
  }

  let fixture = { a: 1, method: defer(sum) };
  fixture.method(2, 3);
  nextTurn = true;
};

exports['test partial function'] = function(assert) {
  function sum(b, c) { return this.a + b + c; }

  let foo = { a : 5 };

  foo.sum7 = partial(sum, 7);
  foo.sum8and4 = partial(sum, 8, 4);

  assert.equal(foo.sum7(2), 14, 'partial one arguments works');

  assert.equal(foo.sum8and4(), 17, 'partial both arguments works');
};

exports["test curry defined numeber of arguments"] = function(assert) {
  var sum = curry(function(a, b, c) {
    return a + b + c;
  });

  assert.equal(sum(2, 2, 1), 5, "sum(2, 2, 1) => 5");
  assert.equal(sum(2, 4)(1), 7, "sum(2, 4)(1) => 7");
  assert.equal(sum(2)(4, 2), 8, "sum(2)(4, 2) => 8");
  assert.equal(sum(2)(4)(3), 9, "sum(2)(4)(3) => 9");
};

exports['test compose'] = function(assert) {
  let greet = function(name) { return 'hi: ' + name; };
  let exclaim = function(sentence) { return sentence + '!'; };

  assert.equal(compose(exclaim, greet)('moe'), 'hi: moe!',
               'can compose a function that takes another');

  assert.equal(compose(greet, exclaim)('moe'), 'hi: moe!',
               'in this case, the functions are also commutative');

  let target = {
    name: 'Joe',
    greet: compose(function exclaim(sentence) {
      return sentence + '!';
    }, function(title) {
      return 'hi : ' + title + ' ' + this.name;
    })
  };

  assert.equal(target.greet('Mr'), 'hi : Mr Joe!',
               'this can be passed in');
  assert.equal(target.greet.call({ name: 'Alex' }, 'Dr'), 'hi : Dr Alex!',
               'this can be applied');

  let single = compose(function(value) {
    return value + ':suffix';
  });

  assert.equal(single('text'), 'text:suffix', 'works with single function');

  let identity = compose();
  assert.equal(identity('bla'), 'bla', 'works with zero functions');
};

exports['test wrap'] = function(assert) {
  let greet = function(name) { return 'hi: ' + name; };
  let backwards = wrap(greet, function(f, name) {
    return f(name) + ' ' + name.split('').reverse().join('');
  });

  assert.equal(backwards('moe'), 'hi: moe eom',
               'wrapped the saluation function');

  let inner = function () { return 'Hello '; };
  let target = {
    name: 'Matteo',
    hi: wrap(inner, function(f) { return f() + this.name; })
  };

  assert.equal(target.hi(), 'Hello Matteo', 'works with this');

  function noop() { }
  let wrapped = wrap(noop, function(f) {
    return Array.slice(arguments);
  });

  let actual = wrapped([ 'whats', 'your' ], 'vector', 'victor');
  assert.deepEqual(actual, [ noop, ['whats', 'your'], 'vector', 'victor' ],
                   'works with fancy stuff');
};

exports['test memoize'] = function(assert) {
  const fib = n => n < 2 ? n : fib(n - 1) + fib(n - 2);
  let fibnitro = memoize(fib);

  assert.equal(fib(10), 55,
        'a memoized version of fibonacci produces identical results');
  assert.equal(fibnitro(10), 55,
        'a memoized version of fibonacci produces identical results');

  function o(key, value) { return value; }
  let oo = memoize(o), v1 = {}, v2 = {};


  assert.equal(oo(1, v1), v1, 'returns value back');
  assert.equal(oo(1, v2), v1, 'memoized by a first argument');
  assert.equal(oo(2, v2), v2, 'returns back value if not memoized');
  assert.equal(oo(2), v2, 'memoized new value');
  assert.notEqual(oo(1), oo(2), 'values do not override');
  assert.equal(o(3, v2), oo(2, 3), 'returns same value as un-memoized');

  let get = memoize(function(attribute) { return this[attribute]; });
  let target = { name: 'Bob', get: get };

  assert.equal(target.get('name'), 'Bob', 'has correct `this`');
  assert.equal(target.get.call({ name: 'Jack' }, 'name'), 'Bob',
               'name is memoized');
  assert.equal(get('name'), 'Bob', 'once memoized can be called without this');
};

exports['test delay'] = function(assert, done) {
  let delayed = false;
  delay(function() {
    assert.ok(delayed, 'delayed the function');
    done();
  }, 1);
  delayed = true;
};

exports['test delay with this'] = function(assert, done) {
  let context = {};
  delay.call(context, function(name) {
    assert.equal(this, context, 'this was passed in');
    assert.equal(name, 'Tom', 'argument was passed in');
    done();
  }, 10, 'Tom');
};

exports['test once'] = function(assert) {
  let n = 0;
  let increment = once(function() { n ++; });

  increment();
  increment();

  assert.equal(n, 1, 'only incremented once');

  let target = {
    state: 0,
    update: once(function() {
      return this.state ++;
    })
  };

  target.update();
  target.update();

  assert.equal(target.state, 1, 'this was passed in and called only once');
};

exports['test once with argument'] = function(assert) {
  let n = 0;
  let increment = once(a => n++);

  increment();
  increment('foo');

  assert.equal(n, 1, 'only incremented once');

  increment();
  increment('foo');

  assert.equal(n, 1, 'only incremented once');
};

exports['test complement'] = assert => {
  let { complement } = require("sdk/lang/functional");

  let isOdd = x => Boolean(x % 2);

  assert.equal(isOdd(1), true);
  assert.equal(isOdd(2), false);

  let isEven = complement(isOdd);

  assert.equal(isEven(1), false);
  assert.equal(isEven(2), true);

  let foo = {};
  let isFoo = function() { return this === foo; };
  let insntFoo = complement(isFoo);

  assert.equal(insntFoo.call(foo), false);
  assert.equal(insntFoo.call({}), true);
};

exports['test constant'] = assert => {
  let { constant } = require("sdk/lang/functional");

  let one = constant(1);

  assert.equal(one(1), 1);
  assert.equal(one(2), 1);
};

exports['test apply'] = assert => {
  let { apply } = require("sdk/lang/functional");

  let dashify = (...args) => args.join("-");

  assert.equal(apply(dashify, 1, [2, 3]), "1-2-3");
  assert.equal(apply(dashify, "a"), "a");
  assert.equal(apply(dashify, ["a", "b"]), "a-b");
  assert.equal(apply(dashify, ["a", "b"], "c"), "a,b-c");
  assert.equal(apply(dashify, [1, 2], [3, 4]), "1,2-3-4");
};

exports['test flip'] = assert => {
  let { flip } = require("sdk/lang/functional");

  let append = (left, right) => left + " " + right;
  let prepend = flip(append);

  assert.equal(append("hello", "world"), "hello world");
  assert.equal(prepend("hello", "world"), "world hello");

  let wrap = function(left, right) {
    return left + " " + this + " " + right;
  };
  let invertWrap = flip(wrap);

  assert.equal(wrap.call("@", "hello", "world"), "hello @ world");
  assert.equal(invertWrap.call("@", "hello", "world"), "world @ hello");

  let reverse = flip((...args) => args);

  assert.deepEqual(reverse(1, 2, 3, 4), [4, 3, 2, 1]);
  assert.deepEqual(reverse(1), [1]);
  assert.deepEqual(reverse(), []);

  // currying still works
  let prependr = curry(prepend);

  assert.equal(prependr("hello", "world"), "world hello");
  assert.equal(prependr("hello")("world"), "world hello");
};

exports["test when"] = assert => {
  let { when } = require("sdk/lang/functional");

  let areNums = (...xs) => xs.every(x => typeof(x) === "number");

  let sum = when(areNums, (...xs) => xs.reduce((y, x) => x + y, 0));

  assert.equal(sum(1, 2, 3), 6);
  assert.equal(sum(1, 2, "3"), undefined);

  let multiply = when(areNums,
                      (...xs) => xs.reduce((y, x) => x * y, 1),
                      (...xs) => xs);

  assert.equal(multiply(2), 2);
  assert.equal(multiply(2, 3), 6);
  assert.deepEqual(multiply(2, "4"), [2, "4"]);

  function Point(x, y) {
    this.x = x;
    this.y = y;
  }

  let isPoint = x => x instanceof Point;

  let inc = when(isPoint, ({x, y}) => new Point(x + 1, y + 1));

  assert.equal(inc({}), undefined);
  assert.deepEqual(inc(new Point(0, 0)), { x: 1, y: 1 });

  let axis = when(isPoint,
                  ({ x, y }) => [x, y],
                  _ => [0, 0]);

  assert.deepEqual(axis(new Point(1, 4)), [1, 4]);
  assert.deepEqual(axis({ foo: "bar" }), [0, 0]);
};

exports["test chainable"] = function(assert) {
  let Player = function () { this.volume = 5; };
  Player.prototype = {
    setBand: chainable(function (band) { return (this.band = band); }),
    incVolume: chainable(function () { return this.volume++; })
  };
  let player = new Player();
  player
    .setBand('Animals As Leaders')
    .incVolume().incVolume().incVolume().incVolume().incVolume().incVolume();

  assert.equal(player.band, 'Animals As Leaders', 'passes arguments into chained');
  assert.equal(player.volume, 11, 'accepts no arguments in chain');
};

exports["test field"] = assert => {
  let Num = field("constructor", 0);
  assert.equal(Num.name, Number.name);
  assert.ok(typeof(Num), "function");

  let x = field("x");

  [
    [field("foo", { foo: 1 }), 1],
    [field("foo")({ foo: 1 }), 1],
    [field("bar", {}), undefined],
    [field("bar")({}), undefined],
    [field("hey", undefined), undefined],
    [field("hey")(undefined), undefined],
    [field("how", null), null],
    [field("how")(null), null],
    [x(1), undefined],
    [x(undefined), undefined],
    [x(null), null],
    [x({ x: 1 }), 1],
    [x({ x: 2 }), 2],
  ].forEach(([actual, expected]) => assert.equal(actual, expected));
};

exports["test query"] = assert => {
  let Num = query("constructor", 0);
  assert.equal(Num.name, Number.name);
  assert.ok(typeof(Num), "function");

  let x = query("x");
  let xy = query("x.y");

  [
    [query("foo", { foo: 1 }), 1],
    [query("foo")({ foo: 1 }), 1],
    [query("foo.bar", { foo: { bar: 2 } }), 2],
    [query("foo.bar")({ foo: { bar: 2 } }), 2],
    [query("foo.bar", { foo: 1 }), undefined],
    [query("foo.bar")({ foo: 1 }), undefined],
    [x(1), undefined],
    [x(undefined), undefined],
    [x(null), null],
    [x({ x: 1 }), 1],
    [x({ x: 2 }), 2],
    [xy(1), undefined],
    [xy(undefined), undefined],
    [xy(null), null],
    [xy({ x: 1 }), undefined],
    [xy({ x: 2 }), undefined],
    [xy({ x: { y: 1 } }), 1],
    [xy({ x: { y: 2 } }), 2]
  ].forEach(([actual, expected]) => assert.equal(actual, expected));
};

exports["test isInstance"] = assert => {
  function X() {}
  function Y() {}
  let isX = isInstance(X);

  [
    isInstance(X, new X()),
    isInstance(X)(new X()),
    !isInstance(X, new Y()),
    !isInstance(X)(new Y()),
    isX(new X()),
    !isX(new Y())
  ].forEach(x => assert.ok(x));
};

exports["test is"] = assert => {

  assert.deepEqual([ 1, 0, 1, 0, 1 ].map(is(1)),
                   [ true, false, true, false, true ],
                   "is can be partially applied");

  assert.ok(is(1, 1));
  assert.ok(!is({}, {}));
  assert.ok(is()(1)()(1), "is is curried");
  assert.ok(!is()(1)()(2));
};

exports["test isnt"] = assert => {

  assert.deepEqual([ 1, 0, 1, 0, 1 ].map(isnt(0)),
                   [ true, false, true, false, true ],
                   "is can be partially applied");

  assert.ok(!isnt(1, 1));
  assert.ok(isnt({}, {}));
  assert.ok(!isnt()(1)()(1));
  assert.ok(isnt()(1)()(2));
};

exports["test debounce"] = (assert, done) => {
  let counter = 0;
  let fn = debounce(() => counter++, 100);

  new Array(10).join(0).split("").forEach(fn);

  assert.equal(counter, 0, "debounce does not fire immediately");
  setTimeout(() => {
    assert.equal(counter, 1, "function called after wait time");
    fn();
    setTimeout(() => {
      assert.equal(counter, 2, "function able to be called again after wait");
      done();
    }, 150);
  }, 200);
};

exports["test throttle"] = (assert, done) => {
  let called = 0;
  let attempt = 0;
  let atleast100ms = false;
  let throttledFn = throttle(() => {
    called++;
    if (called === 2) {
      assert.equal(attempt, 10, "called twice, but attempted 10 times");
      fn();
    }
    if (called === 3) {
      assert.ok(atleast100ms, "atleast 100ms have passed");
      assert.equal(attempt, 11, "called third, waits for delay to happen");
      done();
    }
  }, 200);
  let fn = () => ++attempt && throttledFn();

  setTimeout(() => atleast100ms = true, 100);

  new Array(11).join(0).split("").forEach(fn);
};

require('sdk/test').run(exports);
