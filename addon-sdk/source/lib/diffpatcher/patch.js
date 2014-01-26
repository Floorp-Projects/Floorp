"use strict";

var method = require("../method/core")
var rebase = require("./rebase")

// Method is designed to work with data structures representing application
// state. Calling it with a state and delta should return object representing
// new state, with changes in `delta` being applied to previous.
//
// ## Example
//
// patch(state, {
//   "item-id-1": { completed: false }, // update
//   "item-id-2": null                  // delete
// })
var patch = method("patch@diffpatcher")
patch.define(Object, function patch(hash, delta) {
  return rebase({}, hash, delta)
})

module.exports = patch
