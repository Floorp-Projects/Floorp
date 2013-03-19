<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

## Rationale

Most of the JS APIs are asynchronous complementing its non-blocking nature.
While this has a good reason and many advantages, it comes with a price.
Instead of structuring our programs into logical black boxes:

    function blackbox(a, b) {
      var c = assemble(a);
      return combine(b, c);
    }


We're forced into continuation passing style, involving lots of machinery:

    function sphagetti(a, b, callback) {
      assemble(a, function continueWith(error, c) {
        if (error) callback(error);
        else combine(b, c, callback);
      });
    }

This style also makes doing things in sequence hard:

    widget.on('click', function onClick() {
      promptUserForTwitterHandle(function continueWith(error, handle) {
        if (error) return ui.displayError(error);
        twitter.getTweetsFor(handle, funtion continueWith(error, tweets) {
          if (error) return ui.displayError(error);
          ui.showTweets(tweets);
        });
      });
    });

Doing things in parallel is even harder:

    var tweets, answers, checkins;
    twitter.getTweetsFor(user, function continueWith(result) {
      tweets = result;
      somethingFinished();
    });

    stackOverflow.getAnswersFor(question, function continueWith(result) {
      answers = result;
      somethingFinished();
    });

    fourSquare.getCheckinsBy(user, function continueWith(result) {
      checkins=result;
      somethingFinished();
    }); 

    var finished = 0;
    functions somethingFinished() {
      if (++finished === 3)
        ui.show(tweets, answers, checkins);
    }

This also makes error handling quite of an adventure.

## Promises

Consider another approach, where instead of continuation passing via `callback`,
function returns an object, that represents eventual result, either successful
or failed. This object is a promise, both figuratively and by name, to
eventually resolve. We can call a function on the promise to observe
either its fulfillment or rejection. If the promise is rejected and the
rejection is not explicitly observed, any derived promises will be implicitly
rejected for the same reason.

In the Add-on SDK we follow
[CommonJS Promises/A](http://wiki.commonjs.org/wiki/Promises/A) specification
and model a promise as an object with a `then` method, which can be used to get
the eventual return (fulfillment) value or thrown exception (rejection):

    foo().then(function success(value) {
      // ...
    }, function failure(reason) {
      // ...
    });

If `foo` returns a promise that gets fulfilled with the `value`, `success`
callback (the value handler) will be called with that `value`. However,
if the returned promise gets rejected, the `failure` callback (the error
handler) will be called with the `reason` of an error.

## Propagation

The `then` method of a promise returns a new promise that is resolved with the
return value of either handler. Since function can either return value or throw
an exception, only one handler will be ever called.


    var bar = foo().then(function success(value) {
      // compute something from a value...
    }, function failure(reason) {
      // handle an error...
    });

In this example `bar` is a promise and it's fulfilled by one of two handlers
that are responsible for:

  - If handler returns a value, `bar` will be resolved with it.
  - If handler throws an exception, `bar` will be rejected with it.
  - If handler returns a **promise**, `bar` will "become" that promise. To be
    more precise it will be resolved with a resolution value of the returned
    promise, which will appear and feel as if it was that returned promise.

If the `foo()` promise gets rejected and you omit the error handler, the
**error** will propagate to `bar` (`bar` will be rejected with that error):

    var bar = foo().then(function success(value) {
      // compute something out of the value...
    });

If the `foo()` promise gets fulfilled and you omit the value handler, the
**value** will propagate to `bar` (`bar` will be fulfilled with that value):

    var bar = foo().then(null, function failure(error) {
      // handle error...
    });


## Chaining

There are two ways to chain promise operations. You can chain them using either
inside or outside handlers.

### Flat chaining

You can use `then` for chaining intermediate operations on promises
(`var data = readAsync().then(parse).then(extract)`). You can chain multiple
`then` functions, because `then` returns a promise resolved to a return value
of an operation and errors propagate through the promise chains. In general
good rule of thumb is to prefer `then` based flat chaining. It makes code
easier to read and make changes later:

    var data = readAsync(url).    // read content of url asynchronously
      then(parse).                // parse content from the url
      then(extractQuery).         // extract SQL query
      then(readDBAsync);          // exectue extracted query against DB

### Nested chaining

Flat chaining is not always an option though, as in some cases you may want to
capture an intermediate values of the chain:

    var result = readAsync(url).then(function(source) {
      var json = parse(source)
      return readDBAsync(extractQuery(json)).then(function(data) {
        return writeAsync(json.url, data);
      });
    });

In general, nesting is useful for computing values from more then one promise:

    function eventualAdd(a, b) {
      return a.then(function (a) {
        return b.then(function (b) {
          return a + b;
        });
      });
    }

    var c = eventualAdd(aAsync(), bAsync());

## Error handling

One sometimes-unintuitive aspect of promises is that if you throw an exception
in the value handler, it will not be be caught by the error handler.

    readAsync(url).then(function (value) {
      throw new Error("Can't bar.");
    }, function (error) {
      // We only get here if `readAsync` fails.
    });

To see why this is, consider the parallel between promises and `try`/`catch`.
We are `try`-ing to execute `readAsync()`: the error handler represents a 
`catch` for `readAsync()`, while the value handler represents code that happens
*after* the `try`/`catch` block. That code then needs its own `try`/`catch`
block to handle errors there.

In terms of promises, this means chaining your error handler:

    readAsync(url).
      then(parse).
      then(null, function handleParseError(error) {
        // handle here both `readAsync` and `parse` errors.
    });


# Consuming promises

In general, whole purpose of promises is to avoid a callback spaghetti in the
code. As a matter of fact it would be great if we could convert any synchronous
functions to asynchronous by making it aware of promises. Module exports
`promised` function to do exactly that:

    const { promised } = require('sdk/core/promise');
    function sum(x, y) { return x + y };
    var asyncSum = promised(sum);

    var c = sum(a, b);
    var cAsync = asyncSum(aAsync(), bAsync());

`promised` takes normal function and composes new promise aware version of it
that may take both normal values and promises as arguments and returns promise
that will resolve to value that would have being returned by an original
function if it was called with fulfillment values of given arguments.

This technique is so powerful that it can replace most of the promise utility
functions provided by other promise libraries. For example grouping promises
to observe single resolution of all of them is as simple as this:

    var group = promised(Array);
    var abc = group(aAsync, bAsync, cAsync).then(function(items) {
      return items[0] + items[1] + items[2];
    });

## all

The `all` function is provided to consume an array of promises and return 
a promise that will be accepted upon the acceptance of all the promises
in the initial array. This can be used to perform an action that requires
values from several promises, like getting user information and server
status, for example:

    const { all } = require('sdk/core/promise');
    all([getUser, getServerStatus]).then(function (result) {
      return result[0] + result[1]
    });

If one of the promises in the array is rejected, the rejection handler 
handles the first failed promise and remaining promises remain unfulfilled.

    const { all } = require('sdk/core/promise');
    all([aAsync, failAsync, bAsync]).then(function (result) {
      // success function will not be called
      return result;
    }, function (reason) {
      // rejection handler called because `failAsync` promise
      // was rejected with its reason propagated
      return reason;
    });


# Making promises

Everything above assumes you get a promise from somewhere else. This
is the common case, but every once in a while, you will need to create a
promise from scratch. Add-on SDK's `promise` module provides API for doing
that.

## defer

Module exports `defer` function, which is where all promises ultimately
come from. Lets see implementation of `readAsync` that we used in lot's
of examples above:

    const { defer } = require('sdk/core/promise');
    function readAsync(url) {
      var deferred = defer();

      let xhr = new XMLHttpRequest();
      xhr.open("GET", url, true);
      xhr.onload = function() {
        deferred.resolve(xhr.responseText);
      }
      xhr.onerror = function(event) {
        deferred.reject(event);
      }
      xhr.send();

      return deferred.promise;
    }

So `defer` returns an object that contains `promise` and two `resolve`, `reject`
functions that can be used to resolve / reject that `promise`. **Note:** that
promise can be rejected or resolved and only once. All subsequent attempts will be
ignored.

Another simple example may be `delay` function that returns promise which
is fulfilled with a given `value` in a given `ms`, kind of promise based
alternative to `setTimeout`:

    function delay(ms, value) {
      let { promise, resolve } = defer();
      setTimeout(resolve, ms, value);
      return promise;
    }

    delay(10, 'Hello world').then(console.log);
    // After 10ms => 'Helo world'

# Advanced usage

If general `defer` and `promised` should be enough to doing almost anything
you may think of with promises, but once you start using promises extensively
you may discover some missing pieces and this section of documentation may
help you to discover them.

## Doing things concurrently

So far we have being playing with promises that do things sequentially, but
there are bunch of cases where one would need to do things concurrently. In the
following example we implement functions that takes multiple promises and
returns one that resolves to first on being fulfilled:

    function race() {
      let { promise, resolve } = defer();
      Array.slice(arguments).forEach(function(promise) {
        promise.then(resolve);
      });
      return promise;
    }

    var asyncAorB = race(readAsync(urlA), readAsync(urlB));

*Note: that this implementation forgives failures and would fail if all
promises fail to resolve.*

There are cases when promise may or may not be fulfilled in a reasonable time.
In such cases it's useful to put a timer on such tasks:

    function timeout(promise, ms) {
      let deferred = defer();
      promise.then(deferred.resolve, deferred.reject);
      delay(ms, 'timeout').then(deferred.reject);
      return deferred.promise;
    }

    var tweets = readAsync(url);
    timeout(tweets, 20).then(function(data) {
      ui.display(data);
    }, function() {
      alert('Network is being too slow, try again later');
    });

## Alternative promise APIs

There may be a cases where you will want to provide more than just `then`
method on your promises. In fact some other promise frameworks do that.
Such use cases are also supported. Earlier described `defer` may be passed
optional `prototype` argument, in order to make returned promise and all
the subsequent promises decedents of that `prototype`:

    let { promise, resolve } = defer({
      get: function get(name) {
        return this.then(function(value) {
          return value[name];
        })
      }
    });

    promise.get('foo').get('bar').then(console.log);
    resolve({ foo: { bar: 'taram !!' } });

    // => 'taram !!'

Also `promised` function maybe passed second optional `prototype` argument to
achieve same effect.

## Treat all values as promises

Module provides a simple function for wrapping values into promises:

    const { resolve } = require('sdk/core/promise');

    var a = resolve(5).then(function(value) {
      return value + 2
    });
    a.then(console.log);  // => 7

Also `resolve` not only takes values, but also promises. If you pass it
a promise it will return new identical one:

    const { resolve } = require('sdk/core/promise');

    resolve(resolve(resolve(3))).then(console.log); // => 3

If this construct may look strange at first, but it becomes quite handy
when writing functions that deal with both promises and values. In such
cases it's usually easier to wrap value into promise than branch on value
type:

    function or(a, b) {
      var second = resolve(b).then(function(bValue) { return !!bValue });
      return resolve(a).then(function(aValue) {
        return !!aValue || second;
      }, function() {
        return second;
      })
    }

*Note: We could not use `promised` function here, as they reject returned
promise if any of the given arguments is rejected.*

If you need to customize your promises even further you may pass `resolve` a
second optional `prototype` argument that will have same effect as with `defer`.

## Treat errors as promises

Now that we can create all kinds of eventual values, it's useful to have a
way to create eventual errors. Module exports `reject` exactly for that.
It takes anything as an argument and returns a promise that is rejected with
it.

    const { reject } = require('sdk/core/promise');

    var boom = reject(Error('boom!'));

    future(function() {
      return Math.random() < 0.5 ? boom : value
    })

As with rest of the APIs error may be given second optional `prototype`
argument to customize resulting promise to your needs.
