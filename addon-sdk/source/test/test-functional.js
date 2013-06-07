/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


const { setTimeout } = require('sdk/timers');
const utils = require('sdk/lang/functional');
const { invoke, defer, partial, compose, memoize, once, delay, wrap, curry } = utils;
const { LoaderWithHookedConsole } = require('sdk/test/loader');

exports['test forwardApply'] = function(assert) {
  function sum(b, c) this.a + b + c
  assert.equal(invoke(sum, [2, 3], { a: 1 }), 6,
               'passed arguments and pseoude-variable are used');

  assert.equal(invoke(sum.bind({ a: 2 }), [2, 3], { a: 1 }), 7,
               'bounded `this` pseoudo variable is used');
}

exports['test deferred function'] = function(assert, done) {
  let nextTurn = false;
  function sum(b, c) {
    assert.ok(nextTurn, 'enqueued is called in next turn of event loop');
    assert.equal(this.a + b + c, 6,
                 'passed arguments an pseoude-variable are used');
    done();
  }

  let fixture = { a: 1, method: defer(sum) }
  fixture.method(2, 3);
  nextTurn = true;
};

exports['test partial function'] = function(assert) {
  function sum(b, c) this.a + b + c;

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
      return sentence + '!'
    }, function(title) {
      return 'hi : ' + title + ' ' + this.name;
    })
  }

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

  function noop() { };
  let wrapped = wrap(noop, function(f) {
    return Array.slice(arguments);
  });

  let actual = wrapped([ 'whats', 'your' ], 'vector', 'victor');
  assert.deepEqual(actual, [ noop, ['whats', 'your'], 'vector', 'victor' ],
                   'works with fancy stuff');
};

exports['test memoize'] = function(assert) {
  function fib(n) n < 2 ? n : fib(n - 1) + fib(n - 2)
  let fibnitro = memoize(fib);

  assert.equal(fib(10), 55,
        'a memoized version of fibonacci produces identical results');
  assert.equal(fibnitro(10), 55,
        'a memoized version of fibonacci produces identical results');

  function o(key, value) { return value; };
  let oo = memoize(o), v1 = {}, v2 = {};


  assert.equal(oo(1, v1), v1, 'returns value back');
  assert.equal(oo(1, v2), v1, 'memoized by a first argument');
  assert.equal(oo(2, v2), v2, 'returns back value if not memoized');
  assert.equal(oo(2), v2, 'memoized new value');
  assert.notEqual(oo(1), oo(2), 'values do not override');
  assert.equal(o(3, v2), oo(2, 3), 'returns same value as un-memoized');

  let get = memoize(function(attribute) this[attribute])
  let target = { name: 'Bob', get: get }

  assert.equal(target.get('name'), 'Bob', 'has correct `this`');
  assert.equal(target.get.call({ name: 'Jack' }, 'name'), 'Bob',
               'name is memoized')
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
  let context = {}
  delay.call(context, function(name) {
    assert.equal(this, context, 'this was passed in');
    assert.equal(name, 'Tom', 'argument was passed in');
    done();
  }, 10, 'Tom');
}

exports['test once'] = function(assert) {
  let n = 0;
  let increment = once(function() { n ++; });

  increment();
  increment();

  assert.equal(n, 1, 'only incremented once');

  let target = { state: 0, update: once(function() this.state ++ ) };

  target.update();
  target.update();

  assert.equal(target.state, 1, 'this was passed in and called only once');
};

require('test').run(exports);
