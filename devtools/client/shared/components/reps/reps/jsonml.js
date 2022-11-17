/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function(require, exports, module) {
  // ReactJS
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  // Dependencies
  const { createElement } = require("devtools/client/shared/vendor/react");
  const {
    cleanupStyle,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");

  const {
    wrapRender,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");

  const ALLOWED_TAGS = new Set([
    "span",
    "div",
    "ol",
    "ul",
    "li",
    "table",
    "tr",
    "td",
  ]);

  /**
   * Renders null value
   */
  JsonMl.PropTypes = {
    object: PropTypes.object.isRequired,
    createElement: PropTypes.func,
  };

  function JsonMl(props) {
    // The second item of the array can either be an object holding the attributes
    // for the element or the first child element. Therefore, all array items after the
    // first one are fetched together and split afterwards if needed.
    let [tagName, ...attributesAndChildren] = props.object.header;

    if (!ALLOWED_TAGS.has(tagName)) {
      tagName = "div";
    }

    const attributes = attributesAndChildren[0];
    const hasAttributes =
      Object(attributes) === attributes && !Array.isArray(attributes);
    const style =
      hasAttributes && attributes?.style && props.createElement
        ? cleanupStyle(attributes.style, props.createElement)
        : null;
    const children = attributesAndChildren;
    if (hasAttributes) {
      children.shift();
    }

    const childElements = [];
    if (Array.isArray(children)) {
      children.forEach((child, index) => {
        childElements.push(
          // If the child is an array, it should be a JsonML item, so use this function to
          // render them, otherwise it should be a string and we can directly render it.
          Array.isArray(child)
            ? JsonMl({ ...props, object: { header: child, index } })
            : child
        );
      });
    } else {
      childElements.push(children);
    }

    return createElement(
      tagName,
      {
        className: "objectBox objectBox-jsonml",
        key: `jsonml-${tagName}-${props.object.index ?? 0}`,
        style,
      },
      childElements
    );
  }

  function supportsObject(grip) {
    return grip?.useCustomFormatter === true && Array.isArray(grip?.header);
  }

  // Exports from this module

  module.exports = {
    rep: wrapRender(JsonMl),
    supportsObject,
  };
});
