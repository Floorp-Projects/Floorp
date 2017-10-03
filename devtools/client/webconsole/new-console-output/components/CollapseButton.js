/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  DOM: dom,
} = require("devtools/client/shared/vendor/react");

const { l10n } = require("devtools/client/webconsole/new-console-output/utils/messages");
const messageToggleDetails = l10n.getStr("messageToggleDetails");

function CollapseButton(props) {
  const {
    open,
    onClick,
    title = messageToggleDetails,
  } = props;

  let classes = ["theme-twisty"];

  if (open) {
    classes.push("open");
  }

  return dom.a({
    className: classes.join(" "),
    onClick,
    title: title,
  });
}

module.exports = CollapseButton;
