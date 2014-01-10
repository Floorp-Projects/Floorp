"use strict";

var method = require("method/core")

// Method is designed to work with data structures representing application
// state. Calling it with a state should return object representing `delta`
// that has being applied to a previous state to get to a current state.
//
// Example
//
// diff(state) // => { "item-id-1": { title: "some title" } "item-id-2": null }
var diff = method("diff@diffpatcher")

// diff between `null` / `undefined` to any hash is a hash itself.
diff.define(null, function(from, to) { return to })
diff.define(undefined, function(from, to) { return to })
diff.define(Object, function(from, to) {
  return calculate(from, to || {}) || {}
})

function calculate(from, to) {
  var diff = {}
  var changes = 0
  Object.keys(from).forEach(function(key) {
    changes = changes + 1
    if (!(key in to) && from[key] != null) diff[key] = null
    else changes = changes - 1
  })
  Object.keys(to).forEach(function(key) {
    changes = changes + 1
    var previous = from[key]
    var current = to[key]
    if (previous === current) return (changes = changes - 1)
    if (typeof(current) !== "object") return diff[key] = current
    if (typeof(previous) !== "object") return diff[key] = current
    var delta = calculate(previous, current)
    if (delta) diff[key] = delta
    else changes = changes - 1
  })
  return changes ? diff : null
}

diff.calculate = calculate

module.exports = diff
