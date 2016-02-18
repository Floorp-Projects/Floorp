/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function(require, exports, module) {

// Dependencies
const React = require("devtools/client/shared/vendor/react");
const { createFactories } = require("./rep-utils");
const { ObjectBox } = createFactories(require("./object-box"));

/**
 * Renders a number
 */
const Number = React.createClass({
  displayName: "Number",

  render: function() {
    var value = this.props.object;
    return (
      ObjectBox({className: "number"},
        this.stringify(value)
      )
    )
  },

  stringify: function(object) {
    return (Object.is(object, -0) ? "-0" : String(object));
  },
});

function supportsObject(object, type) {
  return type == "boolean" || type == "number";
}

// Exports from this module

exports.Number = {
  rep: Number,
  supportsObject: supportsObject
};

});
