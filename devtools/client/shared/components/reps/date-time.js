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
  const {
    isGrip,
    wrapRender,
  } = require("./rep-utils");

  // Shortcuts
  const { span } = React.DOM;

  /**
   * Used to render JS built-in Date() object.
   */
  let DateTime = React.createClass({
    displayName: "Date",

    propTypes: {
      object: React.PropTypes.object.isRequired,
      objectLink: React.PropTypes.func,
    },

    getTitle: function (grip) {
      if (this.props.objectLink) {
        return this.props.objectLink({
          object: grip
        }, grip.class + " ");
      }
      return "";
    },

    render: wrapRender(function () {
      let grip = this.props.object;
      let date;
      try {
        date = span({className: "objectBox"},
          this.getTitle(grip),
          span({className: "Date"},
            new Date(grip.preview.timestamp).toISOString()
          )
        );
      } catch (e) {
        date = span({className: "objectBox"}, "Invalid Date");
      }

      return date;
    }),
  });

  // Registration

  function supportsObject(grip, type) {
    if (!isGrip(grip)) {
      return false;
    }

    return (type == "Date" && grip.preview);
  }

  // Exports from this module
  exports.DateTime = {
    rep: DateTime,
    supportsObject: supportsObject
  };
});
