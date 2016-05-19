/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * THIS MODULE IS DEPRECATED. IMPORT "Promise.jsm" INSTEAD.
 */

"use strict";

this.Promise = {};

if (typeof (require) === "function") {
  module.exports = Promise;
} else {
  this.EXPORTED_SYMBOLS = ["Promise"];
}

function fulfilled(value) {
  return { then: function then(fulfill) { fulfill(value); } };
}

function rejected(reason) {
  return { then: function then(fulfill, reject) { reject(reason); } };
}

function isPromise(value) {
  return value && typeof (value.then) === "function";
}

function defer() {
  var observers = [];
  var result = null;
  var promise = {
    then: function then(onFulfill, onError) {
      var deferred = defer();

      function resolve(value) {
        try {
          deferred.resolve(onFulfill ? onFulfill(value) : value);
        } catch (error) {
          deferred.resolve(rejected(error));
        }
      }

      function reject(reason) {
        try {
          if (onError) deferred.resolve(onError(reason));
          else deferred.resolve(rejected(reason));
        } catch (error) {
          deferred.resolve(rejected(error));
        }
      }

      if (observers) {
        observers.push({ resolve: resolve, reject: reject });
      } else {
        result.then(resolve, reject);
      }

      return deferred.promise;
    }
  };

  var deferred = {
    promise: promise,
    resolve: function resolve(value) {
      if (!result) {
        result = isPromise(value) ? value : fulfilled(value);
        while (observers.length) {
          var observer = observers.shift();
          result.then(observer.resolve, observer.reject);
        }
        observers = null;
      }
    },
    reject: function reject(reason) {
      deferred.resolve(rejected(reason));
    }
  };

  return deferred;
}
Promise.defer = defer;

function resolve(value) {
  var deferred = defer();
  deferred.resolve(value);
  return deferred.promise;
}
Promise.resolve = resolve;

function reject(reason) {
  var deferred = defer();
  deferred.reject(reason);
  return deferred.promise;
}
Promise.reject = reject;

var promised = (function () {
  var call = Function.call;
  var concat = Array.prototype.concat;
  function execute(args) { return call.apply(call, args); }
  function promisedConcat(promises, unknown) {
    return promises.then(function (values) {
      return resolve(unknown).then(function (value) {
        return values.concat([ value ]);
      });
    });
  }
  return function promised(f) {
    return function promised() {
      return concat.apply([ f, this ], arguments).
        reduce(promisedConcat, resolve([])).
        then(execute);
    };
  };
})();
Promise.all = promised(Array);
