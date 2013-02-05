/* vim:set ts=2 sw=2 sts=2 expandtab */
/*jshint asi: true undef: true es5: true node: true browser: true devel: true
         forin: true latedef: false */
/*global define: true, Cu: true, __URI__: true */
;(function(id, factory) { // Module boilerplate :(
  if (typeof(define) === 'function') { // RequireJS
    define(factory);
  } else if (typeof(require) === 'function') { // CommonJS
    factory.call(this, require, exports, module);
  } else if (~String(this).indexOf('BackstagePass')) { // JSM
    this[factory.name] = {};
    factory(function require(uri) {
      var imports = {};
      this['Components'].utils.import(uri, imports);
      return imports;
    }, this[factory.name], { uri: __URI__, id: id });
    this.EXPORTED_SYMBOLS = [factory.name];
  } else {  // Browser or alike
    var globals = this
    factory(function require(id) {
      return globals[id];
    }, (globals[id] = {}), { uri: document.location.href + '#' + id, id: id });
  }
}).call(this, 'loader', function Promise(require, exports, module) {

'use strict';

module.metadata = {
  "stability": "unstable"
};

function resolution(value) {
  /**
  Returns non-standard compliant (`then` does not returns a promise) promise
  that resolves to a given `value`. Used just internally only.
  **/
  return { then: function then(resolve) { resolve(value) } }
}

function rejection(reason) {
  /**
  Returns non-standard compliant promise (`then` does not returns a promise)
  that rejects with a given `reason`. This is used internally only.
  **/
  return { then: function then(resolve, reject) { reject(reason) } }
}

function attempt(f) {
  /**
  Returns wrapper function that delegates to `f`. If `f` throws then captures
  error and returns promise that rejects with a thrown error. Otherwise returns
  return value. (Internal utility)
  **/
  return function effort(options) {
    try { return f(options) }
    catch(error) { return rejection(error) }
  }
}

function isPromise(value) {
  /**
  Returns true if given `value` is promise. Value is assumed to be promise if
  it implements `then` method.
  **/
  return value && typeof(value.then) === 'function'
}

function defer(prototype) {
  /**
  Returns object containing following properties:
  - `promise` Eventual value representation implementing CommonJS [Promises/A]
    (http://wiki.commonjs.org/wiki/Promises/A) API.
  - `resolve` Single shot function that resolves returned `promise` with a given
    `value` argument.
  - `reject` Single shot function that rejects returned `promise` with a given
    `reason` argument.

  Given `prototype` argument is used as a prototype of the returned `promise`
  allowing one to implement additional API. If prototype is not passed then
  it falls back to `Object.prototype`.

  ## Examples

  // Simple usage.
  var deferred = defer()
  deferred.promise.then(console.log, console.error)
  deferred.resolve(value)

  // Advanced usage
  var prototype = {
    get: function get(name) {
      return this.then(function(value) {
        return value[name];
      })
    }
  }

  var foo = defer(prototype)
  deferred.promise.get('name').then(console.log)
  deferred.resolve({ name: 'Foo' })
  //=> 'Foo'
  */
  var pending = [], result
  prototype = (prototype || prototype === null) ? prototype : Object.prototype

  // Create an object implementing promise API.
  var promise = Object.create(prototype, {
    then: { value: function then(resolve, reject) {
      // create a new deferred using a same `prototype`.
      var deferred = defer(prototype)
      // If `resolve / reject` callbacks are not provided.
      resolve = resolve ? attempt(resolve) : resolution
      reject = reject ? attempt(reject) : rejection

      // Create a listeners for a enclosed promise resolution / rejection that
      // delegate to an actual callbacks and resolve / reject returned promise.
      function resolved(value) { deferred.resolve(resolve(value)) }
      function rejected(reason) { deferred.resolve(reject(reason)) }

      // If promise is pending register listeners. Otherwise forward them to
      // resulting resolution.
      if (pending) pending.push([ resolved, rejected ])
      else result.then(resolved, rejected)

      return deferred.promise
    }}
  })

  var deferred = {
    promise: promise,
    resolve: function resolve(value) {
      /**
      Resolves associated `promise` to a given `value`, unless it's already
      resolved or rejected.
      **/
      if (pending) {
        // store resolution `value` as a promise (`value` itself may be a
        // promise), so that all subsequent listeners can be forwarded to it,
        // which either resolves immediately or forwards if `value` is
        // a promise.
        result = isPromise(value) ? value : resolution(value)
        // forward all pending observers.
        while (pending.length) result.then.apply(result, pending.shift())
        // mark promise as resolved.
        pending = null
      }
    },
    reject: function reject(reason) {
      /**
      Rejects associated `promise` with a given `reason`, unless it's already
      resolved / rejected.
      **/
      deferred.resolve(rejection(reason))
    }
  }

  return deferred
}
exports.defer = defer

function resolve(value, prototype) {
  /**
  Returns a promise resolved to a given `value`. Optionally second `prototype`
  arguments my be provided to be used as a prototype for a returned promise.
  **/
  var deferred = defer(prototype)
  deferred.resolve(value)
  return deferred.promise
}
exports.resolve = resolve

function reject(reason, prototype) {
  /**
  Returns a promise that is rejected with a given `reason`. Optionally second
  `prototype` arguments my be provided to be used as a prototype for a returned
  promise.
  **/
  var deferred = defer(prototype)
  deferred.reject(reason)
  return deferred.promise
}
exports.reject = reject

var promised = (function() {
  // Note: Define shortcuts and utility functions here in order to avoid
  // slower property accesses and unnecessary closure creations on each
  // call of this popular function.

  var call = Function.call
  var concat = Array.prototype.concat

  // Utility function that does following:
  // execute([ f, self, args...]) => f.apply(self, args)
  function execute(args) { return call.apply(call, args) }

  // Utility function that takes promise of `a` array and maybe promise `b`
  // as arguments and returns promise for `a.concat(b)`.
  function promisedConcat(promises, unknown) {
    return promises.then(function(values) {
      return resolve(unknown).then(function(value) {
        return values.concat([ value ])
      })
    })
  }

  return function promised(f, prototype) {
    /**
    Returns a wrapped `f`, which when called returns a promise that resolves to
    `f(...)` passing all the given arguments to it, which by the way may be
    promises. Optionally second `prototype` argument may be provided to be used
    a prototype for a returned promise.

    ## Example

    var promise = promised(Array)(1, promise(2), promise(3))
    promise.then(console.log) // => [ 1, 2, 3 ]
    **/

    return function promised() {
      // create array of [ f, this, args... ]
      return concat.apply([ f, this ], arguments).
        // reduce it via `promisedConcat` to get promised array of fulfillments
        reduce(promisedConcat, resolve([], prototype)).
        // finally map that to promise of `f.apply(this, args...)`
        then(execute)
    }
  }
})()
exports.promised = promised

})
