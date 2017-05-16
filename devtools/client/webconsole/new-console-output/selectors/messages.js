/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { l10n } = require("devtools/client/webconsole/new-console-output/utils/messages");
const { getAllFilters } = require("devtools/client/webconsole/new-console-output/selectors/filters");
const { getLogLimit } = require("devtools/client/webconsole/new-console-output/selectors/prefs");
const { getGripPreviewItems } = require("devtools/client/shared/components/reps/reps");
const { getSourceNames } = require("devtools/client/shared/source-utils");
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
    // Look for a match in parameters.
    || isTextInParameters(text, message.parameters)
    // Look for a match in location.
    || isTextInFrame(text, message.frame)
    // Look for a match in net events.
    || isTextInNetEvent(text, message.request)
    // Look for a match in stack-trace.
    || isTextInStackTrace(text, message.stacktrace)
    // Look for a match in messageText.
    || isTextInMessageText(text, message.messageText)
    // Look for a match in notes.
    || isTextInNotes(text, message.notes)
  );
}

/**
 * Returns true if given text is included in provided stack frame.
 */
function isTextInFrame(text, frame) {
  if (!frame) {
    return false;
  }

  const { short } = getSourceNames(frame.source);
  return `${short}:${frame.line}:${frame.column}`
    .toLocaleLowerCase()
    .includes(text.toLocaleLowerCase());
}

/**
 * Returns true if given text is included in provided parameters.
 */
function isTextInParameters(text, parameters) {
  if (!parameters) {
    return false;
  }

  text = text.toLocaleLowerCase();
  return getAllProps(parameters).find(prop =>
    (prop + "").toLocaleLowerCase().includes(text)
  );
}

/**
 * Returns true if given text is included in provided net event grip.
 */
function isTextInNetEvent(text, request) {
  if (!request) {
    return false;
  }

  text = text.toLocaleLowerCase();

  let method = request.method.toLocaleLowerCase();
  let url = request.url.toLocaleLowerCase();
  return method.includes(text) || url.includes(text);
}

/**
 * Returns true if given text is included in provided stack trace.
 */
function isTextInStackTrace(text, stacktrace) {
  if (!Array.isArray(stacktrace)) {
    return false;
  }

  // isTextInFrame expect the properties of the frame object to be in the same
  // order they are rendered in the Frame component.
  return stacktrace.some(frame => isTextInFrame(text, {
    functionName: frame.functionName || l10n.getStr("stacktrace.anonymousFunction"),
    filename: frame.filename,
    lineNumber: frame.lineNumber,
    columnNumber: frame.columnNumber
  }));
}

/**
 * Returns true if given text is included in `messageText` field.
 */
function isTextInMessageText(text, messageText) {
  if (!messageText) {
    return false;
  }

  return messageText.toLocaleLowerCase().includes(text.toLocaleLowerCase());
}

/**
 * Returns true if given text is included in notes.
 */
function isTextInNotes(text, notes) {
  if (!Array.isArray(notes)) {
    return false;
  }

  return notes.some(note =>
    // Look for a match in location.
    isTextInFrame(text, note.frame) ||
    // Look for a match in messageBody.
    (note.messageBody &&
        note.messageBody.toLocaleLowerCase()
          .includes(text.toLocaleLowerCase()))
  );
}

/**
 * Get a flat array of all the grips and their properties.
 *
 * @param {Array} Grips
 * @return {Array} Flat array of the grips and their properties.
 */
function getAllProps(grips) {
  let result = grips.reduce((res, grip) => {
    let previewItems = getGripPreviewItems(grip);
    let allProps = previewItems.length > 0 ? getAllProps(previewItems) : [];
    return [...res, grip, grip.class, ...allProps];
  }, []);

  // We are interested only in primitive props (to search for)
  // not in objects and undefined previews.
  result = result.filter(grip =>
    typeof grip != "object" &&
    typeof grip != "undefined"
  );

  return [...new Set(result)];
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
