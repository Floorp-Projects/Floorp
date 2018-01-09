/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const {l10n} = require("devtools/client/webconsole/new-console-output/utils/messages");

// Store common icons so they can be used without recreating the element
// during render.
const CONSTANT_ICONS = {
  "error": getIconElement("level.error"),
  "warn": getIconElement("level.warn"),
  "info": getIconElement("level.info"),
  "log": getIconElement("level.log"),
  "debug": getIconElement("level.debug"),
};

function getIconElement(level) {
  return dom.span({
    className: "icon",
    title: l10n.getStr(level),
    "aria-live": "off",
  });
}

MessageIcon.displayName = "MessageIcon";
MessageIcon.propTypes = {
  level: PropTypes.string.isRequired,
};
function MessageIcon(props) {
  const { level } = props;
  return CONSTANT_ICONS[level] || getIconElement(level);
}

module.exports = MessageIcon;
