/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Disclaimer: Most of the functions in this module implement APIs from
// Jeremy Ashkenas's http://underscorejs.org/ library and all credits for
// those goes to him.

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { setTimeout } = require("../timers");
const { deprecateFunction } = require("../util/deprecate");

/**
 * Takes `lambda` function and returns a method. When returned method is
 * invoked it calls wrapped `lambda` and passes `this` as a first argument
 * and given argument as rest.
 */
function method(lambda) {
  return function method() {
    return lambda.apply(null, [this].concat(Array.slice(arguments)));
  }
}
exports.method = method;

/**
 * Takes a function and returns a wrapped one instead, calling which will call
 * original function in the next turn of event loop. This is basically utility
 * to do `setTimeout(function() { ... }, 0)`, with a difference that returned
 * function is reused, instead of creating a new one each time. This also allows
 * to use this functions as event listeners.
 */
function defer(f) {
  return function deferred()
    setTimeout(invoke, 0, f, arguments, this);
}
exports.defer = defer;
// Exporting `remit` alias as `defer` may conflict with promises.
exports.remit = defer;

/*
 * Takes a funtion and returns a wrapped function that returns `this`
 */
function chain(f) {
  return function chainable(...args) {
    f.apply(this, args);
    return this;
  };
}
exports.chain = chain;

/**
 * Invokes `callee` by passing `params` as an arguments and `self` as `this`
 * pseudo-variable. Returns value that is returned by a callee.
 * @param {Function} callee
 *    Function to invoke.
 * @param {Array} params
 *    Arguments to invoke function with.
 * @param {Object} self
 *    Object to be passed as a `this` pseudo variable.
 */
function invoke(callee, params, self) callee.apply(self, params);
exports.invoke = invoke;

/**
 * Takes a function and bind values to one or more arguments, returning a new
 * function of smaller arity.
 *
 * @param {Function} fn
 *    The function to partial
 *
 * @returns The new function with binded values
 */
function partial(fn) {
  if (typeof fn !== "function")
    throw new TypeError(String(fn) + " is not a function");

  let args = Array.slice(arguments, 1);

  return function() fn.apply(this, args.concat(Array.slice(arguments)));
}
exports.partial = partial;

/**
 * Returns function with implicit currying, which will continue currying until
 * expected number of argument is collected. Expected number of arguments is
 * determined by `fn.length`. Using this with variadic functions is stupid,
 * so don't do it.
 *
 * @examples
 *
 * var sum = curry(function(a, b) {
 *   return a + b
 * })
 * console.log(sum(2, 2)) // 4
 * console.log(sum(2)(4)) // 6
 */
var curry = new function() {
  function currier(fn, arity, params) {
    // Function either continues to curry arguments or executes function
    // if desired arguments have being collected.
    return function curried() {
      var input = Array.slice(arguments);
      // Prepend all curried arguments to the given arguments.
      if (params) input.unshift.apply(input, params);
      // If expected number of arguments has being collected invoke fn,
      // othrewise return curried version Otherwise continue curried.
      return (input.length >= arity) ? fn.apply(this, input) :
             currier(fn, arity, input);
    };
  }

  return function curry(fn) {
    return currier(fn, fn.length);
  }
};
exports.curry = curry;

/**
 * Returns the composition of a list of functions, where each function consumes
 * the return value of the function that follows. In math terms, composing the
 * functions `f()`, `g()`, and `h()` produces `f(g(h()))`.
 * @example
 *
 *   var greet = function(name) { return "hi: " + name; };
 *   var exclaim = function(statement) { return statement + "!"; };
 *   var welcome = compose(exclaim, greet);
 *
 *   welcome('moe');    // => 'hi: moe!'
 */
function compose() {
  let lambdas = Array.slice(arguments);
  return function composed() {
    let args = Array.slice(arguments), index = lambdas.length;
    while (0 <= --index)
      args = [ lambdas[index].apply(this, args) ];
    return args[0];
  };
}
exports.compose = compose;

/*
 * Returns the first function passed as an argument to the second,
 * allowing you to adjust arguments, run code before and after, and
 * conditionally execute the original function.
 * @example
 *
 *  var hello = function(name) { return "hello: " + name; };
 *  hello = wrap(hello, function(f) {
 *    return "before, " + f("moe") + ", after";
 *  });
 *
 *  hello();    // => 'before, hello: moe, after'
 */
function wrap(f, wrapper) {
  return function wrapped()
    wrapper.apply(this, [ f ].concat(Array.slice(arguments)))
};
exports.wrap = wrap;

/**
 * Returns the same value that is used as the argument. In math: f(x) = x
 */
function identity(value) value
exports.identity = identity;

/**
 * Memoizes a given function by caching the computed result. Useful for
 * speeding up slow-running computations. If passed an optional hashFunction,
 * it will be used to compute the hash key for storing the result, based on
 * the arguments to the original function. The default hashFunction just uses
 * the first argument to the memoized function as the key.
 */
function memoize(f, hasher) {
  let memo = Object.create(null);
  hasher = hasher || identity;
  return function memoizer() {
    let key = hasher.apply(this, arguments);
    return key in memo ? memo[key] : (memo[key] = f.apply(this, arguments));
  };
}
exports.memoize = memoize;

/**
 * Much like setTimeout, invokes function after wait milliseconds. If you pass
 * the optional arguments, they will be forwarded on to the function when it is
 * invoked.
 */
function delay(f, ms) {
  let args = Array.slice(arguments, 2);
  setTimeout(function(context) { return f.apply(context, args); }, ms, this);
};
exports.delay = delay;

/**
 * Creates a version of the function that can only be called one time. Repeated
 * calls to the modified function will have no effect, returning the value from
 * the original call. Useful for initialization functions, instead of having to
 * set a boolean flag and then check it later.
 */
function once(f) {
  let ran = false, cache;
  return function() ran ? cache : (ran = true, cache = f.apply(this, arguments))
};
exports.once = once;
// export cache as once will may be conflicting with event once a lot.
exports.cache = once;
