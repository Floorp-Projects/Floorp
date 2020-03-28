/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { l10n } = require("devtools/client/webconsole/utils/messages");

const l10nLevels = {
  error: "level.error",
  warn: "level.warn",
  info: "level.info",
  log: "level.log",
  debug: "level.debug",
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

function getIconElement(level, type) {
  let title = l10n.getStr(l10nLevels[level] || level);
  const classnames = ["icon"];

  if (type && type === "logPoint") {
    title = l10n.getStr("logpoint.title");
    classnames.push("logpoint");
  }

  {
    return dom.span({
      className: classnames.join(" "),
      title,
      "aria-live": "off",
    });
  }
}

MessageIcon.displayName = "MessageIcon";
MessageIcon.propTypes = {
  level: PropTypes.string.isRequired,
  type: PropTypes.string,
};

function MessageIcon(props) {
  const { level, type } = props;

  if (type) {
    return getIconElement(level, type);
  }

  return CONSTANT_ICONS[level] || getIconElement(level);
}

module.exports = MessageIcon;
