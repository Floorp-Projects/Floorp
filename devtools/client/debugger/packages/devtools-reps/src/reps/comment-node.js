/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const PropTypes = require("prop-types");
const {
  isGrip,
  cropString,
  cropMultipleLines,
  wrapRender,
} = require("./rep-utils");
const { MODE } = require("./constants");
const nodeConstants = require("../shared/dom-node-constants");
const dom = require("react-dom-factories");
const { span } = dom;

/**
 * Renders DOM comment node.
 */
CommentNode.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values when supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
};

function CommentNode(props) {
  const { object, mode = MODE.SHORT } = props;

  let { textContent } = object.preview;
  if (mode === MODE.TINY) {
    textContent = cropMultipleLines(textContent, 30);
  } else if (mode === MODE.SHORT) {
    textContent = cropString(textContent, 50);
  }

  return span(
    {
      className: "objectBox theme-comment",
      "data-link-actor-id": object.actor,
    },
    `<!-- ${textContent} -->`
  );
}

// Registration
function supportsObject(object, noGrip = false) {
  if (noGrip === true || !isGrip(object)) {
    return false;
  }
  return (
    object.preview && object.preview.nodeType === nodeConstants.COMMENT_NODE
  );
}

// Exports from this module
module.exports = {
  rep: wrapRender(CommentNode),
  supportsObject,
};
