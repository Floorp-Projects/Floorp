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
 * Renders undefined value
 */
const Undefined = React.createClass({
  displayName: "UndefinedRep",

  render: function() {
    return (
      ObjectBox({className: "undefined"},
        "undefined"
      )
    )
  },
});

function supportsObject(object, type) {
  if (object && object.type && object.type == "undefined") {
    return true;
  }

  return (type == "undefined");
}

// Exports from this module

exports.Undefined = {
  rep: Undefined,
  supportsObject: supportsObject
};

});
