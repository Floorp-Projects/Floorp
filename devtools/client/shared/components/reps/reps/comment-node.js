/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // Dependencies
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const { span } = require("devtools/client/shared/vendor/react-dom-factories");
  const {
    cropString,
    cropMultipleLines,
    wrapRender,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");
  const {
    MODE,
  } = require("devtools/client/shared/components/reps/reps/constants");
  const nodeConstants = require("devtools/client/shared/components/reps/shared/dom-node-constants");

  /**
   * Renders DOM comment node.
   */

  CommentNode.propTypes = {
    object: PropTypes.object.isRequired,
    mode: PropTypes.oneOf(Object.values(MODE)),
    shouldRenderTooltip: PropTypes.bool,
  };

  function CommentNode(props) {
    const { object, mode = MODE.SHORT, shouldRenderTooltip } = props;

    let { textContent } = object.preview;
    if (mode === MODE.TINY || mode === MODE.HEADER) {
      textContent = cropMultipleLines(textContent, 30);
    } else if (mode === MODE.SHORT) {
      textContent = cropString(textContent, 50);
    }

    const config = getElementConfig({
      object,
      textContent,
      shouldRenderTooltip,
    });

    return span(config, `<!-- ${textContent} -->`);
  }

  function getElementConfig(opts) {
    const { object, shouldRenderTooltip } = opts;

    // Run textContent through cropString to sanitize
    const uncroppedText = shouldRenderTooltip
      ? cropString(object.preview.textContent)
      : null;

    return {
      className: "objectBox theme-comment",
      "data-link-actor-id": object.actor,
      title: shouldRenderTooltip ? `<!-- ${uncroppedText} -->` : null,
    };
  }

  // Registration
  function supportsObject(object) {
    return object?.preview?.nodeType === nodeConstants.COMMENT_NODE;
  }

  // Exports from this module
  module.exports = {
    rep: wrapRender(CommentNode),
    supportsObject,
  };
});
