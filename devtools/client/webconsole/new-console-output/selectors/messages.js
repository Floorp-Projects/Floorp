/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

function getAllMessagesById(state) {
  return state.messages.messagesById;
}

function getMessage(state, id) {
  return getAllMessagesById(state).get(id);
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

function getVisibleMessages(state) {
  return state.messages.visibleMessages.map(id => getMessage(state, id));
}

module.exports = {
  getMessage,
  getAllMessagesById,
  getAllMessagesUiById,
  getAllMessagesTableDataById,
  getAllGroupsById,
  getCurrentGroup,
  getVisibleMessages,
};
