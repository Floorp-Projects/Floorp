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

    const { method } = require("sdk/lang/functional");

    const times = (target, x) => target.number *= x;
    const add = (target, x) => target.number += x;

    const myNumber = {
      times: method(times),
      add: method(add),
      number: 0
    };


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

    const { defer } = require("sdk/lang/functional");
    const fn = defer((event, value) => {
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

    const { invoke } = require("sdk/lang/functional");

    const sum = (...args) => args.reduce((a, b) => a + b);
    invoke(sum, [1,2,3,4,5], null); // 15

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

    const { partial } = require("sdk/lang/functional");
    const add = (x, y) => x + y;
    const addOne = partial(add, 1);

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

    const { compose } = require("sdk/lang/functional");

    const square = x => x * x;
    const increment = x => x + 1;

    const f1 = compose(increment, square);
    f1(5); // => 26

    const f2 = compose(square, increment);
    f2(5); // => 36

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

    const { wrap } = require("sdk/lang/functional");

    const hello = name => "hello: " + name;
    const wrappedHello = wrap(hello, (fn, name) =>
      "before, " + fn(name) + "after");

    wrappedHello("moe"); // "before, hello: moe, after"

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

    const { identity } = require("sdk/lang/functional");
    const x = 5;
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

    const { memoize } = require("sdk/lang/functional");

    const memoizedFn = memoize(primeFactorization);

    memoizedFn(50); // Returns [2, 5, 5], had to compute
    memoizedFn(100); // Returns [2, 2, 5, 5], had to compute
    memoizedFn(50); // Returns [2, 5, 5] again, but pulled from cache

    const primeFactorization = x => {
      // Some tricky stuff
    }

    // We can also use a hash function to compute a different
    // hash value. In this example, we'll fabricate a function
    // that takes a string of first and last names that
    // somehow computes the lineage of that name. Our hash
    // function will just parse the last name, as our naive
    // implementation assumes that they will share the same lineage

    const getLineage = memoize(name => {
      // computes lineage
      return data;
    }, hasher);

    // Hashing function takes a string of first and last name
    // and returns the last name.
    const hasher = input => input.split(" ")[1];

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

    const { delay } = require("sdk/lang/functional");

    const printAdd = (a, b) console.log(a + "+" + b + "=" + (a+b));
    delay(printAdd, 2000, 5, 10);
    // Prints "5+10=15" in two seconds (2000ms)

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

    const { once } = require("sdk/lang/functional");
    const setup = once(env => {
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

<api name="complement">
@function
Takes a `f` function and returns a function that takes the same
arguments as `f`, has the same effects, if any, and returns the
opposite truth value.

    const { complement } = require("sdk/lang/functional");

    let isOdd = x => Boolean(x % 2);

    isOdd(1)     // => true
    isOdd(2)     // => false

    let isEven = complement(isOdd);

    isEven(1)     // => false
    isEven(2)     // => true

@param lambda {function}
  The function to compose from

@returns {boolean}
  `!lambda(...)`
</api>

<api name="constant">
@function
Constructs function that returns `x` no matter what is it
invoked with.

    const { constant } = require("sdk/lang/functional");

    const one = constant(1);

    one();              // => 1
    one(2);             // => 1
    one.apply({}, 3);   // => 1

@param x {object}
  Value that will be returned by composed function

@returns {function}
</api>


<api name="apply">
@function
Apply function that behaves like `apply` in other functional
languages:

    const { apply } = require("sdk/lang/functional");

    const dashify = (...args) => args.join("-");

    apply(dashify, 1, [2, 3]);        // => "1-2-3"
    apply(dashify, "a");              // => "a"
    apply(dashify, ["a", "b"]);       // => "a-b"
    apply(dashify, ["a", "b"], "c");  // => "a,b-c"
    apply(dashify, [1, 2], [3, 4]);   // => "1,2-3-4"

@param f {function}
  function to be invoked
</api>

<api name="flip">
@function
Returns function identical to given `f` but with flipped order
of arguments.

    const { flip } = require("sdk/lang/functional");

    const append = (left, right) => left + " " + right;
    const prepend = flip(append);

    append("hello", "world")       // => "hello world"
    prepend("hello", "world")      // => "world hello"

@param f {function}
  function whose arguments should to be flipped

@returns {function}
  function with flipped arguments
</api>

<api name="when">
@function
Takes `p` predicate, `consequent` function and an optional
`alternate` function and composes function that returns
application of arguments over `consequent` if application over
`p` is `true` otherwise returns application over `alternate`.
If `alternate` is not a function returns `undefined`.

    const { when } = require("sdk/lang/functional");

    function Point(x, y) {
      this.x = x
      this.y = y
    }

    const isPoint = x => x instanceof Point;

    const inc = when(isPoint, ({x, y}) => new Point(x + 1, y + 1));

    inc({});                 // => undefined
    inc(new Point(0, 0));    // => { x: 1, y: 1 }

    const axis = when(isPoint,
                      ({ x, y }) => [x, y],
                      _ => [0, 0]);

    axis(new Point(1, 4));   // => [1, 4]
    axis({ foo: "bar" });    // => [0, 0]

@param p {function}
  predicate function whose return value determines to which
  function be delegated control.

@param consequent {function}
  function to which arguments are applied if `predicate` returned
  `true`.

@param alternate {function}
  function to which arguments are applied if `predicate` returned
  `false`.

@returns {object|string|number|function}
  Return value from `consequent` if `p` returned `true` or return
  value from `alternate` if `p` returned `false`. If `alternate`
  is not provided and `p` returned `false` then `undefined` is
  returned.
</api>


<api name="chainable">
@function
Creates a version of the input function that will return `this`.

    const { chainable } = require("sdk/lang/functional");

    function Person (age) { this.age = age; }
    Person.prototype.happyBirthday = chainable(function() {
      return this.age++
    });

    const person = new Person(30);

    person
      .happyBirthday()
      .happyBirthday()
      .happyBirthday()

    console.log(person.age); // 33

@param fn {function}
  The function that will be wrapped by the chain function.

@returns {function}
  The wrapped function that executes `fn` and returns `this`.
</api>

<api name="field">
@function

Takes field `name` and `target` and returns value of that field.
If `target` is `null` or `undefined` it would be returned back
instead of attempt to access it's field. Function is implicitly
curried, this allows accessor function generation by calling it
with only `name` argument.

    const { field } = require("sdk/lang/functional");

    field("x", { x: 1, y: 2});     // => 1
    field("x")({ x: 1 });          // => 1
    field("x", { y: 2 });          // => undefiend

    const getX = field("x");
    getX({ x: 1 });               // => 1
    getX({ y: 1 });               // => undefined
    getX(null);                   // => null

@param name {string}
  Name of the field to be returned

@param target {object}
  Target to get a field by the given `name` from

@returns {object|function|string|number|boolean}
  Field value
</api>

<api name="query">
@function

Takes `.` delimited string representing `path` to a nested field
and a `target` to get it from. For convinience function is
implicitly curried, there for accessors can be created by invoking
it with just a `path` argument.

    const { query } = require("sdk/lang/functional");

    query("x", { x: 1, y: 2});           // => 1
    query("top.x", { x: 1 });            // => undefiend
    query("top.x", { top: { x: 2 } });   // => 2

    const topX = query("top.x");
    topX({ top: { x: 1 } });             // => 1
    topX({ y: 1 });                      // => undefined
    topX(null);                          // => null

@param path {string}
  `.` delimited path to a field

@param target {object}
  Target to get a field by the given `name` from

@returns {object|function|string|number|boolean}
  Field value
</api>

<api name="isInstance">
@function

Takes `Type` (constructor function) and a `value` and returns
`true` if `value` is instance of the given `Type`. Function is
implicitly curried this allows predicate generation by calling
function with just first argument.

    const { isInstance } = require("sdk/lang/functional");

    function X() {}
    function Y() {}
    let isX = isInstance(X);

    isInstance(X, new X);     // true
    isInstance(X)(new X);     // true
    isInstance(X, new Y);     // false
    isInstance(X)(new Y);     // false

    isX(new X);               // true
    isX(new Y);               // false

@param Type {function}
  Type (constructor function)

@param instance {object}
  Instance to test

@returns {boolean}
</api>

<api name="is">
@function

Functions takes `expected` and `actual` values and returns `true` if
`expected === actual`. If invoked with just one argument returns pratially
applied function, which can be invoked to provide a second argument, this
is handy with `map`, `filter` and other high order functions:

    const { is } = require("sdk/util/oops");
    [ 1, 0, 1, 0, 1 ].map(is(1)) // => [ true, false, true, false, true ]

@param expected {object|string|number|boolean}
@param actual {object|string|number|boolean}

@returns {boolean}
</api>

<api name="isnt">
@function

Functions takes `expected` and `actual` values and returns `true` if
`expected !== actual`. If invoked with just one argument returns pratially
applied function, which can be invoked with a second argument, which is
handy with `map`, `filter` and other high order functions:

    const { isnt } = require("sdk/util/oops");
    [ 1, 0, 1, 0, 1 ].map(isnt(0)) // => [ true, false, true, false, true ]

@param expected {object|string|number|boolean}
@param actual {object|string|number|boolean}

@returns {boolean}
</api>
