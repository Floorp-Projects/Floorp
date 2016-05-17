/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // ReactJS
  const React = require("devtools/client/shared/vendor/react");

  // Reps
  const { createFactories, isGrip } = require("./rep-utils");
  const { ObjectLink } = createFactories(require("./object-link"));
  const { cropString } = require("./string");

  /**
   * This component represents a template for Function objects.
   */
  let Func = React.createClass({
    displayName: "Func",

    propTypes: {
      object: React.PropTypes.object.isRequired
    },

    summarizeFunction: function (grip) {
      let name = grip.displayName || grip.name || "function";
      return cropString(name + "()", 100);
    },

    render: function () {
      let grip = this.props.object;

      return (
        ObjectLink({className: "function"},
          this.summarizeFunction(grip)
        )
      );
    },
  });

  // Registration

  function supportsObject(grip, type) {
    if (!isGrip(grip)) {
      return (type == "function");
    }

    return (type == "Function");
  }

  // Exports from this module

  exports.Func = {
    rep: Func,
    supportsObject: supportsObject
  };
});
