/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

'use strict';

/*
 * Uses `Promise.jsm` as a core implementation, with additional sugar
 * from previous implementation, with inspiration from `Q` and `when`
 *
 * https://developer.mozilla.org/en-US/docs/Mozilla/JavaScript_code_modules/Promise.jsm
 * https://github.com/cujojs/when
 * https://github.com/kriskowal/q
 */
const PROMISE_URI = 'resource://gre/modules/Promise.jsm';

getEnvironment.call(this, function ({ require, exports, module, Cu }) {

const { defer, resolve, all, reject, race } = Cu.import(PROMISE_URI, {}).Promise;

module.metadata = {
  'stability': 'unstable'
};

let promised = (function() {
  // Note: Define shortcuts and utility functions here in order to avoid
  // slower property accesses and unnecessary closure creations on each
  // call of this popular function.

  var call = Function.call;
  var concat = Array.prototype.concat;

  // Utility function that does following:
  // execute([ f, self, args...]) => f.apply(self, args)
  function execute (args) call.apply(call, args)

  // Utility function that takes promise of `a` array and maybe promise `b`
  // as arguments and returns promise for `a.concat(b)`.
  function promisedConcat(promises, unknown) {
    return promises.then(function (values) {
      return resolve(unknown)
        .then(function (value) values.concat([value]));
    });
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
        then(execute);
    };
  };
})();

exports.promised = promised;
exports.all = all;
exports.defer = defer;
exports.resolve = resolve;
exports.reject = reject;
exports.race = race;
exports.Promise = Promise;

});

function getEnvironment (callback) {
  let Cu, _exports, _module, _require;

  // CommonJS / SDK
  if (typeof(require) === 'function') {
    Cu = require('chrome').Cu;
    _exports = exports;
    _module = module;
    _require = require;
  }
  // JSM
  else if (String(this).indexOf('BackstagePass') >= 0) {
    Cu = this['Components'].utils;
    _exports = this.Promise = {};
    _module = { uri: __URI__, id: 'promise/core' };
    _require = uri => {
      let imports = {};
      Cu.import(uri, imports);
      return imports;
    };
    this.EXPORTED_SYMBOLS = ['Promise'];
  // mozIJSSubScriptLoader.loadSubscript
  } else if (~String(this).indexOf('Sandbox')) {
    Cu = this['Components'].utils;
    _exports = this;
    _module = { id: 'promise/core' };
    _require = uri => {};
  }

  callback({
    Cu: Cu,
    exports: _exports,
    module: _module,
    require: _require
  });
}

