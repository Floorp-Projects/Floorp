/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  DOM: dom,
  PropTypes
} = require("devtools/client/shared/vendor/react");
const {l10n} = require("devtools/client/webconsole/new-console-output/utils/messages");

MessageIcon.displayName = "MessageIcon";

MessageIcon.propTypes = {
  level: PropTypes.string.isRequired,
};

function MessageIcon(props) {
  const { level } = props;

  const title = l10n.getStr("level." + level);
  return dom.div({
    className: "icon",
    title
  });
}

module.exports.MessageIcon = MessageIcon;
