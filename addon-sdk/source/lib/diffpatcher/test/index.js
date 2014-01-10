"use strict";

var diff = require("../diff")
var patch = require("../patch")

exports["test diff"] = require("./diff")
exports["test patch"] = require("./patch")

exports["test patch(a, diff(a, b)) => b"] = function(assert) {
  var a = { a: { b: 1 }, c: { d: 2 } }
  var b = { a: { e: 3 }, c: { d: 4 } }

  assert.deepEqual(patch(a, diff(a, b)), b, "patch(a, diff(a, b)) => b")
}
