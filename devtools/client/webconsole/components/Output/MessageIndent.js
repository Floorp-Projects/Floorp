/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const INDENT_WIDTH = 12;

// Store common indents so they can be used without recreating the element during render.
const CONSTANT_INDENTS = [getIndentElement(1)];
const IN_WARNING_GROUP_INDENT = getIndentElement(1, "warning-indent");

function getIndentElement(indent, className) {
  return dom.span({
    className: `indent${className ? " " + className : ""}`,
    style: {
      width: indent * INDENT_WIDTH,
    },
  });
}

function MessageIndent(props) {
  const { indent, inWarningGroup } = props;

  if (inWarningGroup) {
    return IN_WARNING_GROUP_INDENT;
  }

  if (!indent) {
    return null;
  }

  return CONSTANT_INDENTS[indent] || getIndentElement(indent);
}

MessageIndent.propTypes = {
  indent: PropTypes.number,
  inWarningGroup: PropTypes.bool,
};

module.exports.MessageIndent = MessageIndent;

// Exported so we can test it with unit tests.
module.exports.INDENT_WIDTH = INDENT_WIDTH;
