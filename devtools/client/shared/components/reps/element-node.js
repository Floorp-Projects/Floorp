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

  // Utils
  const {
    isGrip,
    wrapRender,
  } = require("./rep-utils");
  const { MODE } = require("./constants");
  const nodeConstants = require("devtools/shared/dom-node-constants");

  // Shortcuts
  const { span } = React.DOM;

  /**
   * Renders DOM element node.
   */
  const ElementNode = React.createClass({
    displayName: "ElementNode",

    propTypes: {
      object: React.PropTypes.object.isRequired,
      // @TODO Change this to Object.values once it's supported in Node's version of V8
      mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
      onDOMNodeMouseOver: React.PropTypes.func,
      onDOMNodeMouseOut: React.PropTypes.func,
      objectLink: React.PropTypes.func,
    },

    getElements: function (grip, mode) {
      let {attributes, nodeName} = grip.preview;
      const nodeNameElement = span({
        className: "tag-name theme-fg-color3"
      }, nodeName);

      if (mode === MODE.TINY) {
        let elements = [nodeNameElement];
        if (attributes.id) {
          elements.push(
            span({className: "attr-name theme-fg-color2"}, `#${attributes.id}`));
        }
        if (attributes.class) {
          elements.push(
            span({className: "attr-name theme-fg-color2"},
              attributes.class
                .replace(/(^\s+)|(\s+$)/g, "")
                .split(" ")
                .map(cls => `.${cls}`)
                .join("")
            )
          );
        }
        return elements;
      }
      let attributeElements = Object.keys(attributes)
        .sort(function getIdAndClassFirst(a1, a2) {
          if ([a1, a2].includes("id")) {
            return 3 * (a1 === "id" ? -1 : 1);
          }
          if ([a1, a2].includes("class")) {
            return 2 * (a1 === "class" ? -1 : 1);
          }

          // `id` and `class` excepted,
          // we want to keep the same order that in `attributes`.
          return 0;
        })
        .reduce((arr, name, i, keys) => {
          let value = attributes[name];
          let attribute = span({},
            span({className: "attr-name theme-fg-color2"}, `${name}`),
            `="`,
            span({className: "attr-value theme-fg-color6"}, `${value}`),
            `"`
          );

          return arr.concat([" ", attribute]);
        }, []);

      return [
        "<",
        nodeNameElement,
        ...attributeElements,
        ">",
      ];
    },

    render: wrapRender(function () {
      let {
        object,
        mode,
        onDOMNodeMouseOver,
        onDOMNodeMouseOut
      } = this.props;
      let elements = this.getElements(object, mode);
      let objectLink = this.props.objectLink || span;

      let baseConfig = {className: "objectBox objectBox-node"};
      if (onDOMNodeMouseOver) {
        Object.assign(baseConfig, {
          onMouseOver: _ => onDOMNodeMouseOver(object)
        });
      }

      if (onDOMNodeMouseOut) {
        Object.assign(baseConfig, {
          onMouseOut: onDOMNodeMouseOut
        });
      }

      return objectLink({object},
        span(baseConfig, ...elements)
      );
    }),
  });

  // Registration
  function supportsObject(object, type) {
    if (!isGrip(object)) {
      return false;
    }
    return object.preview && object.preview.nodeType === nodeConstants.ELEMENT_NODE;
  }

  // Exports from this module
  exports.ElementNode = {
    rep: ElementNode,
    supportsObject: supportsObject
  };
});
