"use strict";

exports["test common"] = require("./common")

var Method = require("../core")

exports["test host objects"] = function(assert) {
  var isElement = Method("is-element")
  isElement.define(function() { return false })

  isElement.define(Element, function() { return true })

  assert.notDeepEqual(typeof(Element.prototype[isElement]), "number",
                     "Host object's prototype is extended with a number value")

  assert.ok(!isElement({}), "object is not an Element")
  assert.ok(document.createElement("div"), "Element is an element")
}

require("test").run(exports)
