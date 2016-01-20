/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function(require, exports, module) {

// Dependencies
const React = require("devtools/client/shared/vendor/react");
const DOM = React.DOM;

/**
 * Renders a caption. This template is used by other components
 * that needs to distinguish between a simple text/value and a label.
 */
const Caption = React.createClass({
  displayName: "Caption",

  render: function() {
    return (
      DOM.span({"className": "caption"}, this.props.object)
    );
  },
});

// Exports from this module
exports.Caption = Caption;
});
