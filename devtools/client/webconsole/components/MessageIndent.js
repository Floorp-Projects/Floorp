/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const dom = require("devtools/client/shared/vendor/react-dom-factories");

const INDENT_WIDTH = 12;

// Store common indents so they can be used without recreating the element during render.
const CONSTANT_INDENTS = [getIndentElement(0), getIndentElement(1)];
const IN_WARNING_GROUP_INDENT = getIndentElement(1, "warning-indent");

function getIndentElement(indent, className) {
  return dom.span({
    "data-indent": indent,
    className: `indent${className ? " " + className : ""}`,
    style: {
      "width": indent * INDENT_WIDTH,
    },
  });
}

function MessageIndent(props) {
  const { indent, inWarningGroup } = props;

  if (inWarningGroup) {
    return IN_WARNING_GROUP_INDENT;
  }

  return CONSTANT_INDENTS[indent] || getIndentElement(indent);
}

module.exports.MessageIndent = MessageIndent;

// Exported so we can test it with unit tests.
module.exports.INDENT_WIDTH = INDENT_WIDTH;
