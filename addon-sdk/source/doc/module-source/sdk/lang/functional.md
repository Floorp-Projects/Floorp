<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `lang/functional` module provides functional helper methods. A lot of these
functions implement APIs from Jeremy Ashkenas's [underscore.js](http://underscorejs.org)
and all credits go to him and his contributors.

<api name="method">
@function
Takes a function and returns a method associated with an object.
When the method is invoked on an instance of the object, the original
function is called. It is passed the object instance (i.e. `this`) as
the first parameter, followed by any parameters passed into the method.

    let { method } = require("sdk/lang/functional");
    let myNumber = {
      times: method(times),
      add: method(add),
      number: 0
    };

    function times (target, x) { return target.number *= x; }
    function add (target, x) { return target.number += x; }

    console.log(myNumber.number); // 0
    myNumber.add(10);      // 10
    myNumber.times(2);     // 20
    myNumber.add(3);       // 23

@param lambda {function}
  The function to be wrapped and returned.

@returns {function}
  The wrapped `lambda`.
</api>

<api name="defer">
@function
Takes a function and returns a wrapped version of the function. Calling the
wrapped version will call the original function during the next event loop.
This is similar to calling [setTimeout](modules/sdk/timers.html#setTimeout(callback%2C ms)) with no
wait (i.e. `setTimeout(function () { ... }, 0)`), except that the wrapped function
may be reused and does not need to be repeated each time. This also enables you
to use these functions as event listeners.

    let { defer } = require("sdk/lang/functional");
    let fn = defer(function myEvent (event, value) {
      console.log(event + " : " + value);
    });

    fn("click", "#home");
    console.log("done");

    // This will print 'done' before 'click : #home' since
    // we deferred the execution of the wrapped `myEvent`
    // function, making it non-blocking and executing on the
    // next event loop

@param fn {function}
  The function to be deferred.

@returns {function}
  The new, deferred function.
</api>

<api name="remit">
@function
An alias for [defer](modules/sdk/lang/functional.html#defer(fn)).
</api>

<api name="invoke">
@function
Invokes `callee`, passing `params` as an argument and `self` as `this`.
Returns the value that is returned by `callee`.

    let { invoke } = require("sdk/lang/functional");

    invoke(sum, [1,2,3,4,5], null); // 15

    function sum () {
      return Array.slice(arguments).reduce(function (a, b) {
        return a + b;
      });
    }

@param callee {function}
  Function to invoke.
@param params {Array}
  Parameters to be passed into `callee`.
@param self {mixed}
  Object to be passed as the `this` context to `callee`.
@returns {mixed}
  Returns the return value of `callee`.
</api>

<api name="partial">
@function
Takes a function and bind values to one or more arguments, returning a new function of smaller arity.

    let { partial } = require("sdk/lang/functional");
    let add = function add (x, y) { return x + y; }
    let addOne = partial(add, 1);

    addOne(5); // 6
    addOne(10); // 11
    partial(add, addOne(20))(2); // 23

@param fn {function}
  Function on which partial application is to be performed.

@param arguments... {mixed}
  Additional arguments

@returns {function}
  The partial function.
</api>

<api name="compose">
@function
Returns the [composition](http://en.wikipedia.org/wiki/Function_composition_(computer_science)) of a list of functions, where each function consumes the
return value of the function that follows. In math terms, composing the functions
`f()`, `g()`, and `h()` produces `f(g(h()))`.

    let { compose } = require("sdk/lang/functional");

    let welcome = compose(exclaim, greet);

    welcome('moe'); // "hi: moe!";

    function greet (name) { return "hi: " + name; }
    function exclaim (statement) { return statement + "!"; }

@param fn... {function}
  Takes a variable number of functions as arguments and composes them from right to left.

@returns {function}
  The composed function.
</api>

<api name="wrap">
@function
Returns the first function passed as an argument to the second,
allowing you to adjust arguments, run code before and after, and
conditionally execute the original function.

    let { wrap } = require("sdk/lang/functional");

    let wrappedHello = wrap(hello, function (fn, name) {
      return "before, " + fn(name) + "after";
    });

    wrappedHello("moe"); // "before, hello: moe, after"

    function hello (name) { return "hello: " + name; }

@param fn {function}
  The function to be passed into the `wrapper` function.

@param wrapper {function}
  The function that is called when the return function is executed,
  taking the wrapped `fn` as the first parameter.

@returns {function}
  A function which, when called, executes `wrapper` with `fn` as the first parameter,
  and passes in any additional parameters to the `wrapper` function.
</api>

<api name="identity">
@function
Returns the same value that is used as the argument. In math: f(x) = x.

    let { identity } = require("sdk/lang/functional");
    let x = 5;
    identity(x); // 5

@param value {mixed}
  The value to be returned.

@returns {mixed}
  The value that was originally passed in.
</api>

<api name="memoize">
@function
[Memoizes](http://en.wikipedia.org/wiki/Memoization) a given function by caching
the computed result. Useful for speeding up slow-running computations. If
passed an optional `hashFunction`, it will be used to compute the hash key for
storing the result, based on the arguments to the original function. The
default `hashFunction` just uses the first argument to the memoized function as
the key.

    let { memoize } = require("sdk/lang/functional");

    let memoizedFn = memoize(primeFactorization);

    memoizedFn(50); // Returns [2, 5, 5], had to compute
    memoizedFn(100); // Returns [2, 2, 5, 5], had to compute
    memoizedFn(50); // Returns [2, 5, 5] again, but pulled from cache

    function primeFactorization (x) {
      // Some tricky stuff
    }

    // We can also use a hash function to compute a different
    // hash value. In this example, we'll fabricate a function
    // that takes a string of first and last names that
    // somehow computes the lineage of that name. Our hash
    // function will just parse the last name, as our naive
    // implementation assumes that they will share the same lineage

    let getLineage = memoize(function (name) {
      // computes lineage
      return data;
    }, hasher);

    // Hashing function takes a string of first and last name
    // and returns the last name.
    function hasher (input) {
      return input.split(" ")[1];
    }

    getLineage("homer simpson"); // Computes and returns information for "simpson"
    getLineage("lisa simpson"); // Returns cached for "simpson"

@param fn {function}
  The function that becomes memoized.

@param hasher {function}
  An optional function that takes the memoized function's parameter and returns
  a hash key for storing the result.

@returns {function}
  The memoized version of `fn`.
</api>

<api name="delay">
@function
Much like `setTimeout`, `delay` invokes a function after waiting a set number of
milliseconds. If you pass additional, optional, arguments, they will be forwarded
on to the function when it is invoked.

    let { delay } = require("sdk/lang/functional");

    delay(printAdd, 2000, 5, 10);

    // Prints "5+10=15" in two seconds (2000ms)
    function printAdd (a, b) { console.log(a + "+" + b + "=" + (a+b)); }

@param fn {function}
  A function to be delayed.

@param ms {number}
  Number of milliseconds to delay the execution of `fn`.

@param arguments {mixed}
  Additional arguments to pass to `fn` upon execution
</api>

<api name="once">
@function
Creates a version of the input function that can only be called one time.
Repeated calls to the modified function will have no effect, returning
the value from the original call. Useful for initialization functions, instead
of having to set a boolean flag and checking it later.

    let { once } = require("sdk/lang/functional");
    let setup = once(function (env) {
      // initializing important things
      console.log("successfully initialized " + env);
      return 1; // Assume success and return 1
    });

    setup('dev'); // returns 1
    // prints "successfully initialized dev"
    
    // Future attempts to call this function just return the cached
    // value that was returned previously
    setup('production'); // Returns 1
    // No print message is displayed since the function isn't executed

@param fn {function}
  The function that will be executed only once inside the once wrapper.

@returns {function}
  The wrapped `fn` that can only be executed once.
</api>

<api name="cache">
@function
An alias for [once](modules/sdk/lang/functional.html#once(fn)).
</api>
