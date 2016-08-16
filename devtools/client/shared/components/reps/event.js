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
  const { rep } = createFactories(require("./grip").Grip);

  /**
   * Renders DOM event objects.
   */
  let Event = React.createClass({
    displayName: "event",

    propTypes: {
      object: React.PropTypes.object.isRequired
    },

    render: function () {
      // Use `Object.assign` to keep `this.props` without changes becuase:
      // 1. JSON.stringify/JSON.parse is slow.
      // 2. Immutable.js is planned for the future.
      let props = Object.assign({}, this.props);
      props.object = Object.assign({}, this.props.object);
      props.object.preview = Object.assign({}, this.props.object.preview);
      props.object.preview.ownProperties = props.object.preview.properties;
      delete props.object.preview.properties;
      props.object.ownPropertyLength =
        Object.keys(props.object.preview.ownProperties).length;
      return rep(props);
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
