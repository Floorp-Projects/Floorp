/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // ReactJS
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

  // Reps
  const {
    wrapRender,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");

  const {
    MODE,
  } = require("devtools/client/shared/components/reps/reps/constants");
  const { rep } = require("devtools/client/shared/components/reps/reps/grip");

  /**
   * Renders DOM event objects.
   */
  Event.propTypes = {
    object: PropTypes.object.isRequired,
    mode: PropTypes.oneOf(Object.values(MODE)),
    onDOMNodeMouseOver: PropTypes.func,
    onDOMNodeMouseOut: PropTypes.func,
    onInspectIconClick: PropTypes.func,
  };

  function Event(props) {
    const gripProps = {
      ...props,
      title: getTitle(props),
      object: {
        ...props.object,
        preview: {
          ...props.object.preview,
          ownProperties: {},
        },
      },
    };

    if (gripProps.object.preview.target) {
      Object.assign(gripProps.object.preview.ownProperties, {
        target: gripProps.object.preview.target,
      });
    }
    Object.assign(
      gripProps.object.preview.ownProperties,
      gripProps.object.preview.properties
    );

    delete gripProps.object.preview.properties;
    gripProps.object.ownPropertyLength = Object.keys(
      gripProps.object.preview.ownProperties
    ).length;

    switch (gripProps.object.class) {
      case "MouseEvent":
        gripProps.isInterestingProp = (type, value, name) => {
          return ["target", "clientX", "clientY", "layerX", "layerY"].includes(
            name
          );
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
          return Object.keys(gripProps.object.preview.ownProperties).includes(
            name
          );
        };
    }

    return rep(gripProps);
  }

  function getTitle(props) {
    const preview = props.object.preview;
    let title = preview.type;

    if (
      preview.eventKind == "key" &&
      preview.modifiers &&
      preview.modifiers.length
    ) {
      title = `${title} ${preview.modifiers.join("-")}`;
    }
    return title;
  }

  // Registration
  function supportsObject(grip) {
    return grip?.preview?.kind == "DOMEvent";
  }

  // Exports from this module
  module.exports = {
    rep: wrapRender(Event),
    supportsObject,
  };
});
