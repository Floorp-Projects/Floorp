/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function(require, exports, module) {
  // ReactJS
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  // Dependencies
  const {
    createElement,
    useState,
  } = require("devtools/client/shared/vendor/react");
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
  CustomFormatter.PropTypes = {
    object: PropTypes.object.isRequired,
    createElement: PropTypes.func,
  };

  function CustomFormatter(props) {
    const [state, setState] = useState({ open: false });
    const headerJsonMl = renderJsonMl(props.object.header, {
      ...props,
      open: state.open,
    });

    async function toggleBody(evt) {
      evt.stopPropagation();
      const open = !state.open;
      if (open && !state.bodyJsonMl) {
        const response = await getCustomFormatterBody(
          props.front,
          props.object.customFormatterIndex
        );

        const bodyJsonMl = renderJsonMl(response.customFormatterBody, {
          ...props,
          object: null,
        });
        setState({ ...state, bodyJsonMl, open });
      } else {
        delete state.bodyJsonMl;
        setState({ ...state, open });
      }
    }

    return createElement(
      "span",
      {
        className: "objectBox-jsonml-wrapper",
        "data-expandable": props.object.hasBody,
        "aria-expanded": state.open,
        onClick: props.object.hasBody ? toggleBody : null,
      },
      headerJsonMl,
      state.bodyJsonMl
        ? createElement(
            "div",
            { className: "objectBox-jsonml-body-wrapper" },
            state.bodyJsonMl
          )
        : null
    );
  }

  function renderJsonMl(jsonMl, props, index = 0) {
    // The second item of the array can either be an object holding the attributes
    // for the element or the first child element. Therefore, all array items after the
    // first one are fetched together and split afterwards if needed.
    let [tagName, ...attributesAndChildren] = jsonMl ?? [];

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

    if (props.object?.hasBody) {
      childElements.push(
        createElement("button", {
          "aria-expanded": props.open,
          className: `collapse-button jsonml-header-collapse-button${
            props.open ? " expanded" : ""
          }`,
        })
      );
    }

    if (Array.isArray(children)) {
      children.forEach((child, childIndex) => {
        let childElement;
        // If the child is an array, it should be a JsonML item, so use this function to
        // render them.
        if (Array.isArray(child)) {
          childElement = renderJsonMl(child, props, childIndex);
        } else if (typeof child !== "object") {
          childElement = child;
        }
        childElements.push(childElement);
      });
    } else {
      childElements.push(children);
    }

    return createElement(
      tagName,
      {
        className: `objectBox objectBox-jsonml`,
        key: `jsonml-${tagName}-${index}`,
        style,
      },
      childElements
    );
  }

  async function getCustomFormatterBody(objectFront, customFormatterIndex) {
    return objectFront.customFormatterBody(customFormatterIndex);
  }

  function supportsObject(grip) {
    return grip?.useCustomFormatter === true && Array.isArray(grip?.header);
  }

  // Exports from this module
  module.exports = {
    rep: wrapRender(CustomFormatter),
    supportsObject,
  };
});
