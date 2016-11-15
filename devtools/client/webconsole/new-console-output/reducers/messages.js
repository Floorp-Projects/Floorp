/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Immutable = require("devtools/client/shared/vendor/immutable");
const constants = require("devtools/client/webconsole/new-console-output/constants");
const {isGroupType} = require("devtools/client/webconsole/new-console-output/utils/messages");

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

      return state.withMutations(function (record) {
        // Add the new message with a reference to the parent group.
        record.set(
          "messagesById",
          messagesById.push(newMessage.set("groupId", currentGroup))
        );

        if (newMessage.type === "trace") {
          // We want the stacktrace to be open by default.
          record.set("messagesUiById", messagesUiById.push(newMessage.id));
        } else if (isGroupType(newMessage.type)) {
          record.set("currentGroup", newMessage.id);
          record.set("groupsById",
            groupsById.set(
              newMessage.id,
              getParentGroups(currentGroup, groupsById)
            )
          );

          if (newMessage.type === constants.MESSAGE_TYPE.START_GROUP) {
            // We want the group to be open by default.
            record.set("messagesUiById", messagesUiById.push(newMessage.id));
          }
        }
      });
    case constants.MESSAGES_CLEAR:
      return state.withMutations(function (record) {
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

exports.messages = messages;
