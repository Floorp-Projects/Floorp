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
  const { isGrip, cropString, cropMultipleLines } = require("./rep-utils");

  // Utils
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
      mode: React.PropTypes.string,
    },

    render: function () {
      let {object} = this.props;

      let mode = this.props.mode || "short";

      let {textContent} = object.preview;
      if (mode === "tiny") {
        textContent = cropMultipleLines(textContent, 30);
      } else if (mode === "short") {
        textContent = cropString(textContent, 50);
      }

      return span({className: "objectBox theme-comment"}, `<!-- ${textContent} -->`);
    },
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
