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

  /**
   * Renders DOM event objects.
   */
  let Event = React.createClass({
    displayName: "event",

    propTypes: {
      object: React.PropTypes.object.isRequired
    },

    summarizeEvent: function (grip) {
      let info = [grip.preview.type, " "];

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
        ObjectLink({className: "event"},
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
