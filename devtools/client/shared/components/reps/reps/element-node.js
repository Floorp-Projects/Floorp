/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // ReactJS
  const {
    button,
    span,
  } = require("devtools/client/shared/vendor/react-dom-factories");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

  // Utils
  const {
    appendRTLClassNameIfNeeded,
    wrapRender,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");
  const {
    rep: StringRep,
    isLongString,
  } = require("devtools/client/shared/components/reps/reps/string");
  const {
    MODE,
  } = require("devtools/client/shared/components/reps/reps/constants");
  const nodeConstants = require("devtools/client/shared/components/reps/shared/dom-node-constants");

  const MAX_ATTRIBUTE_LENGTH = 50;

  /**
   * Renders DOM element node.
   */

  ElementNode.propTypes = {
    object: PropTypes.object.isRequired,
    // The class should be in reps.css
    inspectIconTitle: PropTypes.oneOf(["open-inspector", "highlight-node"]),
    inspectIconClassName: PropTypes.string,
    mode: PropTypes.oneOf(Object.values(MODE)),
    onDOMNodeClick: PropTypes.func,
    onDOMNodeMouseOver: PropTypes.func,
    onDOMNodeMouseOut: PropTypes.func,
    onInspectIconClick: PropTypes.func,
    shouldRenderTooltip: PropTypes.bool,
  };

  function ElementNode(props) {
    const { object, mode, shouldRenderTooltip } = props;

    const {
      isAfterPseudoElement,
      isBeforePseudoElement,
      isMarkerPseudoElement,
    } = object.preview;

    let renderElements = [];
    const isInTree = object.preview && object.preview.isConnected === true;
    let config = getElementConfig({ ...props, isInTree });
    const inspectIcon = getInspectIcon({ ...props, isInTree });

    // Elements Case 1: Pseudo Element
    if (
      isAfterPseudoElement ||
      isBeforePseudoElement ||
      isMarkerPseudoElement
    ) {
      const pseudoNodeElement = getPseudoNodeElement(object);

      // Regenerate config if shouldRenderTooltip
      if (shouldRenderTooltip) {
        const tooltipString = pseudoNodeElement.content;
        config = getElementConfig({ ...props, tooltipString, isInTree });
      }

      // Return ONLY pseudo node element as array[0]
      renderElements = [
        span(pseudoNodeElement.config, pseudoNodeElement.content),
      ];
    } else if (mode === MODE.TINY) {
      // Elements Case 2: MODE.TINY
      const tinyElements = getTinyElements(object);

      // Regenerate config to include tooltip title
      if (shouldRenderTooltip) {
        // Reduce for plaintext
        const tooltipString = tinyElements.reduce(function (acc, cur) {
          return acc.concat(cur.content);
        }, "");

        config = getElementConfig({ ...props, tooltipString, isInTree });
      }

      // Reduce for React elements
      const tinyElementsRender = tinyElements.reduce(function (acc, cur) {
        acc.push(span(cur.config, cur.content));
        return acc;
      }, []);

      // Render array of React spans
      renderElements = tinyElementsRender;
    } else {
      // Elements Default case
      renderElements = getElements(props);
    }

    return span(config, ...renderElements, inspectIcon ? inspectIcon : null);
  }

  function getElementConfig(opts) {
    const {
      object,
      isInTree,
      onDOMNodeClick,
      onDOMNodeMouseOver,
      onDOMNodeMouseOut,
      shouldRenderTooltip,
      tooltipString,
    } = opts;

    // Initiate config
    const config = {
      "data-link-actor-id": object.actor,
      "data-link-content-dom-reference": JSON.stringify(
        object.contentDomReference
      ),
      className: "objectBox objectBox-node",
    };

    // Triage event handlers
    if (isInTree) {
      if (onDOMNodeClick) {
        Object.assign(config, {
          onClick: _ => onDOMNodeClick(object),
          className: `${config.className} clickable`,
        });
      }

      if (onDOMNodeMouseOver) {
        Object.assign(config, {
          onMouseOver: _ => onDOMNodeMouseOver(object),
        });
      }

      if (onDOMNodeMouseOut) {
        Object.assign(config, {
          onMouseOut: _ => onDOMNodeMouseOut(object),
        });
      }
    }

    // If tooltip, build tooltip
    if (tooltipString && shouldRenderTooltip) {
      config.title = tooltipString;
    }

    // Return config obj
    return config;
  }

  function getElements(opts) {
    const { object: grip } = opts;

    const { attributes, nodeName } = grip.preview;

    const nodeNameElement = span(
      {
        className: appendRTLClassNameIfNeeded("tag-name", nodeName),
      },
      nodeName
    );

    const attributeKeys = Object.keys(attributes);
    if (attributeKeys.includes("class")) {
      attributeKeys.splice(attributeKeys.indexOf("class"), 1);
      attributeKeys.unshift("class");
    }
    if (attributeKeys.includes("id")) {
      attributeKeys.splice(attributeKeys.indexOf("id"), 1);
      attributeKeys.unshift("id");
    }
    const attributeElements = attributeKeys.reduce((arr, name) => {
      const value = attributes[name];

      let title = isLongString(value) ? value.initial : value;
      if (title.length < MAX_ATTRIBUTE_LENGTH) {
        title = null;
      }

      const attribute = span(
        {},
        span(
          {
            className: appendRTLClassNameIfNeeded("attrName", name),
          },
          name
        ),
        span({ className: "attrEqual" }, "="),
        StringRep({
          className: "attrValue",
          object: value,
          cropLimit: MAX_ATTRIBUTE_LENGTH,
          title,
        })
      );

      return arr.concat([" ", attribute]);
    }, []);

    return [
      span({ className: "angleBracket" }, "<"),
      nodeNameElement,
      ...attributeElements,
      span({ className: "angleBracket" }, ">"),
    ];
  }

  function getTinyElements(grip) {
    const { attributes, nodeName } = grip.preview;

    // Initialize elements array
    const elements = [
      {
        config: {
          className: appendRTLClassNameIfNeeded("tag-name", nodeName),
        },
        content: nodeName,
      },
    ];

    // Push ID element
    if (attributes.id) {
      elements.push({
        config: {
          className: appendRTLClassNameIfNeeded("attrName", attributes.id),
        },
        content: `#${attributes.id}`,
      });
    }

    // Push Classes
    if (attributes.class) {
      const elementClasses = attributes.class
        .trim()
        .split(/\s+/)
        .map(cls => `.${cls}`)
        .join("");
      elements.push({
        config: {
          className: appendRTLClassNameIfNeeded("attrName", elementClasses),
        },
        content: elementClasses,
      });
    }

    return elements;
  }

  function getPseudoNodeElement(grip) {
    const {
      isAfterPseudoElement,
      isBeforePseudoElement,
      isMarkerPseudoElement,
    } = grip.preview;

    let pseudoNodeName;

    if (isAfterPseudoElement) {
      pseudoNodeName = "after";
    } else if (isBeforePseudoElement) {
      pseudoNodeName = "before";
    } else if (isMarkerPseudoElement) {
      pseudoNodeName = "marker";
    }

    return {
      config: { className: "attrName" },
      content: `::${pseudoNodeName}`,
    };
  }

  function getInspectIcon(opts) {
    const {
      object,
      isInTree,
      onInspectIconClick,
      inspectIconTitle,
      inspectIconClassName,
      onDOMNodeClick,
    } = opts;

    if (!isInTree || !onInspectIconClick) {
      return null;
    }

    return button({
      className: inspectIconClassName || "open-inspector",
      // TODO: Localize this with "openNodeInInspector" when Bug 1317038 lands
      title: inspectIconTitle || "Click to select the node in the inspector",
      onClick: e => {
        if (onDOMNodeClick) {
          e.stopPropagation();
        }

        onInspectIconClick(object, e);
      },
    });
  }

  // Registration
  function supportsObject(object) {
    return object?.preview?.nodeType === nodeConstants.ELEMENT_NODE;
  }

  // Exports from this module
  module.exports = {
    rep: wrapRender(ElementNode),
    supportsObject,
    MAX_ATTRIBUTE_LENGTH,
  };
});
