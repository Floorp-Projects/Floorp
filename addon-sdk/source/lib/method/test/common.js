"use strict";

var Method = require("../core")

function type(value) {
  return Object.prototype.toString.call(value).
    split(" ").
    pop().
    split("]").
    shift().
    toLowerCase()
}

var values = [
  null,                   // 0
  undefined,              // 1
  Infinity,               // 2
  NaN,                    // 3
  5,                      // 4
  {},                     // 5
  Object.create({}),      // 6
  Object.create(null),    // 7
  [],                     // 8
  /foo/,                  // 9
  new Date(),             // 10
  Function,               // 11
  function() {},          // 12
  true,                   // 13
  false,                  // 14
  "string"                // 15
]

function True() { return true }
function False() { return false }

var trues = values.map(True)
var falses = values.map(False)

exports["test throws if not implemented"] = function(assert) {
  var method = Method("nope")

  assert.throws(function() {
    method({})
  }, /not implement/i, "method throws if not implemented")

  assert.throws(function() {
    method(null)
  }, /not implement/i, "method throws on null")
}

exports["test all types inherit from default"] = function(assert) {
  var isImplemented = Method("isImplemented")
  isImplemented.define(function() { return true })

  values.forEach(function(value) {
    assert.ok(isImplemented(value),
              type(value) + " inherits deafult implementation")
  })
}

exports["test default can be implemented later"] = function(assert) {
  var isImplemented = Method("isImplemented")
  isImplemented.define(function() {
    return true
  })

  values.forEach(function(value) {
    assert.ok(isImplemented(value),
              type(value) + " inherits deafult implementation")
  })
}

exports["test dispatch not-implemented"] = function(assert) {
  var isDefault = Method("isDefault")
  values.forEach(function(value) {
    assert.throws(function() {
      isDefault(value)
    }, /not implement/, type(value) + " throws if not implemented")
  })
}

exports["test dispatch default"] = function(assert) {
  var isDefault = Method("isDefault")

  // Implement default
  isDefault.define(True)
  assert.deepEqual(values.map(isDefault), trues,
                   "all implementation inherit from default")

}

exports["test dispatch null"] = function(assert) {
  var isNull = Method("isNull")

  // Implement default
  isNull.define(False)
  isNull.define(null, True)
  assert.deepEqual(values.map(isNull),
                   [ true ].
                   concat(falses.slice(1)),
                   "only null gets methods defined for null")
}

exports["test dispatch undefined"] = function(assert) {
  var isUndefined = Method("isUndefined")

  // Implement default
  isUndefined.define(False)
  isUndefined.define(undefined, True)
  assert.deepEqual(values.map(isUndefined),
                   [ false, true ].
                   concat(falses.slice(2)),
                   "only undefined gets methods defined for undefined")
}

exports["test dispatch object"] = function(assert) {
  var isObject = Method("isObject")

  // Implement default
  isObject.define(False)
  isObject.define(Object, True)
  assert.deepEqual(values.map(isObject),
                   [ false, false, false, false, false ].
                   concat(trues.slice(5, 13)).
                   concat([false, false, false]),
                   "all values except primitives inherit Object methods")

}

exports["test dispatch number"] = function(assert) {
  var isNumber = Method("isNumber")
  isNumber.define(False)
  isNumber.define(Number, True)

  assert.deepEqual(values.map(isNumber),
                  falses.slice(0, 2).
                  concat(true, true, true).
                  concat(falses.slice(5)),
                  "all numbers inherit from Number method")
}

exports["test dispatch string"] = function(assert) {
  var isString = Method("isString")
  isString.define(False)
  isString.define(String, True)

  assert.deepEqual(values.map(isString),
                  falses.slice(0, 15).
                  concat(true),
                  "all strings inherit from String method")
}

exports["test dispatch function"] = function(assert) {
  var isFunction = Method("isFunction")
  isFunction.define(False)
  isFunction.define(Function, True)

  assert.deepEqual(values.map(isFunction),
                  falses.slice(0, 11).
                  concat(true, true).
                  concat(falses.slice(13)),
                  "all functions inherit from Function method")
}

exports["test dispatch date"] = function(assert) {
  var isDate = Method("isDate")
  isDate.define(False)
  isDate.define(Date, True)

  assert.deepEqual(values.map(isDate),
                  falses.slice(0, 10).
                  concat(true).
                  concat(falses.slice(11)),
                  "all dates inherit from Date method")
}

exports["test dispatch RegExp"] = function(assert) {
  var isRegExp = Method("isRegExp")
  isRegExp.define(False)
  isRegExp.define(RegExp, True)

  assert.deepEqual(values.map(isRegExp),
                  falses.slice(0, 9).
                  concat(true).
                  concat(falses.slice(10)),
                  "all regexps inherit from RegExp method")
}

exports["test redefine for descendant"] = function(assert) {
  var isFoo = Method("isFoo")
  var ancestor = {}
  isFoo.implement(ancestor, function() { return true })
  var descendant = Object.create(ancestor)
  isFoo.implement(descendant, function() { return false })

  assert.ok(isFoo(ancestor), "defined on ancestor")
  assert.ok(!isFoo(descendant), "overrided for descendant")
}

exports["test on custom types"] = function(assert) {
  function Bar() {}
  var isBar = Method("isBar")

  isBar.define(function() { return false })
  isBar.define(Bar, function() { return true })

  assert.ok(!isBar({}), "object is get's default implementation")
  assert.ok(isBar(new Bar()), "Foo type objects get own implementation")

  var isObject = Method("isObject")
  isObject.define(function() { return false })
  isObject.define(Object, function() { return true })

  assert.ok(isObject(new Bar()), "foo inherits implementation from object")


  isObject.define(Bar, function() { return false })

  assert.ok(!isObject(new Bar()),
            "implementation inherited form object can be overrided")
}


exports["test error types"] = function(assert) {
  var isError = Method("isError")
  isError.define(function() { return false })
  isError.define(Error, function() { return true })

  assert.ok(isError(Error("boom")), "error is error")
  assert.ok(isError(TypeError("boom")), "type error is an error")
  assert.ok(isError(EvalError("boom")), "eval error is an error")
  assert.ok(isError(RangeError("boom")), "range error is an error")
  assert.ok(isError(ReferenceError("boom")), "reference error is an error")
  assert.ok(isError(SyntaxError("boom")), "syntax error is an error")
  assert.ok(isError(URIError("boom")), "URI error is an error")
}

exports["test override define polymorphic method"] = function(assert) {
  var define = Method.define
  var implement = Method.implement

  var fn = Method("fn")
  var methods = {}
  implement(define, fn, function(method, label, implementation) {
    methods[label] = implementation
  })

  function foo() {}

  define(fn, "foo-case", foo)

  assert.equal(methods["foo-case"], foo, "define set property")
}

exports["test override define via method API"] = function(assert) {
  var define = Method.define
  var implement = Method.implement

  var fn = Method("fn")
  var methods = {}
  define.implement(fn, function(method, label, implementation) {
    methods[label] = implementation
  })

  function foo() {}

  define(fn, "foo-case", foo)

  assert.equal(methods["foo-case"], foo, "define set property")
}

require("test").run(exports)
