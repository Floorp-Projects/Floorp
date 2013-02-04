/* vim:set ts=2 sw=2 sts=2 expandtab */
/*jshint asi: true undef: true es5: true node: true devel: true
         forin: true */
/*global define: true */

'use strict';

var core = require('sdk/core/promise'),
    defer = core.defer, resolve = core.resolve, reject = core.reject,
    promised = core.promised

exports['test all observers are notified'] = function(assert, done) {
  var expected = 'Taram pam param!'
  var deferred = defer()
  var pending = 10, i = 0

  function resolved(value) {
    assert.equal(value, expected, 'value resolved as expected: #' + pending)
    if (!--pending) done()
  }

  while (i++ < pending) deferred.promise.then(resolved)

  deferred.resolve(expected)
}

exports['test exceptions dont stop notifications'] = function(assert, done) {
  var threw = false, boom = Error('Boom!')
  var deferred = defer()

  var promise2 = deferred.promise.then(function() {
    threw = true
    throw boom
  })

  deferred.promise.then(function() {
    assert.ok(threw, 'observer is called even though previos one threw')
    promise2.then(function() {
      assert.fail('should not resolve')
    }, function(reason) {
      assert.equal(reason, boom, 'rejects to thrown error')
      done()
    })
  })

  deferred.resolve('go!')
}

exports['test subsequent resolves are ignored'] = function(assert, done) {
  var deferred = defer()
  deferred.resolve(1)
  deferred.resolve(2)
  deferred.reject(3)

  deferred.promise.then(function(actual) {
    assert.equal(actual, 1, 'resolves to first value')
  }, function() {
    assert.fail('must not reject')
  })
  deferred.promise.then(function(actual) {
    assert.equal(actual, 1, 'subsequent resolutions are ignored')
    done()
  }, function() {
    assert.fail('must not reject')
  })
}

exports['test subsequent rejections are ignored'] = function(assert, done) {
  var deferred = defer()
  deferred.reject(1)
  deferred.resolve(2)
  deferred.reject(3)

  deferred.promise.then(function(actual) {
    assert.fail('must not resolve')
  }, function(actual) {
    assert.equal(actual, 1, 'must reject to first')
  })
  deferred.promise.then(function(actual) {
    assert.fail('must not resolve')
  }, function(actual) {
    assert.equal(actual, 1, 'must reject to first')
    done()
  })
}

exports['test error recovery'] = function(assert, done) {
  var boom = Error('Boom!')
  var deferred = defer()

  deferred.promise.then(function() {
    assert.fail('rejected promise should not resolve')
  }, function(reason) {
    assert.equal(reason, boom, 'rejection reason delivered')
    return 'recovery'
  }).then(function(value) {
    assert.equal(value, 'recovery', 'error handled by a handler')
    done()
  })

  deferred.reject(boom)
}


exports['test error recovery with promise'] = function(assert, done) {
  var deferred = defer()

  deferred.promise.then(function() {
    assert.fail('must reject')
  }, function(actual) {
    assert.equal(actual, 'reason', 'rejected')
    var deferred = defer()
    deferred.resolve('recovery')
    return deferred.promise
  }).then(function(actual) {
    assert.equal(actual, 'recovery', 'recorvered via promise')
    var deferred = defer()
    deferred.reject('error')
    return deferred.promise
  }).then(null, function(actual) {
    assert.equal(actual, 'error', 'rejected via promise')
    var deferred = defer()
    deferred.reject('end')
    return deferred.promise
  }).then(null, function(actual) {
    assert.equal(actual, 'end', 'rejeced via promise')
    done()
  })

  deferred.reject('reason')
}

exports['test propagation'] = function(assert, done) {
  var d1 = defer(), d2 = defer(), d3 = defer()

  d1.promise.then(function(actual) {
    assert.equal(actual, 'expected', 'resolves to expected value')
    done()
  })

  d1.resolve(d2.promise)
  d2.resolve(d3.promise)
  d3.resolve('expected')
}

exports['test chaining'] = function(assert, done) {
  var boom = Error('boom'), brax = Error('braxXXx')
  var deferred = defer()

  deferred.promise.then().then().then(function(actual) {
    assert.equal(actual, 2, 'value propagates unchanged')
    return actual + 2
  }).then(null, function(reason) {
    assert.fail('should not reject')
  }).then(function(actual) {
    assert.equal(actual, 4, 'value propagates through if not handled')
    throw boom
  }).then(function(actual) {
    assert.fail('exception must reject promise')
  }).then().then(null, function(actual) {
    assert.equal(actual, boom, 'reason propagates unchanged')
    throw brax
  }).then().then(null, function(actual) {
    assert.equal(actual, brax, 'reason changed becase of exception')
    return 'recovery'
  }).then(function(actual) {
    assert.equal(actual, 'recovery', 'recovered from error')
    done()
  })

  deferred.resolve(2)
}


exports['test reject'] = function(assert, done) {
  var expected = Error('boom')

  reject(expected).then(function() {
    assert.fail('should reject')
  }, function(actual) {
    assert.equal(actual, expected, 'rejected with expected reason')
  }).then(function() {
    done()
  })
}

exports['test resolve to rejected'] = function(assert, done) {
  var expected = Error('boom')
  var deferred = defer()

  deferred.promise.then(function() {
    assert.fail('should reject')
  }, function(actual) {
    assert.equal(actual, expected, 'rejected with expected failure')
  }).then(function() {
    done()
  })

  deferred.resolve(reject(expected))
}

exports['test resolve'] = function(assert, done) {
  var expected = 'value'
  resolve(expected).then(function(actual) {
    assert.equal(actual, expected, 'resolved as expected')
  }).then(function() {
    done()
  })
}

exports['test resolve with prototype'] = function(assert, done) {
  var seventy = resolve(70, {
    subtract: function subtract(y) {
      return this.then(function(x) { return x - y })
    }
  })

  seventy.subtract(17).then(function(actual) {
    assert.equal(actual, 70 - 17, 'resolves to expected')
    done()
  })
}

exports['test promised with normal args'] = function(assert, done) {
  var sum = promised(function(x, y) { return x + y })

  sum(7, 8).then(function(actual) {
    assert.equal(actual, 7 + 8, 'resolves as expected')
    done()
  })
}

exports['test promised with promise args'] = function(assert, done) {
  var sum = promised(function(x, y) { return x + y })
  var deferred = defer()

  sum(11, deferred.promise).then(function(actual) {
    assert.equal(actual, 11 + 24, 'resolved as expected')
    done()
  })

  deferred.resolve(24)
}

exports['test promised with prototype'] = function(assert, done) {
  var deferred = defer()
  var numeric = {}
  numeric.subtract = promised(function(y) { return this - y }, numeric)

  var sum = promised(function(x, y) { return x + y }, numeric)

  sum(7, 70).
    subtract(14).
    subtract(deferred.promise).
    subtract(5).
    then(function(actual) {
      assert.equal(actual, 7 + 70 - 14 - 23 - 5, 'resolved as expected')
      done()
    })

  deferred.resolve(23)
}

exports['test promised error handleing'] = function(assert, done) {
  var expected = Error('boom')
  var f = promised(function() {
    throw expected
  })

  f().then(function() {
    assert.fail('should reject')
  }, function(actual) {
    assert.equal(actual, expected, 'rejected as expected')
    done()
  })
}

exports['test return promise form promised'] = function(assert, done) {
  var f = promised(function() {
    return resolve(17)
  })

  f().then(function(actual) {
    assert.equal(actual, 17, 'resolves to a promise resolution')
    done()
  })
}

exports['test promised returning failure'] = function(assert, done) {
  var expected = Error('boom')
  var f = promised(function() {
    return reject(expected)
  })

  f().then(function() {
    assert.fail('must reject')
  }, function(actual) {
    assert.equal(actual, expected, 'rejects with expected reason')
    done()
  })
}

exports['test promised are greedy'] = function(assert, done) {
  var runs = 0
  var f = promised(function() { ++runs })
  var promise = f()
  assert.equal(runs, 1, 'promised runs task right away')
  done()
}

exports['test arrays should not flatten'] = function(assert, done) {
  var a = defer()
  var b = defer()

  var combine = promised(function(str, arr) {
    assert.equal(str, 'Hello', 'Array was not flattened')
    assert.deepEqual(arr, [ 'my', 'friend' ])
  })

  combine(a.promise, b.promise).then(done)


  a.resolve('Hello')
  b.resolve([ 'my', 'friend' ])
}

require("test").run(exports)
