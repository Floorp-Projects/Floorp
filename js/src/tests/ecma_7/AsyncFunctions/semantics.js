/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Promise = ShellPromise;

async function empty() {

}

async function simpleReturn() {
  return 1;
}

async function simpleAwait() {
  var result = await 2;
  return result;
}

async function simpleAwaitAsync() {
  var result = await simpleReturn();
  return 2 + result;
}

async function returnOtherAsync() {
  return 1 + await simpleAwaitAsync();
}

async function simpleThrower() {
  throw new Error();
}

async function delegatedThrower() {
  var val = await simpleThrower();
  return val;
}

async function tryCatch() {
  try {
    await delegatedThrower();
    return 'FAILED';
  } catch (_) {
    return 5;
  }
}

async function tryCatchThrow() {
  try {
    await delegatedThrower();
    return 'FAILED';
  } catch (_) {
    return delegatedThrower();
  }
}

async function wellFinally() {
  try {
    await delegatedThrower();
  } catch (_) {
    return 'FAILED';
  } finally {
    return 6;
  }
}

async function finallyMayFail() {
  try {
    await delegatedThrower();
  } catch (_) {
    return 5;
  } finally {
    return delegatedThrower();
  }
}

async function embedded() {
  async function inner() {
    return 7;
  }
  return await inner();
}

// recursion, it works!
async function fib(n) {
    return (n == 0 || n == 1) ? n : await fib(n - 1) + await fib(n - 2);
}

// mutual recursion
async function isOdd(n) {
  async function isEven(n) {
      return n === 0 || await isOdd(n - 1);
  }
  return n !== 0 && await isEven(n - 1);
}

// recursion, take three!
var hardcoreFib = async function fib2(n) {
  return (n == 0 || n == 1) ? n : await fib2(n - 1) + await fib2(n - 2);
}

var asyncExpr = async function() {
  return 10;
}

var namedAsyncExpr = async function simple() {
  return 11;
}

async function executionOrder() {
  var value = 0;
  async function first() {
    return (value = value === 0 ? 1 : value);
  }
  async function second() {
    return (value = value === 0 ? 2 : value);
  }
  async function third() {
    return (value = value === 0 ? 3 : value);
  }
  return await first() + await second() + await third() + 6;
}

async function miscellaneous() {
  if (arguments.length === 3 &&
      arguments.callee.name === "miscellaneous")
      return 14;
}

function thrower() {
  throw 15;
}

async function defaultArgs(arg = thrower()) {

}

// Async functions are not constructible
assertThrows(() => {
  async function Person() {

  }
  new Person();
}, TypeError);

Promise.all([
  assertEventuallyEq(empty(), undefined),
  assertEventuallyEq(simpleReturn(), 1),
  assertEventuallyEq(simpleAwait(), 2),
  assertEventuallyEq(simpleAwaitAsync(), 3),
  assertEventuallyEq(returnOtherAsync(), 4),
  assertEventuallyThrows(simpleThrower(), Error),
  assertEventuallyEq(tryCatch(), 5),
  assertEventuallyThrows(tryCatchThrow(), Error),
  assertEventuallyEq(wellFinally(), 6),
  assertEventuallyThrows(finallyMayFail(), Error),
  assertEventuallyEq(embedded(), 7),
  assertEventuallyEq(fib(6), 8),
  assertEventuallyEq(executionOrder(), 9),
  assertEventuallyEq(asyncExpr(), 10),
  assertEventuallyEq(namedAsyncExpr(), 11),
  assertEventuallyEq(isOdd(12).then(v => v ? "oops" : 12), 12),
  assertEventuallyEq(hardcoreFib(7), 13),
  assertEventuallyEq(miscellaneous(1, 2, 3), 14),
  assertEventuallyEq(defaultArgs().catch(e => e), 15)
]).then(() => {
  if (typeof reportCompare === "function")
      reportCompare(true, true);
});
