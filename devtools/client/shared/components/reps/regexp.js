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
  const { isGrip } = require("./rep-utils");

  // Shortcuts
  const { span } = React.DOM;

  /**
   * Renders a grip object with regular expression.
   */
  let RegExp = React.createClass({
    displayName: "regexp",

    propTypes: {
      object: React.PropTypes.object.isRequired,
    },

    getSource: function (grip) {
      return grip.displayString;
    },

    render: function () {
      let grip = this.props.object;
      let objectLink = this.props.objectLink || span;

      return (
        span({className: "objectBox objectBox-regexp"},
          objectLink({
            object: grip,
            className: "regexpSource"
          }, this.getSource(grip))
        )
      );
    },
  });

  // Registration

  function supportsObject(object, type) {
    if (!isGrip(object)) {
      return false;
    }

    return (type == "RegExp");
  }

  // Exports from this module
  exports.RegExp = {
    rep: RegExp,
    supportsObject: supportsObject
  };
});
