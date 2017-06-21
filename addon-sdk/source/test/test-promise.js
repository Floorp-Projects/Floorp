/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Cc, Cu, Ci } = require('chrome');
const { setTimeout } = require('sdk/timers');
const { prefixURI, name } = require('@loader/options');
const addonPromiseURI = prefixURI + name + '/lib/sdk/core/promise.js';
const builtPromiseURI = 'resource://gre/modules/commonjs/sdk/core/promise.js';
var { Promise, defer, resolve, reject, all, promised } = require('sdk/core/promise');

exports['test all observers are notified'] = function(assert, done) {
  let expected = 'Taram pam param!';
  let deferred = defer();
  let pending = 10, i = 0;

  function resolved(value) {
    assert.equal(value, expected, 'value resolved as expected: #' + pending);
    if (!--pending) done();
  }

  while (i++ < pending) deferred.promise.then(resolved);

  deferred.resolve(expected);
};

exports['test exceptions dont stop notifications'] = function(assert, done) {
  let threw = false, boom = Error('Boom!');
  let deferred = defer();

  let promise2 = deferred.promise.then(function() {
    threw = true;
    throw boom;
  });

  deferred.promise.then(function() {
    assert.ok(threw, 'observer is called even though previos one threw');
    promise2.then(function() {
      assert.fail('should not resolve');
    }, function(reason) {
      assert.equal(reason, boom, 'rejects to thrown error');
      done();
    });
  });

  deferred.resolve('go!');
};

exports['test subsequent resolves are ignored'] = function(assert, done) {
  let deferred = defer();
  deferred.resolve(1);
  deferred.resolve(2);
  deferred.reject(3);

  deferred.promise.then(function(actual) {
    assert.equal(actual, 1, 'resolves to first value');
  }, function() {
    assert.fail('must not reject');
  });
  deferred.promise.then(function(actual) {
    assert.equal(actual, 1, 'subsequent resolutions are ignored');
    done();
  }, function() {
    assert.fail('must not reject');
  });
};

exports['test subsequent rejections are ignored'] = function(assert, done) {
  let deferred = defer();
  deferred.reject(1);
  deferred.resolve(2);
  deferred.reject(3);

  deferred.promise.then(function(actual) {
    assert.fail('must not resolve');
  }, function(actual) {
    assert.equal(actual, 1, 'must reject to first');
  });
  deferred.promise.then(function(actual) {
    assert.fail('must not resolve');
  }, function(actual) {
    assert.equal(actual, 1, 'must reject to first');
    done();
  });
};

exports['test error recovery'] = function(assert, done) {
  let boom = Error('Boom!');
  let deferred = defer();

  deferred.promise.then(function() {
    assert.fail('rejected promise should not resolve');
  }, function(reason) {
    assert.equal(reason, boom, 'rejection reason delivered');
    return 'recovery';
  }).then(function(value) {
    assert.equal(value, 'recovery', 'error handled by a handler');
    done();
  });

  deferred.reject(boom);
};

exports['test error recovery with promise'] = function(assert, done) {
  let deferred = defer();

  deferred.promise.then(function() {
    assert.fail('must reject');
  }, function(actual) {
    assert.equal(actual, 'reason', 'rejected');
    let deferred = defer();
    deferred.resolve('recovery');
    return deferred.promise;
  }).then(function(actual) {
    assert.equal(actual, 'recovery', 'recorvered via promise');
    let deferred = defer();
    deferred.reject('error');
    return deferred.promise;
  }).catch(function(actual) {
    assert.equal(actual, 'error', 'rejected via promise');
    let deferred = defer();
    deferred.reject('end');
    return deferred.promise;
  }).catch(function(actual) {
    assert.equal(actual, 'end', 'rejeced via promise');
    done();
  });

  deferred.reject('reason');
};

exports['test propagation'] = function(assert, done) {
  let d1 = defer(), d2 = defer(), d3 = defer();

  d1.promise.then(function(actual) {
    assert.equal(actual, 'expected', 'resolves to expected value');
    done();
  });

  d1.resolve(d2.promise);
  d2.resolve(d3.promise);
  d3.resolve('expected');
};

exports['test chaining'] = function(assert, done) {
  let boom = Error('boom'), brax = Error('braxXXx');
  let deferred = defer();

  deferred.promise.then().then().then(function(actual) {
    assert.equal(actual, 2, 'value propagates unchanged');
    return actual + 2;
  }).catch(function(reason) {
    assert.fail('should not reject');
  }).then(function(actual) {
    assert.equal(actual, 4, 'value propagates through if not handled');
    throw boom;
  }).then(function(actual) {
    assert.fail('exception must reject promise');
  }).then().catch(function(actual) {
    assert.equal(actual, boom, 'reason propagates unchanged');
    throw brax;
  }).then().catch(function(actual) {
    assert.equal(actual, brax, 'reason changed becase of exception');
    return 'recovery';
  }).then(function(actual) {
    assert.equal(actual, 'recovery', 'recovered from error');
    done();
  });

  deferred.resolve(2);
};

exports['test reject'] = function(assert, done) {
  let expected = Error('boom');

  reject(expected).then(function() {
    assert.fail('should reject');
  }, function(actual) {
    assert.equal(actual, expected, 'rejected with expected reason');
  }).then(done, assert.fail);
};

exports['test resolve to rejected'] = function(assert, done) {
  let expected = Error('boom');
  let deferred = defer();

  deferred.promise.then(function() {
    assert.fail('should reject');
  }, function(actual) {
    assert.equal(actual, expected, 'rejected with expected failure');
  }).then(done, assert.fail);

  deferred.resolve(reject(expected));
};

exports['test resolve'] = function(assert, done) {
  let expected = 'value';
  resolve(expected).then(function(actual) {
    assert.equal(actual, expected, 'resolved as expected');
  }).catch(assert.fail).then(done);
};

exports['test promised with normal args'] = function(assert, done) {
  let sum = promised((x, y) => x + y );

  sum(7, 8).then(function(actual) {
    assert.equal(actual, 7 + 8, 'resolves as expected');
  }).catch(assert.fail).then(done);
};

exports['test promised with promise args'] = function(assert, done) {
  let sum = promised((x, y) => x + y );
  let deferred = defer();

  sum(11, deferred.promise).then(function(actual) {
    assert.equal(actual, 11 + 24, 'resolved as expected');
  }).catch(assert.fail).then(done);

  deferred.resolve(24);
};

exports['test promised error handling'] = function(assert, done) {
  let expected = Error('boom');
  let f = promised(function() {
    throw expected;
  });

  f().then(function() {
    assert.fail('should reject');
  }, function(actual) {
    assert.equal(actual, expected, 'rejected as expected');
  }).catch(assert.fail).then(done);
};

exports['test errors in promise resolution handlers are propagated'] = function(assert, done) {
  var expected = Error('Boom');
  var { promise, resolve } = defer();

  promise.then(function() {
    throw expected;
  }).then(function() {
    return undefined;
  }).catch(function(actual) {
    assert.equal(actual, expected, 'rejected as expected');
  }).then(done, assert.fail);

  resolve({});
};

exports['test return promise form promised'] = function(assert, done) {
  let f = promised(function() {
    return resolve(17);
  });

  f().then(function(actual) {
    assert.equal(actual, 17, 'resolves to a promise resolution');
  }).catch(assert.fail).then(done);
};

exports['test promised returning failure'] = function(assert, done) {
  let expected = Error('boom');
  let f = promised(function() {
    return reject(expected);
  });

  f().then(function() {
    assert.fail('must reject');
  }, function(actual) {
    assert.equal(actual, expected, 'rejects with expected reason');
  }).catch(assert.fail).then(done);
};

/*
 * Changed for compliance in Bug 881047, promises are now always async
 */
exports['test promises are always async'] = function (assert, done) {
  let runs = 0;
  resolve(1)
    .then(val => ++runs)
    .catch(assert.fail).then(done);
  assert.equal(runs, 0, 'resolutions are called in following tick');
};

/*
 * Changed for compliance in Bug 881047, promised's are now non greedy
 */
exports['test promised are not greedy'] = function(assert, done) {
  let runs = 0;
  promised(() => ++runs)()
    .catch(assert.fail).then(done);
  assert.equal(runs, 0, 'promised does not run task right away');
};

exports['test promised does not flatten arrays'] = function(assert, done) {
  let p = promised(function(empty, one, two, nested) {
    assert.equal(empty.length, 0, "first argument is empty");
    assert.deepEqual(one, ['one'], "second has one");
    assert.deepEqual(two, ['two', 'more'], "third has two more");
    assert.deepEqual(nested, [[]], "forth is properly nested");
    done();
  });

  p([], ['one'], ['two', 'more'], [[]]);
};

exports['test arrays should not flatten'] = function(assert, done) {
  let a = defer();
  let b = defer();

  let combine = promised(function(str, arr) {
    assert.equal(str, 'Hello', 'Array was not flattened');
    assert.deepEqual(arr, [ 'my', 'friend' ]);
  });

  combine(a.promise, b.promise).catch(assert.fail).then(done);


  a.resolve('Hello');
  b.resolve([ 'my', 'friend' ]);
};

exports['test `all` for all promises'] = function (assert, done) {
  all([
    resolve(5), resolve(7), resolve(10)
  ]).then(function (val) {
    assert.equal(
      val[0] === 5 &&
      val[1] === 7 &&
      val[2] === 10
    , true, 'return value contains resolved promises values');
    done();
  }, function () {
    assert.fail('should not call reject function');
  });
};

exports['test `all` aborts upon first reject'] = function (assert, done) {
  all([
    resolve(5), reject('error'), delayedResolve()
  ]).then(function (val) {
    assert.fail('Successful resolve function should not be called');
  }, function (reason) {
    assert.equal(reason, 'error', 'should reject the `all` promise');
    done();
  });

  function delayedResolve () {
    let deferred = defer();
    setTimeout(deferred.resolve, 50);
    return deferred.promise;
  }
};

exports['test `all` with array containing non-promise'] = function (assert, done) {
  all([
    resolve(5), resolve(10), 925
  ]).then(function (val) {
    assert.equal(val[2], 925, 'non-promises should pass-through value');
    done();
  }, function () {
    assert.fail('should not be rejected');
  });
};

exports['test `all` should resolve with an empty array'] = function (assert, done) {
  all([]).then(function (val) {
    assert.equal(Array.isArray(val), true, 'should return array in resolved');
    assert.equal(val.length, 0, 'array should be empty in resolved');
    done();
  }, function () {
    assert.fail('should not be rejected');
  });
};

exports['test `all` with multiple rejected'] = function (assert, done) {
  all([
    reject('error1'), reject('error2'), reject('error3')
  ]).then(function (value) {
    assert.fail('should not be successful');
  }, function (reason) {
    assert.equal(reason, 'error1', 'should reject on first promise reject');
    done();
  });
};

exports['test Promise constructor resolve'] = function (assert, done) {
  var isAsync = true;
  new Promise(function (resolve, reject) {
    resolve(5);
  }).then(x => {
    isAsync = false;
    assert.equal(x, 5, 'Promise constructor resolves correctly');
  }).catch(assert.fail).then(done);
  assert.ok(isAsync, 'Promise constructor runs async');
};

exports['test Promise constructor reject'] = function (assert, done) {
  new Promise(function (resolve, reject) {
    reject(new Error('deferred4life'));
  }).then(assert.fail, (err) => {
    assert.equal(err.message, 'deferred4life', 'Promise constructor rejects correctly');
  }).catch(assert.fail).then(done);
};

exports['test JSM Load and API'] = function (assert, done) {
  // Use addon URL when loading from cfx/local:
  // resource://90111c90-c31e-4dc7-ac35-b65947434435-at-jetpack/addon-sdk/lib/sdk/core/promise.js
  // Use built URL when testing on try, etc.
  // resource://gre/modules/commonjs/sdk/core/promise.js
  try {
    var { Promise } = Cu.import(addonPromiseURI, {});
  } catch (e) {
    var { Promise } = Cu.import(builtPromiseURI, {});
  }
  testEnvironment(Promise, assert, done, 'JSM');
};

exports['test mozIJSSubScriptLoader exporting'] = function (assert, done) {
  let { Services } = Cu.import('resource://gre/modules/Services.jsm', {});
  let systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
  let Promise = new Cu.Sandbox(systemPrincipal);
  let loader = Cc['@mozilla.org/moz/jssubscript-loader;1']
                 .getService(Ci.mozIJSSubScriptLoader);

  // Use addon URL when loading from cfx/local:
  // resource://90111c90-c31e-4dc7-ac35-b65947434435-at-jetpack/addon-sdk/lib/sdk/core/promise.js
  // Use built URL when testing on try, etc.
  // resource://gre/modules/commonjs/sdk/core/promise.js
  try {
    loader.loadSubScript(addonPromiseURI, Promise);
  } catch (e) {
    loader.loadSubScript(builtPromiseURI, Promise);
  }

  testEnvironment(Promise, assert, done, 'mozIJSSubScript');
};

function testEnvironment ({all, resolve, defer, reject, promised}, assert, done, type) {
  all([resolve(5), resolve(10), 925]).then(val => {
    assert.equal(val[0], 5, 'promise#all works ' + type);
    assert.equal(val[1], 10, 'promise#all works ' + type);
    assert.equal(val[2], 925, 'promise#all works ' + type);
    return resolve(1000);
  }).then(value => {
    assert.equal(value, 1000, 'promise#resolve works ' + type);
    return reject('testing reject');
  }).catch(reason => {
    assert.equal(reason, 'testing reject', 'promise#reject works ' + type);
    let deferred = defer();
    setTimeout(() => deferred.resolve('\\m/'), 10);
    return deferred.promise;
  }).then(value => {
    assert.equal(value, '\\m/', 'promise#defer works ' + type);
    return promised(x => x * x)(5);
  }).then(value => {
    assert.equal(value, 25, 'promise#promised works ' + type);
  }).then(done, assert.fail);
}

require("sdk/test").run(exports);
