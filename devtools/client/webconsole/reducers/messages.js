/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  isGroupType,
  l10n,
} = require("devtools/client/webconsole/utils/messages");

const constants = require("devtools/client/webconsole/constants");
const { DEFAULT_FILTERS, FILTERS, MESSAGE_TYPE, MESSAGE_SOURCE } = constants;

loader.lazyRequireGetter(
  this,
  "getGripPreviewItems",
  "devtools/client/shared/components/reps/reps",
  true
);
loader.lazyRequireGetter(
  this,
  "getUnicodeUrlPath",
  "devtools/client/shared/unicode-url",
  true
);
loader.lazyRequireGetter(
  this,
  "getSourceNames",
  "devtools/client/shared/source-utils",
  true
);
loader.lazyRequireGetter(
  this,
  "createWarningGroupMessage",
  "devtools/client/webconsole/utils/messages",
  true
);
loader.lazyRequireGetter(
  this,
  "isWarningGroup",
  "devtools/client/webconsole/utils/messages",
  true
);
loader.lazyRequireGetter(
  this,
  "getWarningGroupType",
  "devtools/client/webconsole/utils/messages",
  true
);
loader.lazyRequireGetter(
  this,
  "getParentWarningGroupMessageId",
  "devtools/client/webconsole/utils/messages",
  true
);
ChromeUtils.defineModuleGetter(
  this,
  "pointPrecedes",
  "resource://devtools/shared/execution-point-utils.js"
);
ChromeUtils.defineModuleGetter(
  this,
  "pointEquals",
  "resource://devtools/shared/execution-point-utils.js"
);

const { UPDATE_REQUEST } = require("devtools/client/netmonitor/src/constants");

const {
  processNetworkUpdates,
} = require("devtools/client/netmonitor/src/utils/request-utils");

const MessageState = overrides =>
  Object.freeze(
    Object.assign(
      {
        // List of all the messages added to the console.
        messagesById: new Map(),
        // List of additional data associated with messages (populated async or on-demand at a
        // later time after the message is received).
        messagesPayloadById: new Map(),
        // Array of the visible messages.
        visibleMessages: [],
        // Object for the filtered messages.
        filteredMessagesCount: getDefaultFiltersCounter(),
        // List of the message ids which are opened.
        messagesUiById: [],
        // Map of the form {groupMessageId : groupArray},
        // where groupArray is the list of of all the parent groups' ids of the groupMessageId.
        // This handles console API groups.
        groupsById: new Map(),
        // Message id of the current console API group (no corresponding console.groupEnd yet).
        currentGroup: null,
        // This group handles "warning groups" (Content Blocking, CORS, CSP, …)
        warningGroupsById: new Map(),
        // Array of removed actors (i.e. actors logged in removed messages) we keep track of
        // in order to properly release them.
        // This array is not supposed to be consumed by any UI component.
        removedActors: [],
        // Map of the form {messageId : numberOfRepeat}
        repeatById: {},
        // Map of the form {messageId : networkInformation}
        // `networkInformation` holds request, response, totalTime, ...
        networkMessagesUpdateById: {},
        // Set of logpoint IDs that have been removed
        removedLogpointIds: new Set(),
        // Any execution point we are currently paused at, when replaying.
        pausedExecutionPoint: null,
        // Whether any messages with execution points have been seen.
        hasExecutionPoints: false,
      },
      overrides
    )
  );

function cloneState(state) {
  return {
    messagesById: new Map(state.messagesById),
    visibleMessages: [...state.visibleMessages],
    filteredMessagesCount: { ...state.filteredMessagesCount },
    messagesUiById: [...state.messagesUiById],
    messagesPayloadById: new Map(state.messagesPayloadById),
    groupsById: new Map(state.groupsById),
    currentGroup: state.currentGroup,
    removedActors: [...state.removedActors],
    repeatById: { ...state.repeatById },
    networkMessagesUpdateById: { ...state.networkMessagesUpdateById },
    removedLogpointIds: new Set(state.removedLogpointIds),
    pausedExecutionPoint: state.pausedExecutionPoint,
    hasExecutionPoints: state.hasExecutionPoints,
    warningGroupsById: new Map(state.warningGroupsById),
  };
}

/**
 * Add a console message to the state.
 *
 * @param {ConsoleMessage} newMessage: The message to add to the state.
 * @param {MessageState} state: The message state ( = managed by this reducer).
 * @param {FiltersState} filtersState: The filters state.
 * @param {PrefsState} prefsState: The preferences state.
 * @param {UiState} uiState: The ui state.
 * @returns {MessageState} a new messages state.
 */
/* eslint-disable complexity */
function addMessage(newMessage, state, filtersState, prefsState, uiState) {
  const { messagesById, groupsById, currentGroup, repeatById } = state;

  if (newMessage.type === constants.MESSAGE_TYPE.NULL_MESSAGE) {
    // When the message has a NULL type, we don't add it.
    return state;
  }

  // After messages with a given logpoint ID have been removed, ignore all
  // future messages with that ID.
  if (
    newMessage.logpointId &&
    state.removedLogpointIds &&
    state.removedLogpointIds.has(newMessage.logpointId)
  ) {
    return state;
  }

  if (newMessage.type === constants.MESSAGE_TYPE.END_GROUP) {
    // Compute the new current group.
    state.currentGroup = getNewCurrentGroup(currentGroup, groupsById);
    return state;
  }

  if (newMessage.allowRepeating && messagesById.size > 0) {
    const lastMessage = messagesById.get(getLastMessageId(state));

    if (
      lastMessage.repeatId === newMessage.repeatId &&
      lastMessage.groupId === currentGroup
    ) {
      state.repeatById[lastMessage.id] = (repeatById[lastMessage.id] || 1) + 1;
      return state;
    }
  }

  // Add the new message with a reference to the parent group.
  const parentGroups = getParentGroups(currentGroup, groupsById);
  if (!isWarningGroup(newMessage)) {
    newMessage.groupId = currentGroup;
    newMessage.indent = parentGroups.length;
  }

  ensureExecutionPoint(state, newMessage);

  if (newMessage.executionPoint) {
    state.hasExecutionPoints = true;
  }

  // When replaying, we might get two messages with the same execution point and
  // logpoint ID. In this case the first message is provisional and should be
  // removed.
  const removedIds = [];
  if (newMessage.logpointId) {
    const existingMessage = [...state.messagesById.values()].find(existing => {
      return (
        existing.logpointId == newMessage.logpointId &&
        pointEquals(existing.executionPoint, newMessage.executionPoint)
      );
    });
    if (existingMessage) {
      removedIds.push(existingMessage.id);
    }
  }

  // Check if the current message could be placed in a Warning Group.
  // This needs to be done before setting the new message in messagesById so we have a
  // proper message.
  const warningGroupType = getWarningGroupType(newMessage);

  // If the preference for warning grouping is true, and the new message could be in a
  // warning group.
  if (prefsState.groupWarnings && warningGroupType !== null) {
    const warningGroupMessageId = getParentWarningGroupMessageId(newMessage);

    // If there's no warning group for the type/innerWindowID yet
    if (!state.messagesById.has(warningGroupMessageId)) {
      // We create it and add it to the store.
      const groupMessage = createWarningGroupMessage(
        warningGroupMessageId,
        warningGroupType,
        newMessage
      );
      state = addMessage(
        groupMessage,
        state,
        filtersState,
        prefsState,
        uiState
      );
    }

    // We add the new message to the appropriate warningGroup.
    state.warningGroupsById.get(warningGroupMessageId).push(newMessage.id);

    // If the warningGroup message is not visible yet, but should be.
    if (
      !state.visibleMessages.includes(warningGroupMessageId) &&
      getMessageVisibility(state.messagesById.get(warningGroupMessageId), {
        messagesState: state,
        filtersState,
        prefsState,
        uiState,
      }).visible
    ) {
      // Then we put it in the visibleMessages properties, at the position of the first
      // warning message inside the warningGroup.
      // If that first warning message is in a console.group, we place it before the
      // outermost console.group message.
      const firstWarningMessageId = state.warningGroupsById.get(
        warningGroupMessageId
      )[0];
      const firstWarningMessage = state.messagesById.get(firstWarningMessageId);
      const outermostGroupId = getOutermostGroup(
        firstWarningMessage,
        groupsById
      );
      const groupIndex = state.visibleMessages.indexOf(outermostGroupId);
      const warningMessageIndex = state.visibleMessages.indexOf(
        firstWarningMessageId
      );

      if (groupIndex > -1) {
        // We remove the warning message
        if (warningMessageIndex > -1) {
          state.visibleMessages.splice(warningMessageIndex, 1);
        }

        // And we put the warning group before the console.group
        state.visibleMessages.splice(groupIndex, 0, warningGroupMessageId);
      } else {
        // If the warning message is not in a console.group, we replace it by the
        // warning group message.
        state.visibleMessages.splice(
          warningMessageIndex,
          1,
          warningGroupMessageId
        );
      }
    }
  }

  // If we're creating a warningGroup, we init the array for its children.
  if (isWarningGroup(newMessage)) {
    state.warningGroupsById.set(newMessage.id, []);
  }

  const addedMessage = Object.freeze(newMessage);
  state.messagesById.set(newMessage.id, addedMessage);

  if (newMessage.type === "trace") {
    // We want the stacktrace to be open by default.
    state.messagesUiById.push(newMessage.id);
  } else if (isGroupType(newMessage.type)) {
    state.currentGroup = newMessage.id;
    state.groupsById.set(newMessage.id, parentGroups);

    if (newMessage.type === constants.MESSAGE_TYPE.START_GROUP) {
      // We want the group to be open by default.
      state.messagesUiById.push(newMessage.id);
    }
  }

  const { visible, cause } = getMessageVisibility(addedMessage, {
    messagesState: state,
    filtersState,
    prefsState,
    uiState,
  });

  if (visible) {
    // If the message is part of a visible warning group, we want to add it after the last
    // visible message of the group.
    const warningGroupId = getParentWarningGroupMessageId(newMessage);
    if (warningGroupId && state.visibleMessages.includes(warningGroupId)) {
      // Defaults to the warning group message.
      let index = state.visibleMessages.indexOf(warningGroupId);

      // We loop backward through the warning group's messages to get the latest visible
      // messages in it.
      const messagesInWarningGroup = state.warningGroupsById.get(
        warningGroupId
      );
      for (let i = messagesInWarningGroup.length - 1; i >= 0; i--) {
        const idx = state.visibleMessages.indexOf(messagesInWarningGroup[i]);
        if (idx > -1) {
          index = idx;
          break;
        }
      }
      // Inserts the new warning message at the wanted location "in" the warning group.
      state.visibleMessages.splice(index + 1, 0, newMessage.id);
    } else {
      state.visibleMessages.push(newMessage.id);
    }
    maybeSortVisibleMessages(state, false);
  } else if (DEFAULT_FILTERS.includes(cause)) {
    state.filteredMessagesCount.global++;
    state.filteredMessagesCount[cause]++;
  }

  // Append received network-data also into networkMessagesUpdateById
  // that is responsible for collecting (lazy loaded) HTTP payload data.
  if (newMessage.source == "network") {
    state.networkMessagesUpdateById[newMessage.actor] = newMessage;
  }

  return removeMessagesFromState(state, removedIds);
}
/* eslint-enable complexity */

/* eslint-disable complexity */
function messages(
  state = MessageState(),
  action,
  filtersState,
  prefsState,
  uiState
) {
  const {
    messagesById,
    messagesPayloadById,
    messagesUiById,
    networkMessagesUpdateById,
    groupsById,
    visibleMessages,
  } = state;

  const { logLimit } = prefsState;

  let newState;
  switch (action.type) {
    case constants.PAUSED_EXECUTION_POINT:
      if (
        state.pausedExecutionPoint &&
        action.executionPoint &&
        pointEquals(state.pausedExecutionPoint, action.executionPoint)
      ) {
        return state;
      }
      return { ...state, pausedExecutionPoint: action.executionPoint };
    case constants.MESSAGES_ADD:
      // Preemptively remove messages that will never be rendered
      const list = [];
      let prunableCount = 0;
      let lastMessageRepeatId = -1;
      for (let i = action.messages.length - 1; i >= 0; i--) {
        const message = action.messages[i];
        if (
          !message.groupId &&
          !isGroupType(message.type) &&
          message.type !== MESSAGE_TYPE.END_GROUP
        ) {
          if (message.repeatId !== lastMessageRepeatId) {
            prunableCount++;
          }
          // Once we've added the max number of messages that can be added, stop.
          // Except for repeated messages, where we keep adding over the limit.
          if (
            prunableCount <= logLimit ||
            message.repeatId == lastMessageRepeatId
          ) {
            list.unshift(action.messages[i]);
          } else {
            break;
          }
        } else {
          list.unshift(message);
        }
        lastMessageRepeatId = message.repeatId;
      }

      newState = cloneState(state);
      list.forEach(message => {
        newState = addMessage(
          message,
          newState,
          filtersState,
          prefsState,
          uiState
        );
      });

      return limitTopLevelMessageCount(newState, logLimit);

    case constants.MESSAGES_CLEAR:
      return MessageState({
        // Store all actors from removed messages. This array is used by
        // `releaseActorsEnhancer` to release all of those backend actors.
        removedActors: [...state.messagesById.values()].reduce((res, msg) => {
          res.push(...getAllActorsInMessage(msg));
          return res;
        }, []),
      });

    case constants.PRIVATE_MESSAGES_CLEAR: {
      const removedIds = [];
      for (const [id, message] of messagesById) {
        if (message.private === true) {
          removedIds.push(id);
        }
      }

      // If there's no private messages, there's no need to change the state.
      if (removedIds.length === 0) {
        return state;
      }

      return removeMessagesFromState(
        {
          ...state,
        },
        removedIds
      );
    }

    case constants.MESSAGES_CLEAR_LOGPOINT: {
      const removedIds = [];
      for (const [id, message] of messagesById) {
        if (message.logpointId == action.logpointId) {
          removedIds.push(id);
        }
      }

      if (removedIds.length === 0) {
        return state;
      }

      return removeMessagesFromState(
        {
          ...state,
          removedLogpointIds: new Set([
            ...state.removedLogpointIds,
            action.logpointId,
          ]),
        },
        removedIds
      );
    }

    case constants.MESSAGE_OPEN:
      const openState = { ...state };
      openState.messagesUiById = [...messagesUiById, action.id];
      const currMessage = messagesById.get(action.id);

      // If the message is a console.group/groupCollapsed or a warning group.
      if (isGroupType(currMessage.type) || isWarningGroup(currMessage)) {
        // We want to make its children visible
        const messagesToShow = [...messagesById].reduce(
          (res, [id, message]) => {
            if (
              !visibleMessages.includes(message.id) &&
              ((isWarningGroup(currMessage) &&
                !!getWarningGroupType(message)) ||
                (isGroupType(currMessage.type) &&
                  getParentGroups(message.groupId, groupsById).includes(
                    action.id
                  ))) &&
              getMessageVisibility(message, {
                messagesState: openState,
                filtersState,
                prefsState,
                uiState,
                // We want to check if the message is in an open group
                // only if it is not a direct child of the group we're opening.
                checkGroup: message.groupId !== action.id,
              }).visible
            ) {
              res.push(id);
            }
            return res;
          },
          []
        );

        // We can then insert the messages ids right after the one of the group.
        const insertIndex = visibleMessages.indexOf(action.id) + 1;
        openState.visibleMessages = [
          ...visibleMessages.slice(0, insertIndex),
          ...messagesToShow,
          ...visibleMessages.slice(insertIndex),
        ];
      }

      // If the current message is a network event, mark it as opened-once,
      // so HTTP details are not fetched again the next time the user
      // opens the log.
      if (currMessage.source == "network") {
        openState.messagesById = new Map(messagesById).set(action.id, {
          ...currMessage,
          openedOnce: true,
        });
      }
      return openState;

    case constants.MESSAGE_CLOSE:
      const closeState = { ...state };
      const messageId = action.id;
      const index = closeState.messagesUiById.indexOf(messageId);
      closeState.messagesUiById.splice(index, 1);
      closeState.messagesUiById = [...closeState.messagesUiById];

      // If the message is a group
      if (isGroupType(messagesById.get(messageId).type)) {
        // Hide all its children, unless they're in a warningGroup.
        closeState.visibleMessages = visibleMessages.filter((id, i, arr) => {
          const message = messagesById.get(id);
          const warningGroupMessage = messagesById.get(
            getParentWarningGroupMessageId(message)
          );

          // If the message is in a warning group, then we return its current visibility.
          if (
            shouldGroupWarningMessages(
              warningGroupMessage,
              closeState,
              prefsState
            )
          ) {
            return arr.includes(id);
          }

          const parentGroups = getParentGroups(message.groupId, groupsById);
          return parentGroups.includes(messageId) === false;
        });
      } else if (isWarningGroup(messagesById.get(messageId))) {
        // If the message was a warningGroup, we hide all the messages in the group.
        const groupMessages = closeState.warningGroupsById.get(messageId);
        closeState.visibleMessages = visibleMessages.filter(
          id => !groupMessages.includes(id)
        );
      }
      return closeState;

    case constants.MESSAGE_UPDATE_PAYLOAD:
      return {
        ...state,
        messagesPayloadById: new Map(messagesPayloadById).set(
          action.id,
          action.data
        ),
      };

    case constants.NETWORK_MESSAGE_UPDATE:
      return {
        ...state,
        networkMessagesUpdateById: {
          ...networkMessagesUpdateById,
          [action.message.id]: action.message,
        },
      };

    case UPDATE_REQUEST:
    case constants.NETWORK_UPDATE_REQUEST: {
      const request = networkMessagesUpdateById[action.id];
      if (!request) {
        return state;
      }

      return {
        ...state,
        networkMessagesUpdateById: {
          ...networkMessagesUpdateById,
          [action.id]: {
            ...request,
            ...processNetworkUpdates(action.data, request),
          },
        },
      };
    }

    case constants.REMOVED_ACTORS_CLEAR:
      return {
        ...state,
        removedActors: [],
      };

    case constants.WARNING_GROUPS_TOGGLE:
      // There's no warningGroups, and the pref was set to false,
      // we don't need to do anything.
      if (!prefsState.groupWarnings && state.warningGroupsById.size === 0) {
        return state;
      }

      let needSort = false;
      const messageEntries = state.messagesById.entries();
      for (const [msgId, message] of messageEntries) {
        const warningGroupType = getWarningGroupType(message);
        if (warningGroupType) {
          const warningGroupMessageId = getParentWarningGroupMessageId(message);

          // If there's no warning group for the type/innerWindowID yet.
          if (!state.messagesById.has(warningGroupMessageId)) {
            // We create it and add it to the store.
            const groupMessage = createWarningGroupMessage(
              warningGroupMessageId,
              warningGroupType,
              message
            );
            state = addMessage(
              groupMessage,
              state,
              filtersState,
              prefsState,
              uiState
            );
          }

          // We add the new message to the appropriate warningGroup.
          const warningGroup = state.warningGroupsById.get(
            warningGroupMessageId
          );
          if (warningGroup && !warningGroup.includes(msgId)) {
            warningGroup.push(msgId);
          }

          needSort = true;
        }
      }

      // If we don't have any warning messages that could be in a group, we don't do
      // anything.
      if (!needSort) {
        return state;
      }

      return setVisibleMessages({
        messagesState: state,
        filtersState,
        prefsState,
        uiState,
        // If the user disabled warning groups, we want the messages to be sorted by their
        // timestamps.
        forceTimestampSort: !prefsState.groupWarnings,
      });

    case constants.FILTER_TOGGLE:
    case constants.FILTER_TEXT_SET:
    case constants.FILTERS_CLEAR:
    case constants.DEFAULT_FILTERS_RESET:
    case constants.SHOW_CONTENT_MESSAGES_TOGGLE:
      return setVisibleMessages({
        messagesState: state,
        filtersState,
        prefsState,
        uiState,
      });
  }

  return state;
}
/* eslint-enable complexity */

function setVisibleMessages({
  messagesState,
  filtersState,
  prefsState,
  uiState,
  forceTimestampSort = false,
}) {
  const { messagesById } = messagesState;

  const messagesToShow = [];
  const filtered = getDefaultFiltersCounter();

  messagesById.forEach((message, msgId) => {
    const { visible, cause } = getMessageVisibility(message, {
      messagesState,
      filtersState,
      prefsState,
      uiState,
    });

    if (visible) {
      messagesToShow.push(msgId);
    } else if (DEFAULT_FILTERS.includes(cause)) {
      filtered.global = filtered.global + 1;
      filtered[cause] = filtered[cause] + 1;
    }
  });

  const newState = {
    ...messagesState,
    visibleMessages: messagesToShow,
    filteredMessagesCount: filtered,
  };

  maybeSortVisibleMessages(
    newState,
    // Only sort for warningGroups if the feature is enabled
    prefsState.groupWarnings,
    forceTimestampSort
  );

  return newState;
}

/**
 * Returns the new current group id given the previous current group and the groupsById
 * state property.
 *
 * @param {String} currentGroup: id of the current group
 * @param {Map} groupsById
 * @param {Array} ignoredIds: An array of ids which can't be the new current group.
 * @returns {String|null} The new current group id, or null if there isn't one.
 */
function getNewCurrentGroup(currentGroup, groupsById, ignoredIds = []) {
  if (!currentGroup) {
    return null;
  }

  // Retrieve the parent groups of the current group.
  const parents = groupsById.get(currentGroup);

  // If there's at least one parent, make the first one the new currentGroup.
  if (Array.isArray(parents) && parents.length > 0) {
    // If the found group must be ignored, let's search for its parent.
    if (ignoredIds.includes(parents[0])) {
      return getNewCurrentGroup(parents[0], groupsById, ignoredIds);
    }

    return parents[0];
  }

  return null;
}

function getParentGroups(currentGroup, groupsById) {
  let groups = [];
  if (currentGroup) {
    // If there is a current group, we add it as a parent
    groups = [currentGroup];

    // As well as all its parents, if it has some.
    const parentGroups = groupsById.get(currentGroup);
    if (Array.isArray(parentGroups) && parentGroups.length > 0) {
      groups = groups.concat(parentGroups);
    }
  }

  return groups;
}

function getOutermostGroup(message, groupsById) {
  const groups = getParentGroups(message.groupId, groupsById);
  if (groups.length === 0) {
    return null;
  }
  return groups[groups.length - 1];
}

/**
 * Remove all top level messages that exceeds message limit.
 * Also populate an array of all backend actors associated with these
 * messages so they can be released.
 */
function limitTopLevelMessageCount(newState, logLimit) {
  let topLevelCount =
    newState.groupsById.size === 0
      ? newState.messagesById.size
      : getToplevelMessageCount(newState);

  if (topLevelCount <= logLimit) {
    return newState;
  }

  const removedMessagesId = [];

  let cleaningGroup = false;
  for (const [id, message] of newState.messagesById) {
    // If we were cleaning a group and the current message does not have
    // a groupId, we're done cleaning.
    if (cleaningGroup === true && !message.groupId) {
      cleaningGroup = false;
    }

    // If we're not cleaning a group and the message count is below the logLimit,
    // we exit the loop.
    if (cleaningGroup === false && topLevelCount <= logLimit) {
      break;
    }

    // If we're not currently cleaning a group, and the current message is identified
    // as a group, set the cleaning flag to true.
    if (cleaningGroup === false && newState.groupsById.has(id)) {
      cleaningGroup = true;
    }

    if (!message.groupId) {
      topLevelCount--;
    }

    removedMessagesId.push(id);
  }

  return removeMessagesFromState(newState, removedMessagesId);
}

/**
 * Clean the properties for a given state object and an array of removed messages ids.
 * Be aware that this function MUTATE the `state` argument.
 *
 * @param {MessageState} state
 * @param {Array} removedMessagesIds
 * @returns {MessageState}
 */
function removeMessagesFromState(state, removedMessagesIds) {
  if (!Array.isArray(removedMessagesIds) || removedMessagesIds.length === 0) {
    return state;
  }

  const removedActors = [];
  const visibleMessages = [...state.visibleMessages];
  removedMessagesIds.forEach(id => {
    const index = visibleMessages.indexOf(id);
    if (index > -1) {
      visibleMessages.splice(index, 1);
    }

    removedActors.push(...getAllActorsInMessage(state.messagesById.get(id)));
  });

  if (state.visibleMessages.length > visibleMessages.length) {
    state.visibleMessages = visibleMessages;
  }

  if (removedActors.length > 0) {
    state.removedActors = state.removedActors.concat(removedActors);
  }

  const isInRemovedId = id => removedMessagesIds.includes(id);
  const mapHasRemovedIdKey = map => removedMessagesIds.some(id => map.has(id));
  const objectHasRemovedIdKey = obj =>
    Object.keys(obj).findIndex(isInRemovedId) !== -1;

  const cleanUpMap = map => {
    const clonedMap = new Map(map);
    removedMessagesIds.forEach(id => clonedMap.delete(id));
    return clonedMap;
  };
  const cleanUpObject = object =>
    [...Object.entries(object)].reduce((res, [id, value]) => {
      if (!isInRemovedId(id)) {
        res[id] = value;
      }
      return res;
    }, {});

  state.messagesById = cleanUpMap(state.messagesById);

  if (state.messagesUiById.find(isInRemovedId)) {
    state.messagesUiById = state.messagesUiById.filter(
      id => !isInRemovedId(id)
    );
  }

  if (isInRemovedId(state.currentGroup)) {
    state.currentGroup = getNewCurrentGroup(
      state.currentGroup,
      state.groupsById,
      removedMessagesIds
    );
  }

  if (mapHasRemovedIdKey(state.messagesPayloadById)) {
    state.messagesPayloadById = cleanUpMap(state.messagesPayloadById);
  }
  if (mapHasRemovedIdKey(state.groupsById)) {
    state.groupsById = cleanUpMap(state.groupsById);
  }
  if (mapHasRemovedIdKey(state.groupsById)) {
    state.groupsById = cleanUpMap(state.groupsById);
  }

  if (objectHasRemovedIdKey(state.repeatById)) {
    state.repeatById = cleanUpObject(state.repeatById);
  }

  if (objectHasRemovedIdKey(state.networkMessagesUpdateById)) {
    state.networkMessagesUpdateById = cleanUpObject(
      state.networkMessagesUpdateById
    );
  }

  return state;
}

/**
 * Get an array of all the actors logged in a specific message.
 *
 * @param {Message} message: The message to get actors from.
 * @return {Array} An array containing all the actors logged in a message.
 */
function getAllActorsInMessage(message) {
  const { parameters, messageText } = message;

  const actors = [];
  if (Array.isArray(parameters)) {
    message.parameters.forEach(parameter => {
      if (parameter && parameter.actor) {
        actors.push(parameter.actor);
      }
    });
  }

  if (messageText && messageText.actor) {
    actors.push(messageText.actor);
  }

  return actors;
}

/**
 * Returns total count of top level messages (those which are not
 * within a group).
 */
function getToplevelMessageCount(state) {
  let count = 0;
  state.messagesById.forEach(message => {
    if (!message.groupId) {
      count++;
    }
  });
  return count;
}

/**
 * Check if a message should be visible in the console output, and if not, what
 * causes it to be hidden.
 * @param {Message} message: The message to check
 * @param {Object} option: An option object of the following shape:
 *                   - {MessageState} messagesState: The current messages state
 *                   - {FilterState} filtersState: The current filters state
 *                   - {PrefsState} prefsState: The current preferences state
 *                   - {UiState} uiState: The current ui state
 *                   - {Boolean} checkGroup: Set to false to not check if a message should
 *                                 be visible because it is in a console.group.
 *                   - {Boolean} checkParentWarningGroupVisibility: Set to false to not
 *                                 check if a message should be visible because it is in a
 *                                 warningGroup and the warningGroup is visible.
 *
 * @return {Object} An object of the following form:
 *         - visible {Boolean}: true if the message should be visible
 *         - cause {String}: if visible is false, what causes the message to be hidden.
 */
/* eslint-disable complexity */
function getMessageVisibility(
  message,
  {
    messagesState,
    filtersState,
    prefsState,
    uiState,
    checkGroup = true,
    checkParentWarningGroupVisibility = true,
  }
) {
  // Do not display the message if it's not from chromeContext and we don't show content
  // messages.
  if (
    !uiState.showContentMessages &&
    message.chromeContext === false &&
    message.type !== MESSAGE_TYPE.COMMAND &&
    message.type !== MESSAGE_TYPE.RESULT
  ) {
    return {
      visible: false,
      cause: "contentMessage",
    };
  }

  const warningGroupMessageId = getParentWarningGroupMessageId(message);
  const parentWarningGroupMessage = messagesState.messagesById.get(
    warningGroupMessageId
  );

  // Do not display the message if it's in closed group and not in a warning group.
  if (
    checkGroup &&
    !isInOpenedGroup(
      message,
      messagesState.groupsById,
      messagesState.messagesUiById
    ) &&
    !shouldGroupWarningMessages(
      parentWarningGroupMessage,
      messagesState,
      prefsState
    )
  ) {
    return {
      visible: false,
      cause: "closedGroup",
    };
  }

  // If the message is a warningGroup, check if it should be displayed.
  if (isWarningGroup(message)) {
    if (!shouldGroupWarningMessages(message, messagesState, prefsState)) {
      return {
        visible: false,
        cause: "warningGroupHeuristicNotMet",
      };
    }

    // Hide a warningGroup if the warning filter is off.
    if (!filtersState[FILTERS.WARN]) {
      // We don't include any cause as we don't want that message to be reflected in the
      // message count.
      return {
        visible: false,
      };
    }

    // Display a warningGroup if at least one of its message will be visible.
    const childrenMessages = messagesState.warningGroupsById.get(message.id);
    const hasVisibleChild =
      childrenMessages &&
      childrenMessages.some(id => {
        const child = messagesState.messagesById.get(id);
        if (!child) {
          return false;
        }

        const { visible, cause } = getMessageVisibility(child, {
          messagesState,
          filtersState,
          prefsState,
          uiState,
          checkParentWarningGroupVisibility: false,
        });
        return visible && cause !== "visibleWarningGroup";
      });

    if (hasVisibleChild) {
      return {
        visible: true,
        cause: "visibleChild",
      };
    }
  }

  // Do not display the message if it can be in a warningGroup, and the group is
  // displayed but collapsed.
  if (
    parentWarningGroupMessage &&
    shouldGroupWarningMessages(
      parentWarningGroupMessage,
      messagesState,
      prefsState
    ) &&
    !messagesState.messagesUiById.includes(warningGroupMessageId)
  ) {
    return {
      visible: false,
      cause: "closedWarningGroup",
    };
  }

  // Display a message if it is in a warningGroup that is visible. We don't check the
  // warningGroup visibility if `checkParentWarningGroupVisibility` is false, because
  // it means we're checking the warningGroup visibility based on the visibility of its
  // children, which would cause an infinite loop.
  const parentVisibility =
    parentWarningGroupMessage && checkParentWarningGroupVisibility
      ? getMessageVisibility(parentWarningGroupMessage, {
          messagesState,
          filtersState,
          prefsState,
          uiState,
          checkGroup,
          checkParentWarningGroupVisibility,
        })
      : null;
  if (
    parentVisibility &&
    parentVisibility.visible &&
    parentVisibility.cause !== "visibleChild"
  ) {
    return {
      visible: true,
      cause: "visibleWarningGroup",
    };
  }

  // Some messages can't be filtered out (e.g. groups).
  // So, always return visible: true for those.
  if (isUnfilterable(message)) {
    return {
      visible: true,
    };
  }

  // Let's check all level filters (error, warn, log, …) and return visible: false
  // and the message level as a cause if the function returns false.
  if (!passLevelFilters(message, filtersState)) {
    return {
      visible: false,
      cause: message.level,
    };
  }

  if (!passCssFilters(message, filtersState)) {
    return {
      visible: false,
      cause: FILTERS.CSS,
    };
  }

  if (!passNetworkFilter(message, filtersState)) {
    return {
      visible: false,
      cause: FILTERS.NET,
    };
  }

  if (!passXhrFilter(message, filtersState)) {
    return {
      visible: false,
      cause: FILTERS.NETXHR,
    };
  }

  // This should always be the last check, or we might report that a message was hidden
  // because of text search, while it may be hidden because its category is disabled.
  if (!passSearchFilters(message, filtersState)) {
    return {
      visible: false,
      cause: FILTERS.TEXT,
    };
  }

  return {
    visible: true,
  };
}
/* eslint-enable complexity */

function isUnfilterable(message) {
  return [
    MESSAGE_TYPE.COMMAND,
    MESSAGE_TYPE.RESULT,
    MESSAGE_TYPE.START_GROUP,
    MESSAGE_TYPE.START_GROUP_COLLAPSED,
    MESSAGE_TYPE.NAVIGATION_MARKER,
  ].includes(message.type);
}

function isInOpenedGroup(message, groupsById, messagesUI) {
  return (
    !message.groupId ||
    (!isGroupClosed(message.groupId, messagesUI) &&
      !hasClosedParentGroup(groupsById.get(message.groupId), messagesUI))
  );
}

function hasClosedParentGroup(group, messagesUI) {
  return group.some(groupId => isGroupClosed(groupId, messagesUI));
}

function isGroupClosed(groupId, messagesUI) {
  return messagesUI.includes(groupId) === false;
}

/**
 * Returns true if the message shouldn't be hidden because of the network filter state.
 *
 * @param {Object} message - The message to check the filter against.
 * @param {FilterState} filters - redux "filters" state.
 * @returns {Boolean}
 */
function passNetworkFilter(message, filters) {
  // The message passes the filter if it is not a network message,
  // or if it is an xhr one,
  // or if the network filter is on.
  return (
    message.source !== MESSAGE_SOURCE.NETWORK ||
    message.isXHR === true ||
    filters[FILTERS.NET] === true
  );
}

/**
 * Returns true if the message shouldn't be hidden because of the xhr filter state.
 *
 * @param {Object} message - The message to check the filter against.
 * @param {FilterState} filters - redux "filters" state.
 * @returns {Boolean}
 */
function passXhrFilter(message, filters) {
  // The message passes the filter if it is not a network message,
  // or if it is a non-xhr one,
  // or if the xhr filter is on.
  return (
    message.source !== MESSAGE_SOURCE.NETWORK ||
    message.isXHR === false ||
    filters[FILTERS.NETXHR] === true
  );
}

/**
 * Returns true if the message shouldn't be hidden because of levels filter state.
 *
 * @param {Object} message - The message to check the filter against.
 * @param {FilterState} filters - redux "filters" state.
 * @returns {Boolean}
 */
function passLevelFilters(message, filters) {
  // The message passes the filter if it is not a console call,
  // or if its level matches the state of the corresponding filter.
  return (
    (message.source !== MESSAGE_SOURCE.CONSOLE_API &&
      message.source !== MESSAGE_SOURCE.JAVASCRIPT) ||
    filters[message.level] === true
  );
}

/**
 * Returns true if the message shouldn't be hidden because of the CSS filter state.
 *
 * @param {Object} message - The message to check the filter against.
 * @param {FilterState} filters - redux "filters" state.
 * @returns {Boolean}
 */
function passCssFilters(message, filters) {
  // The message passes the filter if it is not a CSS message,
  // or if the CSS filter is on.
  return message.source !== MESSAGE_SOURCE.CSS || filters.css === true;
}

/**
 * Returns true if the message shouldn't be hidden because of search filter state.
 *
 * @param {Object} message - The message to check the filter against.
 * @param {FilterState} filters - redux "filters" state.
 * @returns {Boolean}
 */
function passSearchFilters(message, filters) {
  const text = (filters.text || "").trim().toLocaleLowerCase();
  let regex;
  if (text.startsWith("/") && text.endsWith("/") && text.length > 2) {
    try {
      regex = new RegExp(text.slice(1, -1), "im");
    } catch (e) {}
  }

  // If there is no search, the message passes the filter.
  if (!text) {
    return true;
  }

  return (
    // Look for a match in parameters.
    isTextInParameters(text, regex, message.parameters) ||
    // Look for a match in location.
    isTextInFrame(text, regex, message.frame) ||
    // Look for a match in net events.
    isTextInNetEvent(text, regex, message.request) ||
    // Look for a match in stack-trace.
    isTextInStackTrace(text, regex, message.stacktrace) ||
    // Look for a match in messageText.
    isTextInMessageText(text, regex, message.messageText) ||
    // Look for a match in notes.
    isTextInNotes(text, regex, message.notes) ||
    // Look for a match in prefix.
    isTextInPrefix(text, regex, message.prefix)
  );
}

/**
 * Returns true if given text is included in provided stack frame.
 */
function isTextInFrame(text, regex, frame) {
  if (!frame) {
    return false;
  }

  const { functionName, line, column, source } = frame;
  const { short } = getSourceNames(source);
  const unicodeShort = getUnicodeUrlPath(short);

  const str = `${
    functionName ? functionName + " " : ""
  }${unicodeShort}:${line}:${column}`;
  return regex ? regex.test(str) : str.toLocaleLowerCase().includes(text);
}

/**
 * Returns true if given text is included in provided parameters.
 */
function isTextInParameters(text, regex, parameters) {
  if (!parameters) {
    return false;
  }

  return getAllProps(parameters).some(prop => {
    const str = prop + "";
    return regex ? regex.test(str) : str.toLocaleLowerCase().includes(text);
  });
}

/**
 * Returns true if given text is included in provided net event grip.
 */
function isTextInNetEvent(text, regex, request) {
  if (!request) {
    return false;
  }

  const method = request.method;
  const url = request.url;
  return regex
    ? regex.test(method) || regex.test(url)
    : method.toLocaleLowerCase().includes(text) ||
        url.toLocaleLowerCase().includes(text);
}

/**
 * Returns true if given text is included in provided stack trace.
 */
function isTextInStackTrace(text, regex, stacktrace) {
  if (!Array.isArray(stacktrace)) {
    return false;
  }

  // isTextInFrame expect the properties of the frame object to be in the same
  // order they are rendered in the Frame component.
  return stacktrace.some(frame =>
    isTextInFrame(text, regex, {
      functionName:
        frame.functionName || l10n.getStr("stacktrace.anonymousFunction"),
      source: frame.filename,
      lineNumber: frame.lineNumber,
      columnNumber: frame.columnNumber,
    })
  );
}

/**
 * Returns true if given text is included in `messageText` field.
 */
function isTextInMessageText(text, regex, messageText) {
  if (!messageText) {
    return false;
  }

  if (typeof messageText === "string") {
    return regex
      ? regex.test(messageText)
      : messageText.toLocaleLowerCase().includes(text);
  }

  if (messageText.type === "longString") {
    return regex
      ? regex.test(messageText.initial)
      : messageText.initial.toLocaleLowerCase().includes(text);
  }

  return true;
}

/**
 * Returns true if given text is included in notes.
 */
function isTextInNotes(text, regex, notes) {
  if (!Array.isArray(notes)) {
    return false;
  }

  return notes.some(
    note =>
      // Look for a match in location.
      isTextInFrame(text, regex, note.frame) ||
      // Look for a match in messageBody.
      (note.messageBody &&
        (regex
          ? regex.test(note.messageBody)
          : note.messageBody.toLocaleLowerCase().includes(text)))
  );
}

/**
 * Returns true if given text is included in prefix.
 */
function isTextInPrefix(text, regex, prefix) {
  if (!prefix) {
    return false;
  }

  const str = `${prefix}: `;

  return regex ? regex.test(str) : str.toLocaleLowerCase().includes(text);
}

/**
 * Get a flat array of all the grips and their properties.
 *
 * @param {Array} Grips
 * @return {Array} Flat array of the grips and their properties.
 */
function getAllProps(grips) {
  let result = grips.reduce((res, grip) => {
    const previewItems = getGripPreviewItems(grip);
    const allProps = previewItems.length > 0 ? getAllProps(previewItems) : [];
    return [...res, grip, grip.class, ...allProps];
  }, []);

  // We are interested only in primitive props (to search for)
  // not in objects and undefined previews.
  result = result.filter(
    grip => typeof grip != "object" && typeof grip != "undefined"
  );

  return [...new Set(result)];
}

function getDefaultFiltersCounter() {
  const count = DEFAULT_FILTERS.reduce((res, filter) => {
    res[filter] = 0;
    return res;
  }, {});
  count.global = 0;
  return count;
}

// Make sure that message has an execution point which can be used for sorting
// if other messages with real execution points appear later.
function ensureExecutionPoint(state, newMessage) {
  if (newMessage.executionPoint) {
    return;
  }

  // Add a lastExecutionPoint property which will place this message immediately
  // after the last visible one when sorting.
  let point = { checkpoint: 0, progress: 0 },
    messageCount = 1;
  if (state.visibleMessages.length) {
    const lastId = state.visibleMessages[state.visibleMessages.length - 1];
    const lastMessage = state.messagesById.get(lastId);
    if (lastMessage.executionPoint) {
      point = lastMessage.executionPoint;
    } else {
      point = lastMessage.lastExecutionPoint.point;
      messageCount = lastMessage.lastExecutionPoint.messageCount + 1;
    }
  }
  newMessage.lastExecutionPoint = { point, messageCount };
}

function messageExecutionPoint(state, id) {
  const message = state.messagesById.get(id);
  return message.executionPoint || message.lastExecutionPoint.point;
}

function messageCountSinceLastExecutionPoint(state, id) {
  const message = state.messagesById.get(id);
  return message.lastExecutionPoint
    ? message.lastExecutionPoint.messageCount
    : 0;
}

/**
 * Sort state.visibleMessages if needed.
 *
 * @param {MessageState} state
 * @param {Boolean} sortWarningGroupMessage: set to true to sort warningGroup
 *                                           messages. Default to false, as in some
 *                                           situations we already take care of putting
 *                                           the ids at the right position.
 * @param {Boolean} timeStampSort: set to true to sort messages by their timestamps.
 */
function maybeSortVisibleMessages(
  state,
  sortWarningGroupMessage = false,
  timeStampSort = false
) {
  // When using log points while replaying, messages can be added out of order
  // with respect to how they originally executed. Use the execution point
  // information in the messages to sort visible messages according to how
  // they originally executed. This isn't necessary if we haven't seen any
  // messages with execution points, as either we aren't replaying or haven't
  // seen any messages yet.
  if (state.hasExecutionPoints) {
    state.visibleMessages.sort((a, b) => {
      const pointA = messageExecutionPoint(state, a);
      const pointB = messageExecutionPoint(state, b);
      if (pointPrecedes(pointB, pointA)) {
        return true;
      } else if (pointPrecedes(pointA, pointB)) {
        return false;
      }

      // When messages have the same execution point, they can still be
      // distinguished by the number of messages since the last one which did
      // have an execution point.
      const countA = messageCountSinceLastExecutionPoint(state, a);
      const countB = messageCountSinceLastExecutionPoint(state, b);
      return countA > countB;
    });
  }

  if (state.warningGroupsById.size > 0 && sortWarningGroupMessage) {
    function getNaturalOrder(messageA, messageB) {
      const aFirst = -1;
      const bFirst = 1;

      // It can happen that messages are emitted in the same microsecond, making their
      // timestamp similar. In such case, we rely on which message came first through
      // the console API service, checking their id.
      if (
        messageA.timeStamp === messageB.timeStamp &&
        !Number.isNaN(parseInt(messageA.id, 10)) &&
        !Number.isNaN(parseInt(messageB.id, 10))
      ) {
        return parseInt(messageA.id, 10) < parseInt(messageB.id, 10)
          ? aFirst
          : bFirst;
      }
      return messageA.timeStamp < messageB.timeStamp ? aFirst : bFirst;
    }
    state.visibleMessages.sort((a, b) => {
      const messageA = state.messagesById.get(a);
      const messageB = state.messagesById.get(b);

      const warningGroupIdA = getParentWarningGroupMessageId(messageA);
      const warningGroupIdB = getParentWarningGroupMessageId(messageB);

      const warningGroupA = state.messagesById.get(warningGroupIdA);
      const warningGroupB = state.messagesById.get(warningGroupIdB);

      const aFirst = -1;
      const bFirst = 1;

      // If both messages are in a warningGroup, or if both are not in warningGroups.
      if (
        (warningGroupA && warningGroupB) ||
        (!warningGroupA && !warningGroupB)
      ) {
        return getNaturalOrder(messageA, messageB);
      }

      // If `a` is in a warningGroup (and `b` isn't).
      if (warningGroupA) {
        // If `b` is the warningGroup of `a`, `a` should be after `b`.
        if (warningGroupIdA === messageB.id) {
          return bFirst;
        }
        // `b` is a regular message, we place `a` before `b` if `b` came after `a`'s
        // warningGroup.
        return getNaturalOrder(warningGroupA, messageB);
      }

      // If `b` is in a warningGroup (and `a` isn't).
      if (warningGroupB) {
        // If `a` is the warningGroup of `b`, `a` should be before `b`.
        if (warningGroupIdB === messageA.id) {
          return aFirst;
        }
        // `a` is a regular message, we place `a` after `b` if `a` came after `b`'s
        // warningGroup.
        return getNaturalOrder(messageA, warningGroupB);
      }

      return 0;
    });
  }

  if (timeStampSort) {
    state.visibleMessages.sort((a, b) => {
      const messageA = state.messagesById.get(a);
      const messageB = state.messagesById.get(b);

      return messageA.timeStamp < messageB.timeStamp ? -1 : 1;
    });
  }
}

function getLastMessageId(state) {
  return Array.from(state.messagesById.keys())[state.messagesById.size - 1];
}

/**
 * Returns if a given type of warning message should be grouped.
 *
 * @param {ConsoleMessage} warningGroupMessage
 * @param {MessageState} messagesState
 * @param {PrefsState} prefsState
 */
function shouldGroupWarningMessages(
  warningGroupMessage,
  messagesState,
  prefsState
) {
  if (!warningGroupMessage) {
    return false;
  }

  // Only group if the preference is ON.
  if (!prefsState.groupWarnings) {
    return false;
  }

  // We group warning messages if there are at least 2 messages that could go in it.
  const warningGroup = messagesState.warningGroupsById.get(
    warningGroupMessage.id
  );
  if (!warningGroup || !Array.isArray(warningGroup)) {
    return false;
  }

  return warningGroup.length > 1;
}

exports.messages = messages;

// Export for testing purpose.
exports.ensureExecutionPoint = ensureExecutionPoint;
