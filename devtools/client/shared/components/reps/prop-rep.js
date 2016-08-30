/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  const React = require("devtools/client/shared/vendor/react");
  const { createFactories } = require("./rep-utils");

  const { span } = React.DOM;

  /**
   * Property for Obj (local JS objects) and Grip (remote JS objects)
   * reps. It's used to render object properties.
   */
  let PropRep = React.createFactory(React.createClass({
    displayName: "PropRep",

    propTypes: {
      // Property name.
      name: React.PropTypes.string,
      // Equal character rendered between property name and value.
      equal: React.PropTypes.string,
      // Delimiter character used to separate individual properties.
      delim: React.PropTypes.string,
    },

    render: function () {
      let { Rep } = createFactories(require("./rep"));

      return (
        span({},
          span({
            "className": "nodeName"},
            this.props.name),
          span({
            "className": "objectEqual"
          }, this.props.equal),
          Rep(this.props),
          span({
            "className": "objectComma"
          }, this.props.delim)
        )
      );
    }
  }));

  // Exports from this module
  exports.PropRep = PropRep;
});
