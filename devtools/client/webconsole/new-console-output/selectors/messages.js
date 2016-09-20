/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { l10n } = require("devtools/client/webconsole/new-console-output/utils/messages");
const { getAllFilters } = require("devtools/client/webconsole/new-console-output/selectors/filters");
const { getLogLimit } = require("devtools/client/webconsole/new-console-output/selectors/prefs");
const {
  MESSAGE_TYPE
} = require("devtools/client/webconsole/new-console-output/constants");

function getAllMessages(state) {
  let messages = state.messages.messagesById;
  let logLimit = getLogLimit(state);
  let filters = getAllFilters(state);

  return prune(
    search(
      filterLevel(messages, filters),
      filters.text
    ),
    logLimit
  );
}

function getAllMessagesUiById(state) {
  return state.messages.messagesUiById;
}

function filterLevel(messages, filters) {
  return messages.filter((message) => {
    return filters.get(message.level) === true
      || [MESSAGE_TYPE.COMMAND, MESSAGE_TYPE.RESULT].includes(message.type);
  });
}

function search(messages, text = "") {
  if (text === "") {
    return messages;
  }

  return messages.filter(function (message) {
    // Evaluation Results and Console Commands are never filtered.
    if ([ MESSAGE_TYPE.RESULT, MESSAGE_TYPE.COMMAND ].includes(message.type)) {
      return true;
    }

    return (
      // @TODO currently we return true for any object grip. We should find a way to
      // search object grips.
      message.parameters !== null && !Array.isArray(message.parameters)
      // Look for a match in location.
      || isTextInFrame(text, message.frame)
      // Look for a match in stacktrace.
      || (
        Array.isArray(message.stacktrace) &&
        message.stacktrace.some(frame => isTextInFrame(text,
          // isTextInFrame expect the properties of the frame object to be in the same
          // order they are rendered in the Frame component.
          {
            functionName: frame.functionName ||
              l10n.getStr("stacktrace.anonymousFunction"),
            filename: frame.filename,
            lineNumber: frame.lineNumber,
            columnNumber: frame.columnNumber
          }))
      )
      // Look for a match in messageText.
      || (message.messageText !== null
            && message.messageText.toLocaleLowerCase().includes(text.toLocaleLowerCase()))
      // Look for a match in parameters. Currently only checks value grips.
      || (message.parameters !== null
          && message.parameters.join("").toLocaleLowerCase()
              .includes(text.toLocaleLowerCase()))
    );
  });
}

function isTextInFrame(text, frame) {
  // @TODO Change this to Object.values once it's supported in Node's version of V8
  return Object.keys(frame)
    .map(key => frame[key])
    .join(":")
    .toLocaleLowerCase()
    .includes(text.toLocaleLowerCase());
}

function prune(messages, logLimit) {
  let messageCount = messages.count();
  if (messageCount > logLimit) {
    return messages.splice(0, messageCount - logLimit);
  }

  return messages;
}

exports.getAllMessages = getAllMessages;
exports.getAllMessagesUiById = getAllMessagesUiById;
