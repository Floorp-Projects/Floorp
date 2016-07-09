/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { getLogLimit } = require("devtools/client/webconsole/new-console-output/selectors/prefs");

function getAllMessages(state) {
  let messages = state.messages;
  let messageCount = messages.count();
  let logLimit = getLogLimit(state);

  if (messageCount > logLimit) {
    return messages.splice(0, messageCount - logLimit);
  }

  return messages;
}

exports.getAllMessages = getAllMessages;
