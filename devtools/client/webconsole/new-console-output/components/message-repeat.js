
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
const { PluralForm } = require("devtools/shared/plural-form");
const { l10n } = require("devtools/client/webconsole/new-console-output/utils/messages");

MessageRepeat.displayName = "MessageRepeat";

MessageRepeat.propTypes = {
  repeat: PropTypes.number.isRequired
};

function MessageRepeat(props) {
  const { repeat } = props;
  const visibility = repeat > 1 ? "visible" : "hidden";

  return dom.span({
    className: "message-repeats",
    style: {visibility},
    title: PluralForm.get(repeat, l10n.getStr("messageRepeats.tooltip2"))
      .replace("#1", repeat)
  }, repeat);
}

module.exports = MessageRepeat;
