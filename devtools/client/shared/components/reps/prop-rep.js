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
   * Property for Obj (local JS objects), Grip (remote JS objects)
   * and GripMap (remote JS maps and weakmaps) reps.
   * It's used to render object properties.
   */
  let PropRep = React.createFactory(React.createClass({
    displayName: "PropRep",

    propTypes: {
      // Property name.
      name: React.PropTypes.oneOfType([
        React.PropTypes.string,
        React.PropTypes.object,
      ]).isRequired,
      // Equal character rendered between property name and value.
      equal: React.PropTypes.string,
      // Delimiter character used to separate individual properties.
      delim: React.PropTypes.string,
      mode: React.PropTypes.string,
    },

    render: function () {
      const { Grip } = require("./grip");
      let { Rep } = createFactories(require("./rep"));

      let key;
      // The key can be a simple string, for plain objects,
      // or another object for maps and weakmaps.
      if (typeof this.props.name === "string") {
        key = span({"className": "nodeName"}, this.props.name);
      } else {
        key = Rep({
          object: this.props.name,
          mode: this.props.mode || "tiny",
          defaultRep: Grip,
          objectLink: this.props.objectLink,
        });
      }

      return (
        span({},
          key,
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
