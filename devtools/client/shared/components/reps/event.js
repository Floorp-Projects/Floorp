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
    createFactories,
    isGrip,
    wrapRender,
  } = require("./rep-utils");
  const { rep } = createFactories(require("./grip").Grip);

  /**
   * Renders DOM event objects.
   */
  let Event = React.createClass({
    displayName: "event",

    propTypes: {
      object: React.PropTypes.object.isRequired
    },

    getTitle: function (props) {
      let preview = props.object.preview;
      let title = preview.type;

      if (preview.eventKind == "key" && preview.modifiers && preview.modifiers.length) {
        title = `${title} ${preview.modifiers.join("-")}`;
      }
      return title;
    },

    render: wrapRender(function () {
      // Use `Object.assign` to keep `this.props` without changes because:
      // 1. JSON.stringify/JSON.parse is slow.
      // 2. Immutable.js is planned for the future.
      let gripProps = Object.assign({
        title: this.getTitle(this.props)
      }, this.props);
      gripProps.object = Object.assign({}, this.props.object);
      gripProps.object.preview = Object.assign({}, this.props.object.preview);

      gripProps.object.preview.ownProperties = {};
      if (gripProps.object.preview.target) {
        Object.assign(gripProps.object.preview.ownProperties, {
          target: gripProps.object.preview.target
        });
      }
      Object.assign(gripProps.object.preview.ownProperties,
        gripProps.object.preview.properties);

      delete gripProps.object.preview.properties;
      gripProps.object.ownPropertyLength =
        Object.keys(gripProps.object.preview.ownProperties).length;

      switch (gripProps.object.class) {
        case "MouseEvent":
          gripProps.isInterestingProp = (type, value, name) => {
            return ["target", "clientX", "clientY", "layerX", "layerY"].includes(name);
          };
          break;
        case "KeyboardEvent":
          gripProps.isInterestingProp = (type, value, name) => {
            return ["target", "key", "charCode", "keyCode"].includes(name);
          };
          break;
        case "MessageEvent":
          gripProps.isInterestingProp = (type, value, name) => {
            return ["target", "isTrusted", "data"].includes(name);
          };
          break;
        default:
          gripProps.isInterestingProp = (type, value, name) => {
            // We want to show the properties in the order they are declared.
            return Object.keys(gripProps.object.preview.ownProperties).includes(name);
          };
      }

      return rep(gripProps);
    })
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
