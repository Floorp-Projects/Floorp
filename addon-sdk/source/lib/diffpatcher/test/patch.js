"use strict";

var patch = require("../patch")

exports["test patch delete"] = function(assert) {
  var hash = { a: 1, b: 2 }

  assert.deepEqual(patch(hash, { a: null }), { b: 2 }, "null removes property")
}

exports["test patch delete with void"] = function(assert) {
  var hash = { a: 1, b: 2 }

  assert.deepEqual(patch(hash, { a: void(0) }), { b: 2 },
                   "void(0) removes property")
}

exports["test patch delete missing"] = function(assert) {
  assert.deepEqual(patch({ a: 1, b: 2 }, { c: null }),
                   { a: 1, b: 2 },
                   "null removes property if exists");

  assert.deepEqual(patch({ a: 1, b: 2 }, { c: void(0) }),
                   { a: 1, b: 2 },
                   "void removes property if exists");
}

exports["test delete deleted"] = function(assert) {
  assert.deepEqual(patch({ a: null, b: 2, c: 3, d: void(0)},
                         { a: void(0), b: null, d: null }),
                   {c: 3},
                  "removed all existing and non existing");
}

exports["test update deleted"] = function(assert) {
  assert.deepEqual(patch({ a: null, b: void(0), c: 3},
                         { a: { b: 2 } }),
                   { a: { b: 2 }, c: 3 },
                   "replace deleted");
}

exports["test patch delete with void"] = function(assert) {
  var hash = { a: 1, b: 2 }

  assert.deepEqual(patch(hash, { a: void(0) }), { b: 2 },
                   "void(0) removes property")
}


exports["test patch addition"] = function(assert) {
  var hash = { a: 1, b: 2 }

  assert.deepEqual(patch(hash, { c: 3 }), { a: 1, b: 2, c: 3 },
                   "new properties are added")
}

exports["test patch addition"] = function(assert) {
  var hash = { a: 1, b: 2 }

  assert.deepEqual(patch(hash, { c: 3 }), { a: 1, b: 2, c: 3 },
                   "new properties are added")
}

exports["test hash on itself"] = function(assert) {
  var hash = { a: 1, b: 2 }

  assert.deepEqual(patch(hash, hash), hash,
                   "applying hash to itself returns hash itself")
}

exports["test patch with empty delta"] = function(assert) {
  var hash = { a: 1, b: 2 }

  assert.deepEqual(patch(hash, {}), hash,
                   "applying empty delta results in no changes")
}

exports["test patch nested data"] = function(assert) {
  assert.deepEqual(patch({ a: { b: 1 }, c: { d: 2 } },
                         { a: { b: null, e: 3 }, c: { d: 4 } }),
                   { a: { e: 3 }, c: { d: 4 } },
                   "nested structures can also be patched")
}
