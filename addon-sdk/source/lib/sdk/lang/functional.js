/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Disclaimer: Some of the functions in this module implement APIs from
// Jeremy Ashkenas's http://underscorejs.org/ library and all credits for
// those goes to him.

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { deprecateFunction } = require("../util/deprecate");
const { setImmediate, setTimeout, clearTimeout } = require("../timers");

const arity = f => f.arity || f.length;

const name = f => f.displayName || f.name;

const derive = (f, source) => {
  f.displayName = name(source);
  f.arity = arity(source);
  return f;
};

/**
 * Takes variadic numeber of functions and returns composed one.
 * Returned function pushes `this` pseudo-variable to the head
 * of the passed arguments and invokes all the functions from
 * left to right passing same arguments to them. Composite function
 * returns return value of the right most funciton.
 */
const method = (...lambdas) => {
  return function method(...args) {
    args.unshift(this);
    return lambdas.reduce((_, lambda) => lambda.apply(this, args),
                          void(0));
  };
};
exports.method = method;

/**
 * Takes a function and returns a wrapped one instead, calling which will call
 * original function in the next turn of event loop. This is basically utility
 * to do `setImmediate(function() { ... })`, with a difference that returned
 * function is reused, instead of creating a new one each time. This also allows
 * to use this functions as event listeners.
 */
const defer = f => derive(function(...args) {
  setImmediate(invoke, f, args, this);
}, f);
exports.defer = defer;
// Exporting `remit` alias as `defer` may conflict with promises.
exports.remit = defer;

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
const invoke = (callee, params, self) => callee.apply(self, params);
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
const partial = (f, ...curried) => {
  if (typeof(f) !== "function")
    throw new TypeError(String(f) + " is not a function");

  let fn = derive(function(...args) {
    return f.apply(this, curried.concat(args));
  }, f);
  fn.arity = arity(f) - curried.length;
  return fn;
};
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
const curry = new function() {
  const currier = (fn, arity, params) => {
    // Function either continues to curry arguments or executes function
    // if desired arguments have being collected.
    const curried = function(...input) {
      // Prepend all curried arguments to the given arguments.
      if (params) input.unshift.apply(input, params);
      // If expected number of arguments has being collected invoke fn,
      // othrewise return curried version Otherwise continue curried.
      return (input.length >= arity) ? fn.apply(this, input) :
             currier(fn, arity, input);
    };
    curried.arity = arity - (params ? params.length : 0);

    return curried;
  };

  return fn => currier(fn, arity(fn));
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
function compose(...lambdas) {
  return function composed(...args) {
    let index = lambdas.length;
    while (0 <= --index)
      args = [lambdas[index].apply(this, args)];

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
const wrap = (f, wrapper) => derive(function wrapped(...args) {
  return wrapper.apply(this, [f].concat(args));
}, f);
exports.wrap = wrap;

/**
 * Returns the same value that is used as the argument. In math: f(x) = x
 */
const identity = value => value;
exports.identity = identity;

/**
 * Memoizes a given function by caching the computed result. Useful for
 * speeding up slow-running computations. If passed an optional hashFunction,
 * it will be used to compute the hash key for storing the result, based on
 * the arguments to the original function. The default hashFunction just uses
 * the first argument to the memoized function as the key.
 */
const memoize = (f, hasher) => {
  let memo = Object.create(null);
  let cache = new WeakMap();
  hasher = hasher || identity;
  return derive(function memoizer(...args) {
    const key = hasher.apply(this, args);
    const type = typeof(key);
    if (key && (type === "object" || type === "function")) {
      if (!cache.has(key))
        cache.set(key, f.apply(this, args));
      return cache.get(key);
    }
    else {
      if (!(key in memo))
        memo[key] = f.apply(this, args);
      return memo[key];
    }
  }, f);
};
exports.memoize = memoize;

/**
 * Much like setTimeout, invokes function after wait milliseconds. If you pass
 * the optional arguments, they will be forwarded on to the function when it is
 * invoked.
 */
const delay = function delay(f, ms, ...args) {
  setTimeout(() => f.apply(this, args), ms);
};
exports.delay = delay;

/**
 * Creates a version of the function that can only be called one time. Repeated
 * calls to the modified function will have no effect, returning the value from
 * the original call. Useful for initialization functions, instead of having to
 * set a boolean flag and then check it later.
 */
const once = f => {
  let ran = false, cache;
  return derive(function(...args) {
    return ran ? cache : (ran = true, cache = f.apply(this, args));
  }, f);
};
exports.once = once;
// export cache as once will may be conflicting with event once a lot.
exports.cache = once;

// Takes a `f` function and returns a function that takes the same
// arguments as `f`, has the same effects, if any, and returns the
// opposite truth value.
const complement = f => derive(function(...args) {
  return args.length < arity(f) ? complement(partial(f, ...args)) :
         !f.apply(this, args);
}, f);
exports.complement = complement;

// Constructs function that returns `x` no matter what is it
// invoked with.
const constant = x => _ => x;
exports.constant = constant;

// Takes `p` predicate, `consequent` function and an optional
// `alternate` function and composes function that returns
// application of arguments over `consequent` if application over
// `p` is `true` otherwise returns application over `alternate`.
// If `alternate` is not a function returns `undefined`.
const when = (p, consequent, alternate) => {
  if (typeof(alternate) !== "function" && alternate !== void(0))
    throw TypeError("alternate must be a function");
  if (typeof(consequent) !== "function")
    throw TypeError("consequent must be a function");

  return function(...args) {
    return p.apply(this, args) ?
           consequent.apply(this, args) :
           alternate && alternate.apply(this, args);
  };
};
exports.when = when;

// Apply function that behaves as `apply` does in lisp:
// apply(f, x, [y, z]) => f.apply(f, [x, y, z])
// apply(f, x) => f.apply(f, [x])
const apply = (f, ...rest) => f.apply(f, rest.concat(rest.pop()));
exports.apply = apply;

// Returns function identical to given `f` but with flipped order
// of arguments.
const flip = f => derive(function(...args) {
  return f.apply(this, args.reverse());
}, f);
exports.flip = flip;


// Takes field `name` and `target` and returns value of that field.
// If `target` is `null` or `undefined` it would be returned back
// instead of attempt to access it's field. Function is implicitly
// curried, this allows accessor function generation by calling it
// with only `name` argument.
const field = curry((name, target) =>
  // Note: Permisive `==` is intentional.
  target == null ? target : target[name]);
exports.field = field;

// Takes `.` delimited string representing `path` to a nested field
// and a `target` to get it from. For convinience function is
// implicitly curried, there for accessors can be created by invoking
// it with just a `path` argument.
const query = curry((path, target) => {
  const names = path.split(".");
  const count = names.length;
  let index = 0;
  let result = target;
  // Note: Permisive `!=` is intentional.
  while (result != null && index < count) {
    result = result[names[index]];
    index = index + 1;
  }
  return result;
});
exports.query = query;

// Takes `Type` (constructor function) and a `value` and returns
// `true` if `value` is instance of the given `Type`. Function is
// implicitly curried this allows predicate generation by calling
// function with just first argument.
const isInstance = curry((Type, value) => value instanceof Type);
exports.isInstance = isInstance;

/*
 * Takes a funtion and returns a wrapped function that returns `this`
 */
const chainable = f => derive(function(...args) {
  f.apply(this, args);
  return this;
}, f);
exports.chainable = chainable;
exports.chain =
  deprecateFunction(chainable, "Function `chain` was renamed to `chainable`");

// Functions takes `expected` and `actual` values and returns `true` if
// `expected === actual`. Returns curried function if called with less then
// two arguments.
//
// [ 1, 0, 1, 0, 1 ].map(is(1)) // => [ true, false, true, false, true ]
const is = curry((expected, actual) => actual === expected);
exports.is = is;

const isnt = complement(is);
exports.isnt = isnt;

/**
 * From underscore's `_.debounce`
 * http://underscorejs.org
 * (c) 2009-2014 Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
 * Underscore may be freely distributed under the MIT license.
 */
const debounce = function debounce (fn, wait) {
  let timeout, args, context, timestamp, result;

  let later = function () {
    let last = Date.now() - timestamp;
    if (last < wait) {
      timeout = setTimeout(later, wait - last);
    } else {
      timeout = null;
      result = fn.apply(context, args);
      context = args = null;
    }
  };

  return function (...aArgs) {
    context = this;
    args = aArgs;
    timestamp  = Date.now();
    if (!timeout) {
      timeout = setTimeout(later, wait);
    }

    return result;
  };
};
exports.debounce = debounce;

/**
 * From underscore's `_.throttle`
 * http://underscorejs.org
 * (c) 2009-2014 Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
 * Underscore may be freely distributed under the MIT license.
 */
const throttle = function throttle (func, wait, options) {
  let context, args, result;
  let timeout = null;
  let previous = 0;
  options || (options = {});
  let later = function() {
    previous = options.leading === false ? 0 : Date.now();
    timeout = null;
    result = func.apply(context, args);
    context = args = null;
  };
  return function() {
    let now = Date.now();
    if (!previous && options.leading === false) previous = now;
    let remaining = wait - (now - previous);
    context = this;
    args = arguments;
    if (remaining <= 0) {
      clearTimeout(timeout);
      timeout = null;
      previous = now;
      result = func.apply(context, args);
      context = args = null;
    } else if (!timeout && options.trailing !== false) {
      timeout = setTimeout(later, remaining);
    }
    return result;
  };
};
exports.throttle = throttle;
