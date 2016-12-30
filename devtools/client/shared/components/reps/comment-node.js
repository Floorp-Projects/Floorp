/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // Dependencies
  const React = require("devtools/client/shared/vendor/react");
  const {
    isGrip,
    cropString,
    cropMultipleLines,
    wrapRender,
  } = require("./rep-utils");
  const { MODE } = require("./constants");
  const nodeConstants = require("devtools/shared/dom-node-constants");

  // Shortcuts
  const { span } = React.DOM;

  /**
   * Renders DOM comment node.
   */
  const CommentNode = React.createClass({
    displayName: "CommentNode",

    propTypes: {
      object: React.PropTypes.object.isRequired,
      // @TODO Change this to Object.values once it's supported in Node's version of V8
      mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
    },

    render: wrapRender(function () {
      let {
        object,
        mode = MODE.SHORT
      } = this.props;

      let {textContent} = object.preview;
      if (mode === MODE.TINY) {
        textContent = cropMultipleLines(textContent, 30);
      } else if (mode === MODE.SHORT) {
        textContent = cropString(textContent, 50);
      }

      return span({className: "objectBox theme-comment"}, `<!-- ${textContent} -->`);
    }),
  });

  // Registration
  function supportsObject(object, type) {
    if (!isGrip(object)) {
      return false;
    }
    return object.preview && object.preview.nodeType === nodeConstants.COMMENT_NODE;
  }

  // Exports from this module
  exports.CommentNode = {
    rep: CommentNode,
    supportsObject: supportsObject
  };
});
