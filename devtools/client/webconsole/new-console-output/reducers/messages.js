/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Immutable = require("devtools/client/shared/vendor/immutable");
const constants = require("devtools/client/webconsole/new-console-output/constants");
const {isGroupType} = require("devtools/client/webconsole/new-console-output/utils/messages");
const Services = require("Services");

const logLimit = Math.max(Services.prefs.getIntPref("devtools.hud.loglimit"), 1);

const MessageState = Immutable.Record({
  // List of all the messages added to the console.
  messagesById: Immutable.List(),
  // List of the message ids which are opened.
  messagesUiById: Immutable.List(),
  // Map of the form {messageId : tableData}, which represent the data passed
  // as an argument in console.table calls.
  messagesTableDataById: Immutable.Map(),
  // Map of the form {groupMessageId : groupArray},
  // where groupArray is the list of of all the parent groups' ids of the groupMessageId.
  groupsById: Immutable.Map(),
  // Message id of the current group (no corresponding console.groupEnd yet).
  currentGroup: null,
  // List of removed messages is used to release related (parameters) actors.
  // This array is not supposed to be consumed by any UI component.
  removedMessages: [],
});

function messages(state = new MessageState(), action) {
  const {
    messagesById,
    messagesUiById,
    messagesTableDataById,
    groupsById,
    currentGroup,
  } = state;

  switch (action.type) {
    case constants.MESSAGE_ADD:
      let newMessage = action.message;

      if (newMessage.type === constants.MESSAGE_TYPE.NULL_MESSAGE) {
        // When the message has a NULL type, we don't add it.
        return state;
      }

      if (newMessage.type === constants.MESSAGE_TYPE.END_GROUP) {
        // Compute the new current group.
        return state.set("currentGroup", getNewCurrentGroup(currentGroup, groupsById));
      }

      if (newMessage.allowRepeating && messagesById.size > 0) {
        let lastMessage = messagesById.last();
        if (lastMessage.repeatId === newMessage.repeatId) {
          return state.withMutations(function (record) {
            record.set("messagesById", messagesById.pop().push(
              newMessage.set("repeat", lastMessage.repeat + 1)
            ));
          });
        }
      }

      let parentGroups = getParentGroups(currentGroup, groupsById);
      newMessage = newMessage.withMutations(function (message) {
        message.set("groupId", currentGroup);
        message.set("indent", parentGroups.length);
      });

      return state.withMutations(function (record) {
        // Add the new message with a reference to the parent group.
        record.set("messagesById", messagesById.push(newMessage));

        if (newMessage.type === "trace") {
          // We want the stacktrace to be open by default.
          record.set("messagesUiById", messagesUiById.push(newMessage.id));
        } else if (isGroupType(newMessage.type)) {
          record.set("currentGroup", newMessage.id);
          record.set("groupsById", groupsById.set(newMessage.id, parentGroups));

          if (newMessage.type === constants.MESSAGE_TYPE.START_GROUP) {
            // We want the group to be open by default.
            record.set("messagesUiById", messagesUiById.push(newMessage.id));
          }
        }

        // Remove top level message if the total count of top level messages
        // exceeds the current limit.
        limitTopLevelMessageCount(state, record);
      });
    case constants.MESSAGES_CLEAR:
      return state.withMutations(function (record) {
        // Store all removed messages associated with some arguments.
        // This array is used by `releaseActorsEnhancer` to release
        // all related backend actors.
        record.set("removedMessages",
          record.messagesById.filter(msg => msg.parameters).toArray());

        // Clear immutable state.
        record.set("messagesById", Immutable.List());
        record.set("messagesUiById", Immutable.List());
        record.set("groupsById", Immutable.Map());
        record.set("currentGroup", null);
      });
    case constants.MESSAGE_OPEN:
      return state.set("messagesUiById", messagesUiById.push(action.id));
    case constants.MESSAGE_CLOSE:
      let index = state.messagesUiById.indexOf(action.id);
      return state.deleteIn(["messagesUiById", index]);
    case constants.MESSAGE_TABLE_RECEIVE:
      const {id, data} = action;
      return state.set("messagesTableDataById", messagesTableDataById.set(id, data));
    case constants.NETWORK_MESSAGE_UPDATE:
      let updateMessage = action.message;
      return state.set("messagesById", messagesById.map((message) =>
        (message.id === updateMessage.id) ? updateMessage : message
      ));
    case constants.REMOVED_MESSAGES_CLEAR:
      return state.set("removedMessages", []);
  }

  return state;
}

function getNewCurrentGroup(currentGoup, groupsById) {
  let newCurrentGroup = null;
  if (currentGoup) {
    // Retrieve the parent groups of the current group.
    let parents = groupsById.get(currentGoup);
    if (Array.isArray(parents) && parents.length > 0) {
      // If there's at least one parent, make the first one the new currentGroup.
      newCurrentGroup = parents[0];
    }
  }
  return newCurrentGroup;
}

function getParentGroups(currentGroup, groupsById) {
  let groups = [];
  if (currentGroup) {
    // If there is a current group, we add it as a parent
    groups = [currentGroup];

    // As well as all its parents, if it has some.
    let parentGroups = groupsById.get(currentGroup);
    if (Array.isArray(parentGroups) && parentGroups.length > 0) {
      groups = groups.concat(parentGroups);
    }
  }

  return groups;
}

/**
 * Remove all top level messages that exceeds message limit.
 * Also release all backend actors associated with these
 * messages.
 */
function limitTopLevelMessageCount(state, record) {
  let tempRecord = {
    messagesById: record.messagesById,
    messagesUiById: record.messagesUiById,
    messagesTableDataById: record.messagesTableDataById,
    groupsById: record.groupsById,
  };

  let removedMessages = state.removedMessages;

  // Remove top level messages logged over the limit.
  let topLevelCount = getToplevelMessageCount(tempRecord);
  while (topLevelCount > logLimit) {
    removedMessages.push(...removeFirstMessage(tempRecord));
    topLevelCount--;
  }

  // Filter out messages with no arguments. Only actual arguments
  // can be associated with backend actors.
  removedMessages = state.removedMessages.filter(msg => msg.parameters);

  // Update original record object
  record.set("messagesById", tempRecord.messagesById);
  record.set("messagesUiById", tempRecord.messagesUiById);
  record.set("messagesTableDataById", tempRecord.messagesTableDataById);
  record.set("groupsById", tempRecord.groupsById);
  record.set("removedMessages", removedMessages);
}

/**
 * Returns total count of top level messages (those which are not
 * within a group).
 */
function getToplevelMessageCount(record) {
  return [...record.messagesById].filter(message => !message.groupId).length;
}

/**
 * Remove first (the oldest) message from the store. The methods removes
 * also all its references and children from the store.
 *
 * @return {Array} Flat array of removed messages.
 */
function removeFirstMessage(record) {
  let firstMessage = record.messagesById.first();
  record.messagesById = record.messagesById.shift();

  // Remove from list of opened groups.
  let uiIndex = record.messagesUiById.indexOf(firstMessage);
  if (uiIndex >= 0) {
    record.messagesUiById = record.messagesUiById.delete(uiIndex);
  }

  // Remove from list of tables.
  if (record.messagesTableDataById.has(firstMessage.id)) {
    record.messagesTableDataById = record.messagesTableDataById.delete(firstMessage.id);
  }

  // Remove from list of parent groups.
  if (record.groupsById.has(firstMessage.id)) {
    record.groupsById = record.groupsById.delete(firstMessage.id);
  }

  let removedMessages = [firstMessage];

  // Remove all children. This loop assumes that children of removed
  // group immediately follows the group. We use recursion since
  // there might be inner groups.
  let message = record.messagesById.first();
  while (message.groupId == firstMessage.id) {
    removedMessages.push(...removeFirstMessage(record));
    message = record.messagesById.first();
  }

  // Return array with all removed messages.
  return removedMessages;
}

exports.messages = messages;
