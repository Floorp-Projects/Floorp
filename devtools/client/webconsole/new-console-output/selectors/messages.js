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
  MESSAGE_TYPE,
  MESSAGE_SOURCE
} = require("devtools/client/webconsole/new-console-output/constants");

function getAllMessages(state) {
  let messages = getAllMessagesById(state);
  let logLimit = getLogLimit(state);
  let filters = getAllFilters(state);

  let groups = getAllGroupsById(state);
  let messagesUI = getAllMessagesUiById(state);

  return prune(
    messages.filter(message => {
      return (
        isInOpenedGroup(message, groups, messagesUI)
        && (
          isUnfilterable(message)
          || (
            matchLevelFilters(message, filters)
            && matchCssFilters(message, filters)
            && matchNetworkFilters(message, filters)
            && matchSearchFilters(message, filters)
          )
        )
      );
    }),
    logLimit
  );
}

function getAllMessagesById(state) {
  return state.messages.messagesById;
}

function getAllMessagesUiById(state) {
  return state.messages.messagesUiById;
}

function getAllMessagesTableDataById(state) {
  return state.messages.messagesTableDataById;
}

function getAllGroupsById(state) {
  return state.messages.groupsById;
}

function getCurrentGroup(state) {
  return state.messages.currentGroup;
}

function isUnfilterable(message) {
  return [
    MESSAGE_TYPE.COMMAND,
    MESSAGE_TYPE.RESULT,
    MESSAGE_TYPE.START_GROUP,
    MESSAGE_TYPE.START_GROUP_COLLAPSED,
  ].includes(message.type);
}

function isInOpenedGroup(message, groups, messagesUI) {
  return !message.groupId
    || (
      !isGroupClosed(message.groupId, messagesUI)
      && !hasClosedParentGroup(groups.get(message.groupId), messagesUI)
    );
}

function hasClosedParentGroup(group, messagesUI) {
  return group.some(groupId => isGroupClosed(groupId, messagesUI));
}

function isGroupClosed(groupId, messagesUI) {
  return messagesUI.includes(groupId) === false;
}

function matchLevelFilters(message, filters) {
  return filters.get(message.level) === true;
}

function matchNetworkFilters(message, filters) {
  return (
    message.source !== MESSAGE_SOURCE.NETWORK
    || (filters.get("net") === true && message.isXHR === false)
    || (filters.get("netxhr") === true && message.isXHR === true)
  );
}

function matchCssFilters(message, filters) {
  return (
    message.source != MESSAGE_SOURCE.CSS
    || filters.get("css") === true
  );
}

function matchSearchFilters(message, filters) {
  let text = filters.text || "";
  return (
    text === ""
    // @TODO currently we return true for any object grip. We should find a way to
    // search object grips.
    || (message.parameters !== null && !Array.isArray(message.parameters))
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
}

function isTextInFrame(text, frame) {
  if (!frame) {
    return false;
  }
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
    // If the second non-pruned message is in a group,
    // we want to return the group as the first non-pruned message.
    let firstIndex = messages.size - logLimit;
    let groupId = messages.get(firstIndex + 1).groupId;

    if (groupId) {
      return messages.splice(0, firstIndex + 1)
        .unshift(
          messages.findLast((message) => message.id === groupId)
        );
    }
    return messages.splice(0, firstIndex);
  }

  return messages;
}

exports.getAllMessages = getAllMessages;
exports.getAllMessagesUiById = getAllMessagesUiById;
exports.getAllMessagesTableDataById = getAllMessagesTableDataById;
exports.getAllGroupsById = getAllGroupsById;
exports.getCurrentGroup = getCurrentGroup;
