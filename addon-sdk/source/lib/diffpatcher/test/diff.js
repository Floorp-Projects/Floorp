"use strict";

var diff = require("../diff")

exports["test diff from null"] = function(assert) {
  var to = { a: 1, b: 2 }
  assert.equal(diff(null, to), to, "diff null to x returns x")
  assert.equal(diff(void(0), to), to, "diff undefined to x returns x")

}

exports["test diff to null"] = function(assert) {
  var from = { a: 1, b: 2 }
  assert.deepEqual(diff({ a: 1, b: 2 }, null),
                   { a: null, b: null },
                   "diff x null returns x with all properties nullified")
}

exports["test diff identical"] = function(assert) {
  assert.deepEqual(diff({}, {}), {}, "diff on empty objects is {}")

  assert.deepEqual(diff({ a: 1, b: 2 }, { a: 1, b: 2 }), {},
                   "if properties match diff is {}")

  assert.deepEqual(diff({ a: 1, b: { c: { d: 3, e: 4 } } },
                        { a: 1, b: { c: { d: 3, e: 4 } } }), {},
                   "diff between identical nested hashes is {}")

}

exports["test diff delete"] = function(assert) {
  assert.deepEqual(diff({ a: 1, b: 2 }, { b: 2 }), { a: null },
                   "missing property is deleted")
  assert.deepEqual(diff({ a: 1, b: 2 }, { a: 2 }), { a: 2, b: null },
                   "missing property is deleted another updated")
  assert.deepEqual(diff({ a: 1, b: 2 }, {}), { a: null, b: null },
                   "missing propertes are deleted")
  assert.deepEqual(diff({ a: 1, b: { c: { d: 2 } } }, {}),
                   { a: null, b: null },
                   "missing deep propertes are deleted")
  assert.deepEqual(diff({ a: 1, b: { c: { d: 2 } } }, { b: { c: {} } }),
                   { a: null, b: { c: { d: null } } },
                   "missing nested propertes are deleted")
}

exports["test add update"] = function(assert) {
  assert.deepEqual(diff({ a: 1, b: 2 }, { b: 2, c: 3 }), { a: null, c: 3 },
                   "delete and add")
  assert.deepEqual(diff({ a: 1, b: 2 }, { a: 2, c: 3 }), { a: 2, b: null, c: 3 },
                   "delete and adds")
  assert.deepEqual(diff({}, { a: 1, b: 2 }), { a: 1, b: 2 },
                   "diff on empty objcet returns equivalen of to")
  assert.deepEqual(diff({ a: 1, b: { c: { d: 2 } } }, { d: 3 }),
                   { a: null, b: null, d: 3 },
                   "missing deep propertes are deleted")
  assert.deepEqual(diff({ b: { c: {} }, d: null }, { a: 1, b: { c: { d: 2 } } }),
                   { a: 1, b: { c: { d: 2 } } },
                   "missing nested propertes are deleted")
}
