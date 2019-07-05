/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const dom = require("devtools/client/shared/vendor/react-dom-factories");

const { l10n } = require("devtools/client/webconsole/utils/messages");
const messageToggleDetails = l10n.getStr("messageToggleDetails");

function CollapseButton(props) {
  const { open, onClick, title = messageToggleDetails } = props;

  return dom.button({
    "aria-expanded": open ? "true" : "false",
    "aria-label": title,
    className: "arrow collapse-button",
    onClick,
    onMouseDown: e => {
      // prevent focus from moving to the disclosure if clicked,
      // which is annoying if on the input
      e.preventDefault();
    },
    title: title,
  });
}

module.exports = CollapseButton;
