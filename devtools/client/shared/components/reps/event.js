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
   * Renders DOM event objects.
   */
  let Event = React.createClass({
    displayName: "event",

    propTypes: {
      object: React.PropTypes.object.isRequired
    },

    getTitle: function (grip) {
      if (this.props.objectLink) {
        return this.props.objectLink({
          object: grip
        }, grip.preview.type);
      }
      return grip.preview.type;
    },

    summarizeEvent: function (grip) {
      let info = [];

      let eventFamily = grip.class;
      let props = grip.preview.properties;

      if (eventFamily == "MouseEvent") {
        info.push("clientX=", props.clientX, ", clientY=", props.clientY);
      } else if (eventFamily == "KeyboardEvent") {
        info.push("charCode=", props.charCode, ", keyCode=", props.keyCode);
      } else if (eventFamily == "MessageEvent") {
        info.push("origin=", props.origin, ", data=", props.data);
      }

      return info.join("");
    },

    render: function () {
      let grip = this.props.object;
      return (
        span({className: "objectBox objectBox-event"},
          this.getTitle(grip),
          this.summarizeEvent(grip)
        )
      );
    },
  });

  // Registration

  function supportsObject(grip, type) {
    if (!isGrip(grip)) {
      return false;
    }

    return (grip.preview && grip.preview.kind == "DOMEvent");
  }

  // Exports from this module
  exports.Event = {
    rep: Event,
    supportsObject: supportsObject
  };
});
