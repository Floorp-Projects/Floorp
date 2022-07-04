/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

loader.lazyRequireGetter(
  this,
  ["getParentWarningGroupMessageId", "getWarningGroupType"],
  "devtools/client/webconsole/utils/messages",
  true
);

function getMutableMessagesById(state) {
  return state.messages.mutableMessagesById;
}

function getMessage(state, id) {
  return getMutableMessagesById(state).get(id);
}

function getAllMessagesUiById(state) {
  return state.messages.messagesUiById;
}

function getAllDisabledMessagesById(state) {
  return state.messages.disabledMessagesById;
}

function getAllCssMessagesMatchingElements(state) {
  return state.messages.cssMessagesMatchingElements;
}

function getAllGroupsById(state) {
  return state.messages.groupsById;
}

function getCurrentGroup(state) {
  return state.messages.currentGroup;
}

function getVisibleMessages(state) {
  return state.messages.visibleMessages;
}

function getFilteredMessagesCount(state) {
  return state.messages.filteredMessagesCount;
}

function getAllRepeatById(state) {
  return state.messages.repeatById;
}

function getAllNetworkMessagesUpdateById(state) {
  return state.messages.networkMessagesUpdateById;
}

function getGroupsById(state) {
  return state.messages.groupsById;
}

function getAllWarningGroupsById(state) {
  return state.messages.warningGroupsById;
}

function getLastMessageId(state) {
  return state.messages.lastMessageId;
}

function isMessageInWarningGroup(message, visibleMessages = []) {
  if (!getWarningGroupType(message)) {
    return false;
  }

  return visibleMessages.includes(getParentWarningGroupMessageId(message));
}

module.exports = {
  getAllGroupsById,
  getAllWarningGroupsById,
  getMutableMessagesById,
  getAllCssMessagesMatchingElements,
  getAllMessagesUiById,
  getAllDisabledMessagesById,
  getAllNetworkMessagesUpdateById,
  getAllRepeatById,
  getCurrentGroup,
  getFilteredMessagesCount,
  getGroupsById,
  getLastMessageId,
  getMessage,
  getVisibleMessages,
  isMessageInWarningGroup,
};
