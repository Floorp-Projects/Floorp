/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { getAllFilters } = require("devtools/client/webconsole/new-console-output/selectors/filters");
const { getLogLimit } = require("devtools/client/webconsole/new-console-output/selectors/prefs");

function getAllMessages(state) {
  let messages = state.messages;
  let logLimit = getLogLimit(state);
  let filters = getAllFilters(state);

  return prune(
    search(
      filterSeverity(messages, filters),
      filters.text
    ),
    logLimit
  );
}

function filterSeverity(messages, filters) {
  return messages.filter((message) => filters[message.severity] === true);
}

function search(messages, text = "") {
  if (text === "") {
    return messages;
  }

  return messages.filter(function (message) {
    // @TODO: message.parameters can be a grip, see how we can handle that
    if (!Array.isArray(message.parameters)) {
      return true;
    }
    return message
      .parameters.join("")
      .toLocaleLowerCase()
      .includes(text.toLocaleLowerCase());
  });
}

function prune(messages, logLimit) {
  let messageCount = messages.count();
  if (messageCount > logLimit) {
    return messages.splice(0, messageCount - logLimit);
  }

  return messages;
}

exports.getAllMessages = getAllMessages;
