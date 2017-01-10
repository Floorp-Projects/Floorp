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
    cropString,
    wrapRender,
  } = require("./rep-utils");
  const { MODE } = require("./constants");

  // Shortcuts
  const DOM = React.DOM;

  /**
   * Renders DOM #text node.
   */
  let TextNode = React.createClass({
    displayName: "TextNode",

    propTypes: {
      object: React.PropTypes.object.isRequired,
      // @TODO Change this to Object.values once it's supported in Node's version of V8
      mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
    },

    getTextContent: function (grip) {
      return cropString(grip.preview.textContent);
    },

    getTitle: function (grip) {
      const title = "#text";
      if (this.props.objectLink) {
        return this.props.objectLink({
          object: grip
        }, title);
      }
      return title;
    },

    render: wrapRender(function () {
      let {
        object: grip,
        mode = MODE.SHORT,
      } = this.props;

      let baseConfig = {className: "objectBox objectBox-textNode"};
      if (this.props.onDOMNodeMouseOver) {
        Object.assign(baseConfig, {
          onMouseOver: _ => this.props.onDOMNodeMouseOver(grip)
        });
      }

      if (this.props.onDOMNodeMouseOut) {
        Object.assign(baseConfig, {
          onMouseOut: this.props.onDOMNodeMouseOut
        });
      }

      if (mode === MODE.TINY) {
        return DOM.span(baseConfig, this.getTitle(grip));
      }

      return (
        DOM.span(baseConfig,
          this.getTitle(grip),
          DOM.span({className: "nodeValue"},
            " ",
            `"${this.getTextContent(grip)}"`
          )
        )
      );
    }),
  });

  // Registration

  function supportsObject(grip, type) {
    if (!isGrip(grip)) {
      return false;
    }

    return (grip.preview && grip.class == "Text");
  }

  // Exports from this module
  exports.TextNode = {
    rep: TextNode,
    supportsObject: supportsObject
  };
});
