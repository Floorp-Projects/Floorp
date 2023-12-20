/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const {
  l10n,
} = require("resource://devtools/client/webconsole/utils/messages.js");
const {
  MESSAGE_TYPE,
} = require("resource://devtools/client/webconsole/constants.js");

const l10nLevels = {
  error: "level.error",
  warn: "level.warn",
  info: "level.info",
  log: "level.log",
  debug: "level.debug",
  jstracer: "level.jstracer",
};

// Store common icons so they can be used without recreating the element
// during render.
const CONSTANT_ICONS = Object.entries(l10nLevels).reduce(
  (acc, [key, l10nLabel]) => {
    acc[key] = getIconElement(l10nLabel);
    return acc;
  },
  {}
);

function getIconElement(level, type, title) {
  title = title || l10n.getStr(l10nLevels[level] || level);
  const classnames = ["icon"];

  if (type === "logPoint") {
    title = l10n.getStr("logpoint.title");
    classnames.push("logpoint");
  } else if (type === "blockedReason") {
    title = l10n.getStr("blockedrequest.label");
  } else if (type === MESSAGE_TYPE.COMMAND) {
    title = l10n.getStr("command.title");
  } else if (type === MESSAGE_TYPE.RESULT) {
    title = l10n.getStr("result.title");
  } else if (level == "level.jstracer") {
    classnames.push("logtrace");
  }

  return dom.span({
    className: classnames.join(" "),
    title,
    "aria-live": "off",
  });
}

MessageIcon.displayName = "MessageIcon";
MessageIcon.propTypes = {
  level: PropTypes.string.isRequired,
  type: PropTypes.string,
  title: PropTypes.string,
};

function MessageIcon(props) {
  const { level, type, title } = props;

  if (type) {
    return getIconElement(level, type, title);
  }

  return CONSTANT_ICONS[level] || getIconElement(level);
}

module.exports = MessageIcon;
