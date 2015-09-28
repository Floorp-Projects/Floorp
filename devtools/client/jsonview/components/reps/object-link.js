/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function(require, exports, module) {

// Dependencies
const React = require("react");
const DOM = React.DOM;

/**
 * Renders a link for given object.
 */
const ObjectLink = React.createClass({
  displayName: "ObjectLink",

  render: function() {
    var className = this.props.className;
    var objectClassName = className ? " objectLink-" + className : "";
    var linkClassName = "objectLink" + objectClassName + " a11yFocus";

    return (
      DOM.a({className: linkClassName, _repObject: this.props.object},
        this.props.children
      )
    )
  }
});

// Exports from this module
exports.ObjectLink = ObjectLink;
});
