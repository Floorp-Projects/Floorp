/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Immutable = require("devtools/client/shared/vendor/immutable");
const { l10n } = require("devtools/client/webconsole/new-console-output/utils/messages");

const constants = require("devtools/client/webconsole/new-console-output/constants");
const {isGroupType} = require("devtools/client/webconsole/new-console-output/utils/messages");
const {
  MESSAGE_TYPE,
  MESSAGE_SOURCE
} = require("devtools/client/webconsole/new-console-output/constants");
const { getGripPreviewItems } = require("devtools/client/shared/components/reps/reps");
const { getSourceNames } = require("devtools/client/shared/source-utils");

const MessageState = Immutable.Record({
  // List of all the messages added to the console.
  messagesById: Immutable.OrderedMap(),
  // Array of the visible messages.
  visibleMessages: [],
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
  // Map of the form {messageId : numberOfRepeat}
  repeatById: {},
  // Map of the form {messageId : networkInformation}
  // `networkInformation` holds request, response, totalTime, ...
  networkMessagesUpdateById: {},
});

function messages(state = new MessageState(), action, filtersState, prefsState) {
  const {
    messagesById,
    messagesUiById,
    messagesTableDataById,
    networkMessagesUpdateById,
    groupsById,
    currentGroup,
    repeatById,
    visibleMessages,
  } = state;

  const {logLimit} = prefsState;

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
        if (
          lastMessage.repeatId === newMessage.repeatId
          && lastMessage.groupId === currentGroup
        ) {
          return state.set(
            "repeatById",
            Object.assign({}, repeatById, {
              [lastMessage.id]: (repeatById[lastMessage.id] || 1) + 1
            })
          );
        }
      }

      return state.withMutations(function (record) {
        // Add the new message with a reference to the parent group.
        let parentGroups = getParentGroups(currentGroup, groupsById);
        newMessage.groupId = currentGroup;
        newMessage.indent = parentGroups.length;

        const addedMessage = Object.freeze(newMessage);
        record.set(
          "messagesById",
          messagesById.set(newMessage.id, addedMessage)
        );

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

        if (shouldMessageBeVisible(addedMessage, record, filtersState)) {
          record.set("visibleMessages", [...visibleMessages, newMessage.id]);
        }

        // Remove top level message if the total count of top level messages
        // exceeds the current limit.
        if (record.messagesById.size > logLimit) {
          limitTopLevelMessageCount(state, record, logLimit);
        }
      });

    case constants.MESSAGES_CLEAR:
      return new MessageState({
        // Store all removed messages associated with some arguments.
        // This array is used by `releaseActorsEnhancer` to release
        // all related backend actors.
        "removedMessages": [...state.messagesById].reduce((res, [id, msg]) => {
          if (msg.parameters) {
            res.push(msg);
          }
          return res;
        }, [])
      });

    case constants.MESSAGE_OPEN:
      return state.withMutations(function (record) {
        record.set("messagesUiById", messagesUiById.push(action.id));

        // If the message is a group
        if (isGroupType(messagesById.get(action.id).type)) {
          // We want to make its children visible
          const messagesToShow = [...messagesById].reduce((res, [id, message]) => {
            if (
              !visibleMessages.includes(message.id)
              && getParentGroups(message.groupId, groupsById).includes(action.id)
              && shouldMessageBeVisible(
                message,
                record,
                filtersState,
                // We want to check if the message is in an open group
                // only if it is not a direct child of the group we're opening.
                message.groupId !== action.id
              )
            ) {
              res.push(id);
            }
            return res;
          }, []);

          // We can then insert the messages ids right after the one of the group.
          const insertIndex = visibleMessages.indexOf(action.id) + 1;
          record.set("visibleMessages", [
            ...visibleMessages.slice(0, insertIndex),
            ...messagesToShow,
            ...visibleMessages.slice(insertIndex),
          ]);
        }
      });

    case constants.MESSAGE_CLOSE:
      return state.withMutations(function (record) {
        let messageId = action.id;
        let index = record.messagesUiById.indexOf(messageId);
        record.deleteIn(["messagesUiById", index]);

        // If the message is a group
        if (isGroupType(messagesById.get(messageId).type)) {
          // Hide all its children
          record.set(
            "visibleMessages",
            [...visibleMessages].filter(id => getParentGroups(
                messagesById.get(id).groupId,
                groupsById
              ).includes(messageId) === false
            )
          );
        }
      });

    case constants.MESSAGE_TABLE_RECEIVE:
      const {id, data} = action;
      return state.set("messagesTableDataById", messagesTableDataById.set(id, data));
    case constants.NETWORK_MESSAGE_UPDATE:
      return state.set(
        "networkMessagesUpdateById",
        Object.assign({}, networkMessagesUpdateById, {
          [action.message.id]: action.message
        })
      );

    case constants.REMOVED_MESSAGES_CLEAR:
      return state.set("removedMessages", []);

    case constants.FILTER_TOGGLE:
    case constants.FILTER_TEXT_SET:
      return state.set(
        "visibleMessages",
        [...messagesById].reduce((res, [messageId, message]) => {
          if (shouldMessageBeVisible(message, state, filtersState)) {
            res.push(messageId);
          }
          return res;
        }, [])
      );
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
 * Also populate an array of all backend actors associated with these
 * messages so they can be released.
 */
function limitTopLevelMessageCount(state, record, logLimit) {
  let topLevelCount = record.groupsById.size === 0
    ? record.messagesById.size
    : getToplevelMessageCount(record);

  if (topLevelCount <= logLimit) {
    return record;
  }

  const removedMessagesId = [];
  const removedMessages = [];
  let visibleMessages = [...record.visibleMessages];

  let cleaningGroup = false;
  record.messagesById.forEach((message, id) => {
    // If we were cleaning a group and the current message does not have
    // a groupId, we're done cleaning.
    if (cleaningGroup === true && !message.groupId) {
      cleaningGroup = false;
    }

    // If we're not cleaning a group and the message count is below the logLimit,
    // we exit the forEach iteration.
    if (cleaningGroup === false && topLevelCount <= logLimit) {
      return false;
    }

    // If we're not currently cleaning a group, and the current message is identified
    // as a group, set the cleaning flag to true.
    if (cleaningGroup === false && record.groupsById.has(id)) {
      cleaningGroup = true;
    }

    if (!message.groupId) {
      topLevelCount--;
    }

    removedMessagesId.push(id);

    // Filter out messages with no arguments. Only actual arguments
    // can be associated with backend actors.
    if (message && message.parameters) {
      removedMessages.push(message);
    }

    const index = visibleMessages.indexOf(id);
    if (index > -1) {
      visibleMessages.splice(index, 1);
    }

    return true;
  });

  if (removedMessages.length > 0) {
    record.set("removedMessages", record.removedMessages.concat(removedMessages));
  }

  if (record.visibleMessages.length > visibleMessages.length) {
    record.set("visibleMessages", visibleMessages);
  }

  const isInRemovedId = id => removedMessagesId.includes(id);
  const mapHasRemovedIdKey = map => map.findKey((value, id) => isInRemovedId(id));
  const objectHasRemovedIdKey = obj => Object.keys(obj).findIndex(isInRemovedId) !== -1;
  const cleanUpCollection = map => removedMessagesId.forEach(id => map.remove(id));
  const cleanUpList = list => list.filter(id => {
    return isInRemovedId(id) === false;
  });
  const cleanUpObject = object => [...Object.entries(object)]
    .reduce((res, [id, value]) => {
      if (!isInRemovedId(id)) {
        res[id] = value;
      }
      return res;
    }, {});

  record.set("messagesById", record.messagesById.withMutations(cleanUpCollection));

  if (record.messagesUiById.find(isInRemovedId)) {
    record.set("messagesUiById", cleanUpList(record.messagesUiById));
  }
  if (mapHasRemovedIdKey(record.messagesTableDataById)) {
    record.set("messagesTableDataById",
      record.messagesTableDataById.withMutations(cleanUpCollection));
  }
  if (mapHasRemovedIdKey(record.groupsById)) {
    record.set("groupsById", record.groupsById.withMutations(cleanUpCollection));
  }
  if (objectHasRemovedIdKey(record.repeatById)) {
    record.set("repeatById", cleanUpObject(record.repeatById));
  }

  if (objectHasRemovedIdKey(record.networkMessagesUpdateById)) {
    record.set("networkMessagesUpdateById",
      cleanUpObject(record.networkMessagesUpdateById));
  }

  return record;
}

/**
 * Returns total count of top level messages (those which are not
 * within a group).
 */
function getToplevelMessageCount(record) {
  return record.messagesById.count(message => !message.groupId);
}

function shouldMessageBeVisible(message, messagesState, filtersState, checkGroup = true) {
  return (
    (
      checkGroup === false
      || isInOpenedGroup(message, messagesState.groupsById, messagesState.messagesUiById)
    )
    && (
      isUnfilterable(message)
      || (
        matchLevelFilters(message, filtersState)
        && matchCssFilters(message, filtersState)
        && matchNetworkFilters(message, filtersState)
        && matchSearchFilters(message, filtersState)
      )
    )
  );
}

function isUnfilterable(message) {
  return [
    MESSAGE_TYPE.COMMAND,
    MESSAGE_TYPE.RESULT,
    MESSAGE_TYPE.START_GROUP,
    MESSAGE_TYPE.START_GROUP_COLLAPSED,
  ].includes(message.type);
}

function isInOpenedGroup(message, groupsById, messagesUI) {
  return !message.groupId
    || (
      !isGroupClosed(message.groupId, messagesUI)
      && !hasClosedParentGroup(groupsById.get(message.groupId), messagesUI)
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
  let text = (filters.text || "").trim();
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

  const {
    functionName,
    line,
    column,
    source
  } = frame;
  const { short } = getSourceNames(source);

  return `${functionName ? functionName + " " : ""}${short}:${line}:${column}`
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
  return getAllProps(parameters).some(prop =>
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
    source: frame.filename,
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
    (
      note.messageBody &&
      note.messageBody.toLocaleLowerCase().includes(text.toLocaleLowerCase())
    )
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

exports.messages = messages;
